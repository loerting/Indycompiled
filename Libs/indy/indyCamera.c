#include "indyCamera.h"
#include "indyEnh.h"
#include "indyModern.h"

#include <std/General/std.h>
#include <std/Win95/stdControl.h>

#include <Windows.h>
#include <stdlib.h>

#include <math.h>

#define INDY_CAMERA_YAW_SPEED     150.0f // degrees per second at full deflection
#define INDY_CAMERA_PITCH_SPEED   90.0f
#define INDY_CAMERA_PITCH_MIN     -40.0f // looking down
#define INDY_CAMERA_PITCH_MAX     30.0f  // looking up
#define INDY_CAMERA_RECENTER_WAIT 0.6f   // seconds without input before the camera swings back
#define INDY_CAMERA_RECENTER_RATE 3.0f   // exponential rate (per second) of the swing back; doubled while moving

#define INDY_CAMERA_TRAIL_RATE   1.5f // modern controls: rate (per second) at which the camera trails behind Indy

static float indyCamera_yaw, indyCamera_pitch;
static float indyCamera_secIdle;
static float indyCamera_worldYaw;         // modern controls: camera direction in the world
static bool indyCamera_bWorldYawValid;
static float indyCamera_viewHeading;      // last frame's view direction, for the modern movement
static unsigned int indyCamera_viewFrame; // GetTickCount of the last update

// Right stick of the first gamepad that has it deflected: x right, y up, -1..1.
static void indyCamera_ReadRightStick(float* pX, float* pY)
{
    *pX = *pY = 0.0f;
    for ( int joyNum = 0; joyNum < (int)stdControl_GetNumJoysticks(); joyNum++ )
    {
        if ( !stdControl_IsGamePad(joyNum) )
        {
            continue;
        }

        size_t axisX = STDCONTROL_GET_JOYSTICK_AXIS_RX(joyNum), axisY = STDCONTROL_GET_JOYSTICK_AXIS_RY(joyNum);
        if ( !stdControl_TestAxisFlag(axisX, STDCONTROL_AXIS_ENABLED) )
        {
            stdControl_EnableAxis((int)axisX); // nothing binds the right stick, so the game never reads it
        }
        if ( !stdControl_TestAxisFlag(axisY, STDCONTROL_AXIS_ENABLED) )
        {
            stdControl_EnableAxis((int)axisY);
        }

        float x = stdControl_ReadAxis(axisX), y = -stdControl_ReadAxis(axisY); // engine axes: y positive down
        if ( fabsf(x) + fabsf(y) > fabsf(*pX) + fabsf(*pY) )
        {
            *pX = x;
            *pY = y;
        }
    }
}

bool indyCamera_GetViewHeading(float* pHeading)
{
    *pHeading = indyCamera_viewHeading;
    return GetTickCount() - indyCamera_viewFrame < 500;
}

void J3DAPI indyCamera_ApplyOrbit(rdVector3* pPYR, float secDeltaTime, bool bLookMode, bool bMoving, float heading)
{
    indyCamera_viewFrame   = GetTickCount();
    indyCamera_viewHeading = heading;
    if ( !indyEnh_IsEnabled(INDY_ENH_RIGHT_STICK_CAMERA) )
    {
        return;
    }

    if ( bLookMode )
    {
        indyCamera_yaw = indyCamera_pitch = 0.0f;
        indyCamera_bWorldYawValid = false;
        return;
    }

    float x, y;
    indyCamera_ReadRightStick(&x, &y);

    if ( indyModern_IsCameraWorldStable() )
    {
        // modern controls: the camera keeps its direction in the world while Indy turns
        if ( !indyCamera_bWorldYawValid )
        {
            indyCamera_worldYaw       = heading + indyCamera_yaw;
            indyCamera_bWorldYawValid = true;
        }
        if ( x != 0.0f || y != 0.0f )
        {
            indyCamera_worldYaw -= x * fabsf(x) * INDY_CAMERA_YAW_SPEED * secDeltaTime;
            indyCamera_pitch    += y * fabsf(y) * INDY_CAMERA_PITCH_SPEED * secDeltaTime;
            indyCamera_pitch     = fminf(fmaxf(indyCamera_pitch, INDY_CAMERA_PITCH_MIN), INDY_CAMERA_PITCH_MAX);
        }
        else if ( bMoving && indyModern_IsStickAwayFromCamera() )
        {
            // walking away from the camera: trail behind Indy (no effect on the input direction, which points ahead)
            float behind = remainderf(heading - indyCamera_worldYaw, 360.0f);
            indyCamera_worldYaw += behind * (1.0f - expf(-INDY_CAMERA_TRAIL_RATE * secDeltaTime));
            indyCamera_pitch    *= expf(-INDY_CAMERA_TRAIL_RATE * secDeltaTime);
        }
        indyCamera_worldYaw = remainderf(indyCamera_worldYaw, 360.0f);
        indyCamera_yaw      = remainderf(indyCamera_worldYaw - heading, 360.0f);
        indyCamera_secIdle  = 0.0f;
    }
    else if ( x != 0.0f || y != 0.0f )
    {
        indyCamera_bWorldYawValid = false;
        // squared response: fine control near the centre, full speed at the edge
        indyCamera_yaw   -= x * fabsf(x) * INDY_CAMERA_YAW_SPEED * secDeltaTime; // stick right: camera swings right
        indyCamera_pitch += y * fabsf(y) * INDY_CAMERA_PITCH_SPEED * secDeltaTime;
        indyCamera_pitch  = fminf(fmaxf(indyCamera_pitch, INDY_CAMERA_PITCH_MIN), INDY_CAMERA_PITCH_MAX);
        indyCamera_yaw    = remainderf(indyCamera_yaw, 360.0f);
        indyCamera_secIdle = 0.0f;
    }
    else
    {
        indyCamera_bWorldYawValid = false;
        indyCamera_secIdle += secDeltaTime;
        if ( indyCamera_secIdle >= INDY_CAMERA_RECENTER_WAIT || bMoving )
        {
            float rate  = INDY_CAMERA_RECENTER_RATE * (bMoving ? 2.0f : 1.0f);
            float keep  = expf(-rate * secDeltaTime);
            indyCamera_yaw   *= keep;
            indyCamera_pitch *= keep;
            if ( fabsf(indyCamera_yaw) < 0.05f && fabsf(indyCamera_pitch) < 0.05f )
            {
                indyCamera_yaw = indyCamera_pitch = 0.0f;
            }
        }
    }

    // INDY_INPUT_TRACE: log the orbit four times a second
    static DWORD msecLastTrace;
    if ( getenv("INDY_INPUT_TRACE") && GetTickCount() - msecLastTrace >= 250 )
    {
        msecLastTrace = GetTickCount();
        STDLOG_STATUS("indyCamera trace: t %lu stick %.2f %.2f orbit yaw %.1f pitch %.1f view %.1f world-stable %d\n",
            (unsigned long)msecLastTrace, x, y, indyCamera_yaw, indyCamera_pitch, remainderf(heading + indyCamera_yaw, 360.0f),
            indyModern_IsCameraWorldStable());
    }

    pPYR->x += indyCamera_pitch;
    pPYR->y += indyCamera_yaw;
    indyCamera_viewHeading = remainderf(heading + indyCamera_yaw, 360.0f);
}
