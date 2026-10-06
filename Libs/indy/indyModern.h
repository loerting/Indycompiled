#ifndef INDY_INDYMODERN_H
#define INDY_INDYMODERN_H
// Indycompiled: modern, camera-relative movement for gamepads (ENH-0005, modernControls).
//
// The left stick gives a direction relative to the camera; Indy turns toward it and walks or runs that way, as in
// modern third-person games (and Tomb Raider I-III Remastered's "modern controls"). The game's movement code stays
// as it is: while it processes ground movement (standing, walking, running), sithControl_GetKey returns virtual
// forward / turn keys computed from the stick and the camera. Everything else (climbing, hanging, swimming, pushing,
// vehicles, the keyboard) keeps the classic controls.
#include <j3dcore/j3d.h>
#include <sith/types.h>
#include <stdbool.h>

J3D_EXTERN_C_START

// Around the ground-movement dispatch in sithPlayerControls_ProcessGeneralMove: computes this frame's virtual keys.
void J3DAPI indyModern_Begin(SithThing* pThing, float secDeltaTime);
void indyModern_End(void);

// For sithControl_GetKey: true if the key is overridden this frame (then *pValue is the virtual state).
bool J3DAPI indyModern_GetKey(SithControlFunction function, int* pValue);

// For ENH-0001: turn rate scale and run decision while the modern input is active (false: not active).
bool J3DAPI indyModern_GetTurnScale(SithControlFunction function, float* pScale);
bool indyModern_GetRun(bool* pbRun);

// For the camera (ENH-0002): the camera keeps its world direction while modern input is in use (recently), and
// trails behind Indy only while the stick points away from the camera.
bool indyModern_IsCameraWorldStable(void);
bool indyModern_IsStickAwayFromCamera(void);

J3D_EXTERN_C_END
#endif // INDY_INDYMODERN_H
