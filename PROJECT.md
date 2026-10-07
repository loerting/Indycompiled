# Indycompiled — Project Charter & Standards

> A private fork of [OpenJones3D](https://github.com/smlu/OpenJones3D), the reimplementation of the Jones3D engine behind *Indiana Jones and the Infernal Machine* (1999).
> It is developed entirely on Linux, improved (controls, controller, visuals, bug fixes) and taken step by step to native Linux and Android.

| | |
|---|---|
| **Status** | Stage 3 code complete (2026-10-07): `Scripts/indy/progress.py` reports 2913/2913 functions in C. A/B-verified against the original (`Docs/Indycompiled/notes/stage3-verification.md`): level loading, savegames, AI, physics and the intro videos. Multiplayer is stubbed. Still open for Stage 3: a full playthrough including save/load. From Stage 2: gamepads under Wine (4 XInput fixes), analog movement (ENH-0001, needs your feel test), frame-rate fixes FIX-0001–0004, ultrawide inventory fix. |
| **Last updated** | 2026-10-07 |
| **Upstream base** | `smlu/OpenJones3D`, branch `develop` @ `b9c0eaa` (2026-06-08), 92.5% of engine functions done |
| **Host** | Manjaro Linux with i3, and nothing else. **No Windows VM, no Visual Studio, no MSVC.** |
| **Host exe** | `Indy3D.exe` **v1.2** (Steam/GOG), SHA-256 `4075e655e0cf0db2d352265ba83a19a51fb373156cba8b3e43107fd6c6129ebb`. Upstream targets v1.0, which isn't available, so we run on v1.2 through an address map (§5.7, ADR-0005). |
| **Visibility** | Private: local git only, nothing pushed anywhere public. Publishing the fork later is possible, so every commit must be publication-safe (§7.4). |

This document is the single source of truth for scope, workflow and conventions.
Upstream's `Docs/` folder (engine architecture, COG reference, file formats) is the technical reference, and this document doesn't repeat it.
Changes to §4 (strategy), §5 (toolchain) or §7 (repository workflow) need an ADR in `Docs/Indycompiled/decisions/`.
**MUST**, **SHOULD** and **MAY** carry their usual RFC 2119 meaning.

---

## 1. Goals

### 1.1 Goals, in order

1. **Build on Linux, play under Wine.** OpenJones3D builds on this machine with Linux-only tools, and the result runs under Wine with the **v1.2** exe as its host.
2. **Modding.** Better controls, full controller support, visual upgrades and bug fixes, each behind a toggle.
3. **Complete the engine.** Reimplement the functions upstream hasn't done yet (§3.2).
4. **Native Linux.** The game runs as a Linux program, without Wine and without the original exe (§4.2).
5. **Android.** The same engine on a phone, with touch controls and gamepad support.

### 1.2 Hard constraints

- Every step happens on this Manjaro machine: building, running, debugging and reverse engineering.
- Wine is allowed only as the **runtime**, and only until goal 4 is reached. It isn't used as a build tool, except as a last-resort fallback for shader compilation (§5.3).
- The code stays C11, following upstream's conventions. C++ is used only where upstream already uses it (the `Jones3D.exe` launcher) or where a library requires it.

### 1.3 Non-goals

- **Distribution.** Game data, exes and builds stay on this machine and in the private repo.
- **Multiplayer.** `sithMulti` is stubbed, never implemented.
- **A level editor.** Use Urgon or blender-sith.
- **The N64 version.**

### 1.4 Principles

1. **Stay mergeable with upstream.** Upstream is active, and every function they finish is one we don't have to. Our changes MUST keep merges cheap (§7.3).
2. **Faithful first, then improve.** Every change in behaviour sits behind a toggle. The `vanilla` profile stays as close as possible to the original game (§8.3).
3. **The original binary decides.** For reverse-engineering questions, Ghidra on the v1.2 exe is the authority. Wikis and memories of how the game felt only point the way.
4. **Original data, unmodified.** The game reads the Steam/GOG GOB files as they are.

---

## 2. Inventory: facts established 2026-10-06

### 2.1 What's on disk

| Item | Location now | Notes |
|---|---|---|
| Steam/GOG install (v1.2) | `INDY/INDY/` | Moves to `game/original/` in Stage 0. Its data files include LucasArts' 1.2 level fixes. |
| `Resource/Indy3D.exe` **v1.2** | same | SHA-256 `4075e655e0cf0db2…`. **This is our host exe.** It is a near-identical build of v1.0 with shifted addresses, so upstream's v1.0 addresses are translated to v1.2 by our address map (§5.7). |
| `Resource/ddraw.dll` | same | A DirectDraw-to-Direct3D 9 wrapper added by the 2018 re-release. Not needed: Wine provides DirectDraw itself. |
| `Indy3D.exe` **v1.0** | not available | Not needed: we use the address map instead (decided 2026-10-06). |
| `Resource/*.GOB`, `*.snm` | same | 3 GOB archives (840 MB) and 5 SMUSH cutscene videos |

### 2.2 Levels

| File | Name | | File | Name |
|---|---|---|---|---|
| `00_cyn` | Canyonlands | | `09_olv` | Olmec Valley |
| `01_bab` | Babylon | | `10_sea` | V.I. Pudovkin |
| `02_riv` | Tian Shan River | | `11_pyr` | Meroë Pyramids |
| `03_shs` | Shambala Sanctuary | | `12_sol` | Solomon's Mines |
| `05_lag` | Palawan Lagoon | | `13_nub` | Nub's Tomb |
| `06_vol` | Palawan Volcano | | `14_inf` | Infernal Machine |
| `07_tem` | Palawan Temple | | `15_aet` | Aetherium |
| `08_teo` | Teotihuacan | | `16_jep` | Jeep (the 8th level, between Palawan Temple and Teotihuacan) |
| | | | `17_pru` | Return to Peru (bonus) |

### 2.3 This machine

| Tool | State |
|---|---|
| clang 22.1, gcc 16.2, cmake 4.4, ninja 1.13, Python 3, gdb | Installed |
| Wine 11.13 | Installed. A **WoW64 build**: 32-bit programs run inside normal 64-bit prefixes, and `WINEARCH=win32` isn't available. |
| JDK 21 (needed by Ghidra) | Installed (the default is 17) |
| Android SDK and Studio | Installed. The NDK is missing (needed from Stage 7). |
| `mingw-w64-gcc` 16.2, `lld` 22.1, `vkd3d` 1.19, `ghidra` 12.1, `sdl3` / `lib32-sdl3` | Available in the official repos, **not installed** |
| ImHex | Not in the official repos (AUR or AppImage). Optional. |

---

## 3. Upstream: OpenJones3D

### 3.1 How it works

- **It is a DLL injected into the original game.** `Jones3D.exe` checks the SHA-256 of `Indy3D.exe` v1.0, starts it paused and injects `Jones3D.dll`. The DLL then redirects every reimplemented original function to the new C code (`J3D_HOOKFUNC`, with addresses kept in each module's `RTI/` folder).
- **Functions still missing are called in the original exe, and so are its global variables.** That's why the original exe is still required.
- **Build options**:
  - `JONES3D_QOL_IMPROVEMENTS` (on by default);
  - `JONES3D_SPEEDRUN_BUILD` (keeps the vanilla bugs and glitches);
  - `JONES3D_RUNTIME_GUARDS`;
  - `JONES3D_USE_DIRECTX9` (on by default; off means the original DirectX 6 renderer).
- **Already in `develop`**:
  - a DirectX 9 renderer with anti-aliasing, mipmaps and anisotropic filtering;
  - **XInput gamepad support** with dead zones and rumble;
  - the **`Jones.cfg` JSON config file** (`Docs/Jones.cfg.md`), which replaces the registry for most settings;
  - many engine bug fixes (`CHANGELOG.md`).
- **License: AGPL-3.0.** No obligations while the project stays private.

### 3.2 What's still missing on `develop`

217 of 2,906 functions:

| Module | Missing | What it does | Our plan |
|---|---|---|---|
| sithAIUtil, sithAIInstinct, part of sithAIMove | 53 + 29 + 26 = 108 | Enemy behaviour | Implement in Stage 3 |
| sithDSSThing, sithDSS | 35 + 24 = 59 | Writing world state into savegames | Implement in Stage 3 |
| sithMulti | 28 | Multiplayer leftovers | Stub |
| sithPhysics | 10 of 47 | The rest of the physics | Implement in Stage 3 |
| AudioLib | 9 | Sound helpers | Implement in Stage 3 |
| sithThing | 3 | — | Implement in Stage 3 |

Upstream may finish some of these before we do. Check (§7.3) before starting on any of them.

### 3.3 Upstream branches worth tracking

| Branch | What | Relevance |
|---|---|---|
| `develop` | Main development line | **Our base** |
| `OpenGL`, `OpenGL-exp` | OpenGL renderer with **SDL3** windowing, glad and GLSL shaders (embedded by a Python script). Active as of July 2026. | The starting point for native Linux and Android (Stages 5–7) |
| `remove-fps-dependencies` | Fixes physics and damage at high frame rates | Merge once it's stable |

---

## 4. Strategy (ADR-0001)

### 4.1 Decision

**Fork `develop` and follow the road OpenJKDF2 took for Jedi Knight:**
1. hook DLL;
2. complete the engine;
3. standalone engine;
4. SDL3 platform layer;
5. native builds.

Until the engine runs standalone, Wine is the runtime. The game is playable at every stage, so modding can start right after Stage 1.

### 4.2 Native Linux without Wine: yes, it's realistic

It just can't be the first step, because the hook design needs the original **Windows** exe as its host process. What it takes:

| Step | Size | Notes |
|---|---|---|
| Finish the missing functions | 217, of which 28 are only stubbed | Stage 3 |
| Move all global variables out of the original exe | Mechanical but large | Upstream's own "phases 3–4". After this, `Indy3D.exe` is no longer needed. Stage 4. |
| Replace the Windows platform layer | About 400 functions (per upstream's module table) | Display and 3D come from upstream's OpenGL branch, which already uses SDL3. Still to do: input (DirectInput/XInput → SDL3), sound (DirectSound → SDL3 audio), window and messages (`wkernel`), registry (→ `Jones.cfg`), and the **in-game menus, which are Win32 dialog boxes** (`jonesConfig` and `JonesDialog`, about 165 functions; they become menus drawn by the engine). Stage 5. |
| 64-bit cleanup | Moderate | Pointers inside structs, and the struct-size checks that assume 32 bits (`J3D_32BIT`/`J3D_64BIT` already exist in `j3d.h`). Stage 6. |

The same work is the prerequisite for Android, so native Linux is on the critical path, not a detour.
Proof that it can be done: OpenJKDF2 did exactly this for the parent engine. It went from a hook DLL to native builds for Linux, macOS, Android and WebAssembly.

---

## 5. Toolchain (ADR-0002)

### 5.1 Packages

Install once:

```sh
sudo pacman -S --needed mingw-w64-gcc lld vkd3d ghidra
# mingw-w64-gcc also pulls in the MinGW-w64 headers, CRT, binutils and winpthreads
```

### 5.2 Compiler choice

| | Choice | Why |
|---|---|---|
| **Compiler** | **clang 22** (already installed) with `--target=i686-w64-mingw32`. It uses the MinGW-w64 headers and CRT, plus libgcc, from `mingw-w64-gcc`. | Understands MSVC-style inline asm (`-fasm-blocks`) and `#pragma comment(lib)`. Follows the i386 Windows stack-alignment rules, which matters because our DLL is called from MSVC-compiled code. Same compiler family as the Android NDK. |
| Fallback compiler | `i686-w64-mingw32-gcc` from the same package | Needs one `__asm` line in `std.c` rewritten |
| Linker | **lld** | — |
| Build system | CMake + Ninja, with `CMakePresets.json` and `cmake/toolchains/i686-w64-mingw32-clang.cmake` | — |
| Resources | `llvm-rc` or `i686-w64-mingw32-windres` | — |
| DX9 shaders | **`vkd3d-compiler`** (HLSL → shader model 3 bytecode) | Upstream relies on Visual Studio's built-in shader compiler (the `VS_SHADER_*` properties), so we replace it with a custom CMake command. Fallbacks in §5.3. |
| GL shaders (later) | Python `embed_shaders.py` | Upstream's OpenGL branch. Already Linux-friendly. |

### 5.3 Result of the compile test (2026-10-06)

All of `develop` was compiled with the installed clang for `i686-w64-windows-gnu`, using Wine's Windows headers as a stand-in for MinGW-w64 (syntax and type checking only, no linking).

| Config | Files | Compile cleanly | Notes |
|---|---|---|---|
| DirectX 9 (upstream default) | 134 | **113** | The remaining 21 fail for mechanical reasons, listed below |
| DirectX 6 (original renderer) | 133 | **116** | **No DirectX 6.1 SDK needed**: the standard DirectX headers are enough |

The struct-size `static_assert`s in the shared headers pass. That is the strongest available evidence that clang lays out the engine structs exactly as MSVC did for the original exe.

**What the build actually needed (Stage 1, done 2026-10-06).** Most of it lives in our own files (`cmake/indy.cmake`, `cmake/compat/`, `Scripts/indy/`). The diff to upstream's files is **9 files, +29/−13 lines**, all INDY-marked (now 10 files, including the `sithEvent.c` fix). These patches would also help upstream; whether to offer them is your call.

1. **CMake.**
   - The top-level `CMakeLists.txt` gets one `elseif` that includes `cmake/indy.cmake` for clang/GCC (upstream stops with `FATAL_ERROR` for non-MSVC).
   - The `LIBS/` path casing is fixed.
   - `JONES3D_BUILD_PROGRAMS=OFF`: the generated COG parser is already committed.
   - A deferred fix-up maps MSVC library names to MinGW (`Comctl32.lib` → `comctl32`, `Winmm` → `winmm`), drops `/SAFESEH:NO`, and names the DLL `Jones3D.dll`.
2. **Include casing.** Small redirect headers in `cmake/compat/include/` (`Windows.h`, `CommCtrl.h`, `Mmreg.h`, `Xinput.h`, `D3Dcommon.h`, `DirectX6/*.h`) forward to MinGW-w64's lowercase headers with `#include_next`. No upstream edits. The 13 mis-cased project `#include`s work on the exFAT drive; they still need fixing before native builds.
3. **Missing standard includes.** `cmake/compat/indy_prelude.h` (`assert.h`, `errno.h`, `float.h`) is force-included into every C file.
4. **Header gaps.** Only `PDIRECT3DDEVICE9` was missing in MinGW-w64; `cmake/compat/include/d3d9.h` adds it.
5. **DX9 shaders.** `compile_shader.py` preprocesses with `clang -E` (vkd3d's own preprocessor doesn't expand nested macro arguments), then compiles with `vkd3d-compiler` to shader model 3 bytecode. One INDY fix in `common.hlsli` (two-level token pasting).
6. **C `inline` semantics.** MSVC compiles plain C `inline` like C++ does: a mergeable copy per file. C99 emits either none or one per file. Fixes:
   - header inline functions get their single external definition from a generated file (`gen_inline_externals.py`, no upstream edits);
   - six file-local ones became `static inline` (INDY), and so did `stdConffile_ScanLine`, which was duplicated because of an extra non-inline declaration.
7. **Pointer type mismatches.** E.g. callbacks that return `int` where `void` is declared. MSVC accepts them, and the cdecl ABI is identical, so they're downgraded from errors to warnings.
8. **Launcher (`Jones3D.exe`, C++20).** `UNICODE` plus `-municode` (it uses `wmain`), winpthreads for MinGW's static libstdc++, and everything linked statically.
9. **Libraries from `#pragma comment(lib, …)`.** clang + lld honour them. Only `Xinput.lib` needed a correctly cased alias (→ `libxinput1_4.a`, as in the Windows SDK).
10. **Hooks.** `j3dhook.h` refuses address 0 (unverified hooks) and unaligned addresses (INDY).
11. **MSVC-tolerated undefined behaviour.** Code that MSVC compiles "as written" can be optimised differently by clang:
    - every file is built with `-fno-strict-aliasing -fwrapv` (MSVC semantics);
    - one real upstream bug was found: `sithEvent_ResetFreeBufferTable` wrote one element past an array. Clang `-O2` turned that loop into an endless one, which hung the release build at startup. Fixed (INDY), found by bisecting with the debug aids `INDY_O0_TARGETS` / `INDY_O0_SOURCES` (compile chosen targets or files with `-O0`).
12. **Editor support.** `.clangd` points VS Code's language server at `Build/mingw-dx9-debug/compile_commands.json`.

### 5.4 ABI watch-list

Our clang-built DLL runs inside an MSVC 5-compiled exe, so these points need watching:
- **Struct layouts** are covered by the `static_assert`s. New structs MUST get one as well.
- **Calling conventions**: `__cdecl` by default, and `__stdcall` is marked explicitly (34 places). There are no `__fastcall`/`__thiscall` and no SEH (`__try`).
- **Small structs returned by value** (8 bytes or less): MSVC and MinGW should agree (registers EAX:EDX). Test this explicitly in Stage 1.
- **Memory and FILE handles** MUST NOT cross between our C runtime and the original exe's.

### 5.5 Running under Wine

- **`/mnt/nvme` is exFAT.** That means no symlinks, no hardlinks, no Unix permissions, and case-insensitive file names. Consequences:
  - **The Wine prefixes live on the root filesystem**, because Wine needs symlinks. There are two, about 380 MB each:
    - `~/.local/share/indycompiled/prefix` for playing, with audio;
    - `~/.local/share/indycompiled/prefix-test` for automated runs. Its audio driver is set to none, and it has its own wineserver, so tests never make sound and never touch a game you're playing.
  - **`run/` uses copies instead of symlinks** (about 850 MB, no problem on the nvme drive).
  - **"Read-only" can't be enforced** on `original/`. It is protected by convention instead, plus a `SHA256SUMS` check that the scripts run.
  - **Git sees every file as mode 0755**, so set `core.fileMode=false`. Upstream has no symlinks and no file names exFAT rejects.
  - **Mis-cased `#include`s stop failing here**, because exFAT ignores case. Fix them anyway (§5.3), and run the compile check on a case-sensitive filesystem (e.g. a copy in `/tmp`), because native Linux and Android builds will hit them.
- **Folder layout** (all inside `game/`, which git ignores):

| Folder | Contents |
|---|---|
| `original/` | The pristine Steam/GOG install, never modified, with a `SHA256SUMS` file |
| `run/` | Generated by a script: a copy of the install, with our build output (`Jones3D.exe`, `Jones3D.dll`) added to `Resource/` next to the v1.2 `Indy3D.exe` |
| `screens/` | Screenshots and logs from smoke tests |
| `ghidra/` | The Ghidra project (`Scripts/indy/ghidra.sh`) |

- **Scripts** in `Scripts/indy/`:
  - `rti_remap.py` and `exe_layout_check.py` (both already exist): the address map (§5.7);
  - `setup.sh`: checks `original/` against its checksums, creates both prefixes, imports the registry values from `regs.cmd` (`Start Mode` = 0, otherwise the developer launcher appears), and assembles `run/`;
  - `smoke.sh [seconds]`: headless test run under Xvfb in the test prefix. It takes screenshots every 15 s (pressing Escape to skip intros), reports whether the game survived, and mutes any audio stream the test prefix opens, as a safety net;
  - `play.sh [WxH]`: play in the play prefix, with audio, in a virtual-desktop window;
  - `ghidra.sh init|apply-map|gui`: the Ghidra project, with the address map applied as names and review bookmarks;
  - `debug.sh` (Stage 1): `winedbg --gdb`. Our DLL carries DWARF debug info, so gdb shows source lines.
- **Launching under Wine**: start the exe by its full Windows path (`winepath -w`). `explorer /desktop=…` and `start.exe` don't search the working directory, and then the game silently never starts.
- **Don't copy `ddraw.dll` into `run/`**: Wine provides DirectDraw and Direct3D itself. If the DX9 renderer is slow, set `WINE_D3D_CONFIG=renderer=vulkan`.
- **i3 specifics**:
  - Run the game in a Wine virtual desktop (`wine explorer /desktop=Indy,1920x1080 …`) to avoid fullscreen mode switches, or add an i3 rule `for_window [class="(?i)(jones3d|indy3d)\.exe"] floating enable`.
  - **Start Ghidra with `_JAVA_AWT_WM_NONREPARENTING=1`.** Without it, Java windows stay grey under i3. `ghidra.sh gui` does this, and also sets `JAVA_HOME` to Java 21, which Ghidra 12 requires (the system default is 17).

### 5.6 Reverse-engineering tools

| Tool | Use |
|---|---|
| **Ghidra 12.1** | On the **v1.2** exe. A script (`Scripts/indy/ghidra_import_map.py`) imports all names from the address map (`rti_v12.csv`), so about 2,900 functions and 1,100 globals are named from the start. Each one carries its confidence level, which turns the review list (§5.7) into a Ghidra bookmark list. |
| gdb via `winedbg --gdb` | Debugging at runtime |
| FFmpeg, ImHex (optional) | Video reference, binary formats |

### 5.7 Address map v1.0 → v1.2 (ADR-0005)

Upstream's code refers to the original exe through 4,044 hard-coded v1.0 addresses, kept in each module's `RTI/addresses.h` plus four in `Libs/smush/SmushPlay.h`. We run on v1.2, so these are translated.

**How the map is made.** `Scripts/indy/rti_remap.py <upstream checkout> <v1.2 exe> <out.csv>` produces `Scripts/indy/rti_v12.csv`. That file is committed and every reviewed correction goes into it. The script combines five kinds of evidence:
1. a local shift shared by each address and its neighbours;
2. the source-file names in assert messages;
3. **caller voting**: when upstream's C code of A calls B, the v1.2 body of A must call the v1.2 B;
4. **pointer voting**: the same for functions passed around by address (COG verbs, dialog procedures);
5. **usage voting** for globals, plus interpolation between neighbours.

Each entry gets a confidence level: `verified`, `plausible` (lands on a function boundary but has no independent evidence) or `doubtful`.

**Review.** Entries that aren't verified go through an automated review against Ghidra:
1. `ghidra.sh` builds the project.
2. `ExportReview.java` exports Ghidra's view of each entry: is it a function entry, its callers and references, the decompiled parameter count, and the stack purge.
3. `review_map.py` confirms an entry when one of these holds:
   - its callers or users match upstream's C code;
   - its signature matches;
   - it is called or referenced exactly from its own module;
   - it sits between two verified neighbours with the same shift.

   Confirmations are written to `Scripts/indy/rti_v12_reviewed.csv` (committed). `rti_remap.py` applies them, so they survive every regeneration. Manual corrections go into the same file with `verdict=fixed` and `reviewer=manual`, and they always win.

**Semantic verification (`verify_semantic.py`).** This is the strongest evidence, and it overrides position-based guesses:
1. **COG verb registration:** v1.2 registers each script verb with its name, and so does upstream's C code. That pairs name and address on both sides; 515 verbs were compared.
2. **String fingerprints:** a log or assert string that only one function uses, both in v1.2 and in upstream's C code.

Result on 2026-10-06:
- all 515 COG verbs match, after 4 fixes;
- 682 functions are string-confirmed, with 0 contradictions;
- the string-based checks and the manual table checks (e.g. the 22 platform/file functions, from v1.2's `stdPlatform_InitServices` slot assignments) go into `rti_v12_reviewed.csv` with `reviewer=semantic`/`manual`.

**Lesson from Stage 1:** LucasArts moved several file wrappers to another block in 1.2. Position-based rules, and "parameter count matches" (all of those wrappers take 3 arguments), confirmed wrong addresses. That sent every read through the INSANE video library's file table into upstream's `seek`, and the intro hung. Only semantic evidence counts as strong.

**Cross-reference check (`verify_xref.py`).** For a weakly evidenced function, the v1.2 body at the mapped address must reference every exe global, and call every RTI function, that upstream's C body uses. Callees that upstream declares `static inline` are skipped, because the original compiler inlined them too. Confirmed entries go into `rti_v12_reviewed.csv` with `reviewer=xref`, which counts as strong. Contradictions are checked by hand. On 2026-10-06, every contradiction except one was inlining, an upstream addition (external material/keyframe loading, a debug print) or an upstream fix. The exception was a real map error: `JonesFile_pHS` and `JonesFile_pSysEnv` were swapped. It was harmless, because the DLL keeps its own copies of both.

**Usage scan bug (fixed 2026-10-06):** `rti_remap.py` counted commented-out hooks (`// J3D_HOOKFUNC(x);`, 218 functions) as installed. Those functions still run original code, so the functions they call are reachable. The reach column was too optimistic, and 18 weak hooks were missing from the review list. A minidump from the `INDY_SKIP_WEAK_UNREACHABLE` experiment showed this: the original `sithAIInstinct_InitInstincts` called the original `sithAI_RegisterInstinct`, which crashed.

**Status on 2026-10-06 (before the semantic pass; see §12 for the remaining weak entries):**

| | Total | Verified | Plausible | Doubtful / unmapped |
|---|---|---|---|---|
| Functions | 2,915 | 2,677 | 219 | 10 / 9 |
| Data | 1,122 | 735 | 361 | 26 |
| **Runtime-critical:** trampolines (calls into still-original functions) | 237 | **237** | 0 | 0 |
| **Runtime-critical:** globals upstream still reads from the exe | 174 | **174** | 0 | 0 |

Upstream's call edges found in v1.2: 91%. The remainder comes from code upstream restructured in C and from functions LucasArts changed in 1.2.

Two consistency rules keep the map honest:
- **one function per v1.2 address**: when several v1.0 functions claim the same address, only the best-supported one keeps it;
- **linker order**: functions keep their source order, so mapped addresses must rise with the v1.0 addresses. Anything that breaks the order is `doubtful`.

Without these two rules, 124 address collisions slipped through. Some of them even looked `verified`, because a caller of a shared address confirms every name mapped there.

**Rules**
- **Hooks**: a wrong hook address corrupts code (a crash). A skipped hook only means the original v1.2 function keeps running, which is harmless while our C code calls our own functions directly. So **the DLL hooks only `verified` functions.** The rest are hooked one by one after review in Ghidra.
- **Trampolines and live globals MUST be `verified`** before the first run. Done on 2026-10-06: all 411 entries. 243 came straight from the mapper, and 168 were confirmed by the review: 163 by upstream callers or users, signature or own-module references, and 5 by the sandwich rule.
- **Build integration without editing upstream files.** A generator writes `build/rti_v12/<module>/RTI/addresses.h` from the CSV. That folder comes **before** `Libs/` on the include path, so upstream's `#include <sith/RTI/addresses.h>` picks up the v1.2 version. Only these need INDY-marked edits:
  - `SmushPlay.h` (its four addresses);
  - the launcher's hash check in `exemain.cpp` (accept the v1.2 hash);
  - the hook installer (skip entries that aren't verified).
- **After every upstream sync**, re-run `rti_remap.py`. New or renamed RTI symbols show up as unreviewed entries, and reviewed corrections are kept.
- **Runtime safety net.** At hook time, the DLL checks that each target lands on a function boundary and logs every refusal.

**Known v1.2 differences found by the map**
- 16 functions have a different size in v1.2 (`size_delta` in the CSV; ±0x10 can be mere alignment). The big ones fit the 1.2 README:
  - `jonesConfig_ControlToString` −0x240 (the control configuration dialog);
  - **`sithPlayerControls_ProcessGeneralMove` +0x110**: LucasArts changed player movement in 1.2, so review it before touching the controls (Stage 2);
  - `sithActor_DamageActor` +0x30 and `stdFileGets` −0x50.
- There are 21 breaks in the data layout:
  - some are real (e.g. `JonesHud_aCredits` +0x110, `sithControl_aFunctionNames` +0x18);
  - some come from uninitialised globals that the linker ordered differently, which is why usage evidence outranks layout.
- **Struct layouts may have changed in 1.2** without the `static_assert`s (which check v1.0 sizes) noticing. Check this where the size-changed functions touch shared structs (Stage 1, §11).

---

## 6. Platforms by stage

| Stage | Build target | Runs on |
|---|---|---|
| 1–5 | `i686-w64-mingw32` (Windows DLL and exe) | Wine on this machine |
| 6 | `i686-linux-gnu` first, then `x86_64-linux-gnu` | Natively. 32-bit first, because it keeps the original struct layouts while the platform code changes. `lib32-sdl3` and `lib32-mesa` are in the repos. |
| 7 | `arm64-v8a` Android | A phone, through the SDL3 Android template, OpenGL ES 3.0 and the NDK |

---

## 7. Repository and upstream workflow (ADR-0003)

### 7.1 Setup

- **Use a private repo, not GitHub's fork button**: forks of public repos can't be private. Clone upstream, rename that remote to `upstream`, and add your private remote as `origin`.
- This folder isn't empty, so set it up with `git init` + `git remote add upstream …` + `git fetch` + `git checkout -b main upstream/develop`.

- **Nothing gets pushed anywhere public.** Commit locally as often as useful. A private remote is fine once you set one up; a public one waits for your explicit decision.

### 7.2 Layout

Upstream's tree stays as it is. Our additions live in paths upstream never touches:

```
Indycompiled/
├── PROJECT.md                     this document (ours)
├── README.md, CHANGELOG.md, …     upstream's (never edited)
├── CMakeLists.txt                 upstream's, plus minimal INDY-marked changes
├── CMakePresets.json              ours
├── cmake/toolchains/              ours: mingw now, linux-i686/x86_64 and android later
├── Docs/                          upstream's engine, COG and format docs: read these first
│   └── Indycompiled/              ours: decisions/, enhancements.md, parity.md, notes
├── Jones3D/  Libs/  Programs/  Resources/   upstream code
│   └── Libs/indy/                 ours: toggles, config, new features (new module)
├── Scripts/                       upstream's (analyze.py, gencogparser.py)
│   └── indy/                      ours: setup, run, debug, Ghidra import
├── game/                          git-ignored (§5.5)
└── build/                         git-ignored
```

### 7.3 Rules for staying mergeable

1. **Minimal edits to upstream files.** Put the logic in `Libs/indy/` and call into it from upstream code.
2. **Mark every edit to an upstream file**: `// INDY(ENH-0003): <reason>`, or `// INDY: <reason>` for build and port fixes. Then `grep -r "INDY"` lists our entire diff against upstream.
3. **Never reformat upstream code.**
4. **Sync regularly**: merge `upstream/develop` into `main` (commit type `sync`), at least monthly and before starting any reverse-engineering task. Tag `main` before each sync.
5. **Before implementing a missing function**, check whether upstream already has it, including on open branches. If upstream later implements something we already did, prefer theirs unless ours is demonstrably better.
6. **Write new reimplementations the way upstream does**: RTI address, `J3D_HOOKFUNC`, the same names and file locations. Merges with upstream's version then stay possible.

### 7.4 Publication safety

You may publish the fork one day, so the repository must stay clean from the start. **Never commit**:
- game files or extracted assets (textures, models, levels, sounds, videos);
- bytes of the original exe, or Ghidra/decompiler output dumps (pseudo-C, listings, exported databases);
- anything copied out of the game's manual or help files.

All of that stays in the git-ignored `game/` folder (`game/review/`, `game/ghidra/`, `game/screens/`).

Fine to commit: our own code and docs, and the address map. Its names come from upstream, and addresses and hashes are facts, not copyrighted content.

If the fork is ever published, AGPL-3.0 applies to it as a whole, and upstream's license and notices stay in place.

---

## 8. Standards

### 8.1 Code

- **C11, following upstream's conventions.** Functions are named `fileName_FunctionName`, globals `module_g_name`, and the existing struct naming and `.editorconfig` apply. When unsure, copy the style of the surrounding file.
- **Our module**: files are named `indy<Name>.c/.h` (e.g. `indyEnh.c`), functions `indyEnh_IsEnabled`, globals `indyEnh_g_…`.
- **Warnings**: our files compile without warnings under `-Wall -Wextra`. Don't fix warnings in upstream files unless they are real bugs (and then it's a separate `fix` commit).
- **Every new struct that mirrors original memory or file layouts** gets a `static_assert` on its size.

### 8.2 Commits

Conventional Commits, with the module as the scope:

| Type | Use |
|---|---|
| `feat` | New feature |
| `fix` | Bug fix |
| `enh` | Enhancement behind a toggle |
| `port` | Reimplementation of an original function |
| `re` | Reverse-engineering notes or symbols |
| `build` | Build system and toolchain |
| `sync` | Merge from upstream |
| `docs` | Documentation |
| `chore` | Everything else |

Example: `port(sithAI): implement sithAIInstinct module`.
Each stage gets a milestone tag: `s1-linux-build`, `s2-modded`, `s4-standalone`, `s6-native`, `s7-android`.

### 8.3 Enhancement toggles (ADR-0004)

- Every change in behaviour has an ID (`ENH-0001`, `FIX-0001`) and is registered in `Docs/Indycompiled/enhancements.md`.
- **Runtime toggles live in upstream's `Jones.cfg`**, in a section of our own:

```json
"indycompiled": {
  "profile": "enhanced",
  "toggles": { "analogMovement": true, "cameraRelative": false }
}
```

- **Profiles**:
  - `vanilla`: all our toggles off. For the closest match to the original, build with `JONES3D_QOL_IMPROVEMENTS=OFF`.
  - `fixed`: bug fixes only.
  - `enhanced`: everything on.
- In code: `if ( indyEnh_IsEnabled(INDY_ENH_ANALOG_MOVEMENT) ) { … }`.

### 8.4 Testing

| Check | When |
|---|---|
| Build both renderer configs (DX9 and DX6), with QOL on and off | Every change to the build or to upstream files |
| `Scripts/analyze.py` progress report | After each `port` |
| Smoke test: start a level from the command line under Wine and check the log | Every build (scripted) |
| Savegame round trip (save, reload, compare) | Every change to DSS or savegame code |
| Parity checklist per level in `Docs/Indycompiled/parity.md` | Each stage |
| Level sweep: `Scripts/indy/level_sweep.sh` starts all 17 levels headless (survival, level load, engine and Wine errors) | After every change to the address map or engine code |
| Scripted input: `smoke.sh` with `INDY_SMOKE_ACTIONS` (timed key presses) and `INDY_FAKE_STICK` (simulated stick) | When control code changes |
| Hang analysis: `INDY_SAMPLE_THREADS=<s>` (in-process thread sampler), then `llvm-addr2line` on Jones3D.dll | When a test hangs |
| Savegame round trip: `Scripts/indy/test_savegame.sh` (F5, move, F8, compare screenshots) | Every change to savegame/DSS code |
| Controller: `Scripts/indy/test_gamepad.sh` (virtual Xbox 360 pad via uinput → Wine → XInput → keyset → ENH-0001) | When input code changes |
| Turn rate vs frame rate: `Scripts/indy/test_turnrate.sh` (frame caps via `INDY_FPS_CAP`, rate from the `INDY_INPUT_TRACE` heading) | When turning or timing code changes |
| Continuous damage vs frame rate: `Scripts/indy/test_damage.sh` (`INDY_TEST_DPS`) | When damage or timing code changes |
| "Every N-th frame" events vs frame rate: `Scripts/indy/test_cycles.sh` | When timing code changes |
| Direction change: `Scripts/indy/test_reverse.sh` (virtual pad, forward-to-back flip, ENH-0003 off/on) | When movement code changes |
| Modern controls: `Scripts/indy/test_modern.sh` (virtual pad: stick right / down relative to the camera) | When movement or camera code changes |
| Stage 3 progress: `Scripts/indy/progress.py` (functions still reached through `J3D_TRAMPOLINE_CALL`, by module and size) | After each reimplemented module |
| Differential test (Stage 3): `Scripts/indy/test_diff.sh <suite>`: original v1.2 function vs our C version, same inputs, byte-compared in-process (`Libs/indy/indyDiff.c`) | For every reimplemented function |
| A/B level load (Stage 3): `Scripts/indy/test_ab.sh <func,...> [levels]`: each level loaded with the original functions (`INDY_NOHOOK`) and with ours, world snapshots compared (`INDY_DUMP_WORLD`: every template and thing, pointers written as what they point to) | For every reimplemented loader |
| A/B simulation (Stage 3): `test_ab.sh` with `INDY_FIXED_FRAME_MS=<ms>` (fixed game time per frame), `INDY_DUMP_WORLD_FRAME=<n>` (snapshot after n frames) and `INDY_SMOKE_NOKEYS=1`; differences are named per field by `Scripts/indy/compare_world.py` (float tolerance `INDY_AB_TOL`) | For AI, physics and other per-frame code |
| Exe constants: `Scripts/indy/exe_value.py <0xaddr>...` (values the decompiler shows as `DAT_`) | While reimplementing |

---

## 9. Roadmap

| Stage | Goal | Main work | Exit criterion |
|---|---|---|---|
| **0 Foundation** ✅ | Everything in place | Install the packages (§5.1). Move `INDY/INDY/` to `game/original/` and write its `SHA256SUMS`. Set up the repo (§7.1). Create the Wine prefix on the root filesystem and the run folder (§5.5). **Play the unmodified v1.2 game under Wine first**, as the baseline that separates Wine problems from our own. Review the runtime-critical entries of the address map in Ghidra (§5.7). | The original v1.2 game plays under Wine in our prefix. Every trampoline and live global in `rti_v12.csv` is `verified`. |
| **1 Linux build on v1.2** ✅ | Build it ourselves | Toolchain file, presets, patch set (§5.3), shader step, the generated v1.2 RTI headers and the hook filter (§5.7), the ABI check from §5.4, gdb debugging. Unverified hooks are then reviewed in batches. | Our `develop` build, hosted by the v1.2 exe, plays Canyonlands under Wine, and gdb stops at a breakpoint in our code |
| **2 Modding** 🎮 (started) | The game you want to play | Controls, controller and visuals (§10), building on upstream's XInput support and `Jones.cfg` | A complete playthrough with the `enhanced` profile |
| **3 Completion** ✅ (code and A/B, 2026-10-07; playthrough open) | No more original functions | AI, DSS, physics, AudioLib, sithThing (§3.2), reverse engineered on the v1.2 exe in Ghidra with the names from the address map. Multiplayer stubbed. Every hook in the map is `verified`. | `analyze.py` reports 100%, excluding the stubs. A full playthrough including save/load. |
| **4 Standalone** | No more original exe | All global variables defined in our code. Injection removed. | The game starts and plays with `Indy3D.exe` deleted (still a Windows build, under Wine) |
| **5 Platform layer** | No more Win32 | Merge upstream's OpenGL/SDL3 renderer. Input and audio move to SDL3. Engine-drawn menus replace the Win32 dialogs. The registry goes; `Jones.cfg` takes over. May overlap with Stage 4. | No Win32 or DirectX calls outside one platform file. Still runs under Wine. |
| **6 Native Linux** 🐧 | No more Wine | Linux i686 build, then the 64-bit cleanup and an x86_64 build | A native x86_64 binary plays the whole game |
| **7 Android** 📱 | Play on a phone | NDK, OpenGL ES 3.0 variant, touch overlay, importing game data through the Storage Access Framework, app lifecycle | A full playthrough on a phone |

---

## 10. Enhancement and fix backlog

Check upstream first (`CHANGELOG.md`, `Docs/Architecture/QOL-And-Compatibility.md`): some of these may already exist.

**Controls** (in `sithPlayerControls` / `sithPlayerActions`, both reimplemented upstream)
- Analog movement: speed follows how far the stick is pushed.
- Optional camera-relative movement with an orbit camera. Classic tank controls stay available.
- Jump and ledge assist for the camera-relative scheme.
- Rebinding, dead zones, sensitivity.
- Input buffering for jump and action. Tune carefully; it changes how the game feels.

**Controller**
- Builds on upstream's XInput support.
- Button glyphs in prompts, rumble tuning, optional gyro aiming (needs SDL3, so Stage 5).

**Visuals**
- Widescreen and HUD scaling (upstream issue #13: the health circle is too large in widescreen).
- High frame rates: merge `remove-fps-dependencies`, then interpolation.
- Higher engine limits, 32-bit texture packs.

**Official 1.2 fixes.** The 1.2 README lists exe-side fixes:
- the control configuration dialog;
- wrong field of view when a level opens;
- Infernal Machine part sounds that don't stop.

Because v1.2 is our host, these fixes stay active as long as the affected functions still run as original code. Once upstream's v1.0-based C code replaces such a function (through a hook), the fix is lost, unless we port it. Before a function is hooked, check its `size_delta` in `rti_v12.csv`. If it is non-zero, compare the v1.2 code with upstream's C code in Ghidra and port the difference as a `FIX-` entry.

**Your own bug list.** *Write down the little-known bugs you remember here, with level and reproduction steps.*

---

## 11. Risks

| Risk | Mitigation |
|---|---|
| Wrong addresses in the v1.0 → v1.2 map (crash or silent corruption) | Hook only `verified` entries. Review every runtime-critical entry before the first run. Boundary check at hook time. See §5.7. |
| 1.2 changed a struct's layout, or code that upstream's v1.0-based C code relies on | Before hooking any function with a non-zero `size_delta`, review it in Ghidra. Smoke and savegame tests (§8.4). |
| A hooked v1.0-based function drops one of LucasArts' 1.2 fixes | The `size_delta` review in §10 |
| Merge conflicts as upstream moves on | The rules in §7.3, and syncing often |
| Upstream goes quiet | The fork is self-sufficient. The roadmap doesn't depend on upstream finishing anything. |
| ABI mismatch between the clang-built DLL and the MSVC exe | `static_assert`s, the explicit test in Stage 1, the watch-list in §5.4 |
| Wine quirks (input, fullscreen under i3, WoW64 performance) | Virtual desktop, the Vulkan wined3d renderer, and reproducing anything suspicious with the unmodified v1.2 game |
| Replacing the Win32 menus takes a lot of work (about 165 functions) | Stage 5 can start early and proceed screen by screen |
| 64-bit port | Linux i686 first, so only one thing changes at a time |
| AGPL obligations | Only apply if the project is shared. It stays private. |

---

## 12. Open questions

- **Remaining weakly evidenced map entries (as of 2026-10-06, late evening).** After registration pairing (591 functions: COG verbs, message handlers, AI instincts, console commands), string fingerprints (682 functions) and the cross-reference check:
  - **trampolines:** the 82 weak ones are dead stubs. No C code calls or references them; only original code does, and that uses the real v1.2 addresses directly.
  - **live globals:** the 11 weak ones that C code actually uses are verified against their original v1.2 users.
  - **hooks:** every weak hook that original code can reach is now confirmed, by `verify_xref.py` or by hand, so `game/review/weak_reachable_hooks.md` is empty. 20 weak hooks remain. All are leaf functions that nothing original reaches and that use no global or callee to compare (`wuRegistry_*`, `rdFont_DrawChar`, `stdFnames_ChangeExt` …). A wrong address there would matter only if it patched a different, reachable function.

    Reachability depends on which hooks are really installed: leaving one out makes the original code run again, and with it every function that code calls. So leaving out "unreachable" hooks stays an opt-in experiment (`INDY_SKIP_WEAK_UNREACHABLE=1`).
- **Struct changes in 1.2.** Did LucasArts change any struct layout in 1.2? Check the shared structs touched by size-changed functions first.
- **Remaining map review.** How many of the 234 plausible functions does Ghidra confirm, and do any of them turn out wrong? That decides whether `plausible` hooks may be enabled in bulk.
- ~~**Shaders.** Does `vkd3d-compiler` 1.19 compile upstream's DX9 HLSL shaders (shader model 3)?~~ Yes, since Stage 1 (`compile_shader.py`).
- ~~**Gamepads under Wine.**~~ Works: Wine's SDL backend exposes an Xbox 360 pad as XInput (tested with a virtual pad), after four fixes to upstream's XInput code (enhancements.md). Your own controller still needs a feel test.
- **Upstream bug reports (filed 2026-10-06 from your account `loerting`):** PR [#44](https://github.com/smlu/OpenJones3D/pull/44) XInput stick fixes, PR [#45](https://github.com/smlu/OpenJones3D/pull/45) `sithEvent` out-of-bounds write, issue [#46](https://github.com/smlu/OpenJones3D/issues/46) COG lexer, issue [#47](https://github.com/smlu/OpenJones3D/issues/47) DX9 MSAA under Wine, issue [#48](https://github.com/smlu/OpenJones3D/issues/48) continuous damage truncated (FIX-0002), PR [#49](https://github.com/smlu/OpenJones3D/pull/49) inventory item size on ultrawide, plus measurements and the mouse-turn caveat on issue #10 (FIX-0001). PR branches live in the public fork `loerting/OpenJones3D`, cut from upstream `develop`; the private repo is never pushed. Related upstream work: PR #41 and the unmerged branch `remove-fps-dependencies` (Dec 2025) fix frame-rate dependencies; our FIX-0001 covers turning (keeping mouse turning correct and the rate configurable), and the branch's other fixes (frame-cycle timer, drowning and damage at high frame rates, jewel-fly thrust) are candidates for FIX-0002 onward.
- **Upstream's OpenGL branch (checked 2026-10-06).** `upstream/OpenGL` (509 commits, last 2026-07-24) adds an SDL window/event layer (`Libs/wkernel/wkernelSDL.c`), an OpenGL renderer with shaders and shadows (`Libs/std/Win95/GL/`), and vendored libraries. That is most of Stage 5's rendering and windowing; input, audio, the Win32 dialogs and the registry remain. Android's OpenGL ES 3.0 is close to the GL 3.3 it targets. Stage 5 should build on this branch rather than start from scratch; check its build on our clang toolchain first.
- **Upstream patches.** Should the Linux-host patch set be offered upstream?

---

## 13. Glossary

| Term | Meaning |
|---|---|
| **Upstream** | `smlu/OpenJones3D` |
| **Hook / injection** | Upstream's runtime method: `Jones3D.dll` replaces functions inside the running original `Indy3D.exe` |
| **RTI** | Upstream's per-module headers listing the original addresses and signatures of functions |
| **DSS** | The engine's state serialisation, used by savegames |
| **QOL** | Upstream's built-in improvements (`JONES3D_QOL_IMPROVEMENTS`) |
| **Standalone** | The engine runs without the original exe |
| **Native** | Compiled for the host OS (Linux or Android), no Wine |
| **Sith** | The Jedi Knight engine that Jones3D derives from |
| **COG** | The engine's scripting language |
| **GOB / CND** | Data archive / compiled level file |
| **ADR** | Architecture Decision Record, kept in `Docs/Indycompiled/decisions/` |
