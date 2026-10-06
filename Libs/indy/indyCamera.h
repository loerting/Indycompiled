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
void J3DAPI indyCamera_ApplyOrbit(rdVector3* pPYR, float secDeltaTime, bool bLookMode, bool bMoving);

J3D_EXTERN_C_END
#endif // INDY_INDYCAMERA_H
