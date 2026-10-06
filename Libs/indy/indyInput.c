#include "indyInput.h"
#include "indyEnh.h"

#include <sith/Devices/sithControl.h>
#include <sith/Gameplay/sithPlayer.h>
#include <sith/Gameplay/sithTime.h>
#include "indyFrame.h"
#include <std/General/std.h>
#include <std/General/stdUtil.h>
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

// Diagnostics (INDY_INPUT_TRACE=1): once a second, log the movement bindings and what each reads right now.
// Shows whether a controller's stick is bound at all and whether its values arrive.
// INDY_INPUT_TRACE=<interval in ms> (any other value: 1000)
static DWORD indyInput_TraceInterval(void)
{
    static long interval = -1;
    if ( interval == -1 )
    {
        const char* pTrace = getenv("INDY_INPUT_TRACE");
        interval = !pTrace ? 0 : atol(pTrace) > 0 ? atol(pTrace) : 1000;
    }
    return (DWORD)interval;
}

static bool indyInput_IsTracing(void)
{
    return indyInput_TraceInterval() != 0;
}

static void indyInput_Trace(void)
{
    static DWORD msecLast;
    DWORD msecNow = GetTickCount();
    if ( !indyInput_IsTracing() || msecNow - msecLast < indyInput_TraceInterval() )
    {
        return;
    }
    msecLast = msecNow;

    const SithThing* pPlayer = sithPlayer_g_pLocalPlayerThing;
    if ( pPlayer )
    {
        // heading: angle of the look vector in the horizontal plane (degrees, counter-clockwise)
        float heading = atan2f(pPlayer->orient.lvec.y, pPlayer->orient.lvec.x) * (180.0f / 3.14159265f);
        STDLOG_STATUS("indyInput trace: t %lu player pos %.3f %.3f %.3f heading %.2f moveStatus %d fps %.1f health %.1f cyc16 %u\n",
            (unsigned long)msecNow, pPlayer->pos.x, pPlayer->pos.y, pPlayer->pos.z, heading, (int)pPlayer->moveStatus,
            sithTime_g_fps, pPlayer->thingInfo.actorInfo.health, indyFrame_GetTestCount());
    }

    static const struct { SithControlFunction fn; const char* pName; } aFunctions[] = {
        { SITHCONTROL_FORWARD, "forward" }, { SITHCONTROL_BACK, "back" },
        { SITHCONTROL_TURNLEFT, "left" }, { SITHCONTROL_TURNRIGHT, "right" },
        { SITHCONTROL_RUNFWD, "runfwd" }, { SITHCONTROL_ACT1, "act1" },
    };
    for ( size_t f = 0; f < STD_ARRAYLEN(aFunctions); f++ )
    {
        char aLine[256] = { 0 };
        size_t len = 0;
        size_t numBindings = 0;
        const SithControlBinding* aBindings = sithControl_GetFunctionBindings(aFunctions[f].fn, &numBindings);
        for ( size_t i = 0; aBindings && i < numBindings && len < sizeof(aLine) - 48; i++ )
        {
            const SithControlBinding* pBinding = &aBindings[i];
            if ( (pBinding->flags & SITHCONTROLBIND_AXISCONTROL) != 0 )
            {
                len += snprintf(aLine + len, sizeof(aLine) - len, " axis %08X=%.2f(raw %d%s)", (unsigned)pBinding->controlId,
                    stdControl_ReadAxis(pBinding->controlId), stdControl_ReadAxisRaw(STDCONTROL_GETAID(pBinding->controlId)),
                    stdControl_TestAxisFlag(STDCONTROL_GETAID(pBinding->controlId), STDCONTROL_AXIS_ENABLED) ? "" : " disabled");
            }
            else
            {
                len += snprintf(aLine + len, sizeof(aLine) - len, " key %X=%d", (unsigned)pBinding->controlId, stdControl_ReadKey(pBinding->controlId, NULL));
            }
        }
        STDLOG_STATUS("indyInput trace: %s (%u bindings):%s\n", aFunctions[f].pName, (unsigned)numBindings, aLine);
    }
}

bool indyInput_IsStickRun(void)
{
    indyInput_Trace();

    float deflection = 0.0f;
    bool bRun = indyInput_IsStick(SITHCONTROL_FORWARD, &deflection) && deflection >= INDY_STICK_RUN_THRESHOLD;

    // tests (INDY_FAKE_STICK, INDY_INPUT_TRACE): log each change of the decision, so it can be checked in JonesLog.txt
    static int lastLogged = -1;
    if ( (indyInput_bFake || indyInput_IsTracing()) && deflection > 0.0f && lastLogged != (int)bRun )
    {
        STDLOG_STATUS("indyInput: forward stick %.2f -> %s\n", deflection, bRun ? "run" : "walk");
        lastLogged = (int)bRun;
    }
    return bRun;
}

float indyInput_GetTurnFps(void)
{
    return indyEnh_IsEnabled(INDY_FIX_TURN_RATE) ? indyEnh_GetTurnRateFps() : sithTime_g_fps;
}

#define INDY_STOP_GRACE 0.15f // seconds without movement input before Indy starts stopping (ENH-0003)

bool indyInput_ShouldStopMoving(bool bMoving, float secDeltaTime)
{
    static float secNoInput;
    if ( bMoving )
    {
        secNoInput = 0.0f;
        return false;
    }

    if ( !indyEnh_IsEnabled(INDY_ENH_QUICK_DIRECTION) )
    {
        return true;
    }

    secNoInput += secDeltaTime;
    if ( secNoInput >= INDY_STOP_GRACE )
    {
        secNoInput = 0.0f;
        return true;
    }
    return false;
}
