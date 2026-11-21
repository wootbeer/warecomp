# Status log

How each piece of the port was found and fixed, newest first. PROGRESS.md
has the short version.

## 2026-11-21, round 3: the language screen draws

The owner's Windows run (real GPU) was black too, so it wasn't llvmpipe.
After two frames the game stopped submitting graphics tasks (2 against
2083 audio tasks in 40 s). The main thread waited in `func_80032C78` for
frame buffer 0's busy flag, which the scheduler client clears on that
frame's "done" message, and that frame was never run: libultra's scheduler
(`OSSched` at 0x800483D0 in this build: interrupt queue +0x40, client list
+0x260, audio/graphics lists +0x264/+0x268, current RSP/RDP task
+0x274/+0x278) had the RSP and RDP idle and a frame waiting on its list.
It only starts a swap-buffer frame once its buffer is neither the current
nor the next VI buffer, and `func_80034280`, which it calls for the next
one, was unnamed: it is `osViGetNextFramebuffer` (the same as
`osViGetCurrentFramebuffer` beside it, reading `__osViNext` at 0x80036C84
instead of `__osViCurr`). Run as game code it read libultra's VI state,
which the runtime doesn't keep. Named, the game reaches its language
select screen, drawn by RT64. Every other function using those VI
contexts was already named.

## 2026-11-20, round 2: first runs (Linux, software Vulkan)

Run headless on this machine (no GPU): Xvfb and Mesa's llvmpipe Vulkan
driver, `--game wa` to skip the launcher, the ROM in
`~/.config/warecomp/wa.n64.us.z64`, and gdb for thread states. The launcher
draws correctly (checked with a screenshot). In game, each run got further:

1. **Exited at once**: "Encountered break at original vram 0x80000438". The
   boot stub calls main and then has a `break`. In this runtime osStartThread
   from the boot thread returns (on the N64 it never does), so main returned
   into the `break`, and the runtime exits on one. Fixed with an instruction
   patch: `jr $ra` there, so the boot thread just ends.
2. **Link errors** for `__osSiRawStartDma_recomp` and two more: the function
   calling them, 0x8002ADF0, is `osContInit` (timer wait, then the SI
   requests; just before `__osContGetInitData`). Named by hand.
3. **Main thread spinning** in `func_80032D0C` ("wait for the next frame":
   spins until the retrace counter at 0x80048768 changes). Busy-waits with no
   OS call never give way in this runtime. Hooked to
   `wa_yield_via_priority_drop` (btgarecomp's fix), and the same for
   `func_80032D28` (spins on two flag bytes). 0x80032D28 had been named
   `n_alLoadParam` from a call-target name in another signature; `n_al*`
   names are dropped now (the game uses the plain `al*` library).
4. **Deadlock in the yield**: entered once, never returned. It drops the
   thread to priority 0 for the hand-off; this game's idle thread is at 0
   too (`osSetThreadPri(0, 0)`, then a `j .` loop), and the runtime only
   switches to a strictly higher priority, so the dropped thread never got
   the CPU back. It drops to 1 here (`kHandOffPriority`).
5. **Crash reading 0xA4500004** (AI length register) in 0x80026FF0:
   `osAiGetLength`, too short for a signature. Named by hand, and a check of
   every unnamed boot function that reads a hardware register found one
   more: 0x80034100 reads SP_STATUS, `__osSpDeviceBusy`; the function that
   had that name, 0x8002DC70, reads SI_STATUS and is `__osSiDeviceBusy`.
   Hand names now take priority over signature names.
6. **"Failed to find function at 0x80057440"**, a virtual call into the
   frontend overlay. The function was in the symbol file; the runtime didn't
   know the overlay was loaded. This runtime doesn't register overlays on a
   DMA, and its startup registers every code section in the ROM's first MB
   at a straight-copy address: both overlays, wrongly (frontend at
   0x80038140, the match at 0x8007E4B0). `src/game/overlays.cpp`: the game
   entry's `on_init_callback` removes those, and hooks right after the
   loader's two DMAs (0x800043F0, 0x80004514) register the overlay just
   loaded in place of the other.
7. **"Failed to find function at 0x800048B0"**, a boot function reached only
   through a pointer, after the idle thread's `j .`. Two general rules in
   `tools/make_syms.py`: a stack prologue after any `j`/`jr` + delay slot
   starts a function, and pointers in data (vtables, callback tables) to a
   plausible function start are function starts (runs of 3+ code pointers
   are switch tables and are skipped).
8. **Main thread spinning** in `func_80032C78` (starting a display list:
   waits for the frame buffer's busy flag). Same hook.

Also found while building: the game overlay's code had been disassembled at
VRAM 0x8004D850 instead of 0x8004D860 (a hand-typed number), so all its
spimdisasm labels were 0x10 off; `tools/regen_syms.sh` now computes each
range's VRAM from the segment table. And no function may start in a delay
slot (one did, leaving a `goto` to a label that was never emitted). 2717
functions now.

**Where it is now**: no crash or hang in two minutes. In 45 seconds the game
started 2647 RSP tasks and its threads cycle normally, but swapped buffers
only twice and the screen stays black. llvmpipe renders on the CPU, so this
may just be very slow frames; the next step is a run on a real GPU
(Windows, the owner's PC), and if it's still black, find what the frontend
is waiting for.

## 2026-11-20, round 1: the ROM's layout, the symbol file, the first build

Started from the owner's dump, `Worms Armageddon (USA) (En,Fr,Es).n64`
(byte-swapped; `tools/normalize_rom.py` makes the big-endian `.z64`).

| | |
|---|---|
| Internal name | `WORMS ARMAGEDDON` |
| Game code | `NADE` (version 0) |
| Entry point | `0x80000400` (the usual libultra layout: ROM offset = vram - 0x7FFFF400) |
| CRC1 / CRC2 | `0x13E959A0` / `0x0E93CAB0` |
| Size | 12 MiB |
| SHA-1 (`.n64` as dumped) | `04D60D848A5CB9923EB95B016EEBB6AF829296F2` |
| SHA-1 (normalized `.z64`) | `3B17B36BA91737B86AA56ECB1B48A89444B53572` |
| XXH3-64 (normalized, the launcher's check) | `0xC4642B284910404F` |

### Code layout: a resident segment and two overlays

The boot stub (0x80000400) zeroes the BSS at 0x80038140-0x80048FE0 and calls
`func_800030B0`, which starts the idle thread; that starts the main thread at
`0x8000405C` (stack top 0x80400000). The game is C++: nearly every call
goes through a vtable.

All code is in the first ~1 MB of the ROM, but only the first 0x38D40 bytes
are resident. The main thread's loop at 0x800043A0 loads two overlays to the
**same** address, 0x80048FE0, in turn:

| Segment | ROM | VRAM | Size | Code (ROM) | What |
|---|---|---|---|---|---|
| `.boot` | 0x001000 | 0x80000400 | 0x37D40 | 0x1000-0x1040, 0x3CB0-0x16770, 0x18D60-0x34FC8 | libultra, sound driver, engine core |
| `.front` | 0x038D40 | 0x80048FE0 | 0x46370 | 0x3E790-0x7EABC | frontend (menus); entry `front_80050364` |
| `.game` | 0x07F0B0 | 0x80048FE0 | 0x67FE0 (DMA 0x69110) | 0x83930-0xE6C08 | the match; then `func_80006520` runs it |

The loader: invalidate caches over the range, then DMA (`func_8002FEB0`),
e.g. `0xB0038D40 -> 0x80048FE0, 0x46370 bytes`, then call into the overlay
just loaded. Each overlay starts with a `ptab` marker and its data (jump
tables, vtables), with the code after. Found by voting: for every `jal` in a
ROM block, which ROM-to-VRAM offset makes its target land on a function
prologue; the blocks split cleanly into `+0x800102A0` (frontend) and
`+0x7FFC9F30` (game), both meaning VRAM 0x80048FE0 at their start. Then
confirmed by the loader's own constants.

The rest of the ROM (0xE8000 on) has no code.

### RSP microcode inside the boot segment's code

Boot's code range has a hole at 0x80015B70-0x80018160 (ROM 0x16770-0x18D60):
RSP code (COP0 reads of the DMA-busy register, semaphore writes, vector
stores). The audio thread (0x80022620) gives the audio task ucode text
`0x80015B70` and data `0x800352C0`; another setup (0x80022440) uses
`0x800167D0` (rspboot, 0xD0 bytes) and `0x800168A0` (graphics ucode text,
0x18C0 bytes, handled by RT64's HLE).

The audio microcode is 0xC60 bytes and its data's dispatch table matches
btgarecomp's `n_aspMain` value for value (including the odd 0x02B0 entry),
so `n_aspMain.us.toml` is btgarecomp's config with this ROM's offsets.
RSPRecomp builds it without complaint.

### Naming libultra

N64Recomp needs libultra's functions by name, to replace or skip them.
`tools/sigmatch.py` reads n64sym's signature data (the `.sig` file only:
name, size, two CRC32s, relocations masked) and matches it against the boot
segment. It finds libultra 2.0-era threads, message queues, PI/SI/VI
managers, EEPROM (`osEeprom*`, so the game saves to a cartridge EEPROM, and
has no Controller Pak or Rumble Pak code), the audio library (`al*`) and
the Software Creations sound driver (`Mus*`). Signatures under 0x24 bytes are
dropped: they match ordinary game code by chance (`__dummy`, `ptstart`,
`n_alSeqpDelete` landed in engine code). The overlays have no library code.

Hand-named (assembly with no signature):
- `0x8002ADC0` `osWritebackDCacheAll` (cache op 0x01 over 0x80000000-0x80002000)
- `0x8002B6F0` `__osExceptionPreamble` (`lui/addiu k0; jr k0`), `0x8002B700`
  `__osException`, running to `__osEnqueueAndYield` at 0x8002BD6C

### Function boundaries

`tools/make_syms.py` builds `WASyms/wa.us.syms.toml` from spimdisasm's
functions in each code range, plus:
- every `jal` target (from a segment's own code, from the overlays into
  boot, and from boot into whichever overlay has a plausible function start
  there: the loader calls the overlay it just loaded);
- a stack-frame prologue right after `jr $ra` and its delay slot (a `nop`
  delay slot is skipped);
- signature matches as whole functions (no split inside; one starts right
  after);
- a merge pass: a branch, or a `j` to somewhere that isn't a function start,
  that leaves its function means the boundary is wrong, so the two are
  merged, repeated until none leaves. A call target that another function
  jumps into (shared tail code: `func_8001DE30`, `func_8001DE50`,
  `game_80076D60`) is merged into that function and listed again as an
  overlapping entry of its own. This N64Recomp only applies `manual_funcs`
  to ELF input, so btgarecomp's way (round 54 there) doesn't apply here;
  an overlapping entry in the symbol file does the same.
- hand corrections (`MANUAL`): `0x800145E8` enters with `j 0x80014604`, the
  test at the bottom of its own loop, so 0x80014604 is not a function;
  0x80015B60 is a two-instruction leaf.

Overlay functions are named `front_XXXXXXXX` and `game_XXXXXXXX` (the two
overlays share addresses). `tools/regen_syms.sh` reruns the whole thing.

Result: N64Recomp recompiles all functions with no errors and no
warnings (3561 in this round; 2717 after round 2's corrections). Its only notes are three calls from boot into the overlay range
(the loader), which can't be resolved statically because both overlays
have a function there; they go through the runtime lookup, which is right
for overlays. The runtime loads overlays itself on each ROM DMA.

### The frontend

From btgarecomp (src/main, the launcher and settings menus, RT64, plume and
RecompFrontend patches, lib/lsfg frame generation, crash handler, Add to
Steam, packaging), without its netplay (and the runtime patches only netplay
used), cheats, sound packs and BattleTanx-specific fixes. Settings: Local
Multiplayer and Mouse Stick (General), Screen Filter, Bloom and Frame
Generation (Graphics), plus the frontend's own tabs.

- Save type `Eep4k` (cartridge databases list a 4 Kbit EEPROM; unconfirmed in
  game).
- The menu's per-frame UI pump: btgarecomp replaced its game's swap routine
  with a MIPS patch. Here the only `osViSwapBuffer` call is in libultra's
  scheduler (`func_80033948`, the task-complete handler), and a
  `[[patches.hook]]` right after the call returns (0x800339C4) runs
  `recomp_run_ui_callbacks`. That spot is reached only after a swap (the
  early-outs branch to 0x800339C8), and the registers the pump uses are
  caller-saved ones the call already clobbered. So `patches/` has no
  patches for now.
- App icon: an original placeholder, a "WA" monogram
  (`tools/make_app_icon.py`). Steam artwork URLs are left empty until the
  owner chooses them.
- Submodules pinned to btgarecomp's commits, so its library patches apply
  unchanged (they do).

Built on Linux (Ubuntu 24.04, clang 18). First runs: round 2.
