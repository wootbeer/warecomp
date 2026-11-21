# Status log

How each piece of the port was found and fixed, newest first. PROGRESS.md
has the short version.

## 2026-11-21, round 14: audio crackling

The owner heard crackling every so often. Logging what reaches
`queue_samples` (SDL's dummy driver, which plays at the device rate): the
game queued 552 stereo frames 60 times a second at 22050 Hz, 1.5 times real
time, so the queue sat at the 200 ms latency cap and about 20 of every 60
buffers were dropped there, each one a jump in the waveform. No sample in
the game's output was at full scale, so it wasn't clipping.

The audio thread (`func_80022620`) wakes once a frame, and before making a
buffer it calls `osAiGetStatus`: with the AI FIFO full (bit 31) it skips the
round. On the N64 that paces it to the playback rate (552 frames is about
22050 / 40). The runtime's `osAiGetStatus` never reports full, so it made a
buffer every frame. (Its size comes from `func_80022820`, which adjusts it by
184 frames either way from how much `osAiGetLength` says is left.)

A hook after the `osAiGetStatus` call (0x800226A4, before `$v0` is copied to
`$s0` in the next delay slot) reports full while 60 ms or more is queued
(`wa_audio_fifo_full`, src/main/main.cpp). Measured again: 43 buffers a
second, of 552 and 368 frames (the game's own adjustment at work), the queue
steady at 45-53 ms before each, nothing dropped. Latency also went from
about 200 ms to about 60 ms.

## 2026-11-21, round 13: name tags lost their bottom border (0029 reworked)

Round 12's clamp took the bottom row off every small background: the name
and health tags, and the menus' frames (the owner's report). Logged in
bg1Cyc, the clamp line was right (line 8 of a 9-line tag frame), yet RT64
drew line 7 there; clamping one line lower brought back the bottom-edge
sliver. Rather than depend on how RT64 samples a clamped tile, 0029 now
leaves sampling alone and changes the data: in a frame's last slice, the
TMEM line after the last line shown gets a copy of that line (one extra
1-line load from the image), so whatever the lower part of the last line
samples is that same line. Three new levels: tag borders complete, the
bottom edge clean (0-21 of 1140 columns differ, all at sprite or landscape
edges), the landscape still in step.

## 2026-11-21, round 12: a line of wrong pixels along the bottom edge

The owner saw, sometimes, a line of stray pixels along the bottom of the
window. Their full-size screenshot (720 rows, 3 per game line) showed it
was the lower two of the last game line's three rows, and what they showed
was the level just below the screen (down the left edge, the level's
hazard border where the rows above were sky). My own captures had it too
(280-400 of 1140 columns differing between the top and bottom of the last
line, on three of four levels); I had missed it.

Cause: S2DEX bg1Cyc draws a background in slices and, with bilerp, loads
one image line more than it draws. On the N64 each pixel samples at its
top, so that line only ever gets a zero weight. RT64 drawing at a higher
resolution samples further down in the lower part of each line: within the
frame that's the frame's own next line and looks right, but on the frame's
last line it's the image past the frame (the next line of the level, or its
top where the image wraps).

`lib-patches/rt64/0029`: the last slice of a background clamps T at the
last image line the frame shows (reworked in round 13). New captures on three levels: 0 columns
differ, and the landscape still lines up (round 10's check).

## 2026-11-21, round 11: wave rows cut on the left, the landscape mirrored on the right

The owner's next widescreen run: the top rows of the water (sand waves on
that level) stopped short of the window's left edge, and on the right the
level was mirrored about the old screen edge.

**Water and other repeating images.** Each wave row is one draw-list entry
(type 4, drawn by `func_8000733C`); for an image that repeats horizontally
it wraps the first copy's x into (-width, 0] (0x800074D0-0x800074F8) and
`func_80007C74` draws copies from there to the right edge (448 since
round 7). Starting in (-128, 0] left a gap of up to one copy on the
left. A hook at 0x80007500, where both wrapping paths meet, steps the start
back one copy at a time until it's at or past -128 (the frame list then
shows each row starting at -128, its first copy cut by the round-7 clip).

**Mirroring.** Measured on a frame-synchronous capture (round 10's
method): past game x 320, the landscape on screen was the expected image
mirrored about x = 319.5, within a few levels of error. The game sends one
BG_1CYC for the whole landscape, so it was RT64's: `RDP::drawRect` records
the range of texels a rect samples, cut to the scissor, and the texture
cache decodes only that range. The game's scissor is 0-320, so the decoded
texture stopped at the texel under x = 320, while 0025 draws on past it;
the sampler (the BG render tile is set to mirror) mirrored beyond the last
decoded texel. `lib-patches/rt64/0028`: for a scissor spanning the whole
colour image without origins (the case 0025 widens), the sampled range
isn't cut horizontally. Checked on three new levels: past x 320 the
landscape matches the image directly (average error 3-11 per pixel), and
no mirror fits.

## 2026-11-21, round 10: floating worms in widescreen

The owner saw some worms floating over the ground (or sunk into it) in
widescreen, worse towards the sides. The game's own data had them standing
on the landscape. A screenshot taken while the game thread was stopped at a
frame's graphics task, with RDRAM dumped at the same moment, compared the
landscape on screen with the landscape image that frame's BG_1CYC draws
(a 960-pixel-wide CI8 image, frame -128 to 448): the sprites sat where the
display list put them, but the landscape at the sides was about 45 game
pixels off, towards the centre on both sides. The whole background was
scaled about the centre, by about 3/4 (the 4:3 to 16:9 ratio).

RT64's framebuffer renderer takes any rectangle that covers the whole
scissor width for a full-screen background and stretches it to the wide
target. The game draws its landscape for widescreen itself (round 7), as
strips reaching past both sides of the screen, so it got stretched while
the sprites didn't.

`lib-patches/rt64/0027`: a rectangle reaching past the colour image's sides
(left of 0 or right of its width) is drawn for a wider view and keeps its
coordinates; full-screen rectangles inside the screen are stretched as
before. With it, the same check finds the landscape at shift 0 across the
window (left side, middle and right), and a blend of the expected
landscape over the screenshot lines up everywhere.

## 2026-11-21, round 9: a D3D12 crash in RT64's texture thread

The owner's Windows build crashed starting a level, in the "RT64 Texture"
thread reading address 0. Symbolized with llvm-symbolizer against the
build's .pdb (addresses need the image base, 0x140000000 + the crash log's
offset): `plume::setObjectName` (plume_d3d12.cpp) from
`TextureCache::uploadThreadLoop` (rt64_texture_cache.cpp), which creates a
texture at the upload's size and then names it. D3D12 refuses a size of 0
or past 16384, the texture is null, and naming it crashes. Not reproduced
on Linux/Vulkan in four level starts.

`lib-patches/rt64/0026`: the texture cache keeps a decoded texture between
1 and 8192 on each side, and the S2DEX object rectangle (0024) skips
sprites whose image size is under a texel or has the top bit set (the
game sends some: one frame had a sprite with a negative width). Which draw
asked for the bad size isn't confirmed; this keeps any of them from
crashing.

## 2026-11-21, round 8: widescreen, the old 4:3 edges in RT64

The owner's widescreen run: terrain and backgrounds filled the window, but
name tags, sprites crossing the edge and the water were cut at the old 4:3
edges. A frame's display list (dumped while a tag crossed the edge) was
right: boxes from -128 to 448, none cut at 0 or 320. The cut was RT64's:

- With Expand, a scissor without extended origins is scaled about the
  centre, so the game's full-screen scissor (0-320, set once per frame by
  `func_800095AC`) became the middle 4:3 area and clipped every draw at
  render time.
- S2DEX `bg1Cyc` also cuts each background frame to the scissor, as the
  microcode does (the name tags are BG_1CYC objects).

Anchoring the scissor to the window edges (gEXSetScissor-style origins)
would have tied the playfield to RT64's *extended* aspect, which is the
frontend's HUD Ratio setting; so instead `lib-patches/rt64/0025`: at render
time a draw whose scissor spans the whole colour image width and has no
origins gets the whole wide target horizontally, and bg1Cyc doesn't cut
such a scissor's frame horizontally. Smaller scissors and anchored ones are
unchanged. Tags, worms and crates now cross the old edges on both sides.

Left: the wave strip along the bottom stops short of the left window edge
(about game x -30) in one frame checked; not looked into yet.

## 2026-11-21, round 7: widescreen

With RT64's Expand, only some backgrounds reached into the sides: the game
draws its level in tiles from x = 0 rightwards, and RT64 showed the last
tile's overhang past 320; nothing was drawn at negative x.

How a match draws: the per-frame render (`game_800520C0`) has
`game_8005FC70` walk a draw list of items in world coordinates. Each is
projected by `game_800614CC` (subtract the camera at +4/+8/+0xC, scale by
focal / (depth + focal) for parallax; it never culls), and the camera is
the view's top-left corner (read at runtime: camera x 1410, items from
-188 to past 1557). So the whole scene is submitted, off-screen parts
included, and only the drawing library in the resident code (seven
functions building S2DEX2 commands) culls and clips to fixed bounds:
x in [0, 320], y in [0, 240].

`src/game/widescreen.cpp` and wa.us.toml widen those bounds horizontally
to [-128, 448] (`WA_WS_EXTRA`, fixed; enough for about 21.6:9, and RT64 clips
what's past a narrower window's edge):

| Function | Does | Changed |
|---|---|---|
| `func_8000733C` (blit wrapper) | cull | right 0x141 +128 (patch); left `x + w < 0` +128 (hook) |
| `func_8000762C` (terrain strips) | cull, right clip per 32-px strip | right constants +128; left test +128 |
| `func_800084EC` (sprites) | cull | right +128; left `x < -w` -128 |
| `func_80008010` (blitter) | cull, clip both sides | right constants +128; left test; clip block replaced |
| `func_80007C74` (background) | cull, clip both sides | right +128; clip block replaced |
| `func_80009320` (background) | cull, clip both sides | right +128; clip block replaced |

The replaced clip blocks keep the originals' logic with the edges at -128
and 448, and their delay-slot side effects. The level now fills a 16:9
window; worms by the old 4:3 border draw whole. The HUD stays in the middle
4:3 area. The menus look as before.

Not done yet: the HUD following the Graphics tab's HUD Ratio (RT64 rect
alignment, as btgarecomp does). Unchecked: `func_80008D18` (a full-screen
background, not used in a match so far), `func_80008F60`/`func_8000914C`
(`sltiu 0x13F`) and `func_8000EC44`.

## 2026-11-21, round 6: the worms were invisible (RT64's S2DEX object rectangles)

The owner could play a match, but the worms were invisible (name tags and
health showed). The game draws everything with **S2DEX2**, the 2D sprite
microcode (the graphics microcode at 0x800168A0; opcodes 0x09 BG_1CYC,
0x07 OBJ_LDTX_RECT, 0x0B OBJ_RENDERMODE...). A frame's display list
(decoded from an RDRAM dump taken at osSpTaskStartGo) has 38 BG_1CYC
(terrain, scenery, tags, HUD) and 22 OBJ_LDTX_RECT: the worms (16 x 12-17
texels, CI8, some with G_OBJ_FLAG_FLIPS) and small particles, all in
1-cycle mode with object render mode 0x9 (NOTXCLAMP | BILERP). Their
pixel data was in RDRAM, and RT64's written-back frame had no worms
either: RT64's `objLoadTxRect` loaded the texture and then drew nothing
(a TODO and an assert compiled out in release; upstream RT64 the same),
and G_OBJ_RECTANGLE wasn't mapped.

`lib-patches/rt64/0024-s2dex-object-rectangles.patch` implements the
unrotated object rectangle (`doObjRectangle`): render tile from the
uObjSprite (format, size, TMEM line and address, palette; clamp unless
NOTXCLAMP), one texture rectangle at objX, objY sized imageW/H divided by
the scale, S/T flips, copy mode's inclusive edge. Used by
G_OBJ_LDTX_RECT and G_OBJ_RECTANGLE (S2DEX 0x03, S2DEX2 0x01). The worms,
crates and debris now show. Rotated and matrix sprites (G_OBJ_SPRITE,
G_OBJ_LDTX_SPRITE, the _R variants) are still not implemented; this game's
frames haven't used them so far.

## 2026-11-21, round 5: into a match; the match's coroutines

The owner's run froze at a black screen on starting a single-player quick
match. Reproduced by pressing A through the menus (xdotool on Xvfb). Three
things, in order:

1. **Waiting for the sound to stop**: `func_8000B55C` and `func_8000B5BC`
   (stopping the music as the frontend hands over) loop on `func_8001FC28`,
   the sound driver's count of channels still playing, until it's 0. Only
   the audio thread brings it down. Same yield hook in both loops
   (0x8000B584, 0x8000B5E8).
2. **"Cannot partially unload section"**: my overlay hook unloaded
   0x80048FE0 + 0x69110 (the DMA size), which ends exactly where the
   modules start; the runtime's overlap test counts touching ranges, so it
   tried to unload the loaded module and stopped. Unloads the largest
   section's size (0x67FE0) now.
3. **Segfault clearing the match's BSS**: right after loading the match the
   loader zeroes 0x1120 bytes at 0xB00E7090, a cartridge ROM address (the
   linker gave the ROM address of the BSS). On the N64 writes to cartridge
   ROM are ignored; here it lands far outside memory. The `beqz $s0` that
   skips the clear (0x80004514) is now an unconditional branch.

Then the match ran (level, teams, turn timer, CPU turns, no input: no
crash in 100 s), but the **first A press in a match crashed** with a null
`this` in `game_8009E818`, 16 levels into a recursion of `game_800AAD00`.
That is a coroutine system on the game's own setjmp/longjmp
(`func_8001DA10` saves s0-s7, sp, fp, ra and returns 0; `func_8001DA50`
restores them and returns its argument or 1), used only by three functions:
`game_800AAD00` recurses to carve the coroutine's stack (0x418 a level,
canary 0xAC1DCAFE), `game_800AAC9C` starts it (setjmp(obj + 0x48), runs the
body, the virtual at vtable +0x54, then longjmps back), `game_800AAC68`
yields (setjmp(obj + 0x88), longjmp(obj + 0x48, 1)), and `game_800AABB0`'s
state-1 path resumes (setjmp(obj + 0x48), longjmp(obj + 0x88, 1)).
Recompiled, a longjmp reloads the MIPS registers but execution on the host
carries on where it was, with another frame's registers.

`src/game/coroutines.cpp` runs each body on a fiber of its own (Windows
fibers; ucontext on Linux and Steam Deck) on the same game thread, so the
runtime's per-thread state is unaffected. Hooks replace the start and yield
functions and the resume path; they save and restore exactly the registers
the game's setjmp does and write N64 memory as the original code does (the
jmp_bufs, stack slots, the object's state fields). The game's stack carving
is left as it is. With it, pressing A through a match no longer crashes.
Android has no ucontext: it needs a small aarch64 context switch of its
own before matches work there (the file stops the build on Android until
then).

## 2026-11-21, round 4: past the language screen, the main menu

The owner's run crashed on confirming the language: "Failed to find
function at 0x800BAD30" (the Windows crash log shows the exit that
follows). Nothing was mapped there: it is in one of seven more code blocks.
The frontend's `front_8004F574` picks one by index and DMAs it to
0x800B20F0, right after the match overlay's BSS:

| ROM | Size | Code (offset in the module) |
|---|---|---|
| 0x0E81C0 | 0xA340 | 0x8C10-0x9638 |
| 0x0F2500 | 0xB4D0 | 0x9D70-0xA7A4 |
| 0x0FD9D0 | 0xAD30 | 0x95F0-0xA024 |
| 0x108700 | 0xB290 | 0x9AF0-0xA524 |
| 0x113990 | 0xABD0 | 0x9430-0x9E7C |
| 0x11E560 | 0xB200 | 0x9A60-0xA4AC |
| 0x129760 | 0xB910 | 0xA180-0xABB4 |

Back to back in ROM (the last ends at 0x135070). Each starts with the
`ptab` marker, then data, then ~216 leaf functions: `lui`/`jr $ra`/`addiu`
getters returning the address of an item in the module's data, plus one
indexed lookup. No calls or branches, so a function starts after every
`jr $ra` and its delay slot. Probably the menus' text and other data in
each language or for each screen; named `.mod0`-`.mod6` until that's known.
Found from the crash's pointer (a vtable at ROM 0xF0DA4) and then the
loader's table of cartridge addresses.

They are sections in the symbol file now (4232 functions), and a hook after
the loader's DMA (`wa_load_module`, src/game/overlays.cpp) registers the one
loaded. The runtime's first-megabyte registration had also put the first two
at wrong addresses, so the startup cleanup now covers the whole megabyte past
the resident segment. With that, confirming the language reaches the main
menu.

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
