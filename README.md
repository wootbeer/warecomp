# Worms Armageddon (1999 N64) Recompiled

You will need the game file from a licensed copy in order to play it.

A native PC port of Worms Armageddon for the N64, made by statically
recompiling the N64 game with [N64Recomp](https://github.com/N64Recomp/N64Recomp)
and running it on [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime)
with [RT64](https://github.com/rt64/rt64) for rendering. A sister project of
[BattleTanx: Global Assault Recompiled](https://github.com/wootbeer/btgarecomp),
sharing its launcher, menus and graphics features.

Unofficial, and not affiliated with the rights holders. **No game data is
included.** You need your own dump of the game to build or play this.

**Status: early work, not playable yet.** The game boots, goes through the
menus and plays a quick match; little else has been tested. See
[PROGRESS.md](PROGRESS.md).

## Planned features
- **Local multiplayer** for up to 4 players, with controllers or keyboard and mouse
- **Widescreen**, higher resolutions and anti-aliasing
- **Higher framerates** (interpolated), plus Lossless Scaling frame generation if you own it
- **Screen filters** (CRT, Scanlines, LCD) and Bloom
- **Vulkan or DX12**
- **Remappable controls**, Mouse Stick
- **Mods and texture packs**
- **Add to Steam** from the launcher, and Steam Deck (Linux) builds
- **Android** builds (later)

## Game Files
You need your own data file: Worms Armageddon (USA) (En,Fr,Es).n64
SHA-1 (as dumped, byte-swapped `.n64`):
04D60D848A5CB9923EB95B016EEBB6AF829296F2

The game also accepts `.z64` and `.v64` dumps of the same release.
Place it in an accessible folder on your device.

Settings, saves and mods are kept in the game's own folder (it's portable):
a new install puts a `portable.txt` there on first start. Delete it to keep
them in your user folder instead (`%LOCALAPPDATA%\warecomp` on Windows,
`~/.config/warecomp` on Linux). An install that already uses the user folder
keeps using it; add a `portable.txt` to switch.

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
`--game wa --fullscreen --set graphics.msaa_option=none --set hide_level_border=on`.
Values can be written as the menu shows them (`--set graphics.rr_option=Original`).
The Cheats and Hacks tabs can be set the same way. On Windows, run it from a
command prompt to see `--list-settings` and `--help`.

## Steam Deck
Use the Linux build (`WormsArmageddonRecompiled-<version>-linux-x86_64.tar.gz`).
In Desktop Mode:
1. Extract it somewhere that stays put, such as `~/Games`.
2. Run `WormsArmageddonRecompiled.sh`, choose **Load ROM** and pick your game file.
   The file picker is a desktop window, so do this step in Desktop Mode.
3. Choose **Add to Steam** in the launcher. Steam has to be closed for this, and the
   launcher offers to close it. It then adds the game to your library as a non-Steam
   game, with its icon, and offers to open Steam again.

Then play it from Gaming Mode. On a Deck, the game starts fullscreen and the Deck's
controls work as a controller. The game fills the Deck's 16:10 screen, with the HUD
at its edges.

For longer battery life, set **Framerate** to **Original** and use a lower
**Resolution** or no **MSAA** (Graphics tab).

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
