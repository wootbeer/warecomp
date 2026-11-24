# Changelog

## [0.1.3 Beta] - 2026-11-24

### Fixed
- Android: the crash on phones with older Adreno drivers (Moto G60, Android 12),
  properly this time. 0.1.2 avoided the wrong copy and crashed earlier. On those
  phones the boot screens now take a few seconds longer to appear.

## [0.1.2 Beta] - 2026-11-24

### Fixed
- Android: crash a few seconds after starting on phones with older Adreno
  drivers (Moto G60, Android 12).

## [0.1.1 Beta] - 2026-11-23

### Added
- Android: frame rate and timing lines in the log (Share log), to help with
  slow phone reports.

### Changed
- Android: lighter graphics settings for new installs (Original 2x
  resolution, the game's own framerate, no MSAA). Existing installs keep their
  settings, change them on the Graphics tab.
- Android: the game keeps its busiest threads on the phone's fast cores, or
  uses Android's performance hints where the phone supports them. Much closer
  to full speed on mid-range phones (tested on a Moto G 5G 2024: from 36-55 fps
  to 51-59).

### Fixed
- Android: the on-screen MENU button and Back now open the settings in game.
  Esc opens them on PC too.
- Android: black screen with no sound at start on some phones with Android 12
  (Moto G60).

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
