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

## What LucasArts changed in 1.2 (leads, to verify before touching controls)

`sithPlayerControls_ProcessGeneralMove` is 0x110 bytes larger in v1.2 than in v1.0, which upstream's C code is based on.
Comparing what the v1.2 function calls with what upstream's C helpers call:
- v1.2 calls climbing routines directly (`sithPlayerActions_CenterOnClimbSurface`,
  `sithThing_AttachThingToClimbSurface`) and `sithInventory_SetSwimmingInventory`;
- v1.2 checks `sithPlayerActions_HasActiveWeapon` 7 times, upstream's C code once (on this path).

Likely 1.2 fixes around climbing and swimming with a drawn weapon. Hooking upstream's v1.0-based version replaces them.
To do: compare in detail with `Scripts/indy/ghidra/DecompileFunctions.java` (output goes to `game/review/decomp/`,
never into git), then port the differences as FIX entries.

## Open questions for you

1. What exactly feels janky? Tank turning, camera, jump timing, ledge grabbing, aiming, the whip?
2. Which controller (Xbox, PlayStation, 8BitDo …)? Testing analog movement needs it, so in a real window, not headless.
3. Modern scheme: camera-relative movement with an orbit camera (a bigger change), or "analog tank" controls first (smaller)?

## 1.2 signature changes (found 2026-10-06 by comparing call-site argument counts with upstream's signatures)

All callers of these functions are hooked, i.e. run upstream's C code, so there's no runtime mismatch. But they show where
LucasArts changed code in 1.2, and those fixes are lost wherever upstream's v1.0-based C code replaces the original:
- `sithControl_RegisterKeyFunction`: 2 arguments in v1.2 (49 call sites), 1 in upstream. Possibly part of the 1.2 fix
  to the control configuration dialog.
- `sithThing_AttachThingToClimbSurface`: 3 arguments in v1.2, 2 in upstream. A climbing change, matching the leads in
  `sithPlayerControls_ProcessGeneralMove`.
- Smaller ones (one argument more): `sithAnimate_StartSurfaceLightAnim`, `sithCogParse_ResetTreeNodes`, `Sound_Update`,
  `jonesConfig_MsgBoxDlg_HandleWM_COMMAND`, `jonesConfig_sub_405F60`, `JonesDialog_HandleWM_ERASEBKGND`.

Full list: `game/review/semantic.md` (regenerate with `Scripts/indy/verify_semantic.py`).

## Prototype status (ENH-0001)

Implemented in `Libs/indy/indyInput.c`, with hooks in `sithPlayerControls.c` (stick run in `ProcessGeneralMove`, turn
scale at the 8 turn sites) and a read-only binding accessor in `sithControl.c`. Experimental: off until you've tested it
(see `enhancements.md`).
