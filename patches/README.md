# patches/

C code compiled for the N64 (MIPS) and recompiled alongside the game, for
changes that replace a whole game function. A `RECOMP_PATCH` function here
takes the place of the original function of the same name.

There are none yet. With no `.c` file here the build skips this step
entirely (CMake builds an empty `PatchesLib`).

When one is added the build is automatic: `make` here produces `patches.elf`
(through WSL on Windows, since the Windows clang builds have no MIPS
backend), then N64Recomp recompiles it using `patches.toml` in the repo
root. `syms.ld` gives native runtime functions dummy addresses so the
patches can call them.

Smaller fixes don't live here: hooks that run native code at a point in a
game function are `[[patches.hook]]` entries in `wa.us.toml` (the menu's
per-frame UI pump is one), with any native code in `src/game/`.
