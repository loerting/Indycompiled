# Android build

Debug APK for arm64-v8a (phones) and x86_64 (emulator). For now it is a demo: it plays the intro movie
`jonesopn.snm` with `Libs/smush/smushDecoder.c` (video into an SDL texture, letterboxed 4:3; audio into an
SDL audio stream). A tap or Back exits.

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

Options: `-Pindy.abis=arm64-v8a` (one ABI only), `-Pindy.ninjaJobs=N` (native compile jobs, default 4).

## Game data: local recipe only

The game data is never committed and the APK is never distributed. Before every build, Gradle runs
`Scripts/android/pack_data.sh`, which copies the data into the git-ignored staging dir
`android/app/build/gameassets/`; its contents end up in the APK's `assets/` (stored uncompressed).

- Source folder: `-Pindy.gameData=<dir>` or `INDY_GAME_DATA=<dir>`; default `game/run/Resource` of the checkout
  (or of the main checkout, when building from a git worktree).
- Packed for now: the intro movies (`*.snm`, about 60 MB). Without data the build still works; the app then
  shows an error.

## Layout

- `app/jni/CMakeLists.txt` - builds `libSDL3.so` and `libmain.so` (`demo_main.c` + the SMUSH decoder)
- `app/src/main/java/com/indycompiled/game/IndyActivity.java` - subclass of SDL's `SDLActivity`
- `app/build.gradle` - SDL Java sources, staging dir, `packGameData` task
