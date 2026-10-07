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
| ENH-0001 | `analogMovement` | enhancement (on in `enhanced`); played with a real pad 2026-10-07: "feels good" | With a gamepad stick: pushing it past 85% runs, less walks (no run button needed); turn rate follows stick deflection (30–100%). Keyboard input is unchanged. Walking speed itself isn't scaled yet (the walk animation would slide). See `notes/controls.md`. |
| ENH-0002 | `rightStickCamera` | enhancement; tested headless with the virtual pad (orbit 150°/s, pitch −40°..+30°, swings back 0.6 s after release) | The right stick swings the third-person camera around Indy, as in modern third-person games. With the classic (tank) controls it swings back behind Indy when let go, faster while he moves, so "forward" stays where the camera looks; the game's own look mode (B) takes priority. Wall collision and camera smoothing work as before (the orbit is added to the camera rig's angles). |
| ENH-0003 | `quickDirectionChange` | enhancement; measured headless (`test_reverse.sh`): forward-to-back stick flip, backward motion starts ~0.1 s after the flip instead of ~0.8 s | Walking and running only turn into the stop animation (which locks the controls until it ends) after 0.15 s without movement input, instead of on the first neutral frame. Flipping the stick from forward to back (it passes the centre) or pressing Down just after releasing Up now reverses directly. Releasing everything still stops Indy as before, 0.15 s later. |
| ENH-0004 | `skipIntro` | enhancement; tested headless (travel map 6 s after launch, no key pressed) | No intro video (`jonesopn*.snm`) at game start; the game goes straight to loading. Level cutscenes are unchanged. Off in `vanilla`. |
| ENH-0005 | `modernControls` | enhancement (on in `enhanced`); played with a real pad 2026-10-07: "plays nicely"; tested headless (`test_modern.sh`: stick right turns Indy -88 deg relative to the camera and he walks that way; stick down turns him 178 deg to face the camera) | Camera-relative left stick, like Tomb Raider I-III Remastered's "modern controls": push the stick in a direction and Indy turns toward it (quick turn-around for big angles) and walks, or runs at full deflection. Only on the ground (standing, walking, running); climbing, hanging, swimming, pushing, vehicles and the keyboard stay classic. Standing with a ledge behind Indy, pushing toward it climbs down. While it is in use the camera keeps its direction in the world (right stick turns it) and trails behind Indy only while he walks away from it; 3 s after the last modern input it is the classic camera again. How: `Libs/indy/indyModern.c` turns the stick and the camera direction into virtual forward/turn keys, which `sithControl_GetKey` returns during the ground-movement dispatch. |
| ENH-0006 | `cutscenes4to3` | enhancement (on in `enhanced`); tested headless at 1280×720 (intro: 4:3 frame with black bars, gameplay widescreen) and on a 20:9 phone | On screens wider than 4:3, cutscenes with the cinematic camera show the centred 4:3 part of the picture with black bars, as they were made: wider, the intro's black fade plates (3D objects in front of the camera) and the framing leave the sides uncovered. Gameplay stays widescreen. The widescreen projection keeps the vertical field of view, so the centre is exactly the original 4:3 view; `JonesDisplay_SetCinemaFrame` clips the scene with a scissor rectangle (`std3D_SetScissor`, OpenGL ES and DirectX 9). |

| FIX-0001 | `fpsIndependentTurning` | fix | measured headless (standing turn, 4 s): without the fix 70.8 / 91.5 / 133.0 °/s at 30 / 60 / 120 FPS; with it 92.2 / 91.5 / 90.2 °/s | Turning with keys or a stick no longer depends on the frame rate. The original multiplies the turn direction by the current frame rate (right for mouse deltas, wrong for keys), so Indy turned faster at higher frame rates (upstream issue #10). Turning is calibrated to `indycompiled.turnRateFps` (default 60, i.e. how it felt on 60 Hz screens; 15–240). Mouse turning is unchanged. Test: `Scripts/indy/test_turnrate.sh`. |

| FIX-0002 | `fpsIndependentDamage` | fix | measured headless (100 damage/s for 3 s): without the fix 270 / 180 / 279 lost at 30 / 60 / ~100 FPS; with it 300 / 300 / 300 | Damage dealt a little every frame (drowning, raft leak, IMP blast) no longer depends on the frame rate. `sithThing_DamageThing` passes damage to COG as an integer, so per-frame fractions were lost and amounts below 1 vanished (no drowning at ~250 FPS). The fraction is carried to the next frame. See `notes/timing.md`. |
| FIX-0003 | `fpsIndependentCycles` | fix | measured headless (cycle of 16 per second): without the fix 1.84 / 6.10 at 30 / 100 FPS; with it 1.84 / 1.90 (30/16 = 1.875) | "Every N-th frame" events that are about time (random idle animations, breathing sounds, sprite flicker, AI awareness pings; 16 sites) run on game time (30 intended frames per second) instead of frame count. Per-frame upkeep (matrix normalisation, texture wrap, target search) is unchanged. See `notes/timing.md`. |

| FIX-0004 | `fpsIndependentJewelFly` | fix | arithmetic checked and builds; not playtested (jewel flight is late in the Aetherium level, out of reach for the headless tests) | Up/down acceleration in jewel flight: the original added `0.0002 × fps` to the thrust every frame, so it built up with fps² per second (0.18/s at 30 FPS, 0.72 at 60, 2.88 at 120, 11.5 at 240). Now scaled by the frame time and calibrated to 60 FPS: 0.72/s at any frame rate. |

Kinds: **fix** (on in `fixed` and `enhanced`), **enhancement** (on in `enhanced`), **experimental** (off in every
profile until tested; enable it explicitly in `toggles`).

### Testing ENH-0001 with a controller

1. Build and deploy: `Scripts/indy/play.sh --build` (or `Scripts/indy/deploy.sh mingw-dx9-release`).
2. In `game/run/Resource/Jones.cfg`: `"controls": { "controller": true, "configFile": "XBOX360" }` (the XBOX360
   keyset ships with the game; the in-game Options → Controls dialog sets the same) and
   `"indycompiled": { "toggles": { "analogMovement": true } }`.
3. Start Canyonlands and compare a light push and a full push on the left stick, for walking and turning.
   `INDY_INPUT_TRACE=250` (environment) logs the stick readings and Indy's position to `JonesLog.txt`.
4. Without a controller: `Scripts/indy/test_gamepad.sh` creates a virtual Xbox 360 pad (`virtual_pad.py`, needs
   write access to `/dev/uinput`) and checks the whole path headless; `INDY_TEST_ANALOG=0|1` forces ENH-0001 off/on.
   `INDY_FAKE_STICK="<forward>,<turn>"` simulates a stick with held keyboard keys.

## Upstream bug fixes in OpenJones3D's own additions (no toggle: the original game has no XInput)

| What | Where | Status |
|---|---|---|
| A stick held still read 0: `stdControl_ReadControls` clears all axes every frame, but `stdControl_ReadXInput` skipped devices whose packet number hadn't changed | `Libs/std/Win95/DX9/stdControlDX9.c` | fixed, tested with the virtual pad |
| Stick Y inverted: XInput's up is positive, the engine's (DirectInput's) is negative, so forward on the stick walked backwards | same | fixed, tested |
| Double dead zone on XInput sticks (XInput's, then the engine's, unscaled): the first ~42% of stick travel did nothing | same | fixed |
| `stdControl_IsGamePad`: `>` instead of `>=`, so the first XInput pad counted as no gamepad | same | fixed |
| Inventory items half size at 21:9 and wider: integer division in the HUD aspect factor (`width / height` with `uint32_t`) | `Jones3D/Display/JonesHud.c` | fixed, screenshots at 4:3 / 16:9 / 21:9; upstream [PR #49](https://github.com/smlu/OpenJones3D/pull/49) |

Reported upstream: [PR #44](https://github.com/smlu/OpenJones3D/pull/44). Port fixes below: sithEvent [PR #45](https://github.com/smlu/OpenJones3D/pull/45), COG lexer [issue #46](https://github.com/smlu/OpenJones3D/issues/46), MSAA [issue #47](https://github.com/smlu/OpenJones3D/issues/47).

## Port fixes (no toggle: they make our build behave like the original)

| What | Where | Status |
|---|---|---|
| Out-of-bounds write in `sithEvent_ResetFreeBufferTable` (clang -O2 turned it into an endless loop) | `Libs/sith/Gameplay/sithEvent.c` | fixed |
| Address map: platform/file wrappers rearranged in v1.2 (the intro video hung) | `Scripts/indy/rti_v12_reviewed.csv` | fixed |
| COG lexer read bytes ≥ 0x80 outside its 7-bit table; `10_sea_vol_frets.cog` ("Meroë" in a comment) sent it into an endless loop, so level 11 never loaded | `Libs/sith/Cog/sithCogFlex.c` | fixed (bytes folded to 7 bits; the regenerated lexer needs the same fix) |
| Intro video black under Wine when MSAA is on | `stdDisplay_CopyBufferToSurface`: copying into a multisampled target now draws the source as a quad (`Libs/indy/indyDisplayDX9.c`). Wine's `StretchRect` reports success there but copies nothing. | fixed, tested headless with 8× MSAA |
