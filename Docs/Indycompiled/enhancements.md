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
| ENH-0001 | `analogMovement` | experimental | prototype; wiring tested headless (stick 1.0 → run, 0.5 → walk, toggle off → original); needs a gamepad test | With a gamepad stick: pushing it past 85% runs, less walks (no run button needed); turn rate follows stick deflection (30–100%). Keyboard input is unchanged. Walking speed itself isn't scaled yet (the walk animation would slide). See `notes/controls.md`. |

Kinds: **fix** (on in `fixed` and `enhanced`), **enhancement** (on in `enhanced`), **experimental** (off in every
profile until tested; enable it explicitly in `toggles`).

### Testing ENH-0001 with a controller

1. Deploy the build: `Scripts/indy/deploy.sh mingw-dx9-release`.
2. In `game/run/Resource/Jones.cfg`, add `"indycompiled": { "toggles": { "analogMovement": true } }`.
3. Run `Scripts/indy/play.sh --build`, start Canyonlands and compare a light push and a full push on the left
   stick, for walking and turning.
4. Headless wiring test without a controller: `INDY_FAKE_STICK="<forward>,<turn>"` (e.g. `"1.0,0.5"`) makes a held
   keyboard key count as a stick at that deflection, and JonesLog.txt logs `indyInput: forward stick … -> run/walk`.

## Port fixes (no toggle: they make our build behave like the original)

| What | Where | Status |
|---|---|---|
| Out-of-bounds write in `sithEvent_ResetFreeBufferTable` (clang -O2 turned it into an endless loop) | `Libs/sith/Gameplay/sithEvent.c` | fixed |
| Address map: platform/file wrappers rearranged in v1.2 (the intro video hung) | `Scripts/indy/rti_v12_reviewed.csv` | fixed |
| Intro video black under Wine when MSAA is on | `stdDisplay_CopyBufferToSurface`: copying into a multisampled target now draws the source as a quad (`Libs/indy/indyDisplayDX9.c`). Wine's `StretchRect` reports success there but copies nothing. | fixed, tested headless with 8× MSAA |
