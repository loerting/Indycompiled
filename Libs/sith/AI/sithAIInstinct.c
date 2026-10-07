#include "sithAIInstinct.h"
#include <j3dcore/j3dhook.h>

#include <rdroid/Math/rdMath.h>
#include <rdroid/Math/rdVector.h>

#include <sith/AI/sithAI.h>
#include <sith/AI/sithAIAwareness.h>
#include <sith/AI/sithAIMove.h>
#include <sith/AI/sithAIUtil.h>
#include <sith/Devices/sithSound.h>
#include <sith/Devices/sithSoundMixer.h>
#include <sith/Engine/sithCollision.h>
#include <sith/Engine/sithPhysics.h>
#include <sith/Engine/sithPuppet.h>
#include <sith/Gameplay/sithPlayer.h>
#include <sith/Gameplay/sithPlayerActions.h>
#include <sith/Gameplay/sithPlayerControls.h>
#include <sith/Gameplay/sithTime.h>
#include <sith/RTI/symbols.h>
#include <sith/World/sithSector.h>
#include <sith/World/sithSoundClass.h>
#include <sith/World/sithThing.h>
#include <sith/World/sithWeapon.h>
#include <sith/World/sithWorld.h>

#include <std/General/stdMath.h>

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define sithAIInstinct_flt_539AB8 J3D_DECL_FAR_VAR(sithAIInstinct_flt_539AB8, float)

// Instincts are called with event 0 on their periodic update
#define SITHAIINSTINCT_EVENT_UPDATE ((SithAIEventType)0)

// rand() scaled to [0, 1] the way the instincts compute it: in single precision, with 1/RAND_MAX rounded to float
#define SITHAIINSTINCT_RANDF() ((float)rand() * (1.0f / RAND_MAX))

#define SITHAIINSTINCT_VEHICLEFLAGS (SITH_PF_MINECAR | SITH_PF_RAFT | SITH_PF_JEEP | SITH_PF_UNKNOWN_8000000)

// The snake (Quetzalcoatl boss) tuning values used by SnakeFollow and its helpers are .data variables in the
// original exe that no code writes, so they are constants here

static int sithAIInstinct_TargetLost(SithAIControlBlock* pLocal, const SithThing* pTarget);
static int sithAIInstinct_StopFleeing(SithAIControlBlock* pLocal, SithAIInstinctState* pState);
static int sithAIInstinct_FollowGoal(SithAIControlBlock* pLocal, const SithAIInstinct* pInstinct, SithThing* pGoal);
static int sithAIInstinct_MakeCombatMove(SithAIControlBlock* pLocal, const SithAIInstinct* pInstinct, SithAIInstinctState* pState, bool bCheckFire);
static float sithAIInstinct_GetSpeed(const SithThing* pThing);

void sithAIInstinct_InstallHooks(void)
{
    J3D_HOOKFUNC(sithAIInstinct_InitInstincts);
    J3D_HOOKFUNC(sithAIInstinct_Listen);
    J3D_HOOKFUNC(sithAIInstinct_LookForTarget);
    J3D_HOOKFUNC(sithAIInstinct_SenseDanger);
    J3D_HOOKFUNC(sithAIInstinct_FearGunshot);
    J3D_HOOKFUNC(sithAIInstinct_Hop);
    J3D_HOOKFUNC(sithAIInstinct_HoverDrift);
    J3D_HOOKFUNC(sithAIInstinct_Talk);
    J3D_HOOKFUNC(sithAIInstinct_RandomTurn);
    J3D_HOOKFUNC(sithAIInstinct_OpenDoors);
    J3D_HOOKFUNC(sithAIInstinct_Jump);
    J3D_HOOKFUNC(sithAIInstinct_LobFire);
    J3D_HOOKFUNC(sithAIInstinct_PrimaryFire);
    J3D_HOOKFUNC(sithAIInstinct_AlternateFire);
    J3D_HOOKFUNC(sithAIInstinct_CircleStrafe);
    J3D_HOOKFUNC(sithAIInstinct_Dodge);
    J3D_HOOKFUNC(sithAIInstinct_HitAndRun);
    J3D_HOOKFUNC(sithAIInstinct_Retreat);
    J3D_HOOKFUNC(sithAIInstinct_RandomMove);
    J3D_HOOKFUNC(sithAIInstinct_HumanCombatMove);
    J3D_HOOKFUNC(sithAIInstinct_Roam);
    J3D_HOOKFUNC(sithAIInstinct_ReturnHome);
    J3D_HOOKFUNC(sithAIInstinct_Flee);
    J3D_HOOKFUNC(sithAIInstinct_BasicFallow);
    J3D_HOOKFUNC(sithAIInstinct_WallCrawl);
    J3D_HOOKFUNC(sithAIInstinct_sub_494360);
    J3D_HOOKFUNC(sithAIInstinct_SnakeMungeTestCheck);
    J3D_HOOKFUNC(sithAIInstinct_SnakeFollow);
    J3D_HOOKFUNC(sithAIInstinct_sub_494FF0);
}

void sithAIInstinct_ResetGlobals(void)
{
    float sithAIInstinct_flt_539AB8_tmp = 1.0f;
    memcpy(&sithAIInstinct_flt_539AB8, &sithAIInstinct_flt_539AB8_tmp, sizeof(sithAIInstinct_flt_539AB8));

}

void J3DAPI sithAIInstinct_InitInstincts(void)
{
    INDY_AB_ORIGINAL_VOID(sithAIInstinct_InitInstincts);

    // name, instinct, update modes, modes blocking the update, trigger events
    sithAI_RegisterInstinct("listen", (SithAIInstinctFunc)sithAIInstinct_Listen, 0, 0, (SithAIEventType)0x7);
    sithAI_RegisterInstinct("lookfortarget", (SithAIInstinctFunc)sithAIInstinct_LookForTarget, (SithAIMode)0x204, (SithAIMode)0x800, (SithAIEventType)0x100);
    sithAI_RegisterInstinct("sensedanger", (SithAIInstinctFunc)sithAIInstinct_SenseDanger, (SithAIMode)0x804, 0, (SithAIEventType)0x7);
    sithAI_RegisterInstinct("primaryfire", (SithAIInstinctFunc)sithAIInstinct_PrimaryFire, (SithAIMode)0x2, (SithAIMode)0x800, 0);
    sithAI_RegisterInstinct("alternatefire", (SithAIInstinctFunc)sithAIInstinct_AlternateFire, (SithAIMode)0x2, (SithAIMode)0x800, 0);
    sithAI_RegisterInstinct("lobfire", (SithAIInstinctFunc)sithAIInstinct_LobFire, (SithAIMode)0x2, (SithAIMode)0x800, 0);
    sithAI_RegisterInstinct("opendoors", (SithAIInstinctFunc)sithAIInstinct_OpenDoors, (SithAIMode)0x2, 0, 0);
    sithAI_RegisterInstinct("jump", (SithAIInstinctFunc)sithAIInstinct_Jump, 0, 0, (SithAIEventType)0x604);
    sithAI_RegisterInstinct("randomturn", (SithAIInstinctFunc)sithAIInstinct_RandomTurn, (SithAIMode)0x4, 0, 0);
    sithAI_RegisterInstinct("roam", (SithAIInstinctFunc)sithAIInstinct_Roam, (SithAIMode)0x4, (SithAIMode)0xA12, (SithAIEventType)0xF04);
    sithAI_RegisterInstinct("flee", (SithAIInstinctFunc)sithAIInstinct_Flee, (SithAIMode)0x800, 0, (SithAIEventType)0xF04);
    sithAI_RegisterInstinct("retreat", (SithAIInstinctFunc)sithAIInstinct_Retreat, (SithAIMode)0x2, (SithAIMode)0x1000800, (SithAIEventType)0x1);
    sithAI_RegisterInstinct("circlestrafe", (SithAIInstinctFunc)sithAIInstinct_CircleStrafe, (SithAIMode)0x2, (SithAIMode)0x1000800, (SithAIEventType)0x88E04);
    sithAI_RegisterInstinct("returnhome", (SithAIInstinctFunc)sithAIInstinct_ReturnHome, 0, 0, (SithAIEventType)0x900);
    sithAI_RegisterInstinct("talk", (SithAIInstinctFunc)sithAIInstinct_Talk, (SithAIMode)0xFFFFFFFF, 0, (SithAIEventType)0x400104);
    sithAI_RegisterInstinct("hitandrun", (SithAIInstinctFunc)sithAIInstinct_HitAndRun, (SithAIMode)0xC00, (SithAIMode)0x4, (SithAIEventType)0x4000);
    sithAI_RegisterInstinct("dodge", (SithAIInstinctFunc)sithAIInstinct_Dodge, 0, (SithAIMode)0x100000, (SithAIEventType)0x81803);
    sithAI_RegisterInstinct("hop", (SithAIInstinctFunc)sithAIInstinct_Hop, (SithAIMode)0x1, 0, 0);
    sithAI_RegisterInstinct("hoverdrift", (SithAIInstinctFunc)sithAIInstinct_HoverDrift, (SithAIMode)0x6FE, (SithAIMode)0xF001, 0);
    sithAI_RegisterInstinct("basicfollow", (SithAIInstinctFunc)sithAIInstinct_BasicFallow, (SithAIMode)0x202, (SithAIMode)0x1004814, (SithAIEventType)0x8CE04);
    sithAI_RegisterInstinct("wallcrawl", (SithAIInstinctFunc)sithAIInstinct_WallCrawl, (SithAIMode)0x202, (SithAIMode)0x3004014, (SithAIEventType)0x10A604);
    sithAI_RegisterInstinct("snakefollow", (SithAIInstinctFunc)sithAIInstinct_SnakeFollow, (SithAIMode)0x6, 0, (SithAIEventType)0x8E04);
    sithAI_RegisterInstinct("randommove", (SithAIInstinctFunc)sithAIInstinct_RandomMove, (SithAIMode)0x2, (SithAIMode)0x104805, (SithAIEventType)0x4800);
    sithAI_RegisterInstinct("humancombatmove", (SithAIInstinctFunc)sithAIInstinct_HumanCombatMove, (SithAIMode)0x2, (SithAIMode)0x1104805, (SithAIEventType)0x80901);
    sithAI_RegisterInstinct("feargunshot", (SithAIInstinctFunc)sithAIInstinct_FearGunshot, (SithAIMode)0x6, (SithAIMode)0x800, (SithAIEventType)0x2);
}

int J3DAPI sithAIInstinct_Listen(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing)
{
    INDY_AB_ORIGINAL(sithAIInstinct_Listen, pLocal, pInstinct, pState, event, pThing);

    // aParams[0]: loudest noise heard (0 until the first alert)
    SITH_ASSERTREL(pLocal && pLocal->pClass && pLocal->pOwner);
    if ( (pLocal->mode & SITHAI_MODE_SEARCHING) == 0 )
    {
        return 0;
    }

    if ( pThing && (pThing->flags & SITH_TF_DISABLED) != 0 )
    {
        return 0;
    }

    SithThing* pOwner = pLocal->pOwner;
    SithAIAwarenessSector* pAISector = &sithAIAwareness_g_aSectors[sithSector_GetSectorIndex(pOwner->pInSector)];
    if ( !pAISector )
    {
        SITHLOG_ERROR("Listen: Bad sector data passed!\n");
        return 0;
    }

    if ( event == SITHAI_EVENT_SOUND )
    {
        if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_DEAF) != 0 || pAISector->aLevelAtTransmittingPos[0] == 0.0f )
        {
            return 0;
        }

        if ( pInstinct->fltArg[0] <= SITHAIINSTINCT_RANDF() )
        {
            return 0;
        }

        SithThing* pSource = pAISector->aTransmittingThing[0];
        if ( pSource && (pSource->thingInfo.actorInfo.flags & SITH_AF_INVISIBLE) != 0 )
        {
            return 0;
        }

        if ( !sithAIAwareness_IsInTransmittingRange(pAISector, 0, pOwner) )
        {
            return 0;
        }

        rdVector3 toPos;
        float distance;
        int soundSightState = sithAIUtil_GetDistanceToTargetPos(pOwner, &pOwner->pos, &pAISector->aStartPos[0], -1.0f, pLocal->pClass->sightDistance, 0.0f, &toPos, &distance);
        if ( distance > pLocal->pClass->heardDistance )
        {
            return 0;
        }

        bool bActiveSource   = pSource && pSource->type != SITH_THING_FREE;
        int sourceSightState = 3;
        if ( bActiveSource )
        {
            sourceSightState = sithAIUtil_GetDistanceToTargetPos(pOwner, &pOwner->pos, &pSource->pos, -1.0f, pLocal->pClass->sightDistance, 0.0f, &toPos, &distance);
        }

        bool bStop = false;
        bool bMove = false;
        bool bLook = false;
        bool bHunt = false;
        float noiseLevel;
        if ( bActiveSource && (pLocal->mode & (SITHAI_MODE_HUNTING | SITHAI_MODE_INSTINCTUSEWPNTS)) != 0 )
        {
            bHunt = true;

            // Note: The original never sets the noise level on this path; it reads the pInstinct argument slot instead,
            //       i.e. the bits of the pointer as a float (a tiny positive number), and may store it in aParams[0] below.
            uint32_t instinctBits = (uint32_t)(uintptr_t)pInstinct;
            memcpy(&noiseLevel, &instinctBits, sizeof(noiseLevel));
        }
        else if ( sourceSightState == 0 )
        {
            bMove = bLook = true;
            rdVector_Copy3(&pLocal->alertPos, &pSource->pos);
            noiseLevel = pAISector->aLevelAtTransmittingPos[0];
        }
        else if ( soundSightState == 0 )
        {
            bMove = bLook = true;
            rdVector_Copy3(&pLocal->alertPos, &pAISector->aStartPos[0]);
            noiseLevel = pAISector->aLevelAtTransmittingPos[0];
        }
        else if ( pSource && (pLocal->mode & SITHAI_MODE_HUNTING) == 0 )
        {
            bStop = bLook = true;
            noiseLevel = pAISector->aLevelAtTransmittingPos[0];
        }
        else
        {
            return 0;
        }

        // Sound doesn't cross the water surface
        if ( bActiveSource )
        {
            bool bSourceUnderwater = (pSource->pInSector->flags & SITH_SECTOR_UNDERWATER) != 0;
            bool bOwnerUnderwater  = (pLocal->pOwner->pInSector->flags & SITH_SECTOR_UNDERWATER) != 0;
            if ( bSourceUnderwater != bOwnerUnderwater )
            {
                return 0;
            }
        }

        if ( pState->aParams[0] < noiseLevel )
        {
            pState->aParams[0] = noiseLevel;
        }

        sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_CURIOUS);
        if ( bStop )
        {
            sithAIMove_StopAIMovement(pLocal);
        }

        if ( bMove && SITHAIINSTINCT_RANDF() <= pInstinct->fltArg[1] )
        {
            sithAIMove_AISetMovePos(pLocal, &pLocal->alertPos, 0.75f);
        }

        if ( bLook )
        {
            sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->alertPos);
        }

        if ( bHunt && SITHAIINSTINCT_RANDF() <= pInstinct->fltArg[1] )
        {
            sithAIUtil_AISetMode(pLocal, SITHAI_MODE_HUNTING);
            pLocal->pTargetThing = sithPlayer_g_pLocalPlayerThing;
            sithAIUtil_AIMoveToNextWpnt(pLocal, 1.7f, 0.0f, 0);
        }

        return 0;
    }

    if ( event != SITHAI_EVENT_HIT_SECTOR && event != SITHAI_EVENT_TOUCHED )
    {
        return 0;
    }

    if ( pState->aParams[0] == 0.0f )
    {
        sithAIUtil_AIPlaySoundMode(pLocal, event == SITHAI_EVENT_HIT_SECTOR ? SITHSOUNDCLASS_SURPRISE : SITHSOUNDCLASS_CURIOUS);
        pState->aParams[0] = 0.1f;
    }

    if ( !pThing )
    {
        return 0;
    }

    SithThing* pSource = sithThing_GetThingParent(pThing);
    if ( event == SITHAI_EVENT_HIT_SECTOR )
    {
        if ( pInstinct->fltArg[1] < SITHAIINSTINCT_RANDF() )
        {
            sithAIMove_AISetLookThing(pLocal, pSource);
            return 0;
        }

        if ( (pLocal->mode & (SITHAI_MODE_HUNTING | SITHAI_MODE_INSTINCTUSEWPNTS)) != 0 )
        {
            sithAIUtil_AISetMode(pLocal, SITHAI_MODE_HUNTING);
            pLocal->pTargetThing = sithPlayer_g_pLocalPlayerThing;
            return sithAIUtil_AIMoveToNextWpnt(pLocal, 1.7f, 0.0f, 0) ? 1 : 0;
        }

        sithAIMove_AISetLookThing(pLocal, pSource);
        sithAIMove_AISetMovePos(pLocal, &pSource->pos, 1.7f);
        return 1;
    }

    // Touched
    if ( pSource->type == SITH_THING_PLAYER )
    {
        sithAIMove_StopAIMovement(pLocal);
        sithAIMove_AISetLookPosEyeLevel(pLocal, &pSource->pos);
        return 0;
    }

    if ( pInstinct->fltArg[1] < SITHAIINSTINCT_RANDF() )
    {
        sithAIMove_AISetLookThing(pLocal, pSource);
        return 0;
    }

    if ( (pLocal->mode & (SITHAI_MODE_HUNTING | SITHAI_MODE_INSTINCTUSEWPNTS)) != 0 )
    {
        sithAIUtil_AIMoveToNextWpnt(pLocal, pLocal->moveSpeed, 0.0f, 0);
        return 0;
    }

    // Step back when moving into the toucher
    rdVector3 awayDir;
    rdVector_Sub3(&awayDir, &pLocal->pOwner->pos, &pSource->pos);
    rdVector_Normalize3Acc(&awayDir);
    if ( (pLocal->moveDirection.z * awayDir.z + pLocal->moveDirection.y * awayDir.y) + pLocal->moveDirection.x * awayDir.x < 0.0f )
    {
        rdVector3 movePos;
        rdVector_ScaleAdd3(&movePos, &awayDir, pInstinct->fltArg[2], &pLocal->pOwner->pos);
        sithAIMove_StopAIMovement(pLocal);
        sithAIMove_AISetLookPosEyeLevel(pLocal, &movePos);
        sithAIMove_AISetMoveTargetPos(pLocal, &movePos, 0.75f);
    }

    return 0;
}

int J3DAPI sithAIInstinct_LookForTarget(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithAIMode prevMode)
{
    INDY_AB_ORIGINAL(sithAIInstinct_LookForTarget, pLocal, pInstinct, pState, event, prevMode);

    // aParams[0]: game time (sec) when the hunt started
    if ( event == SITHAI_EVENT_MODECHANGED && prevMode
        && (pLocal->mode & SITHAI_MODE_HUNTING) != 0 && (prevMode & SITHAI_MODE_HUNTING) == 0 )
    {
        pState->aParams[0] = sithTime_g_secGameTime;
    }

    if ( event != SITHAIINSTINCT_EVENT_UPDATE )
    {
        return 0;
    }

    if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_AINOTARGET) != 0 || sithPlayerControls_g_bCutsceneMode == 1 )
    {
        return 0;
    }

    if ( (pLocal->mode & SITHAI_MODE_HUNTING) != 0 )
    {
        bool bEndHunt = true;
        if ( !pLocal->pTargetThing )
        {
            SITHLOG_DEBUG("LookForTarget: AI '%s' lost track of its target.  Ending HUNT mode.\n", pLocal->pOwner->aName);
        }
        else
        {
            float secHuntTime = pInstinct->fltArg[3];
            if ( secHuntTime == 0.0f )
            {
                secHuntTime = 20.0f;
            }

            if ( sithTime_g_secGameTime <= pState->aParams[0] + secHuntTime
                && ((pLocal->pOwner->thingInfo.actorInfo.flags & SITH_AF_HUMAN) != 0
                    || (pLocal->pTargetThing->moveInfo.physics.flags & SITHAIINSTINCT_VEHICLEFLAGS) == 0) )
            {
                bEndHunt = false;
            }
        }

        if ( bEndHunt )
        {
            sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_HUNTING);
        }
    }

    if ( (pLocal->mode & SITHAI_MODE_ACTIVE) != 0 )
    {
        // Give up the attack after intArg[1] msec
        if ( pInstinct->intArg[1] && pLocal->msecAttackStart + pInstinct->intArg[1] < sithTime_g_msecGameTime )
        {
            sithAIUtil_AISetMode(pLocal, SITHAI_MODE_SEARCHING);
            if ( (pLocal->mode & SITHAI_MODE_INSTINCTUSEWPNTS) == 0 )
            {
                return 1;
            }

            if ( (pLocal->pOwner->thingInfo.actorInfo.flags & SITH_AF_HUMAN) != 0
                || (pLocal->pTargetThing->moveInfo.physics.flags & SITHAIINSTINCT_VEHICLEFLAGS) == 0 )
            {
                sithAIUtil_AISetMode(pLocal, SITHAI_MODE_HUNTING);
                sithAIUtil_AIMoveToNextWpnt(pLocal, 1.7f, 0.0f, 0);
                return 1;
            }
        }

        return 0;
    }

    if ( (pLocal->mode & SITHAI_MODE_SEARCHING) == 0 )
    {
        return 0;
    }

    pState->msecNextUpdate = sithTime_g_msecGameTime + pInstinct->intArg[0];
    if ( (pLocal->pOwner->thingInfo.actorInfo.flags & SITH_AF_BLIND) != 0 )
    {
        return 0;
    }

    pLocal->pTargetThing = sithPlayer_g_pLocalPlayerThing;
    SITH_ASSERTREL(pLocal->pTargetThing);
    if ( !sithAIUtil_CanAttackTarget(pLocal->pTargetThing) )
    {
        pLocal->pTargetThing = NULL;
        return 0;
    }

    // Non-human AIs ignore a target in a vehicle unless they're in one themselves
    if ( (pLocal->pOwner->thingInfo.actorInfo.flags & SITH_AF_HUMAN) == 0
        && (pLocal->pOwner->moveInfo.physics.flags & SITHAIINSTINCT_VEHICLEFLAGS) == 0
        && (pLocal->pTargetThing->moveInfo.physics.flags & SITHAIINSTINCT_VEHICLEFLAGS) != 0 )
    {
        pLocal->pTargetThing = NULL;
        return 0;
    }

    sithAIUtil_sub_49B2E0(pLocal, 0);
    if ( pLocal->targetSightState != 0 )
    {
        // Note: Writes the instinct argument shared by all AIs of the class
        if ( pInstinct->fltArg[0] == 0.0f )
        {
            pInstinct->fltArg[0] = 500.0f;
        }

        return 0;
    }

    SithThing* pTarget = pLocal->pTargetThing;
    if ( pLocal->goalThing == pTarget )
    {
        pLocal->unknown141         = (int)sithMain_g_frameNumber;
        pLocal->targetSightState2  = 0;
        rdVector_Copy3(&pLocal->vecUnknown0, &pLocal->toTarget);
        pLocal->targetDistance     = pLocal->distance;
        rdVector_Copy3(&pLocal->vecUnknown5, &pTarget->pos);
        rdVector_Copy3(&pLocal->vecUnknown6, &pTarget->pos);
        pLocal->unknown150         = (int)sithTime_g_msecGameTime;
    }

    if ( pInstinct->fltArg[2] != 0.0f )
    {
        sithAIMove_AISetLookThing(pLocal, pTarget);
        return 0;
    }

    sithAIUtil_AISetMode(pLocal, SITHAI_MODE_ATTACKING);
    if ( (pLocal->submode & SITHAI_SUBMODE_CONTINUOUSWPNTMOTION) == 0 )
    {
        sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TRAVERSEWPNTS);
    }

    sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_ALERT);
    sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_ACTIVATE);
    sithAIAwareness_CreateTransmittingEvent(pLocal->pTargetThing->pInSector, &pLocal->pOwner->pos, 0, 1.5f, pLocal->pTargetThing);
    pLocal->goalThing = pLocal->pTargetThing;
    return 1;
}

int J3DAPI sithAIInstinct_SenseDanger(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing)
{
    INDY_AB_ORIGINAL(sithAIInstinct_SenseDanger, pLocal, pInstinct, pState, event, pThing);

    SithThing* pOwner = pLocal->pOwner;
    SithAIAwarenessSector* pAISector = &sithAIAwareness_g_aSectors[sithSector_GetSectorIndex(pOwner->pInSector)];
    if ( event == SITHAIINSTINCT_EVENT_UPDATE && pInstinct->intArg[0] )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + pInstinct->intArg[0];
    }

    if ( (pLocal->mode & SITHAI_MODE_FLEEING) != 0 || (pLocal->mode & SITHAI_MODE_SEARCHING) == 0 )
    {
        return 0;
    }

    switch ( (int)event )
    {
        case SITHAIINSTINCT_EVENT_UPDATE:
            if ( SITHAIINSTINCT_RANDF() <= pInstinct->fltArg[2] )
            {
                pLocal->pTargetThing = sithPlayer_g_pLocalPlayerThing;
                if ( !sithAIUtil_CanAttackTarget(sithPlayer_g_pLocalPlayerThing) )
                {
                    pLocal->pTargetThing = NULL;
                    return 0;
                }

                sithAIUtil_sub_49B2E0(pLocal, 0);
                if ( pLocal->targetSightState == 0 )
                {
                    sithAIUtil_AISetMode(pLocal, SITHAI_MODE_FLEEING);
                    sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_FEAR);
                    sithAIAwareness_CreateTransmittingEvent(pLocal->pOwner->pInSector, &pLocal->pOwner->pos, 1, 3.0f, pLocal->pOwner);
                    pLocal->pFleeFromThing = pLocal->pTargetThing;
                }
            }

            return 0;

        case SITHAI_EVENT_HIT_SECTOR:
            if ( SITHAIINSTINCT_RANDF() <= pInstinct->fltArg[3] )
            {
                sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_SURPRISE);
                if ( pThing )
                {
                    pLocal->pFleeFromThing = sithThing_GetThingParent(pThing);
                }

                sithAIUtil_AISetMode(pLocal, SITHAI_MODE_FLEEING);
                return 1;
            }

            return 0;

        case SITHAI_EVENT_SOUND:
        {
            if ( pAISector->aLevelAtTransmittingPos[2] <= pInstinct->fltArg[1] )
            {
                return 0;
            }

            SithThing* pDanger = pAISector->aTransmittingThing[2];
            if ( !pDanger || pDanger == pLocal->pOwner )
            {
                return 0;
            }

            rdVector3 toDanger;
            float distance;
            if ( sithAIUtil_GetDistanceToTarget(pOwner, &pOwner->pos, pDanger, -1.0f, pLocal->pClass->heardDistance, 0.0f, &toDanger, &distance) != 0 )
            {
                return 0;
            }

            pLocal->pFleeFromThing = pDanger;
            if ( (pLocal->mode & SITHAI_MODE_FLEEING) == 0 )
            {
                sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_FEAR);
                sithAIAwareness_CreateTransmittingEvent(pLocal->pOwner->pInSector, &pLocal->pOwner->pos, 2, 4.0f, pLocal->pOwner);
            }

            sithAIUtil_AISetMode(pLocal, SITHAI_MODE_FLEEING);
            return 1;
        }

        default:
            return 0;
    }
}

signed int J3DAPI sithAIInstinct_FearGunshot(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_FearGunshot, pLocal, pInstinct, pState, event, pObject);

    SithAIAwarenessSector* pAISector = &sithAIAwareness_g_aSectors[sithSector_GetSectorIndex(pLocal->pOwner->pInSector)];
    if ( (pLocal->mode & SITHAI_MODE_FLEEING) != 0 || event != SITHAI_EVENT_SOUND || pAISector->aLevelAtTransmittingPos[3] <= 0.0f )
    {
        return 0;
    }

    SithThing* pShooter = pAISector->aTransmittingThing[3];
    if ( !pShooter || pShooter == pLocal->pOwner )
    {
        return 0;
    }

    rdVector3 fromShooter;
    rdVector_Sub3(&fromShooter, &pLocal->pOwner->pos, &pShooter->pos);
    float distance = rdVector_Normalize3Acc(&fromShooter);
    if ( distance >= pInstinct->fltArg[0] )
    {
        // Further away the shooter has to aim at the AI and the chance has to hit
        if ( distance >= pInstinct->fltArg[1] )
        {
            return 0;
        }

        const rdVector3* pAim = &pShooter->orient.lvec;
        if ( (pAim->y * fromShooter.y + pAim->z * fromShooter.z) + pAim->x * fromShooter.x <= pInstinct->fltArg[3] )
        {
            return 0;
        }

        if ( !(SITHAIINSTINCT_RANDF() < pInstinct->fltArg[2]) )
        {
            return 0;
        }
    }

    sithAIUtil_AISetMode(pLocal, SITHAI_MODE_FLEEING);
    pLocal->pFleeFromThing = pShooter;
    sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_FEAR);
    return 1;
}

int J3DAPI sithAIInstinct_Hop(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_Hop, pLocal, pInstinct, pState, event, pObject);

    pState->msecNextUpdate = sithTime_g_msecGameTime + (int)(SITHAIINSTINCT_RANDF() * (float)pInstinct->intArg[0]);
    if ( pLocal->pOwner->attach.flags && (pLocal->mode & SITHAI_MODE_MOVING) != 0 )
    {
        sithThing_DetachThing(pLocal->pOwner);

        rdVector3 force;
        rdVector_Set3(&force, 0.0f, 0.0f, SITHAIINSTINCT_RANDF() * pInstinct->fltArg[1]);
        sithPhysics_ApplyForce(pLocal->pOwner, &force);
        sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_JUMP);
    }

    return 0;
}

int J3DAPI sithAIInstinct_HoverDrift(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_HoverDrift, pLocal, pInstinct, pState, event, pObject);

    SITH_ASSERTREL(pLocal && pLocal->pOwner);
    if ( (pLocal->pOwner->moveInfo.physics.flags & SITH_PF_FLY) == 0 )
    {
        return 0;
    }

    float msecInterval = pInstinct->fltArg[0];
    pState->msecNextUpdate = sithTime_g_msecGameTime + (int)((SITHAIINSTINCT_RANDF() - 0.5f) * msecInterval + msecInterval);

    if ( (pLocal->mode & SITHAI_MODE_MOVING) == 0 && (pLocal->pOwner->thingInfo.actorInfo.flags & SITH_AF_IMMOBILE) == 0 )
    {
        float dirX = (SITHAIINSTINCT_RANDF() - 0.5f) * 2.0f;
        float dirY = (SITHAIINSTINCT_RANDF() - 0.5f) * 2.0f;
        float dirZ = (SITHAIINSTINCT_RANDF() - 0.5f) * 2.0f;

        rdVector3 force;
        force.x = SITHAIINSTINCT_RANDF() * pInstinct->fltArg[1] * dirX;
        force.y = SITHAIINSTINCT_RANDF() * pInstinct->fltArg[1] * dirY;
        force.z = SITHAIINSTINCT_RANDF() * pInstinct->fltArg[1] * dirZ;
        sithPhysics_ApplyForce(pLocal->pOwner, &force);
    }

    return 0;
}

int J3DAPI sithAIInstinct_Talk(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_Talk, pLocal, pInstinct, pState, event, pObject);

    // aParams[0]: game time (sec) of the last line spoken
    float talkChance = pInstinct->fltArg[1];
    SithThing* pOwner = pLocal->pOwner;
    tSoundChannelHandle hChannel = 0;
    int result = 0;

    float secNextTalkTime = -1.0f;
    if ( pInstinct->fltArg[2] != 0.0f )
    {
        secNextTalkTime = pState->aParams[0] + pInstinct->fltArg[2];
    }

    switch ( (int)event )
    {
        case SITHAIINSTINCT_EVENT_UPDATE:
        {
            float secInterval = pInstinct->fltArg[0];
            if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_100000) != 0 )
            {
                talkChance  = talkChance + talkChance;
                secInterval = secInterval * 0.5f;
            }

            pState->msecNextUpdate = sithTime_g_msecGameTime + (int)(secInterval * 1000.0f);
            if ( secNextTalkTime > sithTime_g_secGameTime )
            {
                return 0;
            }

            // Talk only when on the same side of the water surface as the player
            bool bPlayerUnderwater = false;
            if ( sithPlayer_g_pLocalPlayerThing )
            {
                bPlayerUnderwater = (sithPlayer_g_pLocalPlayerThing->pInSector->flags & SITH_SECTOR_UNDERWATER) != 0;
            }

            bool bOwnerUnderwater = (pLocal->pOwner->pInSector->flags & SITH_SECTOR_UNDERWATER) != 0;
            if ( bPlayerUnderwater != bOwnerUnderwater )
            {
                return 0;
            }

            if ( !(SITHAIINSTINCT_RANDF() < talkChance) )
            {
                break;
            }

            float healthRatio = pOwner->thingInfo.actorInfo.health / pOwner->thingInfo.actorInfo.maxHealth;
            SithAIMode mode = pLocal->mode;
            if ( (mode & SITHAI_MODE_ACTIVE) == 0 )
            {
                hChannel = sithSoundClass_PlayModeRandom(pOwner, SITHSOUNDCLASS_IDLE);
            }
            else if ( (mode & SITHAI_MODE_FLEEING) != 0 )
            {
                hChannel = sithSoundClass_PlayModeRandom(pOwner, healthRatio > 0.5f ? SITHSOUNDCLASS_GLOAT : SITHSOUNDCLASS_FEAR);
            }
            else if ( (mode & SITHAI_MODE_UNKNOWN_100000) != 0 )
            {
                if ( SITHAIINSTINCT_RANDF() < 0.6f )
                {
                    hChannel = sithSoundClass_PlayModeFirst(pLocal->pOwner, SITHSOUNDCLASS_RESERVED8);
                }
                else
                {
                    hChannel = sithSoundClass_PlayModeRandom(pLocal->pOwner, healthRatio > 0.25f ? SITHSOUNDCLASS_GLOAT : SITHSOUNDCLASS_FEAR);
                }
            }
            else if ( (mode & SITHAI_MODE_TARGETVISIBLE) == 0 )
            {
                hChannel = sithSoundClass_PlayModeRandom(pOwner, SITHSOUNDCLASS_SEARCH);
            }
            else
            {
                hChannel = sithSoundClass_PlayModeRandom(pOwner, healthRatio < 0.25f ? SITHSOUNDCLASS_FEAR : SITHSOUNDCLASS_BOAST);
            }

            break;
        }

        case SITHAI_EVENT_TOUCHED:
        {
            if ( (pLocal->mode & SITHAI_MODE_SEARCHING) == 0 || secNextTalkTime > sithTime_g_secGameTime )
            {
                return 0;
            }

            const SithThing* pThing = (const SithThing*)pObject;
            if ( !pThing )
            {
                return 0;
            }

            if ( (pThing->type == SITH_THING_PLAYER || pThing->type == SITH_THING_ACTOR) && SITHAIINSTINCT_RANDF() < talkChance )
            {
                hChannel = sithSoundClass_PlayModeRandom(pLocal->pOwner, SITHSOUNDCLASS_ALERT);
            }

            break;
        }

        case SITHAI_EVENT_MODECHANGED:
        {
            SithAIMode prevMode = (SithAIMode)(intptr_t)pObject;
            if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_100000) != 0 && (prevMode & SITHAI_MODE_UNKNOWN_100000) == 0 )
            {
                sithSoundMixer_StopAllSoundsThing(pOwner);
                hChannel = sithSoundClass_PlayModeFirst(pLocal->pOwner, SITHSOUNDCLASS_RESERVED8);
            }

            break;
        }

        case SITHAI_EVENT_TALK:
            if ( secNextTalkTime > sithTime_g_secGameTime )
            {
                return 0;
            }

            hChannel = sithSoundClass_PlayModeRandom(pOwner, (SithSoundClassMode)(intptr_t)pObject);
            result   = 1;
            break;

        default:
            return 0;
    }

    if ( (int)hChannel > 0 )
    {
        pState->aParams[0] = sithTime_g_secGameTime;
    }

    return result;
}

int J3DAPI sithAIInstinct_RandomTurn(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_RandomTurn, pLocal, pInstinct, pState, event, pObject);

    pState->msecNextUpdate = sithTime_g_msecGameTime + (pInstinct->intArg[0] ? pInstinct->intArg[0] : 5000);
    if ( (pLocal->mode & SITHAI_MODE_SEARCHING) == 0 )
    {
        return 0;
    }

    rdVector3 pyr;
    rdVector_Set3(&pyr, 0.0f, SITHAIINSTINCT_RANDF() * 360.0f, 0.0f);

    rdVector3 lookDir;
    rdVector_Copy3(&lookDir, &rdroid_g_yVector3);
    rdVector_Rotate3Acc(&lookDir, &pyr);

    SithThing* pOwner = pLocal->pOwner;
    rdVector3 lookPos;
    rdVector_ScaleAdd3(&lookPos, &lookDir, pInstinct->fltArg[1], &pOwner->pos);

    rdVector3 toPos;
    float distance;
    int sightState = sithAIUtil_GetDistanceToTargetPos(pOwner, &pOwner->pos, &lookPos, -1.0f, pInstinct->fltArg[1], 0.0f, &toPos, &distance);
    if ( sightState != 0 )
    {
        return sightState;
    }

    sithAIMove_AISetLookPosEyeLevel(pLocal, &lookPos);

    // Note: The original returns whatever AISetLookPosEyeLevel leaves in eax, the new AI mode (never 0) unless the
    //       AI wants all events, so the update is always reported as handled
    return 1;
}

int J3DAPI sithAIInstinct_OpenDoors(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_OpenDoors, pLocal, pInstinct, pState, event, pObject);

    if ( (pLocal->mode & SITHAI_MODE_MOVING) != 0 )
    {
        sithPlayerActions_Activate(pLocal->pOwner);
        pState->msecNextUpdate = sithTime_g_msecGameTime + 1000;
    }

    return 0;
}

int J3DAPI sithAIInstinct_Jump(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_Jump, pLocal, pInstinct, pState, event, pObject);

    // aParams[0]: game time (msec) of the next possible jump
    SITH_ASSERTREL(pLocal && pLocal->pOwner);
    SithThing* pOwner   = pLocal->pOwner;
    SithSector* pSector = pOwner->pInSector;
    if ( !pOwner->attach.flags || (pLocal->mode & SITHAI_MODE_MOVING) == 0 || pOwner->thingInfo.actorInfo.bForceMovePlay )
    {
        return 0;
    }

    // Don't jump while already moving towards the goal
    rdVector3* pVelocity = &pOwner->moveInfo.physics.velocity;
    if ( (pLocal->vecUnknown0.z * pVelocity->z + pVelocity->y * pLocal->vecUnknown0.y) + pVelocity->x * pLocal->vecUnknown0.x > 0.02f )
    {
        return 0;
    }

    if ( (double)sithTime_g_msecGameTime < pState->aParams[0] )
    {
        return 0;
    }

    pState->aParams[0] = (float)(pInstinct->fltArg[0] + (double)sithTime_g_msecGameTime);

    if ( event == SITHAI_EVENT_TOUCHED || event == SITHAI_EVENT_HIT_WALL )
    {
        // Jump up and over the obstacle
        rdVector3 upPos;
        rdVector_ScaleAdd3(&upPos, &rdroid_g_zVector3, pInstinct->fltArg[1], &pOwner->pos);
        SithSector* pUpSector = sithCollision_FindSectorInRadius(pSector, &pOwner->pos, &upPos, 0.0f);
        if ( !pUpSector )
        {
            return 0;
        }

        rdVector3 jumpPos;
        jumpPos.x = pLocal->moveDirection.x * 0.1f + upPos.x;
        jumpPos.y = pLocal->moveDirection.y * 0.1f + upPos.y;
        jumpPos.z = upPos.z;
        SithSector* pJumpSector = sithCollision_FindSectorInRadius(pUpSector, &upPos, &jumpPos, 0.0f);
        if ( !pJumpSector )
        {
            return 0;
        }

        int bNoCollision = 0;
        if ( sithAIUtil_CheckFloorAtPos(pLocal, &jumpPos, pJumpSector, &bNoCollision) == 1 && bNoCollision )
        {
            sithAIMove_AIJump(pLocal, &jumpPos, 1.0f);
        }

        return 1;
    }

    if ( event == SITHAI_EVENT_HIT_CLIFF )
    {
        // Jump over the gap
        rdVector3 landPos;
        rdVector_ScaleAdd3(&landPos, &pLocal->moveDirection, pInstinct->fltArg[2], &pOwner->pos);
        if ( sithAIUtil_CheckPosition(pLocal, &landPos, NULL) )
        {
            rdVector_MultAcc3(pVelocity, &pLocal->moveDirection, 0.1f);
            sithAIMove_AIJump(pLocal, &pLocal->movePos, 1.0f);
        }

        return 1;
    }

    return 0;
}

int J3DAPI sithAIInstinct_LobFire(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_LobFire, pLocal, pInstinct, pState, event, pObject);

    SithAIUtilFireFlags fireFlags = SITHAIUTIL_FIRE_LOB;
    SITH_ASSERTREL(pLocal);

    SithThing* pActor  = pLocal->pOwner;
    SithThing* pTarget = pLocal->pTargetThing;
    SITH_ASSERTREL(pActor);
    if ( pActor->thingInfo.actorInfo.bForceMovePlay )
    {
        return 0;
    }

    if ( !sithAIUtil_CanAttackTarget(pTarget) )
    {
        return sithAIInstinct_TargetLost(pLocal, pTarget);
    }

    if ( sithPuppet_IsAnyModeOnTrack(pLocal->pOwner, SITHPUPPETSUBMODE_FIRE, SITHPUPPETSUBMODE_FIRE4) )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + 150;
        return 0;
    }

    int weaponNum = pInstinct->intArg[6];
    if ( SITHAIINSTINCT_RANDF() < pInstinct->fltArg[5] )
    {
        ++weaponNum;
    }

    sithAIUtil_SetWeaponFireFlags(weaponNum, &fireFlags);
    if ( sithAIUtil_AIFire(pLocal, pInstinct->fltArg[2], pInstinct->fltArg[3], pInstinct->fltArg[1], pInstinct->fltArg[4], weaponNum, fireFlags, 1) )
    {
        pState->msecNextUpdate   = sithTime_g_msecGameTime + pInstinct->intArg[0];
        pLocal->msecFireWaitTime = sithTime_g_msecGameTime + pInstinct->intArg[0];
        return 0;
    }

    sithAIMove_AISetLookThing(pLocal, pTarget);
    pState->msecNextUpdate = sithTime_g_msecGameTime + 500;
    return 0;
}

int J3DAPI sithAIInstinct_PrimaryFire(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_PrimaryFire, pLocal, pInstinct, pState, event, pObject);

    SithAIUtilFireFlags fireFlags = SITHAIUTIL_FIRE_PRIMARY;
    SITH_ASSERTREL(pLocal);

    SithThing* pActor = pLocal->pOwner;
    SITH_ASSERTREL(pActor);
    if ( (pLocal->mode & SITHAI_MODE_FLEEING) != 0 || pActor->thingInfo.actorInfo.bForceMovePlay )
    {
        return 0;
    }

    SithThing* pTarget = pLocal->pTargetThing;
    if ( !sithAIUtil_CanAttackTarget(pTarget) )
    {
        return sithAIInstinct_TargetLost(pLocal, pTarget);
    }

    if ( sithPuppet_IsAnyModeOnTrack(pLocal->pOwner, SITHPUPPETSUBMODE_FIRE, SITHPUPPETSUBMODE_FIRE4) )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + 250;
        return 0;
    }

    // Fire only when facing the target (fltArg[17] is the max cosine)
    if ( pInstinct->fltArg[17] != 0.0f )
    {
        const rdVector3* pLook = &pActor->orient.lvec;
        float facingDot = (pLocal->toTarget.y * pLook->y + pLocal->toTarget.z * pLook->z) + pLocal->toTarget.x * pLook->x;
        if ( pInstinct->fltArg[17] < facingDot )
        {
            if ( pInstinct->fltArg[16] == 0.0f )
            {
                sithAIMove_AISetLookThing(pLocal, pLocal->pTargetThing);
            }

            return 0;
        }
    }

    if ( pInstinct->fltArg[6] != 0.0f && SITHAIINSTINCT_RANDF() <= pInstinct->fltArg[6] )
    {
        fireFlags |= SITHAIUTIL_FIRE_LEAD;
    }

    int weaponNum = pInstinct->intArg[11];
    if ( (pLocal->mode & SITHAI_MODE_MOVING) != 0 )
    {
        if ( pInstinct->intArg[14] == 1 )
        {
            weaponNum = -1;
        }
        else if ( pInstinct->intArg[14] == 2 )
        {
            sithAIUtil_sub_49B2E0(pLocal, 0);
            return 0;
        }
    }
    else if ( pInstinct->fltArg[10] != 0.0f )
    {
        // Switch to the next weapon beyond fltArg[15]
        float altWeaponDistance = pInstinct->fltArg[15];
        if ( altWeaponDistance == 0.0f )
        {
            weaponNum = pInstinct->intArg[11] + 1;
        }
        else
        {
            sithAIUtil_sub_49B2E0(pLocal, 0);
            if ( altWeaponDistance < pLocal->distance )
            {
                weaponNum = pInstinct->intArg[11] + 1;
            }
        }
    }

    if ( weaponNum == pInstinct->intArg[11] && pInstinct->fltArg[7] != 0.0f && SITHAIINSTINCT_RANDF() <= pInstinct->fltArg[7] )
    {
        weaponNum = pInstinct->intArg[11] + 1;
    }

    sithAIUtil_SetWeaponFireFlags(weaponNum, &fireFlags);

    int burstCount = pInstinct->intArg[8];
    if ( !burstCount )
    {
        burstCount = 1;
    }

    if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_100000) != 0 )
    {
        // Note: Divides by zero when |intArg[8]| is 6
        int absBurstCount = burstCount < 0 ? -burstCount : burstCount;
        burstCount *= rand() % (6 - absBurstCount) + absBurstCount;
    }
    else if ( burstCount < 0 )
    {
        burstCount = rand() % burstCount + 1;
    }

    float minDistance = pInstinct->fltArg[4];
    float maxDistance = pInstinct->fltArg[2];
    if ( sithWeapon_HasWeaponSelected(pActor) )
    {
        maxDistance = sithWeapon_GetWeaponMaxAimDistance((SithWeaponId)pActor->thingInfo.actorInfo.weaponInfo.curWeaponID);
    }

    if ( ((pLocal->mode & SITHAI_MODE_UNKNOWN_100000) != 0 || sithAIUtil_sub_49F1F0(pLocal, &pLocal->pOwner->pos))
        && sithAIUtil_AIFire(pLocal, minDistance, maxDistance, pInstinct->fltArg[1], pInstinct->fltArg[3], weaponNum, fireFlags, burstCount) )
    {
        if ( pInstinct->fltArg[16] == 0.0f && !sithWeapon_IsAiming(pLocal->pOwner) )
        {
            sithAIMove_AISetLookThing(pLocal, pTarget);
        }

        pState->msecNextUpdate = sithTime_g_msecGameTime + (int)((SITHAIINSTINCT_RANDF() * 0.4f - 0.2f + 1.0f) * pInstinct->fltArg[0]);
        return 0;
    }

    if ( pInstinct->fltArg[16] == 0.0f && (pLocal->mode & SITHAI_MODE_MOVING) == 0 )
    {
        int weaponId = pLocal->pOwner->thingInfo.actorInfo.weaponInfo.curWeaponID;
        if ( pLocal->targetSightState == 0 )
        {
            SithThing* pTargetThing = pLocal->pTargetThing;
            const rdVector3* pVelocity = &pTargetThing->moveInfo.physics.velocity;
            if ( pVelocity->x == 0.0f && pVelocity->y == 0.0f && pVelocity->z == 0.0f )
            {
                if ( weaponId >= SITHWEAPON_COMFISTS && weaponId <= SITHWEAPON_COMSHOTGUN )
                {
                    sithAIMove_AISetLookThing(pLocal, pTargetThing);
                }
            }
            else
            {
                // Look where the target will be in half a second
                rdVector3 lookPos;
                rdVector_ScaleAdd3(&lookPos, pVelocity, 0.5f, &pTargetThing->pos);
                sithAIMove_AISetLookPosEyeLevel(pLocal, &lookPos);
            }
        }
        else if ( pLocal->targetSightState == 2 )
        {
            sithAIMove_AISetLookThing(pLocal, pLocal->pTargetThing);
        }
    }

    pState->msecNextUpdate = sithTime_g_msecGameTime + 750;
    return 0;
}

int J3DAPI sithAIInstinct_AlternateFire(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_AlternateFire, pLocal, pInstinct, pState, event, pObject);

    SithAIUtilFireFlags fireFlags = SITHAIUTIL_FIRE_ALT;
    SITH_ASSERTREL(pLocal);

    SithThing* pTarget = pLocal->pTargetThing;
    if ( !sithAIUtil_CanAttackTarget(pTarget) )
    {
        return 1;
    }

    if ( (pLocal->mode & SITHAI_MODE_ATTACKING) == 0 )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + pInstinct->intArg[1];
        return 0;
    }

    if ( sithPuppet_IsAnyModeOnTrack(pLocal->pOwner, SITHPUPPETSUBMODE_FIRE, SITHPUPPETSUBMODE_FIRE4) )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + 150;
        return 0;
    }

    // Negative weapon number picks a random one of 0..-intArg[0]
    int weaponNum = pInstinct->intArg[0];
    if ( weaponNum < 0 )
    {
        weaponNum = rand() % (1 - weaponNum);
    }

    sithAIUtil_SetWeaponFireFlags(weaponNum, &fireFlags);
    if ( pInstinct->fltArg[7] != 0.0f && SITHAIINSTINCT_RANDF() <= pInstinct->fltArg[7] )
    {
        fireFlags |= SITHAIUTIL_FIRE_LEAD;
    }

    int burstCount = pInstinct->intArg[8];
    if ( !burstCount )
    {
        burstCount = 1;
    }

    if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_100000) != 0 )
    {
        burstCount *= rand() % 8 + 3;
    }
    else if ( burstCount < 0 )
    {
        burstCount = rand() % burstCount + 1;
    }

    if ( pInstinct->intArg[9] )
    {
        fireFlags |= SITHAIUTIL_FIRE_STOP_ON_FIRE;
    }

    if ( sithAIUtil_AIFire(pLocal, pInstinct->fltArg[4], pInstinct->fltArg[5], pInstinct->fltArg[3], pInstinct->fltArg[6], weaponNum, fireFlags, burstCount) )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + (int)((SITHAIINSTINCT_RANDF() * 0.4f - 0.2f + 1.0f) * pInstinct->fltArg[2]);
        return 0;
    }

    if ( pLocal->targetSightState == 2 )
    {
        sithAIMove_AISetLookThing(pLocal, pTarget);
    }
    else if ( pLocal->targetSightState == 0 && pTarget->moveType == SITH_MT_PHYSICS )
    {
        const rdVector3* pVelocity = &pTarget->moveInfo.physics.velocity;
        if ( pVelocity->x != 0.0f || pVelocity->y != 0.0f || pVelocity->z != 0.0f )
        {
            // Look where the target will be in half a second
            rdVector3 lookPos;
            rdVector_ScaleAdd3(&lookPos, pVelocity, 0.5f, &pTarget->pos);
            sithAIMove_AISetLookPosEyeLevel(pLocal, &lookPos);
        }
    }

    pState->msecNextUpdate = sithTime_g_msecGameTime + pInstinct->intArg[1];
    return 0;
}

int J3DAPI sithAIInstinct_CircleStrafe(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing)
{
    INDY_AB_ORIGINAL(sithAIInstinct_CircleStrafe, pLocal, pInstinct, pState, event, pThing);

    // aParams[0]: strafe direction (-1 or 1, 0 when not strafing), aParams[1]: number of strafe moves made
    float moveSpeed = 1.25f;
    int result = 0;

    SITH_ASSERTREL(pLocal);
    pState->msecNextUpdate = sithTime_g_msecGameTime + pInstinct->intArg[0];
    if ( (pLocal->mode & (SITHAI_MODE_UNKNOWN_1000000 | SITHAI_MODE_FLEEING | SITHAI_MODE_SEARCHING)) != 0
        || (pLocal->mode & SITHAI_MODE_ATTACKING) == 0 )
    {
        pLocal->mode &= ~SITHAI_MODE_CIRCLESTRAFING;
        return 0;
    }

    if ( !sithAIUtil_CanAttackTarget(pLocal->goalThing) )
    {
        sithAIUtil_AISetMode(pLocal, SITHAI_MODE_SEARCHING);
        pLocal->goalThing = NULL;
        return 0;
    }

    if ( pInstinct->fltArg[7] != 0.0f && sithPuppet_IsAnyModeOnTrack(pLocal->pOwner, SITHPUPPETSUBMODE_FIRE, SITHPUPPETSUBMODE_FIRE4) )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + 500;
        return 0;
    }

    if ( pInstinct->fltArg[8] != 0.0f )
    {
        moveSpeed = pInstinct->fltArg[8];
    }

    switch ( (int)event )
    {
        case SITHAI_EVENT_TOUCHED:
            if ( (pLocal->mode & SITHAI_MODE_CIRCLESTRAFING) == 0 || !pThing || pThing->type == SITH_THING_PLAYER )
            {
                return 0;
            }

            break;

        case SITHAI_EVENT_HIT_WALL:
        case SITHAI_EVENT_HIT_CLIFF:
        case SITHAI_EVENT_HIT_THING:
        case SITHAI_EVENT_GOAL_UNREACHABLE:
            if ( (pLocal->mode & SITHAI_MODE_CIRCLESTRAFING) == 0 )
            {
                return 0;
            }

            break;

        case SITHAI_EVENT_GOAL_REACHED:
            result = 1;
            // fallthrough

        case SITHAIINSTINCT_EVENT_UPDATE:
        {
            if ( (pLocal->mode & SITHAI_MODE_CIRCLESTRAFING) != 0 && !pInstinct->intArg[6] )
            {
                sithAIMove_AISetLookThing(pLocal, pLocal->goalThing);
            }

            int bRandomStrafe = pInstinct->intArg[4];
            sithAIUtil_sub_49B640(pLocal);
            if ( pInstinct->fltArg[2] < pLocal->targetDistance || pLocal->targetSightState2 != 0 )
            {
                break;
            }

            // Strafe only around a goal that isn't facing away
            rdVector3 fromGoal;
            rdVector_Scale3(&fromGoal, &pLocal->vecUnknown0, -pLocal->targetDistance);
            if ( !bRandomStrafe && rdVector_Dot3(&pLocal->goalThing->orient.lvec, &fromGoal) < 0.0f )
            {
                break;
            }

            float numMoves = pState->aParams[1];
            pState->aParams[1] = numMoves + 1.0f;
            if ( numMoves > 4.0f )
            {
                break;
            }

            if ( pState->aParams[0] == 0.0f || bRandomStrafe )
            {
                pState->aParams[0] = SITHAIINSTINCT_RANDF() < 0.5f ? -1.0f : 1.0f;
            }

            float direction = pState->aParams[0];
            rdVector3 pyr;
            rdVector_Zero3(&pyr);
            if ( !bRandomStrafe )
            {
                pyr.yaw = pInstinct->fltArg[1] * direction;
            }
            else
            {
                pyr.yaw = (SITHAIINSTINCT_RANDF() + 0.5f) * direction * pInstinct->fltArg[1];
            }

            rdVector3 toGoal;
            rdVector_Sub3(&toGoal, &pLocal->goalThing->pos, &pLocal->pOwner->pos);
            rdVector_Normalize3Acc(&toGoal);

            rdVector3 strafeDir;
            rdVector_Rotate3(&strafeDir, &toGoal, &pyr);

            SithThing* pOwner = pLocal->pOwner;
            rdVector3 strafePos;
            rdVector_ScaleAdd3(&strafePos, &strafeDir, pOwner->collide.movesize * 3.0f, &pOwner->pos);
            if ( pInstinct->fltArg[5] != 0.0f )
            {
                // Strafe position nearer to the AI than fltArg[5]: move it by fltArg[5] towards the AI
                rdVector3 toOwner;
                rdVector_Sub3(&toOwner, &pOwner->pos, &strafePos);
                if ( rdVector_Normalize3Acc(&toOwner) < pInstinct->fltArg[5] )
                {
                    rdVector_MultAcc3(&strafePos, &toOwner, pInstinct->fltArg[5]);
                }
            }

            rdVector3 toPos;
            float distance;
            if ( sithAIUtil_GetDistanceToTargetPos(pLocal->goalThing, &pLocal->goalThing->pos, &strafePos, -1.0f, pLocal->pClass->sightDistance, 0.0f, &toPos, &distance) == 0 )
            {
                pLocal->mode |= SITHAI_MODE_CIRCLESTRAFING;
                sithAIMove_AISetMovePos(pLocal, &strafePos, moveSpeed);
                if ( !pInstinct->intArg[6] )
                {
                    sithAIMove_AISetLookThing(pLocal, pLocal->goalThing);
                }
                else
                {
                    sithAIMove_AISetLookPosEyeLevel(pLocal, &strafePos);
                }

                pState->msecNextUpdate = sithTime_g_msecGameTime + pInstinct->intArg[3];
                return result;
            }

            // The goal can't see the position, strafe the other way next time
            pState->aParams[0] = -direction;
            pState->aParams[1] = 0.0f;
            pLocal->mode &= ~SITHAI_MODE_CIRCLESTRAFING;
            return result;
        }

        default:
            return 0;
    }

    // Stop strafing
    sithAIMove_AISetLookThing(pLocal, pLocal->goalThing);
    pLocal->mode &= ~SITHAI_MODE_CIRCLESTRAFING;
    pState->aParams[0] = 0.0f;
    pState->aParams[1] = 0.0f;
    return result;
}

int J3DAPI sithAIInstinct_Dodge(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing)
{
    INDY_AB_ORIGINAL(sithAIInstinct_Dodge, pLocal, pInstinct, pState, event, pThing);

    // aParams[0]: game time (sec) of the last dodge
    float moveSpeed       = 1.5f;
    float secNextDodgeTime = -1.0f;
    float angle           = 0.0f;
    int result            = 0;

    SITH_ASSERTREL(pLocal && pLocal->pOwner);
    if ( event == SITHAIINSTINCT_EVENT_UPDATE )
    {
        return 0;
    }

    if ( (event & (SITHAI_EVENT_GOAL_REACHED | SITHAI_EVENT_GOAL_UNREACHABLE)) != 0 && (pLocal->mode & SITHAI_MODE_UNKNOWN_10) != 0 )
    {
        // Dodge move done
        pLocal->mode &= ~SITHAI_MODE_UNKNOWN_10;
        if ( pLocal->pTargetThing )
        {
            sithAIMove_AISetLookThing(pLocal, pLocal->pTargetThing);
        }

        return 0;
    }

    if ( (pLocal->mode & (SITHAI_MODE_UNKNOWN_1000000 | SITHAI_MODE_UNKNOWN_100000 | SITHAI_MODE_FLEEING | SITHAI_MODE_UNKNOWN_10)) != 0 )
    {
        return 0;
    }

    float minDist  = pInstinct->fltArg[0] + 0.03f;
    float maxDist  = pInstinct->fltArg[1] + 0.03f;
    float moveDist = SITHAIINSTINCT_RANDF() * (maxDist - minDist) + minDist;

    float minSightDist = pInstinct->fltArg[4];
    float minMoveDist  = pInstinct->fltArg[10];
    if ( pInstinct->fltArg[2] != 0.0f )
    {
        moveSpeed = pInstinct->fltArg[2];
    }

    if ( pInstinct->fltArg[3] != 0.0f )
    {
        secNextDodgeTime = pState->aParams[0] + pInstinct->fltArg[3];
    }

    rdVector3 dodgeDir;
    float distance;
    int pathFlags   = 0;
    bool bSpecialMove = false;
    switch ( (int)event )
    {
        case SITHAI_EVENT_TARGETED:
        {
            if ( (pLocal->mode & SITHAI_MODE_SEARCHING) != 0 || !pThing )
            {
                return 0;
            }

            if ( (pThing->thingInfo.weaponInfo.damageType & (SITH_DAMAGE_WHIP | SITH_DAMAGE_ELECTROWHIP)) != 0 )
            {
                return 0;
            }

            // Note: Projectiles flagged SITH_WF_AITARGETNODODGE skip the dodge delay and chance instead of never being dodged
            if ( (pThing->thingInfo.weaponInfo.flags & SITH_WF_AITARGETNODODGE) == 0 )
            {
                if ( secNextDodgeTime > sithTime_g_secGameTime || !(SITHAIINSTINCT_RANDF() < pInstinct->fltArg[5]) )
                {
                    return 0;
                }
            }

            result = 1;
            SithThing* pShooter = sithThing_GetThingParent(pThing);
            if ( sithAIUtil_GetDistanceToTarget(pLocal->pOwner, &pLocal->pOwner->pos, pShooter, pLocal->pClass->fov, pLocal->pClass->sightDistance, 0.0f, &dodgeDir, &distance) != 0 )
            {
                return 0;
            }

            if ( distance < minSightDist )
            {
                return 0;
            }

            if ( pInstinct->fltArg[8] != 0.0f && SITHAIINSTINCT_RANDF() < pInstinct->fltArg[8]
                && sithAIMove_AISpecialMove(pLocal, SITHACTORSPECIALMOVE_ROLL | SITHACTORSPECIALMOVE_DIR_RANDOM) )
            {
                bSpecialMove = true;
                break;
            }

            pathFlags = 0x8001;
            break;
        }

        case SITHAI_EVENT_HIT_SECTOR:
        {
            if ( secNextDodgeTime > sithTime_g_secGameTime || !(SITHAIINSTINCT_RANDF() < pInstinct->fltArg[6]) )
            {
                return 0;
            }

            if ( !pThing || pThing->type != SITH_THING_WEAPON || pThing->moveType != SITH_MT_PHYSICS )
            {
                return 0;
            }

            SithThing* pShooter = sithThing_GetThingParent(pThing);
            if ( pShooter == pLocal->pOwner )
            {
                return 0;
            }

            if ( pShooter->controlType == SITH_CT_AI && pShooter->controlInfo.aiControl.pLocal )
            {
                // Get out of the line between the shooting AI and its target
                rdVector_Sub3(&dodgeDir, &pShooter->controlInfo.aiControl.pLocal->pTargetThing->pos, &pShooter->pos);
                rdVector_Normalize3Acc(&dodgeDir);
                pLocal->pFleeFromThing = pShooter;
                pathFlags = 0x9001;
                moveDist  = 1.0f;
            }
            else
            {
                if ( sithAIUtil_GetDistanceToTarget(pLocal->pOwner, &pLocal->pOwner->pos, pShooter, pLocal->pClass->fov, pLocal->pClass->sightDistance, 0.0f, &dodgeDir, &distance) != 0 )
                {
                    return 0;
                }

                if ( distance < minSightDist )
                {
                    return 0;
                }

                angle     = 60.0f;
                pathFlags = 0x8002;
            }

            break;
        }

        case SITHAI_EVENT_SOUND:
        {
            SithAIAwarenessSector* pAISector = &sithAIAwareness_g_aSectors[sithSector_GetSectorIndex(pLocal->pOwner->pInSector)];
            if ( pAISector->aLevelAtTransmittingPos[2] == 0.0f )
            {
                return 0;
            }

            SithThing* pDanger = pAISector->aTransmittingThing[2];
            if ( !pDanger || pDanger->moveType != SITH_MT_PHYSICS )
            {
                return 0;
            }

            if ( pDanger->type == SITH_THING_WEAPON && (pDanger->thingInfo.weaponInfo.damageType & SITH_DAMAGE_WHIP) != 0 )
            {
                return 0;
            }

            SithThing* pSource = sithThing_GetThingParent(pDanger);
            if ( pSource == pLocal->pOwner )
            {
                return 0;
            }

            bool bVehicle = (pSource->moveInfo.physics.flags & SITHAIINSTINCT_VEHICLEFLAGS) != 0;
            if ( !bVehicle )
            {
                if ( sithAIUtil_GetDistanceToTarget(pLocal->pOwner, &pLocal->pOwner->pos, pSource, pLocal->pClass->fov, 1.0f, 0.0f, &dodgeDir, &distance) != 0 )
                {
                    return 0;
                }
            }
            else
            {
                // Watch out for a vehicle within the distance it covers in two seconds
                float speed = sithAIInstinct_GetSpeed(pSource);
                float range = speed + speed;
                if ( pLocal->pClass->sightDistance < range )
                {
                    range = pLocal->pClass->sightDistance;
                }

                if ( range <= 1.0f )
                {
                    range = 1.0f;
                }

                if ( sithAIUtil_GetDistanceToTarget(pLocal->pOwner, &pLocal->pOwner->pos, pSource, -1.0f, range, 0.0f, &dodgeDir, &distance) != 0 )
                {
                    return 0;
                }
            }

            result = 1;
            if ( !bVehicle )
            {
                if ( secNextDodgeTime > sithTime_g_secGameTime || !(SITHAIINSTINCT_RANDF() < pInstinct->fltArg[7]) )
                {
                    return 0;
                }

                sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_FEAR);
                pLocal->pFleeFromThing = pSource;
                angle     = 45.0f;
                pathFlags = 0xC002;
            }
            else
            {
                // Jump aside only when the vehicle is heading at the AI
                rdVector3 heading;
                sithAIUtil_GetXYHeadingVector(pDanger, &heading);
                if ( rdVector_Dot3(&heading, &dodgeDir) > -0.5f )
                {
                    return 0;
                }

                moveDist  = 0.5f;
                pathFlags = 0x8001;

                float rollChance = pInstinct->fltArg[9];
                if ( rollChance == 0.0f )
                {
                    rollChance = 0.98f;
                }

                if ( sithAIInstinct_GetSpeed(pSource) > 1.4f )
                {
                    rollChance -= 0.2f;
                }

                if ( SITHAIINSTINCT_RANDF() < rollChance )
                {
                    pathFlags = 0x9001;
                }

                pLocal->pFleeFromThing = pSource;
            }

            break;
        }

        default:
            return 0;
    }

    if ( !bSpecialMove )
    {
        // Note: Discards the flee-from thing set above
        pLocal->pFleeFromThing = NULL;

        rdVector3 dodgePos;
        if ( !sithAIUtil_MakePathPos(pLocal, &pLocal->pOwner->pos, minMoveDist, moveDist, angle, pathFlags, &dodgeDir, &dodgePos) )
        {
            return 0;
        }

        sithAIMove_AISetLookPosEyeLevel(pLocal, &dodgePos);
        sithAIMove_AISetMoveTargetPos(pLocal, &dodgePos, moveSpeed);
    }

    pLocal->mode |= SITHAI_MODE_UNKNOWN_10;
    sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TRAVERSEWPNTS);
    pState->aParams[0] = sithTime_g_secGameTime;
    return result;
}

int J3DAPI sithAIInstinct_HitAndRun(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_HitAndRun, pLocal, pInstinct, pState, event, pObject);

    // aParams[0]: msec spent attacking, aParams[1]: 1 when it's time to run, aParams[2]: 1 while running,
    // aParams[3]: 1 once the AI fired
    if ( event == SITHAI_EVENT_FIRE )
    {
        pState->aParams[3] = 1.0f;
        if ( SITHAIINSTINCT_RANDF() < pInstinct->fltArg[2] )
        {
            pState->aParams[1] = 1.0f;
            pState->msecNextUpdate = sithTime_g_msecGameTime + 500;
        }

        return 0;
    }

    if ( event != SITHAIINSTINCT_EVENT_UPDATE )
    {
        return 0;
    }

    if ( pState->aParams[1] == 1.0f )
    {
        // Finish the shot first
        if ( sithPuppet_IsAnyModeOnTrack(pLocal->pOwner, SITHPUPPETSUBMODE_FIRE, SITHPUPPETSUBMODE_FIRE4) )
        {
            pLocal->mode &= ~SITHAI_MODE_ATTACKING;
            pState->msecNextUpdate = sithTime_g_msecGameTime + 100;
            return 0;
        }
    }
    else if ( (pLocal->mode & SITHAI_MODE_FLEEING) != 0 )
    {
        if ( pState->aParams[2] == 1.0f )
        {
            sithAIUtil_AISetMode(pLocal, SITHAI_MODE_ATTACKING);
            pState->aParams[2] = 0.0f;
        }

        return 0;
    }
    else
    {
        if ( (pLocal->mode & SITHAI_MODE_TARGETVISIBLE) == 0 || pState->aParams[3] == 0.0f || !sithAIUtil_CanAttackTarget(pLocal->pTargetThing) )
        {
            pState->aParams[0] = 0.0f;
            pState->msecNextUpdate = sithTime_g_msecGameTime + 1000;
            return 0;
        }

        float msecAttacking = pState->aParams[0] + 1000.0f;
        if ( msecAttacking <= pInstinct->fltArg[0] )
        {
            pState->aParams[0] = msecAttacking;
            pState->msecNextUpdate = sithTime_g_msecGameTime + 1000;
            return 0;
        }
    }

    // Run
    sithAIUtil_AISetMode(pLocal, SITHAI_MODE_FLEEING);
    pLocal->pFleeFromThing = pLocal->pTargetThing;

    pState->aParams[2] = 1.0f;
    pState->aParams[0] = 0.0f;
    pState->aParams[1] = 0.0f;
    pState->aParams[3] = 0.0f;
    pState->msecNextUpdate = sithTime_g_msecGameTime + (int)pInstinct->fltArg[1];
    return 0;
}

int J3DAPI sithAIInstinct_Retreat(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_Retreat, pLocal, pInstinct, pState, event, pObject);

    // aParams[0]: number of retreats
    if ( (pLocal->mode & SITHAI_MODE_ATTACKING) == 0 )
    {
        return 0;
    }

    if ( !sithAIUtil_CanAttackTarget(pLocal->pTargetThing) )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + pInstinct->intArg[0];
        return 0;
    }

    if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_100000) != 0 )
    {
        pState->flags |= SITHAI_INSTINCT_DISABLED;
        return 0;
    }

    float healthRatio = pLocal->pOwner->thingInfo.actorInfo.health / pLocal->pOwner->thingInfo.actorInfo.maxHealth;
    if ( pInstinct->fltArg[3] != 0.0f && pInstinct->fltArg[3] < pState->aParams[0] )
    {
        return 0;
    }

    if ( healthRatio < pInstinct->fltArg[1] && (event == SITHAIINSTINCT_EVENT_UPDATE || event == SITHAI_EVENT_HIT_SECTOR) )
    {
        if ( SITHAIINSTINCT_RANDF() < pInstinct->fltArg[2] )
        {
            pState->aParams[0] += 1.0f;
            sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_FLEE);
            sithAIUtil_AISetMode(pLocal, SITHAI_MODE_FLEEING);
            pLocal->pFleeFromThing = pLocal->pTargetThing;
            pState->msecNextUpdate = sithTime_g_msecGameTime + pInstinct->intArg[0];
            return 0;
        }

        if ( SITHAIINSTINCT_RANDF() < 0.1f )
        {
            sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_FEAR);
        }
    }

    pState->msecNextUpdate = sithTime_g_msecGameTime + pInstinct->intArg[0];
    return 0;
}

int J3DAPI sithAIInstinct_RandomMove(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_RandomMove, pLocal, pInstinct, pState, event, pObject);

    // aParams[0..2]: target position when the move started, aParams[3]: 1 when aParams[0..2] is set
    if ( (pLocal->mode & SITHAI_MODE_ATTACKING) == 0 || !sithAIUtil_CanAttackTarget(pLocal->pTargetThing) )
    {
        return 0;
    }

    if ( (pLocal->mode & (SITHAI_MODE_CIRCLESTRAFING | SITHAI_MODE_FLEEING | SITHAI_MODE_SEARCHING)) != 0
        || (pLocal->mode & SITHAI_MODE_UNKNOWN_100000) != 0 )
    {
        return 0;
    }

    if ( pInstinct->intArg[6] && sithPuppet_IsAnyModeOnTrack(pLocal->pOwner, SITHPUPPETSUBMODE_FIRE, SITHPUPPETSUBMODE_FIRE4) )
    {
        return 0;
    }

    sithAIUtil_sub_49B2E0(pLocal, 0);
    switch ( (int)event )
    {
        case SITHAIINSTINCT_EVENT_UPDATE:
            // The original converts the interval as unsigned for the random part
            pState->msecNextUpdate = sithTime_g_msecGameTime + pInstinct->intArg[0]
                + (int)((SITHAIINSTINCT_RANDF() - 0.5f) * (float)(uint32_t)pInstinct->intArg[0]);
            if ( pLocal->targetSightState != 0 )
            {
                return 0;
            }

            break;

        case SITHAI_EVENT_GOAL_REACHED:
            if ( pLocal->targetSightState == 0 && pLocal->pTargetThing )
            {
                sithAIMove_AISetLookThing(pLocal, pLocal->pTargetThing);
                pState->aParams[3] = 0.0f;
                return 0;
            }

            if ( pState->aParams[3] != 0.0f )
            {
                rdVector3 lookPos;
                rdVector_Set3(&lookPos, pState->aParams[0], pState->aParams[1], pState->aParams[2]);
                sithAIMove_AISetLookPosEyeLevel(pLocal, &lookPos);
            }

            pState->aParams[3] = 0.0f;
            return 0;

        case SITHAI_EVENT_FIRE:
            if ( !pInstinct->intArg[7] )
            {
                return 0;
            }

            break;

        default:
            return 0;
    }

    float minDist   = pInstinct->fltArg[1] + 0.03f;
    float distRange = (pInstinct->fltArg[2] + 0.03f) - minDist;
    float moveDist  = SITHAIINSTINCT_RANDF() * distRange + minDist;

    float minSpeed  = pInstinct->fltArg[3];
    float maxSpeed  = pInstinct->fltArg[4];
    float moveSpeed = SITHAIINSTINCT_RANDF() * (maxSpeed - minSpeed) + minSpeed;

    rdVector3 awayDir;
    rdVector_Neg3(&awayDir, &pLocal->toTarget);

    int pathFlags = pInstinct->intArg[9] ? 0x808006 : 0x808002;
    float angle = pInstinct->fltArg[5];
    if ( angle < 0.0f )
    {
        pathFlags |= 0x4000;
    }

    rdVector3 movePos;
    if ( !sithAIUtil_MakePathPos(pLocal, &pLocal->pOwner->pos, minDist, moveDist, angle, pathFlags, &awayDir, &movePos) )
    {
        return 0;
    }

    if ( pLocal->pTargetThing )
    {
        pState->aParams[0] = pLocal->pTargetThing->pos.x;
        pState->aParams[1] = pLocal->pTargetThing->pos.y;
        pState->aParams[2] = pLocal->pTargetThing->pos.z;
        pState->aParams[3] = 1.0f;
    }
    else
    {
        pState->aParams[3] = 0.0f;
    }

    if ( pInstinct->intArg[8] || !(moveDist < minDist + distRange * 0.5f) )
    {
        sithAIMove_AISetLookPosEyeLevel(pLocal, &movePos);
    }

    sithAIMove_AISetMoveTargetPos(pLocal, &movePos, moveSpeed);
    return 0;
}

int J3DAPI sithAIInstinct_HumanCombatMove(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_HumanCombatMove, pLocal, pInstinct, pState, event, pObject);

    // aParams[0]: game time (msec) of the next move while in the line of fire, aParams[1]: chance to stay put,
    // aParams[2]: move directions tried (bit mask), aParams[3]: cover state (3 = peek out, 2 = take cover)
    SithThing* pOwner = pLocal->pOwner;
    SITH_ASSERTREL(pLocal && pLocal->pOwner);
    if ( !sithAIUtil_CanAttackTarget(pLocal->pTargetThing) )
    {
        return 0;
    }

    if ( (pLocal->mode & SITHAI_MODE_ATTACKING) == 0
        || (pLocal->mode & (SITHAI_MODE_UNKNOWN_100000 | SITHAI_MODE_CIRCLESTRAFING | SITHAI_MODE_FLEEING | SITHAI_MODE_SEARCHING)) != 0 )
    {
        pState->aParams[1] = 0.0f;
        pState->aParams[2] = 0.0f;
        return 0;
    }

    sithAIUtil_sub_49B2E0(pLocal, 0);
    float healthRatio = pOwner->thingInfo.actorInfo.health / pOwner->thingInfo.actorInfo.maxHealth;

    switch ( (int)event )
    {
        case SITHAI_EVENT_HIT_SECTOR:
            if ( healthRatio > pInstinct->fltArg[6] )
            {
                return 0;
            }

            if ( SITHAIINSTINCT_RANDF() < pInstinct->fltArg[7] )
            {
                sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_FLEE);
                sithAIUtil_AISetMode(pLocal, SITHAI_MODE_FLEEING);
                pLocal->pFleeFromThing = pLocal->pTargetThing;
                return 0;
            }

            if ( SITHAIINSTINCT_RANDF() < pInstinct->fltArg[8] && sithAI_HasInstinct(pLocal, "circlestrafe") )
            {
                sithAI_EnableInstinct(pLocal, "circlestrafe", 1);
                pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_400000;
            }

            if ( !(SITHAIINSTINCT_RANDF() < pInstinct->fltArg[10]) )
            {
                return 0;
            }

            // Stand and fire
            sithAIMove_StopAIMovement(pLocal);
            pLocal->mode |= SITHAI_MODE_UNKNOWN_100000;
            sithAI_ForceInstinctUpdate(pLocal, "primaryfire", 0);
            sithAI_ForceInstinctUpdate(pLocal, "lobfire", 0);
            sithAI_ForceInstinctUpdate(pLocal, "alternatefire", 0);
            return 0;

        case SITHAIINSTINCT_EVENT_UPDATE:
        {
            if ( pLocal->targetSightState != 0 )
            {
                return 0;
            }

            bool bCheckFire = false;
            if ( sithAIUtil_sub_49F1F0(pLocal, &pLocal->pOwner->pos) )
            {
                bCheckFire = true;

                SithThing* pTarget = pLocal->pTargetThing;
                if ( (pTarget->moveInfo.physics.flags & SITHAIINSTINCT_VEHICLEFLAGS) == 0 && sithAIUtil_IsThingMoving(pTarget) )
                {
                    pState->aParams[1] += 0.2f;
                }

                if ( (double)sithTime_g_msecGameTime < pState->aParams[0] )
                {
                    return 0;
                }

                float msecInterval = pInstinct->fltArg[0];
                pState->aParams[0] = (float)((double)sithTime_g_msecGameTime + ((SITHAIINSTINCT_RANDF() - 0.5f) * msecInterval + msecInterval));
                if ( pState->aParams[1] < SITHAIINSTINCT_RANDF() )
                {
                    pState->aParams[1] += 0.05f;
                    return 0;
                }
            }

            return sithAIInstinct_MakeCombatMove(pLocal, pInstinct, pState, bCheckFire);
        }

        case SITHAI_EVENT_MODECHANGED:
        {
            SithAIMode prevMode = (SithAIMode)(intptr_t)pObject;
            if ( (pLocal->mode & SITHAI_MODE_CHASE_GOAL) == 0 || (prevMode & SITHAI_MODE_CHASE_GOAL) != 0
                || (pLocal->mode & SITHAI_MODE_UNKNOWN_1000000) != 0 )
            {
                return 0;
            }

            sithAIMove_StopAIMovement(pLocal);

            rdVector3 toPos;
            float distance;
            int sightState = sithAIUtil_GetDistanceToTargetPos(pLocal->pOwner, &pLocal->pOwner->pos, &pLocal->vecUnknown5, -1.0f, pLocal->pClass->sightDistance, 0.0f, &toPos, &distance);
            bool bTakeCover = healthRatio <= pInstinct->fltArg[6] && SITHAIINSTINCT_RANDF() < pInstinct->fltArg[9];
            if ( sightState == 0 )
            {
                return 0;
            }

            sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->vecUnknown);
            sithAIMove_AISetMovePos(pLocal, &pLocal->vecUnknown, pInstinct->fltArg[5]);
            if ( bTakeCover )
            {
                pLocal->mode = (pLocal->mode & ~SITHAI_MODE_CHASE_GOAL) | SITHAI_MODE_UNKNOWN_1000000;
                pState->aParams[3] = 3.0f;
            }

            return 1;
        }

        case SITHAI_EVENT_GOAL_REACHED:
            if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_1000000) != 0 )
            {
                if ( pState->aParams[3] == 3.0f )
                {
                    if ( pLocal->targetSightState != 0 )
                    {
                        pLocal->mode &= ~SITHAI_MODE_UNKNOWN_1000000;
                        return 0;
                    }

                    sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->vecUnknown122);
                    sithAIUtil_AIPauseMove(pLocal, rand() % 1001 + 2000);
                    pState->aParams[3] = 2.0f;
                }
                else if ( pState->aParams[3] == 2.0f )
                {
                    if ( pLocal->targetSightState == 0 )
                    {
                        pLocal->mode &= ~SITHAI_MODE_UNKNOWN_1000000;
                        return 0;
                    }

                    sithAIUtil_AIPauseMove(pLocal, rand() % 2001 + 1000);
                    pState->aParams[3] = 3.0f;
                }

                // Roll sideways
                rdVector3 toA;
                rdVector_Sub3(&toA, &pLocal->vecUnknown122, &pLocal->pOwner->pos);
                rdVector_Normalize3Acc(&toA);

                rdVector3 toB;
                rdVector_Sub3(&toB, &pLocal->vecUnknown, &pLocal->pOwner->pos);
                rdVector_Normalize3Acc(&toB);

                pOwner->thingInfo.actorInfo.stateChange.type = SITHACTORSTATECHANGE_ANIMMOVE;
                if ( rdMath_DeltaAngleNormalized(&toB, &toA, &pOwner->orient.uvec) < 0.0f )
                {
                    pOwner->thingInfo.actorInfo.stateChange.params.moveFlags = SITHACTORSPECIALMOVE_ROLL | SITHACTORSPECIALMOVE_DIR_LEFT;
                }
                else
                {
                    pOwner->thingInfo.actorInfo.stateChange.params.moveFlags = SITHACTORSPECIALMOVE_ROLL | SITHACTORSPECIALMOVE_DIR_RIGHT;
                }

                return 1;
            }

            if ( (pLocal->mode & SITHAI_MODE_CHASE_GOAL) != 0 )
            {
                rdVector3 toPos;
                float distance;
                if ( sithAIUtil_GetDistanceToTargetPos(pLocal->pOwner, &pLocal->pOwner->pos, &pLocal->vecUnknown5, -1.0f, pLocal->pClass->sightDistance, 0.0f, &toPos, &distance) != 0 )
                {
                    sithAIMove_AISetMovePos(pLocal, &pLocal->vecUnknown5, 1.7f);
                    sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->vecUnknown5);
                    return 1;
                }
            }
            else if ( pLocal->pTargetThing )
            {
                sithAIMove_AISetLookThing(pLocal, pLocal->pTargetThing);
            }

            pState->aParams[1] = 0.0f;
            pState->aParams[2] = 0.0f;
            return 0;

        case SITHAI_EVENT_GOAL_UNREACHABLE:
            pState->aParams[1] = 0.0f;
            pState->aParams[2] = 0.0f;
            return 0;

        default:
            return 0;
    }
}

int J3DAPI sithAIInstinct_Roam(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_Roam, pLocal, pInstinct, pState, event, pObject);

    float moveSpeed = 0.75f;
    int bAllowPitch = 0;
    if ( (pLocal->mode & (SITHAI_MODE_FLEEING | SITHAI_MODE_ACTIVE | SITHAI_MODE_UNKNOWN_10 | SITHAI_MODE_ATTACKING)) != 0 )
    {
        return 0;
    }

    SithAIMode prevMode = (SithAIMode)(intptr_t)pObject;
    if ( event == SITHAI_EVENT_MODECHANGED && prevMode
        && (pLocal->mode & SITHAI_MODE_SEARCHING) != 0 && (prevMode & SITHAI_MODE_SEARCHING) == 0 )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + pInstinct->intArg[0];
        return 0;
    }

    if ( (pLocal->mode & SITHAI_MODE_TRAVERSEWPNTS) != 0 )
    {
        return 0;
    }

    // fltArg[1] > 0 roams around the home position within that distance, otherwise around the current position
    float minDist  = pLocal->pOwner->collide.movesize + 0.03f;
    float roamDist = pInstinct->fltArg[1];
    float maxDist  = roamDist < 0.0f ? -roamDist : roamDist;
    float moveDist = SITHAIINSTINCT_RANDF() * (maxDist - minDist) + minDist;
    if ( pInstinct->fltArg[3] != 0.0f )
    {
        moveSpeed = pInstinct->fltArg[3];
    }

    rdVector3 center;
    rdVector_Copy3(&center, roamDist > 0.0f ? &pLocal->homePos : &pLocal->pOwner->pos);
    if ( pInstinct->fltArg[2] != 0.0f )
    {
        bAllowPitch = 1;
    }

    rdVector3 movePos;
    switch ( (int)event )
    {
        case SITHAI_EVENT_TOUCHED:
        case SITHAI_EVENT_HIT_WALL:
        case SITHAI_EVENT_HIT_CLIFF:
        {
            // Turn back
            rdVector3 heading;
            sithAIUtil_GetXYHeadingVector(pLocal->pOwner, &heading);
            rdVector_Neg3Acc(&heading);
            if ( sithAIUtil_MakePathPos(pLocal, &pLocal->pOwner->pos, minDist, moveDist, 45.0f, 0x20C002, &heading, &movePos) )
            {
                sithAIMove_StopAIMovement(pLocal);
                if ( sithAIMove_AISetMovePos(pLocal, &movePos, moveSpeed) )
                {
                    sithAIMove_AISetLookPosEyeLevel(pLocal, &movePos);
                    return 1;
                }
            }

            break;
        }

        case SITHAI_EVENT_GOAL_REACHED:
            if ( (pLocal->submode & (SITHAI_SUBMODE_CONTINUOUSMOTION | SITHAI_SUBMODE_SEMICONTINUOUSMOTION)) == 0 )
            {
                return 0;
            }

            // fallthrough

        case SITHAIINSTINCT_EVENT_UPDATE:
        {
            pState->msecNextUpdate = sithTime_g_msecGameTime + pInstinct->intArg[0];
            if ( (pLocal->mode & SITHAI_MODE_INSTINCTUSEWPNTS) != 0 && sithAIUtil_AIMoveToNextWpnt(pLocal, moveSpeed, 0.0f, 1) )
            {
                return 0;
            }

            if ( roamDist > 0.0f )
            {
                rdVector3 fromHome;
                rdVector_Sub3(&fromHome, &pLocal->pOwner->pos, &pLocal->homePos);
                if ( sqrtf(fromHome.x * fromHome.x + fromHome.y * fromHome.y + fromHome.z * fromHome.z) > roamDist )
                {
                    // Too far, head home
                    sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->homePos);
                    sithAIMove_AISetMovePos(pLocal, &pLocal->homePos, moveSpeed);
                    return 0;
                }
            }

            break;
        }

        default:
            return 0;
    }

    if ( sithAIUtil_sub_49F010(pLocal, &center, minDist, moveDist, bAllowPitch, 2, &movePos) && sithAIMove_AISetMovePos(pLocal, &movePos, moveSpeed) )
    {
        sithAIMove_AISetLookPosEyeLevel(pLocal, &movePos);
        return 1;
    }

    if ( sithAIMove_AISetMovePos(pLocal, &pLocal->homePos, moveSpeed) )
    {
        sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->homePos);
        return 1;
    }

    return 0;
}

int J3DAPI sithAIInstinct_ReturnHome(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_ReturnHome, pLocal, pInstinct, pState, event, pObject);

    if ( event == SITHAI_EVENT_MODECHANGED )
    {
        SithAIMode prevMode = (SithAIMode)(intptr_t)pObject;
        if ( (pLocal->mode & SITHAI_MODE_SEARCHING) != 0 && (prevMode & SITHAI_MODE_SEARCHING) == 0 )
        {
            sithAIMove_AISetMovePos(pLocal, &pLocal->homePos, 1.2f);
            sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->homePos);
        }
    }
    else if ( event == SITHAI_EVENT_GOAL_REACHED && (pLocal->mode & SITHAI_MODE_SEARCHING) != 0 )
    {
        // Face the home direction
        rdVector3 lookPos;
        rdVector_Add3(&lookPos, &pLocal->homeOrient, &pLocal->pOwner->pos);
        sithAIMove_AISetLookPosEyeLevel(pLocal, &lookPos);
    }

    return 0;
}

int J3DAPI sithAIInstinct_Flee(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_Flee, pLocal, pInstinct, pState, event, pObject);

    // aParams[0]: game time (sec) when fleeing started
    float secFleeTime = pInstinct->fltArg[2];
    float minDist     = pLocal->pOwner->collide.movesize + 0.03f;
    float fleeDist    = pInstinct->fltArg[0];
    float moveSpeed   = 1.5f;
    bool bJustStarted = false;

    if ( (pLocal->mode & SITHAI_MODE_FLEEING) == 0 )
    {
        if ( pState->aParams[0] > 0.0f )
        {
            return sithAIInstinct_StopFleeing(pLocal, pState);
        }

        return 0;
    }

    pLocal->goalThing    = NULL;
    pLocal->pTargetThing = NULL;
    if ( pState->aParams[0] == 0.0f )
    {
        pState->aParams[0] = sithTime_g_secGameTime;
        bJustStarted = true;
    }

    if ( secFleeTime == 0.0f )
    {
        secFleeTime = (pLocal->mode & SITHAI_MODE_FLEEINGTOWAYPOINT) != 0 ? 25.1f : 10.0f;
    }

    if ( !pLocal->pFleeFromThing || pState->aParams[0] + secFleeTime < sithTime_g_secGameTime )
    {
        return sithAIInstinct_StopFleeing(pLocal, pState);
    }

    if ( pInstinct->fltArg[4] != 0.0f )
    {
        moveSpeed = pInstinct->fltArg[4];
    }

    pState->msecNextUpdate = sithTime_g_msecGameTime + (pInstinct->intArg[1] ? pInstinct->intArg[1] : 5000);

    rdVector3 fleeDir;
    float distance;
    if ( sithAIUtil_GetDistanceToTarget(pLocal->pOwner, &pLocal->pOwner->pos, pLocal->pFleeFromThing, -1.0f, fleeDist, 0.0f, &fleeDir, &distance) != 0 && !bJustStarted )
    {
        // Out of the threat's sight
        if ( (pLocal->submode & SITHAI_SUBMODE_CONTINUOUSWPNTMOTION) != 0 )
        {
            return 0;
        }

        return sithAIInstinct_StopFleeing(pLocal, pState);
    }

    bool bAwayFromThreat;
    switch ( (int)event )
    {
        case SITHAI_EVENT_TOUCHED:
        {
            const SithThing* pThing = (const SithThing*)pObject;
            if ( pThing && pThing->type == SITH_THING_PLAYER )
            {
                return 1;
            }

            bAwayFromThreat = false;
            break;
        }

        case SITHAIINSTINCT_EVENT_UPDATE:
        case SITHAI_EVENT_GOAL_REACHED:
            bAwayFromThreat = true;
            break;

        case SITHAI_EVENT_MODECHANGED:
        {
            SithAIMode prevMode = (SithAIMode)(intptr_t)pObject;
            if ( !prevMode || (pLocal->mode & SITHAI_MODE_FLEEING) == 0 || (prevMode & SITHAI_MODE_FLEEING) != 0 )
            {
                return 0;
            }

            bAwayFromThreat = true;
            break;
        }

        default:
            bAwayFromThreat = false;
            break;
    }

    if ( bAwayFromThreat )
    {
        rdVector_Neg3Acc(&fleeDir);
    }
    else
    {
        // Keep going in the current direction
        sithAIUtil_GetXYHeadingVector(pLocal->pOwner, &fleeDir);
        pLocal->mode &= ~SITHAI_MODE_TRAVERSEWPNTS;
    }

    if ( (pLocal->mode & SITHAI_MODE_INSTINCTUSEWPNTS) != 0 )
    {
        if ( (pLocal->mode & SITHAI_MODE_TRAVERSEWPNTS) != 0 || sithAIUtil_AIMoveToNextWpnt(pLocal, moveSpeed, 0.0f, 0) )
        {
            return 1;
        }
    }

    sithAIMove_StopAIMovement(pLocal);

    int pathFlags = 0x21002;
    if ( pInstinct->fltArg[5] > 0.0f )
    {
        minDist = pInstinct->fltArg[5];
    }

    float angle = pInstinct->fltArg[3];
    if ( angle != 0.0f )
    {
        pathFlags = angle < 0.0f ? 0x2D002 : 0x29002;
    }

    rdVector3 fleePos;
    if ( (sithAIUtil_MakePathPos(pLocal, &pLocal->pOwner->pos, minDist, fleeDist, angle, pathFlags, &fleeDir, &fleePos) && sithAIMove_AISetMovePos(pLocal, &fleePos, moveSpeed))
        || (sithAIUtil_MakePathPos(pLocal, &pLocal->pOwner->pos, minDist, fleeDist, angle, 0x29001, &fleeDir, &fleePos) && sithAIMove_AISetMovePos(pLocal, &fleePos, moveSpeed)) )
    {
        sithAIMove_AISetLookPosEyeLevel(pLocal, &fleePos);
        return 1;
    }

    return sithAIInstinct_StopFleeing(pLocal, pState);
}

int J3DAPI sithAIInstinct_BasicFallow(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject)
{
    INDY_AB_ORIGINAL(sithAIInstinct_BasicFallow, pLocal, pInstinct, pState, event, pObject);

    // aParams[0]: number of moves around obstacles, aParams[1]: 1 after bumping into something
    rdVector3 heading;
    rdVector_Zero3(&heading);

    SITH_ASSERTREL(pLocal && pLocal->pClass && pLocal->pOwner);
    SithThing* pOwner = pLocal->pOwner;
    if ( (pLocal->mode & (SITHAI_MODE_UNKNOWN_1000000 | SITHAI_MODE_FLEEING | SITHAI_MODE_UNKNOWN_10)) != 0 )
    {
        return 0;
    }

    if ( (pLocal->mode & SITHAI_MODE_CIRCLESTRAFING) != 0 )
    {
        if ( (pLocal->mode & SITHAI_MODE_MOVING) != 0 )
        {
            return 0;
        }

        pLocal->mode &= ~SITHAI_MODE_CIRCLESTRAFING;
    }

    if ( (pLocal->mode & SITHAI_MODE_TRAVERSEWPNTS) != 0 )
    {
        return 0;
    }

    if ( (event & (SITHAI_EVENT_HIT_THING | SITHAI_EVENT_HIT_WALL | SITHAI_EVENT_TOUCHED)) != 0 )
    {
        if ( (pLocal->mode & SITHAI_MODE_SEARCHING) != 0 || (pLocal->mode & (SITHAI_MODE_ACTIVE | SITHAI_MODE_MOVING)) == 0 )
        {
            return 0;
        }

        if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_4) != 0 )
        {
            return 1;
        }

        sithAIUtil_GetXYHeadingVector(pOwner, &heading);
        if ( pState->aParams[0] > 3.0f )
        {
            // Stuck, turn around
            rdVector_Neg3Acc(&heading);
            pState->aParams[0] = 0.0f;
            pState->aParams[1] = 0.0f;
            rdVector_Copy3(&pLocal->vecUnknown3, &pLocal->movePos);

            rdVector3 backPos;
            rdVector_ScaleAdd3(&backPos, &heading, pOwner->collide.movesize * 4.0f, &pOwner->pos);
            if ( !sithAIUtil_sub_49EE50(pLocal, &backPos, 0.0f, 60.0f, 30.0f, 0x100) )
            {
                return 0;
            }

            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_8;
            return 1;
        }
    }

    if ( (event & (SITHAI_EVENT_HIT_THING | SITHAI_EVENT_HIT_CLIFF | SITHAI_EVENT_HIT_WALL | SITHAI_EVENT_TOUCHED)) != 0 )
    {
        pState->aParams[1] = 1.0f;
    }

    if ( pInstinct->intArg[5] && sithPuppet_IsAnyModeOnTrack(pOwner, SITHPUPPETSUBMODE_FIRE, SITHPUPPETSUBMODE_FIRE4) )
    {
        pState->msecNextUpdate = sithTime_g_msecGameTime + 500;
        return 0;
    }

    SithThing* pGoal = pLocal->goalThing;
    if ( !pGoal )
    {
        return 0;
    }

    sithAIUtil_sub_49B640(pLocal);
    switch ( (int)event )
    {
        case SITHAIINSTINCT_EVENT_UPDATE:
            if ( pInstinct->intArg[0] )
            {
                pState->msecNextUpdate = sithTime_g_msecGameTime + pInstinct->intArg[0];
            }

            return sithAIInstinct_FollowGoal(pLocal, pInstinct, pGoal);

        case SITHAI_EVENT_TOUCHED:
        case SITHAI_EVENT_HIT_THING:
        {
            const SithThing* pThing = (const SithThing*)pObject;
            if ( pThing )
            {
                if ( pThing->type == SITH_THING_PLAYER )
                {
                    return 0;
                }

                // Already moving away from it
                rdVector3 awayDir;
                rdVector_Set3(&awayDir, pOwner->pos.x - pThing->pos.x, pOwner->pos.y - pThing->pos.y, 0.0f);
                rdVector_Normalize3Acc(&awayDir);
                if ( rdVector_Dot3(&heading, &awayDir) > 0.0f )
                {
                    return 1;
                }
            }

            pState->aParams[0] += 1.0f;
            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_4;
            return sithAIUtil_sub_49EE50(pLocal, &pLocal->movePos, 45.0f, 135.0f, 45.0f, 0x100) != 0;
        }

        case SITHAI_EVENT_HIT_WALL:
        {
            const SithSurface* pSurface = (const SithSurface*)pObject;
            if ( !pSurface )
            {
                return 0;
            }

            rdVector3 wallNormal;
            rdVector_Set3(&wallNormal, stdMath_ClipNearZero(pSurface->face.normal.x), stdMath_ClipNearZero(pSurface->face.normal.y), 0.0f);
            rdVector_Normalize3Acc(&wallNormal);
            if ( pLocal->targetSightState2 == 0 )
            {
                // Keep sliding along the wall while heading towards the goal
                rdVector3 goalDir;
                rdVector_Set3(&goalDir, stdMath_ClipNearZero(pLocal->vecUnknown0.x), stdMath_ClipNearZero(pLocal->vecUnknown0.y), 0.0f);
                rdVector_Normalize3Acc(&goalDir);
                float goalDot = heading.x * goalDir.x + heading.y * goalDir.y;
                float wallDot = heading.x * wallNormal.x + heading.y * wallNormal.y;
                if ( goalDot > 0.70099998f && wallDot > -0.96600002f )
                {
                    return 1;
                }
            }

            if ( pLocal->pClass->rank < 0.5f && SITHAIINSTINCT_RANDF() > pLocal->pClass->rank )
            {
                return 0;
            }

            pState->aParams[0] += 1.0f;
            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_4;

            rdVector3 movePos;
            bool bPathFound = false;
            if ( pLocal->targetSightState2 == 0 )
            {
                float moveDist = pOwner->collide.movesize;
                bPathFound = (sithAIUtil_CheckPathToPos(pLocal, pOwner->pInSector, &pOwner->pos, 0x2120, moveDist, moveDist + moveDist, 0.0f, &wallNormal, &movePos) & 2) == 0;
            }

            if ( !bPathFound )
            {
                float minDist  = pOwner->collide.movesize > 0.089999996f ? pOwner->collide.movesize : 0.089999996f;
                float moveDist = SITHAIINSTINCT_RANDF() * 0.1f + minDist;
                if ( !sithAIUtil_MakePathPos(pLocal, &pOwner->pos, minDist, moveDist, 45.0f, 0x8102, &wallNormal, &movePos) )
                {
                    return 0;
                }

                sithAIMove_StopAIMovement(pLocal);
            }

            sithAIMove_AISetLookPosEyeLevel(pLocal, &movePos);
            sithAIMove_AISetMoveTargetPos(pLocal, &movePos, 1.0f);
            return 1;
        }

        case SITHAI_EVENT_GOAL_REACHED:
        {
            bool bWasBlocked = pState->aParams[1] == 1.0f;
            pState->aParams[0] = 0.0f;
            if ( bWasBlocked )
            {
                pState->aParams[1] = 0.0f;
                sithAIMove_AISetLookPosEyeLevel(pLocal, &pGoal->pos);
            }

            if ( (pLocal->mode & SITHAI_MODE_SEARCHING) != 0
                || (pLocal->submode & (SITHAI_SUBMODE_CONTINUOUSMOTION | SITHAI_SUBMODE_SEMICONTINUOUSMOTION)) == 0 )
            {
                return 0;
            }

            return sithAIInstinct_FollowGoal(pLocal, pInstinct, pGoal);
        }

        default:
            return 0;
    }
}

int J3DAPI sithAIInstinct_WallCrawl(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithSurface* pSurface)
{
    INDY_AB_ORIGINAL(sithAIInstinct_WallCrawl, pLocal, pInstinct, pState, event, pSurface);

    SITH_ASSERTREL(pLocal && pLocal->pClass && pLocal->pOwner);
    SithThing* pOwner = pLocal->pOwner;
    if ( (event & (SITHAI_EVENT_HIT_FLOOR | SITHAI_EVENT_HIT_WALL)) != 0 && (pLocal->mode & (SITHAI_MODE_ACTIVE | SITHAI_MODE_MOVING)) == 0 )
    {
        return 0;
    }

    float maxStepHeight = pOwner->collide.movesize + 0.03f;
    rdVector3 movePos;
    if ( (pLocal->mode & (SITHAI_MODE_ACTIVE | SITHAI_MODE_ATTACKING)) != 0 )
    {
        sithAIUtil_sub_49B640(pLocal);
        sithAIUtil_AIGetMovePos(pLocal, 0x100, &movePos);
    }
    else
    {
        sithAIUtil_AIGetMovePos(pLocal, 0x400, &movePos);
    }

    float moveHeight = movePos.z - pOwner->pos.z;

    bool bMount = false;
    switch ( (int)event )
    {
        case SITHAIINSTINCT_EVENT_UPDATE:
            if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) != 0 )
            {
                // Back on the floor
                if ( (pOwner->attach.flags & SITH_ATTACH_SURFACE) != 0 && (pOwner->attach.attachedToStructure.pSurfaceAttached->flags & SITH_SURFACE_ISFLOOR) != 0 )
                {
                    sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_WALLCRAWLING);
                }

                pState->msecNextUpdate = sithTime_g_msecGameTime + 500;
                return 1;
            }

            break;

        case SITHAI_EVENT_TOUCHED:
        case SITHAI_EVENT_HIT_THING:
            if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) != 0 )
            {
                return 1;
            }

            break;

        case SITHAI_EVENT_HIT_WALL:
            if ( (pLocal->submode & (SITHAI_SUBMODE_WALLCRAWLLOCKED | SITHAI_SUBMODE_UNKNOWN_4)) != 0 )
            {
                return 1;
            }

            if ( !pSurface )
            {
                break;
            }

            // Climb walls when the move position is higher than a step
            if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) == 0 )
            {
                if ( moveHeight <= maxStepHeight )
                {
                    break;
                }

                sithAIUtil_AISetMode(pLocal, SITHAI_MODE_WALLCRAWLING);
            }

            bMount = true;
            break;

        case SITHAI_EVENT_HIT_CLIFF:
            if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) != 0 )
            {
                sithAIMove_Unreachable(pLocal);
                return 1;
            }

            return 0;

        case SITHAI_EVENT_HIT_FLOOR:
        case SITHAI_EVENT_LAND_FLOOR:
            if ( !pSurface )
            {
                break;
            }

            if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_400000) != 0 )
            {
                pLocal->mode &= ~SITHAI_MODE_UNKNOWN_400000;
                return 1;
            }

            if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) != 0 )
            {
                sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_WALLCRAWLING);
                bMount = true;
            }

            break;

        default:
            return 0;
    }

    if ( bMount )
    {
        if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_4) != 0 )
        {
            return 1;
        }

        // Mount the surface when facing it
        const rdVector3* pLook   = &pOwner->orient.lvec;
        const rdVector3* pNormal = &pSurface->face.normal;
        if ( (pLook->y * pNormal->y + pLook->z * pNormal->z) + pLook->x * pNormal->x < 0.0f )
        {
            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_4;
            rdVector_Copy3(&pLocal->vecUnknown3, pNormal);
            if ( !sithAIMove_AISpecialMove(pLocal, SITHACTORSPECIALMOVE_MOUNTWALL) )
            {
                sithThing_AttachThingToSurface(pOwner, pSurface, 1);
            }
        }

        return 1;
    }

    if ( (pLocal->mode & (SITHAI_MODE_UNKNOWN_1000000 | SITHAI_MODE_CIRCLESTRAFING | SITHAI_MODE_FLEEING | SITHAI_MODE_UNKNOWN_10 | SITHAI_MODE_SEARCHING)) != 0 )
    {
        return 0;
    }

    return sithAIInstinct_BasicFallow(pLocal, pInstinct, pState, event, pSurface);
}

void J3DAPI sithAIInstinct_sub_494360(SithAIControlBlock* pLocal, rdVector3* pMovePos, rdVector3* pAltMovePos)
{
    INDY_AB_ORIGINAL_VOID(sithAIInstinct_sub_494360, pLocal, pMovePos, pAltMovePos);

    // Finds a reachable snake move position: pMovePos, then the alternative position, then around the current heading.
    // The result is written to pMovePos.
    SithThing* pOwner = pLocal->pOwner;

    rdVector3 delta;
    rdVector3 dir;
    rdVector_Sub3(&delta, pMovePos, &pOwner->pos);
    float distance = rdVector_Normalize3(&dir, &delta);
    if ( sithAIInstinct_SnakeMungeTestCheck(pLocal, &dir, distance, pMovePos) )
    {
        return;
    }

    rdVector_Sub3(&delta, pAltMovePos, &pOwner->pos);
    distance = rdVector_Normalize3(&dir, &delta);
    if ( sithAIInstinct_SnakeMungeTestCheck(pLocal, &dir, distance, pMovePos) )
    {
        return;
    }

    // Try directions around the heading, alternating sides with growing yaw (15, -15, 30, -30, ...)
    int side = SITHAIINSTINCT_RANDF() - 0.5f < 0.0f ? -1 : 1;
    rdVector_Copy3(&dir, &pOwner->orient.lvec);

    rdVector3 pyr;
    rdVector_Zero3(&pyr);
    float yaw = 0.0f;
    for ( int i = 0; i <= 24; ++i )
    {
        bool bFound = sithAIInstinct_SnakeMungeTestCheck(pLocal, &dir, 0.3f, pMovePos) != 0;
        if ( yaw == 0.0f )
        {
            yaw = (float)side * 15.0f;
        }
        else if ( (yaw < 0.0f ? -1 : 1) == side )
        {
            yaw = -yaw;
        }
        else
        {
            yaw = (float)side * 15.0f - yaw;
        }

        pyr.yaw = yaw;
        rdVector_Rotate3(&dir, &pOwner->orient.lvec, &pyr);
        if ( bFound )
        {
            return;
        }
    }

    rdVector_Copy3(pMovePos, &pOwner->pos);
}

int J3DAPI sithAIInstinct_SnakeMungeTestCheck(SithAIControlBlock* pLocal, rdVector3* pDirection, float distance, rdVector3* pOutPoint)
{
    INDY_AB_ORIGINAL(sithAIInstinct_SnakeMungeTestCheck, pLocal, pDirection, distance, pOutPoint);

    // Checks whether the snake can move distance along pDirection; pOutPoint receives the end point
    // (shortened to the obstacle when the way is partly blocked)
    SithThing* pOwner = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pOwner) )
    {
        return 0;
    }

    float fudgeHeight = 0.0f;
    if ( pOwner->userblock.pQuetz->bPosFudged )
    {
        fudgeHeight = pOwner->userblock.pQuetz->unknown393;
    }

    rdVector_ScaleAdd3(pOutPoint, pDirection, distance, &pOwner->pos);

    rdVector3 startPos;
    rdVector_Copy3(&startPos, &pOwner->pos);
    startPos.z = (0.3f - sithPhysics_GetThingHeight(pOwner) + 0.002f - fudgeHeight) + startPos.z;

    rdVector3 testPos;
    rdVector_Copy3(&testPos, &startPos);
    SithSector* pSector = sithCollision_FindSectorInRadius(pOwner->pInSector, &pOwner->pos, &startPos, 0.0f);
    if ( !pSector )
    {
        SITHLOG_ERROR("_SnakeMungeTestCheck - couldn't find sector of start pointt");
        return 0;
    }

    // The start position is clipped when it can't be reached from the snake's position
    float dx = stdMath_ClipNearZero(testPos.x - startPos.x);
    float dy = stdMath_ClipNearZero(testPos.y - startPos.y);
    float dz = stdMath_ClipNearZero(testPos.z - startPos.z);
    if ( dx != 0.0f || dy != 0.0f || dz != 0.0f )
    {
        SITHLOG_ERROR("_SnakeMungeTestCheck - no LOS to start point");
        return 0;
    }

    // Note: The original ORs the raw bits of the float distance into the path flags
    uint32_t distanceBits;
    memcpy(&distanceBits, &distance, sizeof(distanceBits));

    rdVector3 hitPos;
    int pathResult = sithAIUtil_CheckPathToPos(pLocal, pSector, &startPos, (int)(distanceBits | 0x62010), 0.0f, distance + 0.05f, 0.3f, pDirection, &hitPos);
    if ( pathResult == 0 )
    {
        return 0;
    }

    if ( pathResult == 1 )
    {
        return 1;
    }

    // Blocked further away: stop short of the obstacle
    float hitDx = startPos.x - hitPos.x;
    float hitDy = startPos.y - hitPos.y;
    float moveDist = sqrtf(hitDx * hitDx + hitDy * hitDy) - 0.05f;
    if ( moveDist < 0.2f )
    {
        return 0;
    }

    rdVector_ScaleAdd3(pOutPoint, pDirection, moveDist, &pOwner->pos);
    return 1;
}

signed int J3DAPI sithAIInstinct_SnakeFollow(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing)
{
    INDY_AB_ORIGINAL(sithAIInstinct_SnakeFollow, pLocal, pInstinct, pState, event, pThing);

    // aParams[0]: 1 while the goal is out of sight
    SITH_ASSERTREL(pLocal && pLocal->pClass && pLocal->pOwner);
    SithThing* pOwner = pLocal->pOwner;
    bool bFollow = true;
    switch ( (int)event )
    {
        case SITHAIINSTINCT_EVENT_UPDATE:
            if ( (pLocal->mode & SITHAI_MODE_MOVING) != 0 )
            {
                bFollow = false;
            }

            break;

        case SITHAI_EVENT_TOUCHED:
            if ( pThing->type == SITH_THING_PLAYER )
            {
                bFollow = false;
            }

            break;

        case SITHAI_EVENT_HIT_WALL:
        case SITHAI_EVENT_HIT_CLIFF:
        case SITHAI_EVENT_HIT_THING:
            if ( (pLocal->mode & SITHAI_MODE_MOVING) == 0 || (pLocal->mode & SITHAI_MODE_ACTIVE) == 0 )
            {
                return 1;
            }

            break;

        case SITHAI_EVENT_GOAL_REACHED:
            break;

        default:
            return 0;
    }

    if ( !pLocal->goalThing )
    {
        pLocal->submode &= ~SITHAI_SUBMODE_CONTINUOUSMOTION;
        return 0;
    }

    pState->msecNextUpdate = sithTime_g_msecGameTime + 500;
    sithAIUtil_sub_49B640(pLocal);
    sithAIUtil_sub_49B2E0(pLocal, 0);

    rdVector3 toGoal;
    float goalDist;
    if ( pLocal->targetSightState2 == 0 || pLocal->targetSightState2 == 2 )
    {
        pState->aParams[0] = 0.0f;
        rdVector_Copy3(&toGoal, &pLocal->vecUnknown0);
        goalDist = pLocal->targetDistance;
        pLocal->mode |= SITHAI_MODE_LOSTSIGHTOFGOAL;
    }
    else
    {
        pState->aParams[0] = 1.0f;
        rdVector_Sub3(&toGoal, &pLocal->vecUnknown5, &pLocal->pOwner->pos);
        goalDist = rdVector_Normalize3Acc(&toGoal);
        pLocal->mode &= ~SITHAI_MODE_LOSTSIGHTOFGOAL;
    }

    if ( bFollow )
    {
        const rdMatrix34* pOrient = &pLocal->pOwner->orient;
        float lookDot  = rdVector_Dot3(&pOrient->lvec, &toGoal);
        float rightDot = (pOrient->rvec.z * toGoal.z + pOrient->rvec.x * toGoal.x) + pOrient->rvec.y * toGoal.y;
        float angle    = rdMath_DeltaAngleNormalized(&pOrient->lvec, &toGoal, &rdroid_g_zVector3);
        if ( angle < 0.0f )
        {
            angle = -angle;
        }

        float nearDist;
        float farDist;
        if ( (pLocal->mode & SITHAI_MODE_ATTACKING) != 0 )
        {
            nearDist = 0.3f;
            farDist  = 0.8f;
        }
        else
        {
            nearDist = sithAIMove_g_flt_585468;
            farDist  = 0.65f;
        }

        // The head moves to headYaw/headDist relative to the heading, the tail position is the fallback
        float headYaw;
        float headDist;
        float tailYaw;
        float tailDist;
        if ( lookDot <= -0.98f )
        {
            // Goal behind, turn to a random side
            float side = SITHAIINSTINCT_RANDF() - 0.5f < 0.0f ? -1.0f : 1.0f;
            headDist = 0.25f;
            tailDist = 0.25f;
            headYaw  = side * 45.0f;
            tailYaw  = -(side * 45.0f);
        }
        else if ( lookDot < 0.5f )
        {
            headDist = 0.25f;
            tailDist = 0.25f;
            headYaw  = 45.0f;
            tailYaw  = 60.0f;
        }
        else if ( goalDist > farDist )
        {
            // Goal ahead and far: swing the head once facing it after the previous update
            if ( sithAIInstinct_flt_539AB8 < 0.5f )
            {
                headDist = 0.2f;
                tailDist = 0.2f;
                headYaw  = angle;
                tailYaw  = 0.0f;
            }
            else
            {
                headYaw = angle + 30.0f;
                if ( headYaw < 0.0f )
                {
                    headYaw = 0.0f;
                }
                else if ( headYaw > 60.0f )
                {
                    headYaw = 60.0f;
                }

                tailYaw  = angle - 10.0f;
                headDist = 0.5f;
                tailDist = 0.5f;
            }
        }
        else if ( goalDist > nearDist )
        {
            headYaw = angle;
            tailYaw = angle;
            if ( (pLocal->mode & SITHAI_MODE_ATTACKING) != 0 )
            {
                headDist = goalDist - 0.1f;
                if ( headDist < 0.1f )
                {
                    headDist = 0.1f;
                }
                else if ( headDist > goalDist )
                {
                    headDist = goalDist;
                }

                tailDist = goalDist - 0.45f;
            }
            else
            {
                headDist = goalDist;
                tailDist = goalDist - 0.05f;
            }

            if ( tailDist < 0.1f )
            {
                tailDist = 0.1f;
            }
            else if ( tailDist > goalDist )
            {
                tailDist = goalDist;
            }
        }
        else
        {
            headDist = goalDist + 0.1f;
            tailDist = goalDist + 0.1f;
            tailYaw  = angle + angle;
            headYaw  = -angle;
        }

        sithAIInstinct_flt_539AB8 = lookDot;
        if ( rightDot > 0.0f )
        {
            headYaw = -headYaw;
            tailYaw = -tailYaw;
        }

        rdVector3 pyr;
        rdVector3 dir;
        rdVector_Set3(&pyr, 0.0f, headYaw, 0.0f);
        rdVector_Rotate3(&dir, &pLocal->pOwner->orient.lvec, &pyr);

        rdVector3 headPos;
        rdVector_ScaleAdd3(&headPos, &dir, headDist, &pLocal->pOwner->pos);

        rdVector_Set3(&pyr, 0.0f, tailYaw, 0.0f);
        rdVector_Rotate3(&dir, &pLocal->pOwner->orient.lvec, &pyr);

        rdVector3 tailPos;
        rdVector_ScaleAdd3(&tailPos, &dir, tailDist, &pLocal->pOwner->pos);

        headPos.z = pLocal->pOwner->pos.z;
        tailPos.z = pLocal->pOwner->pos.z;
        sithAIInstinct_sub_494360(pLocal, &headPos, &tailPos);
        sithAIMove_AISetMovePos(pLocal, &headPos, 2.0f);

        rdVector3 lookPos;
        rdVector_Copy3(&lookPos, &pLocal->movePos);
        lookPos.z = pLocal->pOwner->thingInfo.actorInfo.eyeOffset.z + pLocal->movePos.z;
        sithAIMove_AISetLookPos(pLocal, &lookPos);

        rdVector3 toMovePos;
        rdVector_Sub3(&toMovePos, &pLocal->movePos, &pOwner->pos);
        const rdVector3* pRight = &pLocal->pOwner->orient.rvec;
        sithAIInstinct_sub_494FF0(pOwner, (pRight->z * toMovePos.z + pRight->x * toMovePos.x) + pRight->y * toMovePos.y);

        if ( pLocal->pOwner->userblock.pQuetz )
        {
            pLocal->pOwner->userblock.pQuetz->unknown391 = 1;
        }
    }

    pLocal->mode &= ~SITHAI_MODE_UNKNOWN_80;
    pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_100 | SITHAI_SUBMODE_CONTINUOUSMOTION | SITHAI_SUBMODE_UNKNOWN_400;
    return event == SITHAI_EVENT_HIT_WALL || event == SITHAI_EVENT_TOUCHED || event == SITHAI_EVENT_HIT_CLIFF || event == SITHAI_EVENT_HIT_THING;
}

void J3DAPI sithAIInstinct_sub_494FF0(const SithThing* pThing, float sideMove)
{
    INDY_AB_ORIGINAL_VOID(sithAIInstinct_sub_494FF0, pThing, sideMove);

    // Plays the snake's slither sound when its sideways direction changes
    float slitherDir;
    if ( (sideMove < 0.0f ? -sideMove : sideMove) < 0.1f )
    {
        slitherDir = 0.0f;
    }
    else
    {
        slitherDir = sideMove > 0.0f ? 1.0f : -1.0f;
    }

    if ( sithAIMove_g_flt_585464 == slitherDir )
    {
        return;
    }

    tSoundHandle hSnd = sithSound_Load(sithWorld_g_pCurrentWorld, slitherDir < 0.0f ? "olv_boss_slither1.wav" : "olv_boss_slither2.wav");
    if ( hSnd )
    {
        sithSoundMixer_PlaySoundThing(hSnd, pThing, 1.0f, 0.01f, 7.0f, SOUNDPLAY_THING_POS);
    }

    sithAIMove_g_flt_585464 = slitherDir;
}

static int sithAIInstinct_TargetLost(SithAIControlBlock* pLocal, const SithThing* pTarget)
{
    // Celebrate a kill made in sight
    if ( pTarget && (pTarget->flags & (SITH_TF_DESTROYED | SITH_TF_DYING)) != 0 && (pLocal->mode & SITHAI_MODE_TARGETVISIBLE) != 0 )
    {
        sithAIUtil_AIPlaySoundMode(pLocal, SITHSOUNDCLASS_VICTORY);
        sithPuppet_PlayMode(pLocal->pOwner, SITHPUPPETSUBMODE_VICTORY, NULL);
    }

    sithAIUtil_AISetMode(pLocal, SITHAI_MODE_SEARCHING);
    pLocal->pTargetThing = NULL;
    return 1;
}

static int sithAIInstinct_StopFleeing(SithAIControlBlock* pLocal, SithAIInstinctState* pState)
{
    if ( pLocal->pFleeFromThing )
    {
        sithAIMove_AISetLookPosEyeLevel(pLocal, &pLocal->pFleeFromThing->pos);
    }

    sithAIMove_StopAIMovement(pLocal);
    sithAIUtil_AISetMode(pLocal, SITHAI_MODE_SEARCHING);
    pState->aParams[0] = 0.0f;
    pLocal->pFleeFromThing = NULL;
    return 0;
}

static int sithAIInstinct_FollowGoal(SithAIControlBlock* pLocal, const SithAIInstinct* pInstinct, SithThing* pGoal)
{
    SithThing* pOwner = pLocal->pOwner;
    float moveSpeed = 1.0f;

    pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_2000000;
    if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_HUMAN) == 0 && (pGoal->moveInfo.physics.flags & SITHAIINSTINCT_VEHICLEFLAGS) != 0 )
    {
        return 0;
    }

    // Distance band to keep to the goal: back off under minDist (to idealDist when set), close in over maxDist
    float minDist;
    float maxDist;
    float idealDist;
    if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_100000) != 0 )
    {
        moveSpeed = 2.0f;
        minDist   = pInstinct->fltArg[1];
        maxDist   = pOwner->collide.movesize * 3.0f;
        if ( !(maxDist < minDist) )
        {
            maxDist = minDist;
        }

        idealDist = 0.0f;
    }
    else
    {
        minDist   = pInstinct->fltArg[1];
        maxDist   = pInstinct->fltArg[2];
        idealDist = pInstinct->fltArg[3];
        if ( sithWeapon_HasWeaponSelected(pOwner) )
        {
            maxDist   = sithWeapon_GetWeaponMaxAimDistance((SithWeaponId)pOwner->thingInfo.actorInfo.weaponInfo.curWeaponID) - 0.1f;
            idealDist = 0.0f;
        }

        if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_400000) != 0 )
        {
            minDist   = (float)(minDist * 0.5);
            maxDist   = (float)(maxDist * 0.5);
            idealDist = (float)(idealDist * 0.5);
        }
    }

    // Match the velocity of a moving goal within two radii
    const rdVector3* pGoalVelocity = &pLocal->goalThing->moveInfo.physics.velocity;
    SithAISubMode submode = pLocal->submode;
    if ( (submode & SITHAI_SUBMODE_USEMATCHVELOCITY) != 0
        && (pGoalVelocity->x != 0.0f || pGoalVelocity->y != 0.0f || pGoalVelocity->z != 0.0f)
        && pLocal->targetDistance >= 0.0f
        && pLocal->targetDistance <= pOwner->collide.movesize + pOwner->collide.movesize )
    {
        pLocal->mode |= SITHAI_MODE_UNKNOWN_8000;
    }
    else
    {
        pLocal->mode &= ~SITHAI_MODE_UNKNOWN_8000;
    }

    if ( pLocal->targetSightState2 != 0 && pLocal->targetSightState2 != 2 )
    {
        // Lost sight of the goal, chase it
        if ( (pLocal->mode & SITHAI_MODE_CHASE_GOAL) != 0 )
        {
            return 0;
        }

        if ( (pGoal->type == SITH_THING_PLAYER || pGoal->type == SITH_THING_ACTOR) && (pGoal->thingInfo.actorInfo.flags & SITH_AF_INVISIBLE) != 0 )
        {
            return 0;
        }

        if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_BLIND) != 0 )
        {
            return 0;
        }

        pLocal->mode |= SITHAI_MODE_CHASE_GOAL;
        return 1;
    }

    pLocal->mode &= ~SITHAI_MODE_CHASE_GOAL;
    if ( (submode & SITHAI_SUBMODE_CONTINUOUSWPNTMOTION) == 0 )
    {
        sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TRAVERSEWPNTS);
    }

    sithAIMove_AISetLookPosEyeLevel(pLocal, &pGoal->pos);

    float offset = 0.0f;
    if ( pLocal->targetDistance > maxDist )
    {
        offset = pLocal->targetDistance - maxDist;
    }
    else if ( pLocal->targetDistance < minDist )
    {
        offset = pLocal->targetDistance - (idealDist != 0.0f ? idealDist : minDist);
    }

    rdVector3 movePos;
    if ( offset == 0.0f )
    {
        pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_2000000;

        // Within the band only flying AIs move, to keep their height
        if ( (pOwner->moveInfo.physics.flags & SITH_PF_FLY) == 0 )
        {
            return 0;
        }

        float height = pOwner->collide.movesize * 0.5f + pGoal->pos.z;
        float dz     = pOwner->pos.z - height;
        if ( (dz < 0.0f ? -dz : dz) <= 0.05f )
        {
            return 0;
        }

        rdVector_Set3(&movePos, pOwner->pos.x, pOwner->pos.y, height);
    }
    else
    {
        if ( offset < 0.0f )
        {
            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_2000000;
        }

        rdVector_ScaleAdd3(&movePos, &pLocal->vecUnknown0, offset, &pOwner->pos);
        if ( (pOwner->moveInfo.physics.flags & SITH_PF_FLY) != 0 )
        {
            movePos.z = pOwner->collide.movesize + pGoal->pos.z;
        }
        else if ( (pOwner->flags & SITH_TF_SUBMERGED) != 0 )
        {
            movePos.z = pGoal->pos.z;
        }
    }

    rdVector3 toPos;
    float distance;
    int sightState = sithAIUtil_GetDistanceToTargetPos(pGoal, &pGoal->pos, &movePos, -1.0f, pLocal->pClass->sightDistance, 0.0f, &toPos, &distance);
    if ( pInstinct->fltArg[4] != 0.0f || sightState == 0 )
    {
        sithAIMove_AISetMovePos(pLocal, &movePos, moveSpeed);
        return 1;
    }

    if ( (pLocal->mode & SITHAI_MODE_WANTALLEVENTS) != 0 )
    {
        sithAI_EmitEvent(pLocal, SITHAI_EVENT_GOAL_UNREACHABLE, NULL);
    }

    return 0;
}

static int sithAIInstinct_MakeCombatMove(SithAIControlBlock* pLocal, const SithAIInstinct* pInstinct, SithAIInstinctState* pState, bool bCheckFire)
{
    float minDist  = pInstinct->fltArg[2] + 0.03f;
    float maxDist  = pInstinct->fltArg[3] + 0.03f;
    float moveDist = SITHAIINSTINCT_RANDF() * (maxDist - minDist) + minDist;

    float minSpeed  = pInstinct->fltArg[4];
    float moveSpeed = pInstinct->fltArg[5];
    int triedMask   = (int)pState->aParams[2];

    // Directions: 1 towards the target, 2 back off, 4 sideways
    int moveTry;
    if ( triedMask == 0 )
    {
        float choice = SITHAIINSTINCT_RANDF();
        if ( choice >= 0.0f && choice <= 0.4f )
        {
            moveTry = 1;
        }
        else if ( choice >= 0.5f && choice <= 0.8f )
        {
            moveTry = 2;
        }
        else
        {
            moveTry = 4;
        }
    }
    else if ( (triedMask & 1) == 0 )
    {
        moveTry = 1;
    }
    else if ( (triedMask & 2) == 0 )
    {
        moveTry = 2;
    }
    else if ( (triedMask & 4) == 0 )
    {
        moveTry = 4;
    }
    else
    {
        pState->aParams[2] = 0.0f;
        return 0;
    }

    SithThing* pOwner  = pLocal->pOwner;
    bool bLookAtMovePos = true;
    rdVector3 moveDir;
    float angle;
    int pathFlags;
    if ( moveTry == 1 )
    {
        rdVector_Copy3(&moveDir, &pLocal->toTarget);
        angle     = 0.0f;
        pathFlags = 0x8001;
    }
    else if ( moveTry == 2 )
    {
        if ( !pInstinct->intArg[1] && pLocal->distance > pOwner->collide.movesize * 4.0 )
        {
            return 1;
        }

        rdVector_Neg3(&moveDir, &pLocal->toTarget);
        bLookAtMovePos = false;
        moveSpeed      = minSpeed;
        angle          = 30.0f;
        pathFlags      = 0xC002;
    }
    else
    {
        if ( !pInstinct->intArg[1] )
        {
            return 1;
        }

        moveSpeed = SITHAIINSTINCT_RANDF() * (moveSpeed - minSpeed) + minSpeed;
        angle     = 30.0f;
        pathFlags = 0xC002;
        rdVector_Copy3(&moveDir, &pLocal->toTarget);
    }

    rdVector3 movePos;
    if ( sithAIUtil_MakePathPos(pLocal, &pLocal->pOwner->pos, minDist, moveDist, angle, pathFlags, &moveDir, &movePos)
        && (!bCheckFire || sithAIUtil_sub_49F1F0(pLocal, &movePos)) )
    {
        pState->aParams[1] = 0.0f;
        pState->aParams[2] = 0.0f;

        rdVector3 lookPos;
        if ( bLookAtMovePos )
        {
            rdVector_Copy3(&lookPos, &movePos);
        }
        else
        {
            // Back off facing the target: look at the move position mirrored through the AI
            lookPos.x = (pOwner->pos.x - movePos.x) + pOwner->pos.x;
            lookPos.y = (pOwner->pos.y - movePos.y) + pOwner->pos.y;
            lookPos.z = (pOwner->pos.z - movePos.z) + pOwner->pos.z;
        }

        sithAIMove_AISetLookPosEyeLevel(pLocal, &lookPos);
        sithAIMove_AISetMoveTargetPos(pLocal, &movePos, moveSpeed);
        return 0;
    }

    pState->aParams[2] = (float)(triedMask | moveTry);
    return 0;
}

static float sithAIInstinct_GetSpeed(const SithThing* pThing)
{
    const rdVector3* pVelocity = &pThing->moveInfo.physics.velocity;
    return sqrtf(pVelocity->x * pVelocity->x + pVelocity->y * pVelocity->y + pVelocity->z * pVelocity->z);
}
