# Enhancements and fixes registry

Every deviation from the original game's behaviour has an ID and a toggle (PROJECT.md §8.3). The code is in
`Libs/indy/indyEnh.c`, and `Jones.cfg` controls it:

```json
"indycompiled": { "profile": "enhanced", "toggles": { "analogMovement": false } }
```

- `profile`: `vanilla` (everything off), `fixed` (bug fixes only), `enhanced` (everything on, the default).
- `toggles`: only explicit overrides. Toggles not listed here follow the profile.

IDs are permanent. New entries go at the end of the `IndyEnh` enum.

## Toggles

| ID | Key | Kind | Status | What it does |
|---|---|---|---|---|
| ENH-0001 | `analogMovement` | enhancement | planned | Stick deflection scales walking/running speed and turn rate (today the stick acts like digital keys). See `notes/controls.md`. |

## Port fixes (no toggle: they make our build behave like the original)

| What | Where | Status |
|---|---|---|
| Out-of-bounds write in `sithEvent_ResetFreeBufferTable` (clang -O2 turned it into an endless loop) | `Libs/sith/Gameplay/sithEvent.c` | fixed |
| Address map: platform/file wrappers rearranged in v1.2 (the intro video hung) | `Scripts/indy/rti_v12_reviewed.csv` | fixed |
| Intro video black under Wine when MSAA is on | upstream DX9 back-buffer copy path | open (MSAA off for now) |
