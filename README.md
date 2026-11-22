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

Add a blank portable.txt in the application's folder to make portable if you want to save settings locally.

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
