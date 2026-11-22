# Android port: plan

Status (2026-11-22): builds from Android Studio and plays on the owner's device (first run,
controller). Next: touch controls.

## Decisions (agreed with the project owner)

- **Base: snprecomp's Android port** (wootbeer/snprecomp, `android/`, itself from the
  BattleTanx: Global Assault port), reused as is where it fits: `android/`,
  `cmake/Android.cmake`, `cmake/CrossCompile.cmake`, `cmake/android/`, `src/android/`
  (startup, file picker) and its Android library patches.
- **Android Studio workflow**: a second checkout (`C:\dev\warecomp-android`), its `android/`
  folder opened in Android Studio, Run (README.md). All Android code is behind `ANDROID` /
  `__ANDROID__` or `CMAKE_CROSSCOMPILING`; the desktop build is unaffected.
- **Ship the compiled game code in the APK**, no ROM (the build checks); the ROM is chosen in
  the launcher (Load ROM, the system document picker).
- **First milestone: controllers only** (touch controls next, as snprecomp's
  `touch_controls.cpp` / `TouchControlsView.java`).

## Toolchain (as snprecomp)

Android Gradle Plugin 9.3.2, Gradle 9.5.0, NDK r28c (28.2.13676358), SDK CMake 3.31.6,
`compileSdk`/`targetSdk` 35, `minSdk` 28, `arm64-v8a` only, `ANDROID_STL=c++_shared`, debug
builds `RelWithDebInfo`. SDL 2.32.10 and FreeType 2.13.3 built from source. App id
`io.github.wootbeer.warecomp`.

## What came over from snprecomp

Library patches, renumbered after this repo's own (the submodules are at the same commits):

| snprecomp | here |
| --- | --- |
| N64Recomp 0002 (float-to-int as x86 on ARM64) | N64Recomp 0001 |
| plume 0001, 0002, 0005, 0006, 0007, 0008, 0010 | plume 0006-0012 (0011's context adjusted for this repo's plume 0005) |
| rt64 0002-0006, 0008, 0022-0026, 0028 | rt64 0032-0043 |
| RecompFrontend 0013-0017 | RecompFrontend 0022-0026 |

Native code: `src/android/android_startup.cpp` (logcat and log.txt, tag `WA`; the window for
plume after the background; app storage as the working directory), `src/android/nfd_android.cpp`
(file dialogs through the document picker). Java: `WAActivity` (snprecomp's `SnPActivity`
without the touch overlay), `LogFiles`, `LogProvider`, `ShareLogActivity`.

## Specific to this game

- **Coroutines** (`src/game/coroutines.cpp`): bionic has no ucontext, so ARM64 gets its own
  stack switch (`wa_arm64_switch`: x19-x30 and d8-d15 saved on the stack, the stack pointer
  swapped). Checked under qemu-aarch64: a coroutine yields five times and finishes, and the
  caller's callee-saved registers survive every switch.
- **Crash handler**: `backtrace()` is API 33+ in bionic; Android walks the stack with
  `_Unwind_Backtrace`.
- **Launcher**: no Add to Steam.
- Lossless Scaling frame generation (`lib/lsfg`) builds on Android too; its "Load Lossless.dll"
  has nothing to load there.

## Next

1. Touch controls (snprecomp's, with Worms' own layout: move, jump, fire with power, aim,
   weapon panel).
