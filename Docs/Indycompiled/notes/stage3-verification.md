# Stage 3 verification (2026-10-07)

Each Stage 3 module was A/B-tested on Indy3D.exe v1.2 under Wine. Side A runs the exe's original functions
(`INDY_NOHOOK`); side B runs our C. The two sides' world snapshots are compared field by field, with exact
comparison (`INDY_AB_TOL=0`). Simulation runs use a fixed 33 ms per frame and no input. For the tools, see
PROJECT.md §8.4.

| Module | Test | Levels | Result |
|---|---|---|---|
| sithThing CND reader (`ReadThingsListBinary`) | world snapshot after loading | all 17 | identical |
| sithThing CND writer (`WriteThingsListBinary`) | `test_diff.sh sithThing`, in-process byte comparison | 1–5 | 0 mismatches |
| sithDSS, sithDSSThing | savegame written at frame 150 | 1, 2, 5 | byte-identical (sound clock masked) |
| sithDSS, sithDSSThing | world snapshot at frame 200 | 1, 2, 5 | identical |
| sithDSS `Process*` | both sides restore the original's level-1 savegame | 1 | identical (242 things) |
| sithAIUtil | 300 frames | 1–3 | identical |
| All AI (AIUtil, AIMove, AIInstinct; 107 functions) | 400 frames, idle | 2, 4, 9, 17 | identical |
| All AI | `INDY_SIM_ENGAGE=30` (the player is placed in front of the nearest AI), snapshot at frame 330 | 2, 4, 9, 17 | identical |
| sithPhysics (vehicles, Quetzalcoatl) | 400 frames | 2, 7, 8 (jeep), 10, 13 | identical |
| SMUSH player | `test_smush.sh`, against FFmpeg | intro videos | audio bit-exact; video identical except edge pixels |
| SMUSH player | intro smoke test, `skipIntro=0` | — | logo and Canyonlands map play through our player |
| Everything | `level_sweep.sh 60` | all 17 | all load, 0 errors |

## Gaps

- **Engaged AI.** The engaged run moved Indy and one actor, but no fight took place. Combat paths (attack, flee,
  dodge) are only lightly covered. Real play is the better test.
- **Vehicle physics.** Without input, the mine car, jeep and raft paths only run while idle. A playthrough with
  input covers them.
- **Multiplayer.** Stubbed on purpose (Android and Linux are single-player).

## Bugs found on the way (all platforms)

- DSS (hybrid build only): the camera-aspect reset flag lives in our DLL, but the original savegame code read the exe's unused copy. `sithRender` now has accessors for it.
- `sithThing.c`: printf arguments swapped.
- `sithCogParse.c`: realloc undersized, causing a heap overflow once a COG passes 8096 nodes.
- `RAND_MAX`: the engine assumes 32767. `SITH_RAND` and the instinct random factor now divide by 32767.
