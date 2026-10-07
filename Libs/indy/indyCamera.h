#ifndef INDY_INDYCAMERA_H
#define INDY_INDYCAMERA_H
// Indycompiled: right stick swings the third-person camera around Indy (ENH-0002, rightStickCamera).
#include <j3dcore/j3d.h>
#include <rdroid/types.h>
#include <stdbool.h>

J3D_EXTERN_C_START

// Called by the external camera for the local player once per frame: adds the camera orbit (pitch, yaw in degrees)
// to the camera rig's angles. The orbit follows a gamepad's right stick and swings back behind Indy when the stick
// is let go (classic controls: forward has to stay where the camera looks). bLookMode: the game's own look mode is
// active, which takes over the camera; the orbit is reset.
// heading: Indy's facing (degrees, counter-clockwise from +x). With modern controls in use (ENH-0005) the camera keeps its
// world direction instead (the orbit compensates Indy's turning) and only trails behind him while he walks away from it.
void J3DAPI indyCamera_ApplyOrbit(rdVector3* pPYR, float secDeltaTime, bool bLookMode, bool bMoving, float heading);

// World direction (degrees, counter-clockwise from +x) the third-person camera looked along in the last frame; false if
// the external camera didn't run for the local player recently (first person, cutscene camera).
bool indyCamera_GetViewHeading(float* pHeading);

// True while a finger drags the camera (touch controls): the camera then follows the finger without its usual delay
bool indyCamera_IsTouchDrag(void);

J3D_EXTERN_C_END
#endif // INDY_INDYCAMERA_H
