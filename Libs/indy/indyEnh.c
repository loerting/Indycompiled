#include "indyEnh.h"

#include <std/General/stdConfig.h>

#include <stdio.h>
#include <string.h>

typedef enum eIndyEnhKind
{
    INDY_KIND_FIX, // bug fix: on in the "fixed" and "enhanced" profiles
    INDY_KIND_ENH, // enhancement: on in the "enhanced" profile only
    INDY_KIND_EXP, // experimental: off in every profile until tested; enable explicitly in "toggles"
} IndyEnhKind;

typedef struct sIndyEnhInfo
{
    const char* pId;  // registry ID, e.g. "ENH-0001"
    const char* pKey; // key below "indycompiled.toggles" in Jones.cfg
    IndyEnhKind kind;
} IndyEnhInfo;

static const IndyEnhInfo indyEnh_aInfos[INDY_ENH_COUNT] = {
    [INDY_ENH_ANALOG_MOVEMENT] = { "ENH-0001", "analogMovement", INDY_KIND_EXP }, // until tested with a gamepad
    [INDY_FIX_TURN_RATE]       = { "FIX-0001", "fpsIndependentTurning", INDY_KIND_FIX },
    [INDY_FIX_CONTINUOUS_DAMAGE] = { "FIX-0002", "fpsIndependentDamage", INDY_KIND_FIX },
    [INDY_FIX_TIME_CYCLES]       = { "FIX-0003", "fpsIndependentCycles", INDY_KIND_FIX },
    [INDY_FIX_JEWEL_FLY_THRUST]  = { "FIX-0004", "fpsIndependentJewelFly", INDY_KIND_FIX },
};

static bool indyEnh_bLoaded;
static IndyProfile indyEnh_profile = INDY_PROFILE_ENHANCED;
static bool indyEnh_aEnabled[INDY_ENH_COUNT];
static float indyEnh_turnRateFps = 60.0f;

static const char* const indyEnh_aProfileNames[] = { "vanilla", "fixed", "enhanced" };

static IndyProfile indyEnh_ParseProfile(const char* pName)
{
    for ( size_t i = 0; i < sizeof(indyEnh_aProfileNames) / sizeof(indyEnh_aProfileNames[0]); i++ )
    {
        if ( strcmp(pName, indyEnh_aProfileNames[i]) == 0 )
        {
            return (IndyProfile)i;
        }
    }
    return INDY_PROFILE_ENHANCED;
}

static bool indyEnh_ProfileDefault(IndyEnhKind kind, IndyProfile profile)
{
    switch ( profile )
    {
        case INDY_PROFILE_VANILLA:
            return false;
        case INDY_PROFILE_FIXED:
            return kind == INDY_KIND_FIX;
        default:
            return kind != INDY_KIND_EXP;
    }
}

static void indyEnh_Load(void)
{
    char aProfile[32];
    stdConfig_GetString("indycompiled.profile", aProfile, sizeof(aProfile), "enhanced");
    indyEnh_profile = indyEnh_ParseProfile(aProfile);

    // Toggles only hold explicit overrides; everything else follows the profile (so switching the profile works).
    for ( size_t i = 0; i < INDY_ENH_COUNT; i++ )
    {
        char aKey[96];
        snprintf(aKey, sizeof(aKey), "indycompiled.toggles.%s", indyEnh_aInfos[i].pKey);
        bool bDefault = indyEnh_ProfileDefault(indyEnh_aInfos[i].kind, indyEnh_profile);
        indyEnh_aEnabled[i] = stdConfig_Contains(aKey) ? stdConfig_GetBool(aKey, bDefault) : bDefault;
    }

    // 60: how most players have experienced the game (vsync on 60 Hz screens)
    float fps = stdConfig_GetFloat("indycompiled.turnRateFps", 60.0f);
    indyEnh_turnRateFps = fps < 15.0f ? 15.0f : fps > 240.0f ? 240.0f : fps;

    indyEnh_bLoaded = true;
}

bool J3DAPI indyEnh_IsEnabled(IndyEnh id)
{
    if ( (unsigned)id >= INDY_ENH_COUNT )
    {
        return false;
    }

    if ( !indyEnh_bLoaded )
    {
        if ( !stdConfig_HasStarted() )
        {
            // Too early to know: behave like the original game.
            return false;
        }
        indyEnh_Load();
    }

    return indyEnh_aEnabled[id];
}

float indyEnh_GetTurnRateFps(void)
{
    return indyEnh_turnRateFps;
}

IndyProfile indyEnh_GetProfile(void)
{
    return indyEnh_profile;
}

void indyEnh_Reload(void)
{
    indyEnh_bLoaded = false;
}
