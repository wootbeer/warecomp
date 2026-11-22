# Changelog

## [0.1.0 Beta] - 2026-11-22

### Changed
- Widescreen by default, showing more of the level at the sides instead of
  stretching. The HUD sits at the screen edges. Aspect Ratio on the Graphics tab
  brings back 4:3.
- Higher resolution and anti-aliasing by default.

### Added
- Runs natively on Windows and Android, plus Linux / Steam Deck builds,
  untested on real hardware, let me know.
- Widescreen and ultrawide, with the HUD following the HUD Ratio setting.
- Hide Level Border on the Graphics tab, turns off the striped border around
  the level.
- Cheats tab: the game's own cheat codes as toggles (PowPow and World War 3
  schemes, Intermediate Scheme instead of Tournament, Extra Team Choice, All
  Missions and Training).
- Hacks tab: GameShark codes (All Missions, All Training, Basic Training,
  Infinite Health).
- Controller and keyboard, fully remappable, and Mouse Stick.
- Local Multiplayer setting, a controller for each player.
- CRT, Scanlines and LCD screen filters, and bloom.
- Add to Steam on the launcher, with artwork.
- Restart in the menu and in the quit prompt.
- Portable by default: settings, saves and mods stay in the game's folder.
- Command line options: `--rom`, `--game wa`, `--set`, `--fullscreen`,
  `--windowed`, `--list-settings` and `--help`.
- crash_log.txt generates upon crash.
- Android: on-screen N64 controller with size and opacity settings, Share log
  from the app icon, and updates through Obtainium.

### Fixed
- A line of stray pixels the game draws along the bottom of the screen is
  cropped (Crop Bottom Line on the Graphics tab, on by default).
