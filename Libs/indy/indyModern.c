#include "indyModern.h"
#include "indyCamera.h"
#include "indyEnh.h"
#include "indyTouch.h"

#include <sith/Gameplay/sithPlayer.h>
#include <sith/Gameplay/sithPlayerActions.h>
#include <std/Win95/stdControl.h>

#include <math.h>

#define INDY_MODERN_MIN_STICK      0.25f // below: no movement (same threshold as the classic stick-as-key reading)
#define INDY_MODERN_RUN_STICK      0.85f // at or above: run (ENH-0001's threshold)
#define INDY_MODERN_WALK_ANGLE     60.0f // walks while still turning toward the stick direction up to this error
#define INDY_MODERN_TURN_DEADBAND  2.0f  // degrees
#define INDY_MODERN_TURN_FULL      20.0f // error (degrees) at which the turn rate reaches 1, up to INDY_MODERN_TURN_MAX
#define INDY_MODERN_TURN_MAX       2.5f  // quick turn-around for big errors (the classic run-key turn boost is 2.5 too)
#define INDY_MODERN_CAMERA_HOLD    3.0f  // seconds the camera stays world-stable after the last modern input
#define INDY_PI                    3.14159265f

static bool indyModern_bInScope;  // inside the ground-movement dispatch
static bool indyModern_bActive;   // virtual keys valid this frame
static bool indyModern_bForward, indyModern_bBack, indyModern_bLeft, indyModern_bRight;
static float indyModern_turnScale = 1.0f;
static float indyModern_magnitude;
static float indyModern_stickAngle; // stick direction relative to the camera (degrees, 0 = away from the camera)
static float indyModern_secSinceUse = 1e9f;

// Left stick of the first gamepad that has it deflected, or the touch stick: x right, y up, -1..1
static void indyModern_ReadLeftStick(float* pX, float* pY)
{
    if ( indyTouch_GetStick(pX, pY) )
    {
        return;
    }

    *pX = *pY = 0.0f;
    for ( int joyNum = 0; joyNum < (int)stdControl_GetNumJoysticks(); joyNum++ )
    {
        if ( !stdControl_IsGamePad(joyNum) )
        {
            continue;
        }
        float x = stdControl_ReadAxis(STDCONTROL_GET_JOYSTICK_AXIS_X(joyNum));
        float y = -stdControl_ReadAxis(STDCONTROL_GET_JOYSTICK_AXIS_Y(joyNum)); // engine axes: y positive down
        if ( fabsf(x) + fabsf(y) > fabsf(*pX) + fabsf(*pY) )
        {
            *pX = x;
            *pY = y;
        }
    }
}

static float indyModern_WrapDegrees(float angle)
{
    return remainderf(angle, 360.0f);
}

void J3DAPI indyModern_Begin(SithThing* pThing, float secDeltaTime)
{
    indyModern_bInScope = true;
    indyModern_bActive  = false;
    indyModern_secSinceUse += secDeltaTime;

    const bool bSurfaceSwimming = pThing->moveStatus == SITHPLAYERMOVE_SWIMIDLE && (pThing->moveInfo.physics.flags & SITH_PF_ONWATERSURFACE) != 0;
    if ( !indyEnh_IsEnabled(INDY_ENH_MODERN_CONTROLS) || pThing != sithPlayer_g_pLocalPlayerThing
        || (pThing->moveStatus != SITHPLAYERMOVE_STILL && pThing->moveStatus != SITHPLAYERMOVE_WALKING
            && pThing->moveStatus != SITHPLAYERMOVE_RUNNING && !bSurfaceSwimming) )
    {
        return;
    }

    float x, y, viewHeading;
    indyModern_ReadLeftStick(&x, &y);
    float magnitude = sqrtf(x * x + y * y);
    if ( magnitude < INDY_MODERN_MIN_STICK || !indyCamera_GetViewHeading(&viewHeading) )
    {
        return;
    }

    indyModern_bActive     = true;
    indyModern_secSinceUse = 0.0f;
    indyModern_magnitude   = magnitude > 1.0f ? 1.0f : magnitude;
    indyModern_stickAngle  = atan2f(-x, y) * (180.0f / INDY_PI); // 0: up (away from the camera), positive: left

    float heading = atan2f(pThing->orient.lvec.y, pThing->orient.lvec.x) * (180.0f / INDY_PI);
    float error   = indyModern_WrapDegrees(viewHeading + indyModern_stickAngle - heading);

    indyModern_bForward = indyModern_bBack = indyModern_bLeft = indyModern_bRight = false;

    // standing with a ledge behind Indy and the stick pointing at it: climb down, like Back in the classic controls
    if ( pThing->moveStatus == SITHPLAYERMOVE_STILL && fabsf(error) > 135.0f && sithPlayerActions_CheckClimbDownWall(pThing) != 0 )
    {
        indyModern_bBack = true;
        return;
    }

    indyModern_bLeft    = error > INDY_MODERN_TURN_DEADBAND;
    indyModern_bRight   = error < -INDY_MODERN_TURN_DEADBAND;
    indyModern_turnScale = fminf(fmaxf(fabsf(error) / INDY_MODERN_TURN_FULL, 0.1f), INDY_MODERN_TURN_MAX);
    indyModern_bForward = fabsf(error) < INDY_MODERN_WALK_ANGLE;
}

void indyModern_End(void)
{
    indyModern_bInScope = false;
}

bool J3DAPI indyModern_GetKey(SithControlFunction function, int* pValue)
{
    if ( !indyModern_bInScope || !indyModern_bActive )
    {
        return false;
    }

    switch ( function )
    {
        case SITHCONTROL_FORWARD:   *pValue = indyModern_bForward; return true;
        case SITHCONTROL_BACK:      *pValue = indyModern_bBack;    return true;
        case SITHCONTROL_TURNLEFT:  *pValue = indyModern_bLeft;    return true;
        case SITHCONTROL_TURNRIGHT: *pValue = indyModern_bRight;   return true;
        default:                    return false;
    }
}

bool J3DAPI indyModern_GetTurnScale(SithControlFunction function, float* pScale)
{
    J3D_UNUSED(function);
    if ( !indyModern_bInScope || !indyModern_bActive )
    {
        return false;
    }
    *pScale = indyModern_turnScale;
    return true;
}

bool indyModern_GetRun(bool* pbRun)
{
    if ( !indyModern_bInScope || !indyModern_bActive )
    {
        return false;
    }
    *pbRun = indyModern_bForward && indyModern_magnitude >= INDY_MODERN_RUN_STICK;
    return true;
}

bool indyModern_IsCameraWorldStable(void)
{
    return indyEnh_IsEnabled(INDY_ENH_MODERN_CONTROLS) && indyModern_secSinceUse < INDY_MODERN_CAMERA_HOLD;
}

bool indyModern_IsStickAwayFromCamera(void)
{
    return indyModern_bActive && fabsf(indyModern_stickAngle) < 30.0f;
}
