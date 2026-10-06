#ifndef INDY_INDYINPUT_H
#define INDY_INDYINPUT_H
// Indycompiled input helpers: analog stick support for the classic controls (ENH-0001, analogMovement).
// All functions behave like the original game (scale 1, no stick run) unless ENH-0001 is enabled and the
// control is driven by a gamepad stick; keyboard input is never changed.
#include <j3dcore/j3d.h>
#include <sith/types.h>
#include <stdbool.h>

J3D_EXTERN_C_START

// Stick deflection (0..1) of the axis bindings of a control function, in the bound direction; 0 without a stick.
// Headless tests can set the environment variable INDY_FAKE_STICK="<forward>,<turn>" (e.g. "0.5,0.4"): a pressed
// key of that control then counts as a stick at that deflection.
float J3DAPI indyInput_GetStickDeflection(SithControlFunction function);

// Turn-rate scale for TURNLEFT/TURNRIGHT: 1 for keys, 0.3..1 following stick deflection.
float J3DAPI indyInput_GetTurnScale(SithControlFunction function);

// True when the forward stick is pushed far enough to run (classic controls walk unless the run key is held).
bool indyInput_IsStickRun(void);

// Frame rate used for key/stick turning (sithPlayerControls_CalculateTurnVelocity): the current one like the
// original, or the fixed rate from Jones.cfg with FIX-0001 (fpsIndependentTurning).
float indyInput_GetTurnFps(void);

J3D_EXTERN_C_END
#endif // INDY_INDYINPUT_H
