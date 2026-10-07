# Android build

Debug APK for arm64-v8a (phones) and x86_64 (emulator): the whole game. Gradle builds the engine with its own
CMake project (top-level `CMakeLists.txt`, `cmake/native.cmake`) as `libmain.so`, next to SDL3's `libSDL3.so`;
SDL3's `SDLActivity` loads both and runs the game's `SDL_main` (`Jones3D/main.c`). Platform layer as on Linux
(SDL3, OpenGL ES 3); Android-only: `Libs/std/SDL/stdAndroidSDL.c` (game data, below).

## Touch controls

`Libs/indy/indyTouch.c` (fed by `wkernelSDL.c`, drawn by `JonesMain.c` after the HUD):
- Left half: a floating stick where the thumb lands; camera-relative movement (ENH-0005), full push runs. While
  climbing or swimming it acts as the classic keys.
- Right half: drag turns the camera (ENH-0002); a tap attacks; a quick flick down toggles crouching.
- Buttons: Jump, Action (held while touched), Menu (top right). A tap on the health indicator opens the menu too.
- In the menu (the game's own strip: inventory, save, load, options): swipe to move, tap to select, Menu or Back
  closes it. Android Back is Escape.
- The overlay hides while a keyboard or gamepad is used, and comes back on the next touch.
- Tests on the PC: `INDY_TOUCH_OVERLAY=1` shows the overlay without a touch screen.

## Requirements

- Android SDK in `~/Android/Sdk` with NDK `29.0.14206865`, CMake `3.31.6`, platform `android-37.0`, build-tools `36.1.0`
- JDK 17
- `android/local.properties` (git-ignored) with `sdk.dir=/home/<you>/Android/Sdk`, or `ANDROID_HOME` set

Versions: AGP 9.4.1, Gradle 9.6.1 (wrapper), SDL 3.3.0 (`Libs/external/SDL3`, Java glue used in place),
compileSdk/targetSdk 37, minSdk 29. The first build downloads Gradle and AGP into `~/.gradle`; later builds
can run with `--offline`.

## Build

```sh
bash android/gradlew -p android assembleDebug
# -> android/app/build/outputs/apk/debug/app-debug.apk
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
```

Test without a phone: `Scripts/android/test_emulator.sh` boots the x86_64 AVD headless, installs and starts the
APK, and saves screenshots, the logcat and the game's logs to `game/screens/android-<timestamp>/`. It needs about
3 GB of RAM and refuses to start with less than 7 GB available (`INDY_EMU_MIN_MB`); it once froze this PC.

Options: `-Pindy.abis=arm64-v8a` (one ABI only), `-Pindy.ninjaJobs=N` (native compile jobs, default 4).

## Game data: local recipe only

The game data is never committed and the APK is never distributed. Before every build, Gradle runs
`Scripts/android/pack_data.sh`, which copies the data into the git-ignored staging dir
`android/app/build/gameassets/`; its contents end up in the APK's `assets/` (stored uncompressed).

- Game folder: `-Pindy.gameData=<dir>` or `INDY_GAME_DATA=<dir>` (the installed game, or its `Resource` folder);
  default `game/run` of the checkout (or of the main checkout, when building from a git worktree).
- Packed: `Resource/*.gob` and `*.snm` (about 870 MB), `KeySets/`, and the list `indy_assets.txt`. Without data the
  build still works; the game then reports the missing files.
- On the phone: GOBs and movies are read in place from the APK (stored, not compressed); `KeySets/` is copied to
  internal storage on first start, where `Resource/` is the working dir (`Jones.cfg`, `JonesLog.txt`, `stderr.txt`)
  and `SaveGames/` holds the saves. Logs of a debug build: `adb shell run-as com.indycompiled.game cat
  files/Resource/stderr.txt`.
