# Status log

How each piece of the port was found and fixed, newest first. PROGRESS.md
has the short version.

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

Result: N64Recomp recompiles all 3561 functions with no errors and no
warnings. Its only notes are three calls from boot into the overlay range
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

Built on Linux (Ubuntu 24.04, clang 18). Not run yet: this machine has no
GPU, and the game needs testing on the owner's PC.

### Next

- First run on Windows: does the main thread get through the frontend
  overlay to the title screen? Likely trouble spots, from btgarecomp's
  history: busy-wait loops that need `wa_yield_via_priority_drop`
  (src/main/scheduler_workaround.cpp), divisions by zero that the N64
  tolerated, and the clock rate.
- Confirm the EEPROM size when the game first saves.
