#include "indyInput.h"
#include "indyEnh.h"

#include <sith/Devices/sithControl.h>
#include <std/General/std.h>
#include <std/Win95/stdControl.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// INDY accessor in sithControl.c (read-only view of the bindings)
const SithControlBinding* J3DAPI sithControl_GetFunctionBindings(SithControlFunction functionId, size_t* pNumBindings);

#define INDY_STICK_KEY_THRESHOLD 0.25f // stdControl_ReadAxisAsKey: gamepad axes count as pressed above this
#define INDY_STICK_RUN_THRESHOLD 0.85f
#define INDY_TURN_SCALE_MIN      0.3f

static bool indyInput_bFakeParsed;
static bool indyInput_bFake;
static float indyInput_fakeForward;
static float indyInput_fakeTurn;

static bool indyInput_GetFake(SithControlFunction function, float* pDeflection)
{
    if ( !indyInput_bFakeParsed )
    {
        const char* pFake = getenv("INDY_FAKE_STICK");
        indyInput_bFake = pFake && sscanf(pFake, "%f,%f", &indyInput_fakeForward, &indyInput_fakeTurn) == 2;
        indyInput_bFakeParsed = true;
    }

    if ( !indyInput_bFake )
    {
        return false;
    }

    bool bTurn = function == SITHCONTROL_TURNLEFT || function == SITHCONTROL_TURNRIGHT;
    *pDeflection = sithControl_GetKey(function, NULL) ? (bTurn ? indyInput_fakeTurn : indyInput_fakeForward) : 0.0f;
    return true;
}

static bool indyInput_IsKeyBindingPressed(const SithControlBinding* aBindings, size_t numBindings)
{
    for ( size_t i = 0; i < numBindings; i++ )
    {
        const SithControlBinding* pBinding = &aBindings[i];
        if ( (pBinding->flags & SITHCONTROLBIND_KEYCONTROL) != 0 && stdControl_ReadKey(pBinding->controlId, NULL) )
        {
            return true;
        }
    }
    return false;
}

float J3DAPI indyInput_GetStickDeflection(SithControlFunction function)
{
    float deflection;
    if ( indyInput_GetFake(function, &deflection) )
    {
        return deflection;
    }

    size_t numBindings;
    const SithControlBinding* aBindings = sithControl_GetFunctionBindings(function, &numBindings);

    deflection = 0.0f;
    for ( size_t i = 0; i < numBindings; i++ )
    {
        const SithControlBinding* pBinding = &aBindings[i];
        // gamepad/joystick sticks only: mouse movement bound to forward/turn keeps the original behaviour
        if ( (pBinding->flags & SITHCONTROLBIND_AXISCONTROL) == 0 || STDCONTROL_ISMOUSEAXIS(pBinding->controlId) )
        {
            continue;
        }

        float pos = stdControl_ReadAxis(pBinding->controlId);
        uint32_t dir = pBinding->controlId & STDCONTROL_AID_NEGATIVE_AXIS;
        float value = dir == STDCONTROL_AID_POSITIVE_AXIS ? pos : dir == STDCONTROL_AID_NEGATIVE_AXIS ? -pos : fabsf(pos);
        if ( value > deflection )
        {
            deflection = value;
        }
    }
    return deflection > 1.0f ? 1.0f : deflection;
}

// Keyboard (or other key) input keeps the original behaviour; only a stick changes anything.
static bool indyInput_IsStick(SithControlFunction function, float* pDeflection)
{
    if ( !indyEnh_IsEnabled(INDY_ENH_ANALOG_MOVEMENT) )
    {
        return false;
    }

    float fake;
    if ( indyInput_GetFake(function, &fake) )
    {
        *pDeflection = fake;
        return fake > 0.0f;
    }

    size_t numBindings;
    const SithControlBinding* aBindings = sithControl_GetFunctionBindings(function, &numBindings);
    if ( !aBindings || indyInput_IsKeyBindingPressed(aBindings, numBindings) )
    {
        return false;
    }

    *pDeflection = indyInput_GetStickDeflection(function);
    return *pDeflection > 0.0f;
}

float J3DAPI indyInput_GetTurnScale(SithControlFunction function)
{
    float deflection = 0.0f;
    if ( !indyInput_IsStick(function, &deflection) )
    {
        return 1.0f;
    }

    float t = (deflection - INDY_STICK_KEY_THRESHOLD) / (1.0f - INDY_STICK_KEY_THRESHOLD);
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    return INDY_TURN_SCALE_MIN + (1.0f - INDY_TURN_SCALE_MIN) * t;
}

bool indyInput_IsStickRun(void)
{
    float deflection = 0.0f;
    bool bRun = indyInput_IsStick(SITHCONTROL_FORWARD, &deflection) && deflection >= INDY_STICK_RUN_THRESHOLD;

    // headless tests (INDY_FAKE_STICK): log each change of the decision, so the wiring can be checked in JonesLog.txt
    static int lastLogged = -1;
    if ( indyInput_bFake && deflection > 0.0f && lastLogged != (int)bRun )
    {
        STDLOG_STATUS("indyInput: forward stick %.2f -> %s\n", deflection, bRun ? "run" : "walk");
        lastLogged = (int)bRun;
    }
    return bRun;
}
