# 64-bit audit (2026-10-07)

## Method

- All 139 engine translation units were re-run as syntax-only passes for `x86_64-w64-mingw32` and
  `i686-w64-mingw32`, and the results diffed.
- Supporting checks:
  - clang record layouts (struct sizes on both targets);
  - an AST scan of pointer↔int casts;
  - an `-funsigned-char` run;
  - a printf format check on `stdPrintf`;
  - a scan of the 21 shipped CND files.

## Numbers

| Item | Count |
|---|---|
| `static_assert(sizeof…)` checks | 209 |
| … failing on x86_64 | 96 |
| … of those, runtime structs with pointers | ~75 (expected) |
| … of those, failing only because of `size_t` fields | ~13 |
| Implicit pointer↔int conversions | 0 |
| Real pointer smuggles | a handful (below) |
| `long` in structs | 0 |
| `char` signedness dependencies | 0 (`-funsigned-char` adds no warnings) |
| `-Wshorten-64-to-32` sites | 530 |
| printf mismatches seen only on x64 | 171 (mostly `%d` with `size_t`; harmless in practice) |

**The COG VM needs no handle tables.** Objects are referenced by index and guid, `SithCogValue` has a real `void*`
member, and message and event params are ints. That's unlike OpenJKDF2.

## Persisted data that breaks on 64-bit (needs fixed-width disk structs)

| Data | Problem |
|---|---|
| `CndWorld` | 4 `size_t` fields; `sithWorld.c:1481` also reads `sizeof(CndWorld)` straight into `SithWorld` |
| `CndThingSectionCounts`, `aPathFrameCounts` | `size_t`; fixed in our code, now `uint32_t` |
| `SoundInfo` (`sound/types.h`) | has a pointer (always 0 in the shipped files); the sound-bank globals are `size_t` but read and written as 4 bytes |
| `sizePVS` (`sithPVS.c`) | `size_t` read as 4 bytes |
| `NdsHeader` (savegame) | `SithLevelStatistic` `size_t`s plus the COG value union; must stay exactly 1188 bytes on disk |
| BMP headers (`stdBmp`, savegame thumbnail) | use Windows structs |
| DSS wide strings (`sithDSS_Write/ReadWString`) | use `sizeof(wchar_t)` (2 on Windows, 4 on Linux) |

Everything else (Cnd* records, MAT, GOB, GCF, WAV, the DSS message body) is fixed-width.

## Pointer smuggles

| Where | What | Severity |
|---|---|---|
| `Sound.c:1009/1248` | `Sound_LoadStatic` passes a pointer through a `uint32_t[2]` | breaks |
| `AudioLib.c:320/373` | a pointer returned as a `uint32_t` "offset"; only `indyDiff` uses it | breaks |
| `stdMemory.c` zone allocator | 32-bit by design; use `malloc` on 64-bit | breaks |
| `rdCache.c:493` | pointer-difference comparator truncated to `int` | effectively dead code |
| `sithCogExec.c:1035` | reads a symbol id through `pointerValue` | works only by truncation |
| `sithAI.c`, `sithCogFunctionAI.c` | AI mode passed as `void*` | round-trips fine; use `intptr_t` |

## Bugs on every platform (found by the format check and the scan)

| Where | Bug | Status |
|---|---|---|
| `sithThing.c:3974` | printf arguments swapped (`%d` gets a `char*`, `%s` gets a `size_t`) | fixed |
| `sithCogParse.c:611` | realloc uses `sizeof(int32_t)` instead of `sizeof(SithCogSyntaxNode)`: heap overflow once a COG passes 8096 nodes | fixed |
| `sithCogFunctionThing.c:2545` | `%s` given an `rdKeyframe*` | open |
| `jonesConfig.c:1278` | `%s` given 0 | open |
| `stdDisplayDX9.c:1418` | `%d` given a string | open |
| `DriverDX9.c:152` | a `va_list` passed as a variadic argument | open |

## arm64

- **Float → unsigned:** arm64 saturates negative values to 0, x86 wraps. About 12 sites can go negative: COG
  durations, HUD colour packing. Add a clamp helper.
- **Floating point:** use `-ffp-contract=off`, since clang fuses multiply-add into FMA on arm64. SSE/NEON match the
  original better than an x87 i686 build would: our D3D device runs x87 at 24-bit precision.
- **`RAND_MAX`:** 2³¹−1 on glibc/bionic, while the engine expects 32767. Fixed: `SITH_RAND` now divides by 32767.

## Proposed approach

1. **`j3d.h`:** `J3DAPI` becomes `__cdecl` only on 32-bit Windows.
2. **Assert macros:** `J3D_ABI_ASSERT_SIZE` for runtime structs shared with the exe (checked only in hook builds),
   `J3D_DISK_ASSERT_SIZE` for on-disk records (checked everywhere).
3. **Disk twins:** fixed-width disk structs for the data in the table above, converted field by field. Each is
   byte-identical on today's 32-bit build, so the savegame A/B harness verifies them now.
4. **CI:** an x86_64 syntax pass that tracks the failing-assert count down to 0.
5. **Order:** go straight to x86_64. After Stage 4, optionally build an `x86_64-w64-mingw32` standalone under Wine
   (it changes only the pointer size); skip native i686 Linux.

Effort after Stages 4 and 5: about 8 person-days, plus 2 for the deferrable 64-bit warning triage.
