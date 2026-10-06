#include "indyDamage.h"
#include "indyEnh.h"

#include <sith/Gameplay/sithPlayer.h>
#include <sith/Gameplay/sithTime.h>
#include <sith/World/sithThing.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Carried fractions, per thing (index + signature, so a reused slot starts at 0) and damage type. Only a few things
// take continuous damage at the same time; the least recently used entry is replaced.
typedef struct sIndyDamageCarry
{
    int thingIdx;
    uint32_t signature;
    SithDamageType type;
    float carry;
    unsigned int lastUse;
} IndyDamageCarry;

static IndyDamageCarry indyDamage_aCarry[32];
static unsigned int indyDamage_useCounter;

static float* indyDamage_GetCarry(const SithThing* pThing, SithDamageType type)
{
    IndyDamageCarry* pOldest = &indyDamage_aCarry[0];
    for ( size_t i = 0; i < sizeof(indyDamage_aCarry) / sizeof(indyDamage_aCarry[0]); i++ )
    {
        IndyDamageCarry* pEntry = &indyDamage_aCarry[i];
        if ( pEntry->lastUse && pEntry->thingIdx == pThing->idx && pEntry->signature == pThing->signature && pEntry->type == type )
        {
            pEntry->lastUse = ++indyDamage_useCounter;
            return &pEntry->carry;
        }
        if ( pEntry->lastUse < pOldest->lastUse )
        {
            pOldest = pEntry;
        }
    }

    *pOldest = (IndyDamageCarry){ pThing->idx, pThing->signature, type, 0.0f, ++indyDamage_useCounter };
    return &pOldest->carry;
}

void J3DAPI indyDamage_ApplyContinuous(SithThing* pThing, float damage, SithDamageType type)
{
    if ( !indyEnh_IsEnabled(INDY_FIX_CONTINUOUS_DAMAGE) )
    {
        sithThing_DamageThing(pThing, pThing, damage, type);
        return;
    }

    float* pCarry = indyDamage_GetCarry(pThing, type);
    float total   = *pCarry + damage;
    float whole   = floorf(total);
    *pCarry       = total - whole;
    if ( whole >= 1.0f )
    {
        sithThing_DamageThing(pThing, pThing, whole, type);
    }
}

void J3DAPI indyDamage_TestTick(SithThing* pThing, unsigned int msecDeltaTime)
{
    // INDY_TEST_DPS="<damage per second>[@<start>+<duration>]" (game seconds since the level started)
    static float dps = -1.0f, secStart, secDuration = 1e9f;
    if ( dps < 0.0f )
    {
        const char* pDps = getenv("INDY_TEST_DPS");
        dps = 0.0f;
        if ( pDps && sscanf(pDps, "%f@%f+%f", &dps, &secStart, &secDuration) < 1 )
        {
            dps = 0.0f;
        }
    }

    float secNow = (float)sithTime_g_msecGameTime / 1000.0f;
    if ( dps > 0.0f && pThing == sithPlayer_g_pLocalPlayerThing && secNow >= secStart && secNow < secStart + secDuration )
    {
        indyDamage_ApplyContinuous(pThing, dps * (float)msecDeltaTime / 1000.0f, SITH_DAMAGE_DROWN);
    }
}
