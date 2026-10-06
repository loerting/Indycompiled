# Controls: how input reaches Indy (analysis, 2026-10-06)

Written in our own words from upstream's C code and the v1.2 binary. No decompiler output is reproduced here.

## Input path

1. **Devices** (`Libs/std/Win95/DX9/stdControlDX9.c`): DirectInput 8 for keyboard and mouse; XInput for gamepads
   (upstream's addition: dead zones, rumble).
2. **Bindings** (`Libs/sith/Devices/sithControl.c`): each control function (`SITHCONTROL_FORWARD`, `_TURNLEFT`, ...)
   has up to N bindings, of three kinds:
   - key;
   - axis read as a key (`stdControl_ReadAxisAsKey`: on or off, past a threshold);
   - axis (`sithControl_GetAxis`: an analogue value scaled by a per-binding sensitivity).
3. **Player movement** (`Libs/sith/Gameplay/sithPlayerControls.c`, reimplemented upstream from v1.0): reads
   almost everything with `sithControl_GetKey`, i.e. digitally. That's 4× each for forward/back/turn-left/turn-right
   and 5× for jump. The only analogue read is mouse turning (`SITHCONTROL_MOUSETURN`).

**Consequence:** a gamepad stick behaves like arrow keys. Movement and turning have fixed speeds; walking vs running
comes from the run button or the "always run" option. These are classic tank controls.

## Building blocks for analog movement (ENH-0001)

- A helper `indyInput_GetKeyMagnitude(fn)`: for each binding of `fn`, 1.0 for a pressed key, and the normalised
  deflection (after the dead zone) for an axis bound as a key. Return the maximum.
- Where it applies: the walk/run/turn helpers that `sithPlayerControls_ProcessGeneralMove` dispatches to.
- **Design issue:** movement is an animation state machine (still → walk → run). Scaling speed alone makes the feet
  slide, so the walk/run animation speed (puppet) must scale too, or deflection must choose the state
  (e.g. < 60% walks, ≥ 60% runs).

## Turn rate and frame rate (FIX-0001)

All player turning goes through `sithPlayerControls_CalculateAngularVelocity(actor, axis, key, speed)`
= `axis × fps + maxRotVelocity × key × min(speed, 1)` (degrees per second). For the mouse, `axis` is the movement of
this frame, so multiplying by the frame rate gives a rate: correct. For keys and sticks every caller passes a
constant ±1 (±0.5 when crawling), so the first term is simply the frame rate: Indy turned faster the higher the
frame rate (upstream issue #10; upstream PR #41 discusses replacing it with a constant 30 or 60).

FIX-0001 adds `sithPlayerControls_CalculateTurnVelocity(actor, direction, speed)` for the 20 constant-direction
callers (player controls and the whip), with `indyInput_GetTurnFps()` in place of the frame rate: the fixed
`indycompiled.turnRateFps` (default 60) with the fix, the real frame rate without. The 8 mouse callers keep the
original formula. Standing turns are then divided by 1.4, or multiplied by 2.5 with the run key.

## What LucasArts changed in 1.2 (leads, to verify before touching controls)

`sithPlayerControls_ProcessGeneralMove` is 0x110 bytes larger in v1.2 than in v1.0, which upstream's C code is based on.
Comparing what the v1.2 function calls with what upstream's C helpers call:
- v1.2 calls climbing routines directly (`sithPlayerActions_CenterOnClimbSurface`,
  `sithThing_AttachThingToClimbSurface`) and `sithInventory_SetSwimmingInventory`;
- v1.2 checks `sithPlayerActions_HasActiveWeapon` 7 times, upstream's C code once on this path. **Checked: mostly a
  refactoring artifact.** Upstream moved the same "can climb and has no weapon drawn" checks into helpers
  (`sithPlayerControls_CanDoClimbOn1m/2m`).

So the +0x110 bytes are still unexplained. Signature differences are only hints, not proof of a 1.2 change. Hooking upstream's v1.0-based version replaces them.
To do: compare in detail with `Scripts/indy/ghidra/DecompileFunctions.java` (output goes to `game/review/decomp/`,
never into git), then port the differences as FIX entries.

## Gamepad layout (XBOX360 key set, shipped with the game) and the plan (2026-10-07)

A jump/swim, B look, X activate (grab: then push/pull with the stick), Y roll, LB next weapon, RB run toggle,
Back map, Start inventory; D-pad up draw/holster, down crawl, left/right sidestep. Left stick: classic tank movement
(ENH-0001 makes it analog). Right stick: camera (ENH-0002). Triggers: unbound.

Your feedback after the first pad test: analog movement feels good; reversing waits for the stop/idle animation;
you reached for the right stick to look around. Modern third-person games: left stick moves the character relative
to the camera, right stick orbits the camera. Tomb Raider I-III Remastered (2024) offers exactly that as a setting
("modern controls") next to the original tank controls, which is the model here:
1. ENH-0002 right stick camera (done).
2. ENH-0003 faster direction changes (done): the delay came from the walk-to-stand animation, which starts on the
   first frame without input and locks the controls until it ends; a stick flip passes the centre for a few frames.
3. ENH-0005 modern movement (done, experimental until played): camera-relative, as a toggle next to the classic
   controls; see `enhancements.md`.

## Open questions for you

1. What exactly feels janky? Tank turning, camera, jump timing, ledge grabbing, aiming, the whip?
2. Which controller (Xbox, PlayStation, 8BitDo …)? Testing analog movement needs it, so in a real window, not headless.
3. Modern scheme: camera-relative movement with an orbit camera (a bigger change), or "analog tank" controls first (smaller)?

## 1.2 signature changes (found 2026-10-06 by comparing call-site argument counts with upstream's signatures)

All callers of these functions are hooked, i.e. run upstream's C code, so there's no runtime mismatch. They were leads
for code LucasArts changed in 1.2, but the two biggest turned out to be dead arguments (the callee never reads them):
- `sithControl_RegisterKeyFunction`: callers push 2 arguments in v1.2 (49 call sites), 1 in upstream. **Checked: not a
  behaviour change.** The v1.2 function reads only the function id and stores `KEY | REGISTERED`, like upstream's C.
- `sithThing_AttachThingToClimbSurface`: callers push 3 arguments in v1.2, 2 in upstream. **Checked: not a behaviour change.**
  The v1.2 function only reads two parameters; the third (always 1) is a dead argument.
- Smaller ones (one argument more): `sithAnimate_StartSurfaceLightAnim`, `sithCogParse_ResetTreeNodes`, `Sound_Update`,
  `jonesConfig_MsgBoxDlg_HandleWM_COMMAND`, `jonesConfig_sub_405F60`, `JonesDialog_HandleWM_ERASEBKGND`.

Full list: `game/review/semantic.md` (regenerate with `Scripts/indy/verify_semantic.py`).

## Prototype status (ENH-0001)

Implemented in `Libs/indy/indyInput.c`, with hooks in `sithPlayerControls.c` (stick run in `ProcessGeneralMove`, turn
scale at the 8 turn sites) and a read-only binding accessor in `sithControl.c`. Experimental: off until you've tested it
(see `enhancements.md`).
