# Native port: platform dependencies outside the renderer (2026-10-07)

Inventory of Win32/DirectX use in `Libs/**` and `Jones3D/**`. External libraries, RTI, DX6 and the renderer are
excluded; for the renderer see `renderer-gl.md`. Token-based scan against about 10,900 API names from the MinGW
headers.

## Totals

About **1,750 direct Win32/DirectX call sites**. About two thirds of them (~1,180) are in dialog code.

| Category | Sites | Where |
|---|---|---|
| Dialogs and common controls (user32 dialog functions, windowsx, comctl32, commdlg) | 840 | jonesConfig 686, JonesMain developer dialogs 117, JonesDialog 36 |
| Window, messages, cursor, WM_TIMER | 211 | 157 of them in dialogs; the rest in wkernel 33, JonesMain 12, stdConsole 6 |
| GDI | 155 | jonesConfig 81, JonesDialog 45, sithGamesave thumbnail 21, stdBmp 5 |
| Input (DirectInput8, XInput, WMI) | 66 + 23 | all in `stdControlDX9.c`; `DIK_*` codes are the engine's key IDs (278 uses) |
| Sound (DirectSound8 and 3D) | 78 | all in `sound/DriverDX9.c` |
| Registry | 35 | `wuRegistry.c`; only the fallback for config, plus the startup check `JonesMain.c:283` |
| Files | 47 | jonesConfig 28, stdFileUtil 8, stdGob 7 |
| Path literals with `\\` | 77 + ~25 char tests | `JonesLevel.h` alone has 36 |
| CRT `_s` functions, `__asm`, `#pragma comment` | 105 | `sscanf_s` 76 (3 need size arguments), `_s` wrappers in `stdUtil.h`/`std.c`, `__asm fnstcw` at `std.c:72` (result unused) |
| Threads, timers, console, debug, COM, shell | ~190 | `stdPlatform` (QPC, crash handler, dbghelp), `stdConsole`, `exemain`/`dllmain` (injector), `indy*` debug tools |

Global facts:
- `j3d.h:54` includes `<Windows.h>` in every translation unit.
- `J3DAPI` is `__cdecl` (one macro).
- Game logic is single-threaded: no critical sections, SEH or winsock.

## Dialogs: the biggest item

Facts:
- 23 dialogs are in use. They're all modal (`DialogBoxParam`), and their templates live in the exe's resources;
  there is no `.rc` file in the repo.
- About 8,900 lines in total: `jonesConfig.c` has 146 functions (131 dialog-related), `JonesDialog.c` hosts them
  over the fullscreen frame, and the developer dialogs 101/106 are in `JonesMain.c`.
- `upstream/OpenGL` doesn't convert any dialog. It only skips the DX background capture when showing them.

A native build needs an in-engine modal UI: a nested loop that renders frames and polls events until the dialog
closes, so callers keep their synchronous results.

| Dialog | Single-player Android needs it? |
|---|---|
| 101/106 developer launcher | no: start from config / command line |
| 121 message, 211 exit, 150 death, 233 level end, 167 insert CD | yes (167: never; data is in the APK) |
| 154/214 save, 159/163 load (with thumbnails) | yes (engine-drawn browser) |
| 112 gameplay, 113 sound, 114/148 display options | reduced: a few settings |
| 111/115/116/117/120 control schemes (~2,200 LOC) | no on Android (touch/gamepad presets); desktop keeps keyboard schemes in `.kfg` files |
| 164 statistics | yes (end of level) |
| 190/212 store | yes (gameplay: buying items between levels) |

Estimates:
- All dialogs: 25–40 days.
- MVP (message, exit, death, level end, save/load, basic settings, store, statistics): about 8–12 days.

## What upstream/OpenGL replaces

- **wkernel:** `wkernelSDL.c` provides the SDL window, GL context and event pump. It still subclasses the window's
  HWND and translates SDL events into `WM_*` messages. Its non-Windows path is an untested sketch: it calls
  `MapVirtualKey` and requires the HWND, so it wouldn't compile as is.
- **Input, sound, dialogs, registry, files, GDI:** unchanged. No upstream branch has SDL input.

## Effort, native Linux (person-days; renderer and Stage 3 excluded)

| Package | Days | Kind |
|---|---|---|
| Compat base: no `<Windows.h>`, type shims, CRT `_s` shims, `J3DAPI`/`CALLBACK` empty, `J3D_HOOKFUNC` no-op, real `main()` | 3–5 | mechanical |
| wkernel → SDL3: event model for JonesMain's 9 window-message handlers, intro window, WM_TIMER users | 3–5 | design |
| stdControl device layer (~1,000 LOC) → SDL keyboard/mouse/gamepad (scancode→DIK table, rumble, our analog fixes) | 4–6 | mostly mechanical |
| Sound → OpenAL Soft (1:1 for DS3D) or SDL3 audio + own mixer | 5–8 | mechanical with OpenAL |
| SMUSH player | in Stage 3 (agent) | — |
| Dialogs (MVP / all) | 8–12 / 25–40 | design |
| GDI outside dialogs (savegame thumbnail, `stdBmp_Load`) | 1–2 | mechanical |
| Files: POSIX find/dirs, one path layer (`\\`→`/`, case-insensitive lookup) | 2–4 | design |
| Registry stub, config JSON-only | 0.5 | mechanical |
| Timers, console, crash handler, MessageBox → SDL | 2–3 | mechanical |

Totals:
- **Native Linux:** about 55–85 days with all dialogs, or about 35–55 with the dialog MVP.
- **Android extra:** about 15–25 days (touch, storage, GLES).

## Blockers for a native link

- **Stage 3:** done; no engine function runs in the exe.
- **Exe globals:** done in the standalone build (below).
- **64-bit layout:** 204 `static_assert(sizeof…)` checks in 28 files assume a 32-bit struct layout. arm64 needs
  on-disk structs separated from runtime structs.


## Standalone build (Stage 4, first slice)

`cmake --preset mingw-dx9-standalone` (option `JONES3D_STANDALONE`, default OFF) builds `Jones3D.exe` as the game
itself: no injection, no hooks, none of Indy3D.exe's code. Still a Windows build, run under Wine. No host exe is
needed to build it.

- **Globals:** in the standalone build `J3D_DECL_FAR_VAR(name, type)` expands to `name` (a macro doesn't expand its
  own name again), so `#define name J3D_DECL_FAR_VAR(...)` names a real C object. Each of the 174 former exe globals
  (118 in headers, 56 file-local) is declared `extern` in its module's header and defined at the end of the owning
  `.c`, with the exe's initial value (only 19 are not zero), all under `#ifdef J3D_STANDALONE`. `ResetGlobals` is not
  called. AudioLib's step-index table pointers became C tables.
- **Hook build unchanged:** nothing is inserted before code in a `.c` file, so `__LINE__` (asserts, logs, `STDMALLOC`)
  doesn't move. `.text`, `.rdata`, `.data` and `.reloc` of `Jones3D.dll` are byte-identical; only debug info differs.
  `J3D_STANDALONE` is a compile definition, not part of the generated `j3d.h`, which both build directories share.
- **Other exe uses:** `J3D_TRAMPOLINE_CALL`/`J3D_CALLFUNCFAR` are undefined in standalone, so any call into the exe
  fails to compile. `sithAIMove` reaches `sithPlayerControls_ProcessWeaponAim` through a wrapper
  (`SITHAIMOVE_PROCESSWEAPONAIM`); `sithDSS`'s vanilla puppet callbacks are `NULL` (`J3D_EXE_FUNC`); `indyRand` uses
  its own seed; `indyDiff` isn't built.
- **Resources:** `WinMain` (end of `dllmain.c`) loads `Indy3D.exe` with `LOAD_LIBRARY_AS_DATAFILE |
  LOAD_LIBRARY_AS_IMAGE_RESOURCE`; dialogs, icons and file dialog templates use it through `STDWIN95_RESOURCES()`.
  Nothing is extracted. Indy3D.exe therefore still has to sit next to the game, but none of its code runs.
- **x87 precision:** `WinMain` sets 53-bit precision, as under Indy3D.exe; MinGW's startup code selects 64 bits.
- **Testing:** copy `Build/mingw-dx9-standalone/Jones3D/Jones3D.exe` into `game/run/Resource` under another name and
  run e.g. `INDY_SMOKE_PROC=IndyStandalone.exe Scripts/indy/smoke.sh 60 Resource/IndyStandalone.exe 1`, or
  `INDY_SWEEP_EXE=IndyStandalone.exe Scripts/indy/level_sweep.sh`. A/B tests need the hook build.
- **Next:** resources from our own files (Stage 5 replaces the dialogs anyway), then a native link.
