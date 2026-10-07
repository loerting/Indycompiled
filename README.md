# Indycompiled

**Indiana Jones and the Infernal Machine (1999), running natively on Linux and Android.**

Indycompiled builds on [OpenJones3D](https://github.com/smlu/OpenJones3D), the open-source reimplementation of the
game's Jones3D engine. It adds a native platform layer (SDL3 and OpenGL ES 3, no Wine, no original executable), an Android
app with touch controls, and optional improvements that leave the game's character intact.

> **No game data is included.** You need your own copy of the game
> ([Steam](https://store.steampowered.com/app/904540/) or [GOG](https://www.gog.com/en/game/indiana_jones_and_the_infernal_machine)).
> Indycompiled only reads the data files of that copy.

## Status

Work in progress. Tested so far:

| Platform | Build | State |
|---|---|---|
| Linux x86_64 / i686 | `linux-x86_64`, `linux-i686` | Plays; Canyonlands tested, its level end leads on to Babylon |
| Android arm64 (Android 10+) | `android/` | Plays on a SHIFTphone 8 at 120 fps; touch controls, menus |
| Windows (cross-built with MinGW) | `mingw-dx9-standalone` | Plays as `Jones3D.exe`, without `Indy3D.exe` |

A full playthrough of all 17 levels is still open on every platform.

## What's different from the original

- **Native**: the engine runs without the original executable. Window, input and sound go through SDL3; rendering is the
  original DirectX pipeline reimplemented in OpenGL ES 3.
- **Android**: a floating stick and camera-relative movement, drag to turn the camera, tap to attack; Jump, Action and a
  put-away button; the game's own inventory menu works by touch. Engine-drawn menus replace the Windows dialogs: start
  menu (continue, load, new game), save and load, game over, the end-of-level statistics and shop.
- **Improvements**, each with a toggle in `Jones.cfg` (`indycompiled.toggles`) and grouped into the profiles `vanilla`,
  `fixed` and `enhanced`: analog and camera-relative gamepad controls, a right-stick camera, frame-rate independent
  turning, damage and timers, 4:3 framing for cutscenes and loading screens on wide screens, and more. See
  [Docs/Indycompiled/enhancements.md](Docs/Indycompiled/enhancements.md).
- **Fixes** for crashes and memory errors found on the way; the ones in OpenJones3D's own code go upstream as pull
  requests.

## Building

### Linux

Needs clang, CMake (3.25+), Ninja and Python 3. SDL3 is included.

```sh
cmake --preset linux-x86_64
cmake --build --preset linux-x86_64
```

Run it from the game's `Resource` folder (the one with `Jones3D.GOB`, `CD1.GOB` and `CD2.GOB`):

```sh
cd /path/to/game/Resource
/path/to/Indycompiled/Build/linux-x86_64/Jones3D/Jones3D
```

The game creates `Jones.cfg` there on first start; saves go to `../SaveGames`.

### Android

See [android/README.md](android/README.md). The APK is built locally: a script packs the data files of your copy of the
game into it. **Never share such an APK**, it contains the game's data.

### Windows

The MinGW presets (`mingw-dx9-release`, `mingw-dx9-standalone`) cross-build from Linux. Indycompiled doesn't build with
Visual Studio (its own layer, `Libs/indy`, is MinGW and native only); for that, use
[OpenJones3D](https://github.com/smlu/OpenJones3D) ([README](Docs/OpenJones3D-README.md)).

## Documentation

- [PROJECT.md](PROJECT.md): goals, decisions, roadmap and workflow
- [Docs/Indycompiled/](Docs/Indycompiled/): enhancements, notes on the native and Android ports
- [Docs/](Docs/): OpenJones3D's documentation of the engine, COG scripting and file formats

## Relationship to OpenJones3D

Indycompiled follows OpenJones3D's `develop` branch. Engine reimplementations and bug fixes are contributed back as
[pull requests](https://github.com/smlu/OpenJones3D/pulls?q=author%3Aloerting); the platform ports and the
improvements live here.

## License

AGPL-3.0, as OpenJones3D: see [LICENSE](LICENSE). SDL3 (`Libs/external/SDL3`) is under the zlib license.

Indiana Jones and the Infernal Machine is © Lucasfilm Ltd. LucasArts, Indiana Jones and related properties are
trademarks of Lucasfilm Ltd. This project is not affiliated with or endorsed by Lucasfilm, LucasArts or Disney.
