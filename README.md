# Worms Armageddon (1999 N64) Recompiled and Port

You will need the game file from a licensed copy in order to play it.

A native PC and Android port of Worms Armageddon for the N64, made by statically
recompiling the N64 game with [N64Recomp](https://github.com/N64Recomp/N64Recomp)
and running it on [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime)
with [RT64](https://github.com/rt64/rt64) for rendering. A sister project of
[BattleTanx: Global Assault Recompiled](https://github.com/wootbeer/btgarecomp) and
[Sin and Punishment Recompiled](https://github.com/wootbeer/snprecomp).

Unofficial, and not affiliated with the rights holders. **No game data is
included.** You need your own dump of the game to build or play this.

**Beta:** it plays, but expect bugs. If you find one, open an issue and attach
`crash_log.txt` if the game made one.

## Features
- Runs natively on Windows and Android. Linux and Steam Deck builds are untested
  on real hardware, let me know.
- Widescreen and ultrawide, showing more of the level, with the HUD at the
  screen edges (follows the HUD Ratio setting).
- High resolutions and anti-aliasing.
- Hide Level Border: turns off the striped border around the level.
- Crop Bottom Line (on by default): hides a line of stray pixels the game draws
  along the bottom of the screen.
- The game's cheat codes as toggles on the Cheats tab, no codes to type, and
  GameShark codes on the Hacks tab.
- Controller and keyboard, fully remappable. Mouse Stick to move the stick
  with the mouse.
- Local Multiplayer setting, a controller for each player (untested so far).
- CRT, Scanlines and LCD screen filters, and bloom.
- Add to Steam from the launcher, with artwork.
- Restart from the menu.
- Android: plays with a controller or a handheld's own controls, and an
  on-screen N64 controller with adjustable size and opacity.

## Game Files
You need your own data file: Worms Armageddon (USA) (En,Fr,Es).
SHA-1 (as dumped, byte-swapped `.n64`):
`04D60D848A5CB9923EB95B016EEBB6AF829296F2`

`.z64` and `.v64` dumps of the same release work too.

Start the game, pick **Load ROM** and select your file. It only has to be done
once.

Settings, saves and mods are kept in the game's own folder (it's portable):
a new install puts a `portable.txt` there on first start. Delete it to keep
them in your user folder instead (`%LOCALAPPDATA%\warecomp` on Windows,
`~/.config/warecomp` on Linux).

## Download
Get the latest build from the [Releases](https://github.com/wootbeer/warecomp/releases) page.
- **Windows:** unzip it anywhere and run `WormsArmageddonRecompiled.exe`.
- **Linux / Steam Deck (untested):** extract the `.tar.gz` and run
  `WormsArmageddonRecompiled.sh`.
- **Android:** install `WormsArmageddonRecompiled-<version>-android.apk` (allow
  installs from your browser or file manager when asked). Needs Android 9 or
  newer, a 64-bit ARM device and Vulkan with descriptor indexing (older Mali
  GPUs, like the Mali-G52, lack it). On first start, **Load ROM** opens the
  system file picker; the ROM is copied into the app, so it's needed only once.
  Updates work with Obtainium.

## Steam Deck
In Desktop Mode:
1. Extract it somewhere that stays put, like `~/Games`.
2. Run `WormsArmageddonRecompiled.sh`, pick **Load ROM** and select your file.
   The file picker is a desktop window, so do this in Desktop Mode.
3. Pick **Add to Steam** on the launcher. Steam has to be closed for this, it
   offers to close it, then adds the game with its artwork.

Then play it from Gaming Mode. It starts fullscreen on a Deck and fills the
16:10 screen. For longer battery life, set **Framerate** to **Original** and
use a lower **Resolution** or no **MSAA** (Graphics tab).

## Command line

| Option | What it does |
|---|---|
| `--game wa` | Start the game straight away (the ROM must have been loaded before). |
| `--rom <file>` | Load that ROM, as **Load ROM** does, and start it. |
| `--set <tab.setting=value>` | Change a setting and save it, as the menu does. Repeat for more. The tab can be left out when only one tab has the setting. |
| `--fullscreen`, `--windowed` | The window mode. |
| `--list-settings` | List every setting, its values and its current value. |
| `--help` | List the options. |

For example, as Steam launch options:
`--game wa --fullscreen --set hide_level_border=on`.
On Windows, run it from a command prompt to see `--list-settings` and `--help`.

## Controls
F1 or Esc opens the settings (on Android: the MENU button or Back). Change the controls on the Controls tab.

| N64 | Keyboard | Controller |
|---|---|---|
| Stick | W A S D | Left stick |
| A | Space | A (bottom button) |
| B | Left Shift | X (left button) |
| Z | Q | Left trigger |
| R | R | Right trigger |
| L | E | Left bumper |
| Start | Enter | Start |
| C buttons | Arrow keys | Right stick |
| D-pad | I J K L | D-pad |

## Building
See [BUILDING.md](BUILDING.md).

## Credits
- [N64Recomp and N64ModernRuntime](https://github.com/N64Recomp) by Mr-Wiseguy
  and contributors, which this port is built with
- [RT64](https://github.com/rt64/rt64) by Dario and contributors, the renderer
- [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) by the N64Recomp
  contributors, the launcher and input layer
- [spimdisasm](https://github.com/Decompollaborate/spimdisasm) and
  [rabbitizer](https://github.com/Decompollaborate/rabbitizer), used to find the
  game's functions, and [n64sym](https://github.com/shygoo/n64sym)'s signature
  data, used to name its library functions (`tools/`, STATUS.md)

## License
The project's own code is GPL-3.0; see [COPYING](COPYING). The game itself remains
the property of its rights holders.
