# Timing: frame-rate dependencies (2026-10-06)

The engine was built for roughly 30 FPS. On a modern machine (60–240 FPS) several things change speed. Three causes
found so far, each with a fix toggle (`enhancements.md`) and a headless test that measures it at fixed frame caps
(`INDY_FPS_CAP`, see `Libs/indy/indyDebug.c`).

## 1. Turning (FIX-0001, `fpsIndependentTurning`)

`sithPlayerControls_CalculateAngularVelocity` adds `direction × current fps`. Right for the mouse (per-frame delta),
wrong for keys and sticks (constant ±1): standing turns went 70.8 → 91.5 → 133 °/s at 30 → 60 → 120 FPS. The 20
constant-direction callers now use a fixed rate (`indycompiled.turnRateFps`, default 60). Details: `controls.md`.
Test: `Scripts/indy/test_turnrate.sh`.

## 2. Continuous damage (FIX-0002, `fpsIndependentDamage`)

`sithThing_DamageThing` hands the damage to COG as an **integer** (`SITHCOG_MSG_DAMAGED` parameter) and applies what
comes back. Damage dealt a little every frame therefore loses its fraction, and amounts below 1 vanish:

| | per frame | 30 FPS | 60 FPS | 144 FPS | 250 FPS |
|---|---|---|---|---|---|
| Drowning (`msecDelta / 5`, intended 200/s) | 6.6 / 3.3 / 1.4 / 0.8 | 180/s | 180/s | 144/s | **0** |

So at high frame rates Indy drowns slowly or not at all. The raft leak (`sithActor_Update`) and the IMP blast
(`sithPlayer.c`) have the same pattern; upstream `develop` clamps the IMP damage to at least 1 per frame, which deals
too much above ~40 FPS, and upstream's unmerged `remove-fps-dependencies` branch does the same for drowning ("not
ideal solution"). FIX-0002 (`Libs/indy/indyDamage.c`) carries the fraction per thing and damage type to the next frame
and only hands whole numbers to `sithThing_DamageThing`: exact at any frame rate.

Measured (`Scripts/indy/test_damage.sh`, 100 damage/s for 3 s through the drowning path): without the fix 270 / 180 /
279 lost at 30 / 60 / ~100 FPS, with it 300 / 300 / 300.

## 3. "Every N-th frame" events (FIX-0003, `fpsIndependentCycles`)

`SITH_ISFRAMECYCLE(offset, n)` is true on every n-th frame. 25 call sites; at 120 FPS they come round 4× as often as
at 30. Whether that matters depends on the site:

| Kept per frame (`SITH_ISFRAMECYCLE`) | Moved to game time (`SITH_ISTIMECYCLE`, 30 intended frames per second) |
|---|---|
| Orientation matrix normalisation (physics ×3, collision): numerical upkeep, more frames need it more | Random idle/fidget animations (`sithPuppet`, 9 sites): a random chance per cycle, so more frequent with FPS |
| Texture-scroll wrap (`sithAnimate` ×3): keeps the offsets small | Breathing, gasping and drowning sounds (`sithActor` ×3) |
| Aim target search every other frame (`sithPlayerControls`): performance | Random sprite cel (fire, smoke: `sithFX`) |
| | AI awareness pings from the jeep, mine car and decaying weapons (`sithPhysics` ×2, `sithWeapon`) |

`Libs/indy/indyFrame.c` counts intended frames from the frame time (`sithUpdate`) and reports a cycle when an
intended frame with `(k + offset) % n == 0` passed since the previous frame. This is the idea of upstream's
unmerged branch, applied per site instead of to every call. Test: `Scripts/indy/test_cycles.sh`.

## Not covered yet

- Functions still running as original v1.2 code (AI: `sithAIUtil`, `sithAIMove`, `sithAIInstinct`) may have their
  own frame-rate dependencies; they can only be fixed once reimplemented (Stage 3).
- Debug fly mode (`sithPlayerControls_ProcessFlyMove`, not gameplay) pitches at `±fps` degrees per second.

## 4. Jewel-flight thrust (FIX-0004, `fpsIndependentJewelFly`)

`sithPlayerControls_ProcessJewelFlyMove` adds `0.0002 × fps` to the vertical thrust every frame: per second that is
`0.0002 × fps²` (0.18 at 30 FPS, 0.72 at 60, 2.88 at 120, 11.5 at 240). With the fix the increment is
`0.0002 × 60 × 60 × frame time`: 0.72 per second at any frame rate, the same as the original at 60 FPS. Not
playtested (the jewel flight is late in the Aetherium level).
