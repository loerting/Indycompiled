#include "sithAIMove.h"
#include <j3dcore/j3dhook.h>

#include <rdroid/Math/rdMath.h>
#include <rdroid/Math/rdMatrix.h>
#include <rdroid/Math/rdVector.h>

#include <sith/AI/sithAI.h>
#include <sith/AI/sithAIUtil.h>
#include <sith/Cog/sithCog.h>
#include <sith/Engine/sithCollision.h>
#include <sith/Engine/sithPuppet.h>
#include <sith/Engine/sithPhysics.h>
#include <sith/Gameplay/sithPlayerActions.h>
#include <sith/Gameplay/sithTime.h>
#include <sith/RTI/symbols.h>
#include <sith/World/sithActor.h>
#include <sith/World/sithSoundClass.h>
#include <sith/World/sithTemplate.h>
#include <sith/World/sithThing.h>
#include <sith/World/sithWeapon.h>
#include <sith/World/sithWorld.h>

#include <std/General/stdMath.h>
#include <std/General/stdUtil.h>

#include <math.h>
#include <string.h>

static float sithAIMove_maxTurnSpeed = 0.0049999999f;
static float sithAIMove_maxWalkSpeed = 0.27000001f;
static float sithAIMove_minWalkSpeed = 0.001f;

// Quetzalcoatl strike aim: the target distance range and the joint pitches at either end of it
static const float sithAIMove_strikeMinDist          = 0.3f;
static const float sithAIMove_strikeMaxDist          = 0.5f;
static const float sithAIMove_strikeNearJoint1Pitch  = -15.0f;
static const float sithAIMove_strikeFarJoint1Pitch   = -60.0f;
static const float sithAIMove_strikeFarJoint2Pitch   = 60.0f; // the near one is sithAIMove_g_flt_585470

// Snapshots 1-13 of strike 3 (strike 5 uses the first 8): time, then the angles of the 5 strike joints
static const SithQuetzSnapShot sithAIMove_aStrikeSnapShots[13] = {
    { 0.1f, -10.0f,   0.0f, 50.0f,   0.0f, -40.0f },
    { 0.3f, -15.0f,  30.0f, 60.0f,  50.0f, -60.0f },
    { 0.6f, -12.0f, -30.0f, 55.0f, -50.0f, -50.0f },
    { 0.9f, -15.0f,  30.0f, 60.0f,  50.0f, -35.0f },
    { 1.2f, -12.0f, -30.0f, 55.0f, -50.0f, -50.0f },
    { 1.5f, -15.0f,  30.0f, 60.0f,  50.0f, -60.0f },
    { 1.8f, -12.0f, -30.0f, 55.0f, -50.0f, -50.0f },
    { 2.1f, -15.0f,  30.0f, 60.0f,  50.0f, -35.0f },
    { 2.4f, -12.0f, -30.0f, 55.0f, -50.0f, -50.0f },
    { 2.7f, -15.0f,  30.0f, 60.0f,  50.0f, -60.0f },
    { 3.0f, -12.0f, -30.0f, 55.0f, -50.0f, -50.0f },
    { 3.3f, -10.0f,   0.0f, 20.0f,   0.0f, -15.0f },
    { 3.7f,   0.0f,   0.0f,  5.0f,   0.0f, -10.0f },
};

// Debug vars, for debugging AISetMoveToPos
static SithThing* sithAIMove_pMoveToDebugMarkThing      = NULL;
static SithThing* sithAIMove_pMoveToPosGroundMarkThing  = NULL;;
static bool sithAIMove_bDebugMoveToPo = false;

void J3DAPI sithAIMove_AIFinalizeSpecialMove(SithAIControlBlock* pLocal, SithActorSpecialMoveFlags type);

static void sithAIMove_ProcessWeaponAim(SithThing* pThing, float secDeltaTime);
static float sithAIMove_GetTurnStep(const SithThing* pThing, float angle, float secDeltaTime);
static void sithAIMove_TurnStep(SithThing* pThing, float angle, float turnStep);
static void sithAIMove_AlignLook(SithThing* pThing, const rdVector3* pLookDir);
static void sithAIMove_SetTurnMoveStatus(SithThing* pThing, float angle);
static void sithAIMove_SetMineCarGunYaw(const SithThing* pThing, rdVector3* pGunPYR, const rdVector3* pDir);

// The strike fields of SithQuetzUserBlock are unnamed in types.h: strike.unknown0 is the running strike (0 = none),
// strike.unknown1 the strike time, strike.unknown3 the number of snapshots and aSnapShots[].unknown0 the snapshot time.
// angle1-5 are joint 1 pitch and yaw, joint 2 pitch and yaw, and joint 28 pitch.
static void sithAIMove_StartStrike(SithQuetzStrike* pStrike, const SithThing* pThing);
static void sithAIMove_GetStrikePose(SithQuetzSnapShot* pSnap, const SithThing* pThing);
static void sithAIMove_SetStrikePose(SithThing* pThing, const SithQuetzSnapShot* pSnap);
static bool sithAIMove_PlayStrike(SithThing* pThing, SithQuetzStrike* pStrike, const SithQuetzSnapShot* pNext, float secDeltaTime, float secLeft);

void sithAIMove_InstallHooks(void)
{
    // Uncomment only lines for functions that have full definition and doesn't call original function (non-thunk functions)

    J3D_HOOKFUNC(sithAIMove_Update);
    J3D_HOOKFUNC(sithAIMove_UpdateMineCar);
    J3D_HOOKFUNC(sithAIMove_sub_4958B0);
    J3D_HOOKFUNC(sithAIMove_sub_495CD0);
    J3D_HOOKFUNC(sithAIMove_sub_4961A0);
    J3D_HOOKFUNC(sithAIMove_sub_496200);
    J3D_HOOKFUNC(sithAIMove_sub_496550);
    J3D_HOOKFUNC(sithAIMove_sub_4966D0);
    J3D_HOOKFUNC(sithAIMove_sub_496820);
    J3D_HOOKFUNC(sithAIMove_AIGetMoveState);
    J3D_HOOKFUNC(sithAIMove_AISpecialTurn);
    J3D_HOOKFUNC(sithAIMove_UpdateAIMove);
    J3D_HOOKFUNC(sithAIMove_GetAIMoveModes);
    J3D_HOOKFUNC(sithAIMove_SetSubMode);
    J3D_HOOKFUNC(sithAIMove_PuppetCallback);
    J3D_HOOKFUNC(sithAIMove_AISetLookThing);
    J3D_HOOKFUNC(sithAIMove_AISetLookPos);
    J3D_HOOKFUNC(sithAIMove_AISetLookPosEyeLevel);
    J3D_HOOKFUNC(sithAIMove_AISetMovePos);
    J3D_HOOKFUNC(sithAIMove_AISetMoveTargetPos);
    J3D_HOOKFUNC(sithAIMove_AISpecialMove);
    J3D_HOOKFUNC(sithAIMove_AIFinalizeSpecialMove);
    J3D_HOOKFUNC(sithAIMove_AIJump);
    J3D_HOOKFUNC(sithAIMove_AIStop);
    J3D_HOOKFUNC(sithAIMove_SetGoalReached);
    J3D_HOOKFUNC(sithAIMove_Unreachable);
    J3D_HOOKFUNC(sithAIMove_StopAIMovement);
    J3D_HOOKFUNC(sithAIMove_ResetAILook);
    J3D_HOOKFUNC(sithAIMove_UpdateBoss);
    J3D_HOOKFUNC(sithAIMove_sub_499090);
    J3D_HOOKFUNC(sithAIMove_sub_4996C0);
    J3D_HOOKFUNC(sithAIMove_sub_499A80);
    J3D_HOOKFUNC(sithAIMove_sub_499CA0);
    J3D_HOOKFUNC(sithAIMove_sub_49A020);
    J3D_HOOKFUNC(sithAIMove_sub_49A1B0);
    J3D_HOOKFUNC(sithAIMove_sub_49A450);
    J3D_HOOKFUNC(sithAIMove_sub_49A630);
    J3D_HOOKFUNC(sithAIMove_sub_49A810);
    J3D_HOOKFUNC(sithAIMove_UpdateQuetzTail);
    J3D_HOOKFUNC(sithAIMove_sub_49AA60);
    J3D_HOOKFUNC(sithAIMove_sub_49AB80);
    J3D_HOOKFUNC(sithAIMove_sub_49AC50);
    J3D_HOOKFUNC(sithAIMove_sub_49AF80);
    J3D_HOOKFUNC(sithAIMove_sub_49B1B0);
    J3D_HOOKFUNC(sithAIMove_UpdateMardukTail);
}

void sithAIMove_ResetGlobals(void)
{
    memset(&sithAIMove_g_flt_585464, 0, sizeof(sithAIMove_g_flt_585464));
    memset(&sithAIMove_g_flt_585468, 0, sizeof(sithAIMove_g_flt_585468));
    memset(&sithAIMove_g_flt_58546C, 0, sizeof(sithAIMove_g_flt_58546C));
    memset(&sithAIMove_g_flt_585470, 0, sizeof(sithAIMove_g_flt_585470));
}

void J3DAPI sithAIMove_Update(SithThing* pThing, float secDeltatTime)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_Update, pThing, secDeltatTime);

    SITH_ASSERTREL(pThing);
    SITH_ASSERTREL(pThing->controlType == SITH_CT_AI);
    if ( pThing->type != SITH_THING_ACTOR || pThing->thingInfo.actorInfo.health <= 0.0f )
    {
        return;
    }

    SithAIControlBlock* pLocal = pThing->controlInfo.aiControl.pLocal;
    SITH_ASSERTREL(pLocal);

    if ( (pLocal->mode & SITHAI_MODE_SLEEPING) != 0
        && (pLocal->mode & SITHAI_MODE_BLOCK) == 0
        && (pThing->thingInfo.actorInfo.flags & SITH_AF_NOSLOPEMOVE) == 0 )
    {
        return;
    }

    if ( (pThing->flags & (SITH_TF_DISABLED | SITH_TF_DYING | SITH_TF_DESTROYED)) != 0 )
    {
        return;
    }

    // Look where we are walking to
    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_100) != 0
        && (pLocal->mode & SITHAI_MODE_MOVING) != 0
        && (pLocal->mode & SITHAI_MODE_DISABLED) == 0 )
    {
        rdVector3 lookPos = pLocal->movePos;
        lookPos.z = pLocal->pOwner->thingInfo.actorInfo.eyeOffset.z + lookPos.z;
        sithAIMove_AISetLookPos(pLocal, &lookPos);
    }

    // Turn the body
    const bool bTurning = (pLocal->mode & SITHAI_MODE_TURNING) != 0;
    if ( bTurning || (pLocal->submode & SITHAI_SUBMODE_BODYTRACKINGMOTION) != 0 )
    {
        if ( (pThing->thingInfo.actorInfo.flags & SITH_AF_BOSS) != 0 )
        {
            sithAIMove_UpdateBoss(pLocal, secDeltatTime);
        }
        else if ( pLocal->pOwner->moveType == SITH_MT_PHYSICS && (pThing->moveInfo.physics.flags & SITH_PF_MINECAR) != 0 )
        {
            sithAIMove_UpdateMineCar(pLocal, secDeltatTime);
        }
        else
        {
            if ( (pLocal->submode & SITHAI_SUBMODE_BODYTRACKINGMOTION) != 0 && pLocal->pTargetThing )
            {
                if ( bTurning
                    || (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_400) == 0
                    || fabsf(pThing->thingInfo.actorInfo.headPYR.y) > 20.0f )
                {
                    sithAIMove_AISetLookThing(pLocal, pLocal->pTargetThing);
                }
            }

            sithAIMove_sub_4958B0(pLocal, secDeltatTime);
        }
    }

    // Move
    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_10) != 0 )
    {
        if ( (pLocal->mode & SITHAI_MODE_TRAVERSEWPNTS) != 0 )
        {
            sithAIUtil_sub_49D170(pLocal);
        }
        else
        {
            sithAIMove_sub_4961A0(pLocal);
        }
    }

    if ( (pLocal->mode & SITHAI_MODE_MOVING) != 0 && (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_10) == 0 )
    {
        sithAIMove_sub_495CD0(pLocal, secDeltatTime);
    }

    if ( sithWeapon_HasWeaponSelected(pLocal->pOwner) )
    {
        sithAIMove_ProcessWeaponAim(pLocal->pOwner, secDeltatTime);
    }

    // Turn the head
    if ( !sithWeapon_IsAiming(pLocal->pOwner)
        && ((pLocal->mode & (SITHAI_MODE_UNKNOWN_80 | SITHAI_MODE_TURNING)) != 0 || (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_400) != 0) )
    {
        if ( ((pLocal->mode & SITHAI_MODE_BLOCK) != 0 || (pThing->thingInfo.actorInfo.flags & SITH_AF_NOSLOPEMOVE) != 0)
            && (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_400) == 0 )
        {
            sithAIMove_sub_4996C0(pLocal, secDeltatTime);
        }
        else
        {
            sithAIMove_sub_499090(pLocal, secDeltatTime);
        }
    }

    const SithThing* pAttached = pLocal->pOwner->pAttachedThing;
    if ( pAttached && (pAttached->attach.flags & SITH_ATTACH_TAIL) != 0 )
    {
        sithAIMove_UpdateQuetzTail(pLocal, secDeltatTime);
        return;
    }

    if ( strncmp(pThing->aName, "marduk", 6) == 0 )
    {
        sithAIMove_UpdateMardukTail(pThing, secDeltatTime);
    }
}

void J3DAPI sithAIMove_UpdateMineCar(SithAIControlBlock* pLocal, float secDeltaTime)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_UpdateMineCar, pLocal, secDeltaTime);

    // Aims the gun turret (the "redarmy" mesh) of an AI mine car at the look position
    SithThing* pThing = pLocal->pOwner;
    if ( (pThing->thingInfo.actorInfo.flags & SITH_AF_IMMOBILE) != 0 )
    {
        return;
    }

    // Note: no check for a model without a "redarmy" mesh (index -1)
    int meshNum = sithThing_GetThingMeshIndex(pThing, "redarmy");
    rdVector3 gunPos = pThing->renderData.paJointMatrices[meshNum].dvec;
    rdVector3* pGunPYR = &pThing->renderData.apTweakedAngles[meshNum];

    rdVector3 toLook;
    toLook.x = pLocal->lookPos.x - gunPos.x;
    toLook.y = pLocal->lookPos.y - gunPos.y;
    toLook.z = pLocal->lookPos.z - gunPos.z;

    // Current gun direction, flattened onto the thing's horizontal plane
    const rdVector3* pUp = &pThing->orient.uvec;
    rdVector3 gunDir;
    rdVector_Rotate3(&gunDir, &pThing->orient.lvec, pGunPYR);
    rdVector_Normalize3Acc(&gunDir);

    float dist = -((gunDir.y * pUp->y + gunDir.z * pUp->z) + gunDir.x * pUp->x);
    rdVector3 gunFlat;
    gunFlat.x = pUp->x * dist + gunDir.x;
    gunFlat.y = pUp->y * dist + gunDir.y;
    gunFlat.z = pUp->z * dist + gunDir.z;
    rdVector_Normalize3Acc(&gunFlat);

    // Note: unlike in sithAIMove_sub_4958B0 the vector tested here isn't normalized
    if ( fabsf((pUp->x * toLook.x + pUp->z * toLook.z) + pUp->y * toLook.y) > 0.999f )
    {
        if ( (pThing->moveInfo.physics.flags & SITH_PF_FLY) == 0 )
        {
            SITHLOG_ERROR("Strange... AI '%s' is looking straight up or down.\n", pThing->aName);
        }

        pLocal->mode &= ~SITHAI_MODE_TURNING;
        return;
    }

    dist = -((toLook.z * pUp->z + toLook.x * pUp->x) + toLook.y * pUp->y);
    rdVector3 lookFlat;
    lookFlat.x = pUp->x * dist + toLook.x;
    lookFlat.y = pUp->y * dist + toLook.y;
    lookFlat.z = pUp->z * dist + toLook.z;
    rdVector_Normalize3Acc(&lookFlat);

    float angle    = rdMath_DeltaAngleNormalized(&lookFlat, &gunFlat, pUp);
    float turnStep = sithAIMove_GetTurnStep(pThing, angle, secDeltaTime);

    // Note: the original tests a move state here like sithAIMove_sub_4958B0 does, but never sets it. The variable shares
    // the angle's stack slot, so it reads as zero only when the angle is exactly +0.0 (on the "looking up" path above
    // the slot holds pLocal, so the move status is left alone there).
    const bool bMoveStateZero = angle == 0.0f && !signbit(angle);

    if ( fabsf(angle) > turnStep * 1.05f )
    {
        rdVector3 pyr;
        pyr.x = 0.0f;
        pyr.y = angle < 0.0f ? turnStep : -turnStep;
        pyr.z = 0.0f;
        rdVector_Rotate3Acc(&gunFlat, &pyr);
        sithAIMove_SetMineCarGunYaw(pThing, pGunPYR, &gunFlat);

        if ( bMoveStateZero )
        {
            sithAIMove_SetTurnMoveStatus(pThing, angle);
        }

        return;
    }

    sithAIMove_SetMineCarGunYaw(pThing, pGunPYR, &lookFlat);

    pLocal->mode &= ~SITHAI_MODE_TURNING;
    if ( bMoveStateZero )
    {
        pThing->moveStatus = SITHPLAYERMOVE_STILL;
    }
}

void J3DAPI sithAIMove_sub_4958B0(SithAIControlBlock* pLocal, float secDeltatTime)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_4958B0, pLocal, secDeltatTime);

    // Turns the body toward the look direction (goalLVec)
    SithThing* pThing = pLocal->pOwner;
    rdVector3 goalDir = pLocal->goalLVec;
    if ( (pThing->thingInfo.actorInfo.flags & SITH_AF_IMMOBILE) != 0 )
    {
        return;
    }

    int moveState = sithAIMove_AIGetMoveState(pLocal);
    if ( moveState == 2 )
    {
        return;
    }

    if ( fabsf(rdVector_Dot3(&pThing->orient.uvec, &goalDir)) > 0.999f )
    {
        if ( (pThing->moveInfo.physics.flags & SITH_PF_FLY) == 0 )
        {
            SITHLOG_ERROR("Strange... AI '%s' is looking straight up or down.\n", pThing->aName);
        }
    }
    else
    {
        rdVector3 lookDir;
        rdMath_ProjectPointOntoPlaneNormalized(&lookDir, &goalDir, &pThing->orient.uvec, &rdroid_g_zeroVector3);

        float angle = rdMath_DeltaAngleNormalized(&lookDir, &pThing->orient.lvec, &pThing->orient.uvec);
        if ( sithAIMove_AISpecialTurn(pLocal, angle) )
        {
            return;
        }

        float turnStep = sithAIMove_GetTurnStep(pThing, angle, secDeltatTime);
        if ( fabsf(angle) > turnStep * 1.05f )
        {
            sithAIMove_TurnStep(pThing, angle, turnStep);
            if ( !moveState )
            {
                sithAIMove_SetTurnMoveStatus(pThing, angle);
            }

            return;
        }

        sithAIMove_AlignLook(pThing, &lookDir);
    }

    pLocal->mode &= ~SITHAI_MODE_TURNING;
    if ( !moveState )
    {
        pThing->moveStatus = SITHPLAYERMOVE_STILL;
    }
}

void J3DAPI sithAIMove_sub_495CD0(SithAIControlBlock* pLocal, float secDeltaTime)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_495CD0, pLocal, secDeltaTime);

    // Moves the AI toward movePos
    int bReached = 0;
    SITH_ASSERTREL(pLocal);

    SithThing* pThing = pLocal->pOwner;
    SITH_ASSERTREL(pThing && (pThing->type == SITH_THING_ACTOR));

    if ( (pThing->flags & (SITH_TF_DISABLED | SITH_TF_DYING | SITH_TF_DESTROYED)) != 0
        || (pThing->thingInfo.actorInfo.flags & SITH_AF_IMMOBILE) != 0
        || sithTime_g_msecGameTime < pLocal->msecPauseMoveUntil )
    {
        return;
    }

    pLocal->submode &= ~(SITHAI_SUBMODE_UNKNOWN_4000 | SITHAI_SUBMODE_UNKNOWN_4);

    rdVector3 curPos  = pThing->pos;
    rdVector3 movePos = pLocal->movePos;
    float speed       = pThing->thingInfo.actorInfo.maxThrust * secDeltaTime * pLocal->moveSpeed;
    float reachDist   = (float)sithAIMove_sub_496200(pLocal, &curPos, &movePos);

    pLocal->moveDirection.x = movePos.x - curPos.x;
    pLocal->moveDirection.y = movePos.y - curPos.y;
    pLocal->moveDirection.z = movePos.z - curPos.z;
    pLocal->moveDistance    = rdVector_Normalize3Acc(&pLocal->moveDirection);

    rdVector3 thrust;
    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_100) == 0 )
    {
        thrust.x = pLocal->moveDirection.x * speed;
        thrust.y = pLocal->moveDirection.y * speed;
        thrust.z = pLocal->moveDirection.z * speed;
    }
    else
    {
        // Walk where we look, slower the more the move direction is off to the side
        const rdVector3* pLook = &pThing->orient.lvec;
        float dot = (pLook->x * pLocal->moveDirection.x + pLook->z * pLocal->moveDirection.z) + pLook->y * pLocal->moveDirection.y;
        if ( dot < 0.3f )
        {
            dot = 0.3f;
        }
        else if ( dot > 1.0f )
        {
            dot = 1.0f;
        }

        speed    = dot * speed;
        thrust.x = pLook->x * speed;
        thrust.y = pLook->y * speed;
        thrust.z = speed * pLook->z;
    }

    if ( fabsf(thrust.x) <= 0.00001f )
    {
        thrust.x = 0.0f;
    }

    if ( fabsf(thrust.y) <= 0.00001f )
    {
        thrust.y = 0.0f;
    }

    if ( fabsf(thrust.z) <= 0.00001f )
    {
        thrust.z = 0.0f;
    }

    rdVector3* pVelocity = &pThing->moveInfo.physics.velocity;
    if ( (pLocal->mode & SITHAI_MODE_UNKNOWN_8000) != 0 && pLocal->goalThing )
    {
        // Match the speed of the goal thing
        rdVector3 goalVelocity = pLocal->goalThing->moveInfo.physics.velocity;
        float goalSpeed = rdVector_Normalize3Acc(&goalVelocity);

        rdVector3 dir = *pVelocity;
        rdVector_Normalize3Acc(&dir);
        pVelocity->x = goalSpeed * dir.x;
        pVelocity->y = goalSpeed * dir.y;
        pVelocity->z = goalSpeed * dir.z;
    }
    else
    {
        pVelocity->x = thrust.x + pVelocity->x;
        pVelocity->y = thrust.y + pVelocity->y;
        pVelocity->z = thrust.z + pVelocity->z;
    }

    if ( sithAIMove_AIGetMoveState(pLocal) == 2 )
    {
        memset(&pLocal->pOwner->moveInfo.physics.velocity, 0, sizeof(pLocal->pOwner->moveInfo.physics.velocity));
    }

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_100) != 0 )
    {
        float curSpeed = sqrtf(rdVector_Dot3(pVelocity, pVelocity));
        pVelocity->x = pThing->orient.lvec.x * curSpeed;
        pVelocity->y = pThing->orient.lvec.y * curSpeed;
        pVelocity->z = pThing->orient.lvec.z * curSpeed;
    }

    if ( (pLocal->mode & SITHAI_MODE_NOCHECKFORCLIFF) == 0 && (pVelocity->x != 0.0f || pVelocity->y != 0.0f || pVelocity->z != 0.0f) )
    {
        if ( (pThing->moveInfo.physics.flags & SITH_PF_FLY) == 0 && pThing->attach.flags )
        {
            bReached = sithAIMove_sub_496550(pLocal, secDeltaTime);
        }
        else if ( (pThing->moveInfo.physics.flags & SITH_PF_FLY) != 0 && (pThing->thingInfo.actorInfo.flags & SITH_AF_BREATHEUNDERWATER) == 0 )
        {
            bReached = sithAIMove_sub_4966D0(pLocal, secDeltaTime);
        }
        else if ( (pThing->flags & (SITH_TF_SUBMERGED | SITH_TF_AIRDESTROYED)) != 0 )
        {
            bReached = sithAIMove_sub_496820(pLocal, secDeltaTime);
        }

        if ( (pLocal->mode & SITHAI_MODE_MOVING) == 0 )
        {
            return;
        }
    }

    if ( pLocal->moveDistance <= pThing->collide.movesize * 3.0f )
    {
        sithAI_EmitEvent(pLocal, SITHAI_EVENT_GOAL_SET, NULL);
    }

    if ( !bReached && pLocal->moveDistance > reachDist )
    {
        return;
    }

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_2) != 0 )
    {
        sithAIMove_StopAIMovement(pLocal);
        sithAIMove_AISetMovePos(pLocal, &pLocal->vecUnknown3, pLocal->moveSpeed);
        return;
    }

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_8) != 0 )
    {
        int pathFlags = (pLocal->mode & SITHAI_MODE_ACTIVE) != 0 ? 0x100 : 0;
        if ( sithAIUtil_sub_49EE50(pLocal, &pLocal->vecUnknown3, 45.0f, 90.0f, 45.0f, pathFlags) )
        {
            return;
        }
    }

    if ( (pLocal->mode & SITHAI_MODE_TRAVERSEWPNTS) == 0 || !sithAIUtil_AIAdvanceToNextWpnt(pLocal, 1) )
    {
        sithAIMove_SetGoalReached(pLocal);
    }
}

void J3DAPI sithAIMove_sub_4961A0(SithAIControlBlock* pLocal)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_4961A0, pLocal);

    // Movement waits while the AI turns; with NOMOVEBACKWARDS it keeps waiting as long as the goal is behind it
    if ( (pLocal->submode & SITHAI_SUBMODE_NOMOVEBACKWARDS) == 0
        || rdVector_Dot3(&pLocal->goalLVec, &pLocal->pOwner->orient.lvec) >= 0.0f )
    {
        pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_10;
    }
}

double J3DAPI sithAIMove_sub_496200(SithAIControlBlock* pLocal, rdVector3* pStartPos, rdVector3* pEndPos)
{
    INDY_AB_ORIGINAL(sithAIMove_sub_496200, pLocal, pStartPos, pEndPos);

    // Adjusts the start and end of a move step and returns the distance at which the goal counts as reached
    SithThing* pThing = pLocal->pOwner;
    SITH_ASSERTREL(pThing);

    if ( (pThing->pInSector->flags & SITH_SECTOR_UNDERWATER) != 0 || (pThing->moveInfo.physics.flags & SITH_PF_FLY) != 0 )
    {
        return pThing->collide.movesize >= 0.05f ? 0.05f : pThing->collide.movesize;
    }

    float reachDist = pThing->collide.movesize >= 0.03f ? 0.03f : pThing->collide.movesize;

    if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) == 0 )
    {
        pStartPos->z = pThing->pos.z;
        pEndPos->z   = pThing->pos.z;

        if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_100) != 0 )
        {
            // The goal is right beside us
            float rightDist = stdMath_Dist2D1(
                (pThing->orient.rvec.x * 0.05f + pStartPos->x) - pEndPos->x,
                (pThing->orient.rvec.y * 0.05f + pStartPos->y) - pEndPos->y
            );
            float leftDist = stdMath_Dist2D1(
                (pThing->orient.rvec.x * -0.05f + pStartPos->x) - pEndPos->x,
                (pThing->orient.rvec.y * -0.05f + pStartPos->y) - pEndPos->y
            );
            if ( rightDist < 0.05f || leftDist < 0.05f )
            {
                *pEndPos = *pStartPos;
            }
        }

        return reachDist;
    }

    // Wall crawling
    if ( pThing->moveStatus == SITHPLAYERMOVE_MOUNTING_WALL )
    {
        return reachDist;
    }

    rdVector3 delta;
    rdVector_Sub3(&delta, pEndPos, pStartPos);

    // Snap the axes on which the goal is already reached
    int numReached = 0;
    if ( fabsf(delta.x) <= reachDist )
    {
        pEndPos->x = pStartPos->x;
        delta.x = 0.0f;
        numReached = 1;
    }

    if ( fabsf(delta.y) <= reachDist )
    {
        ++numReached;
        pEndPos->y = pStartPos->y;
        delta.y = 0.0f;
    }

    if ( fabsf(delta.z) <= reachDist )
    {
        ++numReached;
        pEndPos->z = pStartPos->z;
        delta.z = 0.0f;
    }

    bool bAlongSurface = false;
    float dist = 0.0f;
    if ( (pThing->attach.flags & SITH_ATTACH_SURFACE) != 0 )
    {
        dist = rdVector_Normalize3Acc(&delta);
        const rdVector3* pNormal = &pThing->attach.attachedToStructure.pSurfaceAttached->face.normal;
        bAlongSurface = fabsf((pNormal->y * delta.y + pNormal->z * delta.z) + pNormal->x * delta.x) < 0.996f;
    }

    if ( bAlongSurface )
    {
        return reachDist;
    }

    // The goal is straight off the surface
    if ( numReached != 2 && dist > pThing->collide.movesize + pThing->collide.movesize )
    {
        sithAIMove_Unreachable(pLocal);
        return reachDist;
    }

    *pEndPos = *pStartPos;
    return reachDist;
}

int J3DAPI sithAIMove_sub_496550(SithAIControlBlock* pLocal, float secDeltaTime)
{
    INDY_AB_ORIGINAL(sithAIMove_sub_496550, pLocal, secDeltaTime);

    // Checks the floor ahead of a walking AI
    SithThing* pThing = pLocal->pOwner;

    rdVector3 nextPos;
    nextPos.x = pThing->moveInfo.physics.velocity.x * secDeltaTime + pThing->pos.x;
    nextPos.y = pThing->moveInfo.physics.velocity.y * secDeltaTime + pThing->pos.y;
    nextPos.z = pThing->moveInfo.physics.velocity.z * secDeltaTime + pThing->pos.z;

    // Every other frame look further ahead
    if ( ((pThing->idx + sithMain_g_frameNumber) & 1) != 0 )
    {
        rdVector3 heading;
        float probeDist;
        if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) != 0 )
        {
            sithAIUtil_GetXYZHeadingVector(pThing, &heading);
            probeDist = pLocal->pOwner->collide.movesize - 0.03f;
            if ( probeDist <= 0.0f )
            {
                probeDist = pLocal->pOwner->collide.movesize;
            }
        }
        else
        {
            sithAIUtil_GetXYHeadingVector(pThing, &heading);
            probeDist = pLocal->pOwner->collide.movesize + 0.03f;
        }

        nextPos.x = probeDist * heading.x + nextPos.x;
        nextPos.y = probeDist * heading.y + nextPos.y;
        nextPos.z = probeDist * heading.z + nextPos.z;
    }

    switch ( sithAIUtil_CheckPosition(pLocal, &nextPos, NULL) )
    {
        case 1: // floor
            break;

        case 2: // not a walkable floor
            if ( sithAI_HasInstinct(pLocal, "wallcrawl") )
            {
                break;
            }
            // fallthrough

        case 0: // blocked
        case 8: // no floor
            sithAI_EmitEvent(pLocal, SITHAI_EVENT_HIT_CLIFF, NULL);
            break;

        case 4: // blocked by a thing
            if ( (pLocal->submode & SITHAI_SUBMODE_ALLOWSTEPTHING) == 0 )
            {
                sithAI_EmitEvent(pLocal, SITHAI_EVENT_HIT_THING, NULL);
            }
            break;

        default:
            sithAIMove_Unreachable(pLocal);
            break;
    }

    return 0;
}

int J3DAPI sithAIMove_sub_4966D0(SithAIControlBlock* pLocal, float secDeltaTime)
{
    INDY_AB_ORIGINAL(sithAIMove_sub_4966D0, pLocal, secDeltaTime);

    // Keeps a flying AI out of the water
    SithThing* pThing = pLocal->pOwner;
    if ( (pThing->flags & SITH_TF_SUBMERGED) != 0 )
    {
        sithAIMove_StopAIMovement(pLocal);
        pThing->moveStatus = SITHPLAYERMOVE_WALKING;
        pThing->moveInfo.physics.velocity.z = pThing->moveInfo.physics.velocity.z + 0.5f;
        return 0;
    }

    rdVector3 nextPos;
    nextPos.x = pThing->moveInfo.physics.velocity.x * secDeltaTime + pThing->pos.x;
    nextPos.y = pThing->moveInfo.physics.velocity.y * secDeltaTime + pThing->pos.y;
    nextPos.z = pThing->moveInfo.physics.velocity.z * secDeltaTime + pThing->pos.z;

    rdVector3 heading;
    sithAIUtil_GetXYHeadingVector(pThing, &heading);
    nextPos.x = (pThing->collide.movesize + 0.05f) * heading.x + nextPos.x;
    nextPos.y = (pThing->collide.movesize + 0.05f) * heading.y + nextPos.y;
    nextPos.z = (pThing->collide.movesize + 0.05f) * heading.z + nextPos.z;

    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &nextPos, 0.0f);
    if ( !pSector || (pSector->flags & SITH_SECTOR_NOACTORENTER) != 0 )
    {
        sithAIMove_Unreachable(pLocal);
        return 1;
    }

    if ( (pSector->flags & SITH_SECTOR_UNDERWATER) != 0 )
    {
        sithAIMove_StopAIMovement(pLocal);
        pThing->moveStatus = SITHPLAYERMOVE_WALKING;
        pThing->moveInfo.physics.velocity.z = pThing->moveInfo.physics.velocity.z + 0.5f;
        return 1;
    }

    return 0;
}

int J3DAPI sithAIMove_sub_496820(SithAIControlBlock* pLocal, float secDeltaTime)
{
    INDY_AB_ORIGINAL(sithAIMove_sub_496820, pLocal, secDeltaTime);

    // Keeps a swimming AI in the water
    SithThing* pThing = pLocal->pOwner;
    if ( pThing->moveInfo.physics.velocity.z != 0.0f )
    {
        rdVector3 probePos = pThing->pos;
        probePos.z = pThing->collide.movesize * ((pLocal->submode & SITHAI_SUBMODE_SWIMNEARSURFACE) != 0 ? 0.2f : 0.5f) + probePos.z;

        SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &probePos, 0.0f);
        if ( pSector && (pSector->flags & SITH_SECTOR_UNDERWATER) == 0 )
        {
            // Close to the surface: dive
            pThing->moveInfo.physics.velocity.z = -0.05f;
            if ( pLocal->movePos.z > pThing->pos.z )
            {
                pLocal->movePos.z = pThing->pos.z - 0.05f;
            }
        }
    }

    rdVector3 nextPos;
    nextPos.x = pThing->moveInfo.physics.velocity.x * secDeltaTime + pThing->pos.x;
    nextPos.y = pThing->moveInfo.physics.velocity.y * secDeltaTime + pThing->pos.y;
    nextPos.z = pThing->moveInfo.physics.velocity.z * secDeltaTime + pThing->pos.z;

    rdVector3 heading;
    sithAIUtil_GetXYHeadingVector(pLocal->pOwner, &heading);
    nextPos.x = (pLocal->pOwner->collide.movesize + 0.03f) * heading.x + nextPos.x;
    nextPos.y = (pLocal->pOwner->collide.movesize + 0.03f) * heading.y + nextPos.y;
    nextPos.z = (pLocal->pOwner->collide.movesize + 0.03f) * heading.z + nextPos.z;

    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, &nextPos, 0.0f);
    if ( !pSector || (pSector->flags & SITH_SECTOR_NOACTORENTER) != 0 || (pSector->flags & SITH_SECTOR_UNDERWATER) == 0 )
    {
        sithAIMove_Unreachable(pLocal);
    }

    return 0;
}

int J3DAPI sithAIMove_AIGetMoveState(const SithAIControlBlock* pLocal)
{
    int result;

    switch ( pLocal->pOwner->moveStatus )
    {
        case SITHPLAYERMOVE_MOUNTING_WALL:
        case SITHPLAYERMOVE_TURNING_LEFT_45_DEGREES:
        case SITHPLAYERMOVE_TURNING_RIGHT_45_DEGREES:
        case SITHPLAYERMOVE_TURNING_LEFT_90_DEGREES:
        case SITHPLAYERMOVE_TURNING_RIGHT_90_DEGREES:
        case SITHPLAYERMOVE_TURNING_LEFT_135_DEGREES:
        case SITHPLAYERMOVE_TURNING_RIGHT_135_DEGREES:
        case SITHPLAYERMOVE_TURNING_180_DEGREES:
            result = 2;
            break;

        case SITHPLAYERMOVE_ROLLING_LEFT:
        case SITHPLAYERMOVE_ROLLING_RIGHT:
        case SITHPLAYERMOVE_STRAFING_LEFT:
        case SITHPLAYERMOVE_STRAFING_RIGHT:
        case SITHPLAYERMOVE_SLIDEDOWNFORWARD:
        case SITHPLAYERMOVE_STAND_TO_CRAWL:
        case SITHPLAYERMOVE_CRAWL_TO_STAND:
        case SITHPLAYERMOVE_WALK2STAND:
        case SITHPLAYERMOVE_STAND2WALK:
        case SITHPLAYERMOVE_UNKNOWN_78:
        case SITHPLAYERMOVE_STAND2RUN:
        case SITHPLAYERMOVE_UNKNOWN_80:
        case SITHPLAYERMOVE_UNKNOWN_81:
        case SITHPLAYERMOVE_SLIDEDOWNBACK:
        case SITHPLAYERMOVE_KNOCKEDOUT:
        case SITHPLAYERMOVE_RUNOVER:
            result = 1;
            break;

        default:
            result = 0;
            break;
    }

    return result;
}

int J3DAPI sithAIMove_AISpecialTurn(SithAIControlBlock* pLocal, float angle)
{
    BOOL bTurn90;
    BOOL bTurn135;
    SithActorSpecialMoveFlags moveFlags;
    float normAngle;
    int bTurnSuccess;

    bTurnSuccess = 0;

    if ( (pLocal->submode & SITHAI_SUBMODE_SPECIALTURNS) == 0 )
    {
        return 0;
    }

    if ( sithAIMove_AIGetMoveState(pLocal) )
    {
        return 1;
    }

    if ( angle < 0.0f )
    {
        normAngle = -angle;
    }
    else
    {
        normAngle = angle;
    }

    if ( normAngle < 22.5f )
    {
        return 0;
    }

    if ( angle >= 0.0f )
    {
        moveFlags = SITHACTORSPECIALMOVE_DIR_RIGHT;
    }
    else
    {
        moveFlags = SITHACTORSPECIALMOVE_DIR_LEFT;
    }

    if ( normAngle >= 112.5f && normAngle <= 180.0f )
    {
        bTurn135 = normAngle >= 112.5f && normAngle <= 157.5f;
        if ( bTurn135 && sithAIMove_AISpecialMove(pLocal, (SithActorSpecialMoveFlags)(moveFlags | SITHACTORSPECIALMOVE_TURN135)) )
        {
            bTurnSuccess = 1;
        }

        if ( !bTurnSuccess )
        {
            bTurnSuccess = sithAIMove_AISpecialMove(pLocal, SITHACTORSPECIALMOVE_TURN180) != 0;
        }
    }
    else
    {
        bTurn90 = normAngle >= 67.5f && normAngle <= 112.5f;
        if ( bTurn90 && sithAIMove_AISpecialMove(pLocal, (SithActorSpecialMoveFlags)(moveFlags | SITHACTORSPECIALMOVE_TURN90)) )
        {
            bTurnSuccess = 1;
        }
    }

    if ( !bTurnSuccess )
    {
        return sithAIMove_AISpecialMove(pLocal, (SithActorSpecialMoveFlags)(moveFlags | SITHACTORSPECIALMOVE_TURN45)) != 0;
    }

    return bTurnSuccess;
}

float J3DAPI sithAIMove_UpdateAIMove(SithAIControlBlock* pLocal)
{
    int v2;
    float x;
    float v4;
    float v5;
    float v6;
    float y;
    float z;
    float absVelRight;
    float absVelFwd;
    float absVelUp;
    SithThing* pThing;
    rdVector3 velocity;
    SithPuppetSubMode newSubmode;
    SithThingMoveStatus newMoveStatus;
    float moveSpeed;
    SithPhysicsInfo* pPhysics;
    float minFlyMoveSpeed;
    int axis;

    pPhysics = (SithPhysicsInfo*)&pLocal->pOwner->moveInfo;
    pThing = pLocal->pOwner;
    moveSpeed = 0.0f;
    newSubmode = SITHPUPPETSUBMODE_STAND;
    newMoveStatus = SITHPLAYERMOVE_STILL;
    minFlyMoveSpeed = 0.0099999998f;

    SITH_ASSERTREL(pThing);
    if ( pThing->pInSector
        && (pPhysics->velocity.x != 0.0f || pPhysics->velocity.y != 0.0f || pPhysics->velocity.z != 0.0f || (pLocal->mode & SITHAI_MODE_TURNING) != 0)
        && (pThing->thingInfo.actorInfo.flags & SITH_AF_IMMOBILE) == 0 )
    {
        // Transform to world velocity
        rdMatrix_TransformVectorOrtho34(&velocity, &pPhysics->velocity, &pThing->orient);

        if ( (pPhysics->flags & SITH_PF_FLY) != 0
            && (velocity.z >= 0.0f ? (absVelUp = velocity.z) : (absVelUp = -velocity.z),
                velocity.y >= 0.0f ? (absVelFwd = velocity.y) : (absVelFwd = -velocity.y),
                absVelUp > (double)absVelFwd
                && (velocity.z >= 0.0f ? (absVelUp = velocity.z) : (absVelUp = -velocity.z),
                    velocity.x >= 0.0f ? (absVelRight = velocity.x) : (absVelRight = -velocity.x),
                    absVelUp > (double)absVelRight)) )
        {
            moveSpeed = velocity.z;
            if ( velocity.z > (double)minFlyMoveSpeed )
            {
                newMoveStatus = SITHPLAYERMOVE_UNKNOWN_103;
                newSubmode = SITHPUPPETSUBMODE_RISING;
            }

            else if ( -minFlyMoveSpeed > moveSpeed )
            {
                newMoveStatus = SITHPLAYERMOVE_UNKNOWN_104;
                newSubmode = SITHPUPPETSUBMODE_FALLFORWARD;
            }
        }
        else                                    // not flying
        {
            if ( velocity.z >= 0.0f )
            {
                z = velocity.z;
            }
            else
            {
                z = -velocity.z;
            }

            if ( velocity.y >= 0.0f )
            {
                y = velocity.y;
            }
            else
            {
                y = -velocity.y;
            }

            if ( z > (double)y
                && (velocity.z >= 0.0f ? (v6 = velocity.z) : (v6 = -velocity.z), velocity.x >= 0.0f ? (v5 = velocity.x) : (v5 = -velocity.x), v6 > (double)v5) )
            {
                moveSpeed = velocity.z;
                axis = 2;
            }
            else
            {
                if ( velocity.y < 0.0f )
                {
                    v4 = -velocity.y;
                }
                else
                {
                    v4 = velocity.y;
                }

                if ( velocity.x < 0.0f )
                {
                    x = -velocity.x;
                }
                else
                {
                    x = velocity.x;
                }

                if ( v4 <= (double)x )
                {
                    moveSpeed = velocity.x;
                    axis = 0;
                }
                else
                {
                    moveSpeed = velocity.y;
                    axis = 1;
                }
            }

            sithAIMove_GetAIMoveModes(pThing, moveSpeed, axis, &newMoveStatus, &newSubmode);
        }
    }

    if ( pThing->thingInfo.actorInfo.bControlsDisabled || pThing->thingInfo.actorInfo.bForceMovePlay )
    {
        return moveSpeed;
    }

    v2 = sithAIMove_AIGetMoveState(pLocal);
    switch ( v2 )
    {
        case 0:                                 // turning
            break;

        case 1:                                 // moving
            return moveSpeed;

        case 2:
            return 0.0f;
    }

    if ( pThing->pPuppetState->submode != newSubmode )
    {
        sithAIMove_SetSubMode(pLocal, newMoveStatus, newSubmode);
    }

    return moveSpeed;
}

void J3DAPI sithAIMove_GetAIMoveModes(const SithThing* pThing, float moveSpeed, int axis, SithThingMoveStatus* pOutMoveStatus, SithPuppetSubMode* pOutSubmode)
{
    float v5;
    float v6;
    float v7;
    float v8;
    float absMoveSpeed;
    float maxTurnSpeed;
    SithAIControlBlock* pLocal;
    float minWalkSpeed;
    float maxWalkSpeed;

    pLocal = pThing->controlInfo.aiControl.pLocal;

    minWalkSpeed = 0.001f;
    maxWalkSpeed = 0.27000001f;
    maxTurnSpeed = 0.0049999999f;

    SITH_ASSERTREL(pLocal);
    if ( (pThing->thingInfo.actorInfo.flags & SITH_AF_FASTMOVE15) != 0 )
    {
        minWalkSpeed = sithAIMove_minWalkSpeed * 1.15f;
        maxWalkSpeed = sithAIMove_maxWalkSpeed * 1.15f;
    }

    else if ( (pThing->thingInfo.actorInfo.flags & SITH_AF_FASTMOVE10) != 0 )
    {
        minWalkSpeed = sithAIMove_minWalkSpeed * 1.1f;
        maxWalkSpeed = sithAIMove_maxWalkSpeed * 1.1f;
    }

    if ( (pLocal->mode & SITHAI_MODE_TURNING) == 0 )
    {
        goto LABEL_25;
    }

    if ( (pThing->thingInfo.actorInfo.flags & SITH_AF_FLYERMOVE) != 0 )
    {
        if ( pThing->moveStatus == SITHPLAYERMOVE_UNKNOWN_105 )
        {
            *pOutMoveStatus = SITHPLAYERMOVE_TURNING_LEFT;
            *pOutSubmode = SITHPUPPETSUBMODE_TURNLEFT;
            return;
        }

        if ( pThing->moveStatus == SITHPLAYERMOVE_UNKNOWN_106 )
        {
            *pOutMoveStatus = SITHPLAYERMOVE_TURNING_RIGHT;
            *pOutSubmode = SITHPUPPETSUBMODE_TURNRIGHT;
            return;
        }
    }

    if ( (pThing->thingInfo.actorInfo.flags & SITH_AF_FASTMOVE15) != 0 )
    {
        maxTurnSpeed = sithAIMove_maxTurnSpeed * 1.15f;
    }

    else if ( (pThing->thingInfo.actorInfo.flags & SITH_AF_FASTMOVE10) != 0 )
    {
        maxTurnSpeed = sithAIMove_maxTurnSpeed * 1.1f;
    }

    absMoveSpeed = moveSpeed >= 0.0f ? moveSpeed : -moveSpeed;
    if ( absMoveSpeed < (double)maxTurnSpeed )
    {
        if ( pThing->moveStatus == SITHPLAYERMOVE_TURNING_LEFT || pThing->moveStatus == SITHPLAYERMOVE_UNKNOWN_105 )
        {
            *pOutMoveStatus = SITHPLAYERMOVE_TURNING_LEFT;
            *pOutSubmode = SITHPUPPETSUBMODE_TURNLEFT;
        }
        else
        {
            *pOutMoveStatus = SITHPLAYERMOVE_TURNING_RIGHT;
            *pOutSubmode = SITHPUPPETSUBMODE_TURNRIGHT;
        }
    }
    else
    {
    LABEL_25:
        if ( !axis ) // x - right
        {
            if ( moveSpeed < 0.0f )
            {
                v8 = -moveSpeed;
            }
            else
            {
                v8 = moveSpeed;
            }

            if ( v8 >= (double)maxWalkSpeed )
            {
                *pOutMoveStatus = SITHPLAYERMOVE_RUNNING;
                *pOutSubmode = SITHPUPPETSUBMODE_RUN;
            }
            else
            {
                if ( moveSpeed < 0.0f )
                {
                    v7 = -moveSpeed;
                }
                else
                {
                    v7 = moveSpeed;
                }

                if ( v7 >= (double)minWalkSpeed && (moveSpeed >= 0.0f ? (v6 = moveSpeed) : (v6 = -moveSpeed), v6 < (double)maxWalkSpeed) )
                {
                    *pOutMoveStatus = SITHPLAYERMOVE_WALKING;
                    *pOutSubmode = SITHPUPPETSUBMODE_WALK;
                }
                else
                {
                    *pOutMoveStatus = SITHPLAYERMOVE_STILL;
                    *pOutSubmode = SITHPUPPETSUBMODE_STAND;
                }
            }
        }
        else
        {
            if ( axis == 1 ) // y - forward
            {
                goto LABEL_54;
            }

            if ( axis != 2 ) // z - up
            {
                *pOutMoveStatus = SITHPLAYERMOVE_STILL;
                *pOutSubmode = SITHPUPPETSUBMODE_STAND;
                return;
            }

            // axis should be z - up
            if ( !pThing->attach.flags )
            {
                if ( -minWalkSpeed <= moveSpeed && moveSpeed <= (double)minWalkSpeed )
                {
                    *pOutMoveStatus = SITHPLAYERMOVE_STILL;
                    *pOutSubmode = SITHPUPPETSUBMODE_STAND;
                }
                else
                {
                    if ( moveSpeed >= 0.0f )
                    {
                        v5 = moveSpeed;
                    }
                    else
                    {
                        v5 = -moveSpeed;
                    }

                    if ( v5 > (double)minWalkSpeed )
                    {
                        *pOutMoveStatus = SITHPLAYERMOVE_FALLING;
                        *pOutSubmode = SITHPUPPETSUBMODE_FALL;
                    }
                }
            }
            else
            {
            LABEL_54:
                if ( moveSpeed >= (double)maxWalkSpeed )
                {
                    *pOutMoveStatus = SITHPLAYERMOVE_RUNNING;
                    *pOutSubmode = SITHPUPPETSUBMODE_RUN;
                }

                else if ( moveSpeed >= (double)minWalkSpeed && moveSpeed < (double)maxWalkSpeed )
                {
                    *pOutMoveStatus = SITHPLAYERMOVE_WALKING;
                    *pOutSubmode = SITHPUPPETSUBMODE_WALK;
                }

                else if ( -minWalkSpeed < moveSpeed && moveSpeed < (double)minWalkSpeed )
                {
                    *pOutMoveStatus = SITHPLAYERMOVE_STILL;
                    *pOutSubmode = SITHPUPPETSUBMODE_STAND;
                }

                else if ( -minWalkSpeed >= moveSpeed )
                {
                    *pOutMoveStatus = SITHPLAYERMOVE_WALKING;
                    *pOutSubmode = SITHPUPPETSUBMODE_WALKBACK;
                }
            }
        }
    }
}

void J3DAPI sithAIMove_SetSubMode(SithAIControlBlock* pLocal, SithThingMoveStatus moveStatus, SithPuppetSubMode submode)
{
    SithThing* pThing;
    const rdKeyframe* pKeyframe;
    int majorMode;
    void (J3DAPI * pCallback)(SithThing*, int, rdKeyMarkerType);
    SithPuppetTrack* pTrack;
    SithPuppetSubMode bdSubmode;
    SithThingMoveStatus bdModeStatus;
    SithPuppetSubMode curSubmode;


    pCallback    = NULL;
    pThing       = pLocal->pOwner;
    bdSubmode    = 0;
    bdModeStatus = 0;

    if ( !pLocal->pOwner || !pThing->pPuppetClass || !pThing->pPuppetState )
    {
        SITHLOG_ERROR("sithAIMove_SetSubMode: Bad puppet data for AI '%s'.\n", pThing->aName); // TODO: potential null pointer dereference, pThing can be null here
    }
    else
    {
        majorMode  = pThing->pPuppetState->majorMode;
        curSubmode = pThing->pPuppetState->submode;

        switch ( curSubmode )
        {
            case SITHPUPPETSUBMODE_STAND:
            case SITHPUPPETSUBMODE_TURNLEFT:
            case SITHPUPPETSUBMODE_TURNRIGHT:
                if ( (unsigned int)submode >= SITHPUPPETSUBMODE_WALK && (unsigned int)submode <= SITHPUPPETSUBMODE_WALKBACK )
                {
                    bdSubmode = SITHPUPPETSUBMODE_STAND2WALK;
                    bdModeStatus = SITHPLAYERMOVE_STAND2WALK;
                }

                break;

            case SITHPUPPETSUBMODE_WALK:
                if ( submode == SITHPUPPETSUBMODE_STAND
                    || (unsigned int)submode > SITHPUPPETSUBMODE_STRAFERIGHT && (unsigned int)submode <= SITHPUPPETSUBMODE_TURNRIGHT )
                {
                    bdSubmode = SITHPUPPETSUBMODE_WALK2STAND;
                    bdModeStatus = SITHPLAYERMOVE_WALK2STAND;
                }

                break;

            case SITHPUPPETSUBMODE_RUN:
                if ( submode == SITHPUPPETSUBMODE_STAND
                    || (unsigned int)submode > SITHPUPPETSUBMODE_STRAFERIGHT && (unsigned int)submode <= SITHPUPPETSUBMODE_TURNRIGHT )
                {
                    bdSubmode = SITHPUPPETSUBMODE_WALK2STAND;
                    bdModeStatus = SITHPLAYERMOVE_WALK2STAND;
                }

                break;

            case SITHPUPPETSUBMODE_FALL:
                if ( submode == SITHPUPPETSUBMODE_STAND && sithPuppet_PlayMode(pThing, SITHPUPPETSUBMODE_LAND, sithAIMove_PuppetCallback) > -1 )
                {
                    moveStatus = SITHPLAYERMOVE_LAND;
                }

                break;

            default:
                break;
        }

        // Play transition animation
        if ( bdSubmode )
        {
            if ( pThing->pPuppetClass->aModes[majorMode][bdSubmode].pKeyframe )
            {
                sithPuppet_ClearMode(pThing, curSubmode);

                moveStatus = bdModeStatus;
                submode    = bdSubmode;
                pCallback = sithAIMove_PuppetCallback;

                if ( bdSubmode == SITHPUPPETSUBMODE_STAND2WALK )
                {
                    memset(&pThing->moveInfo.physics.velocity, 0, sizeof(pThing->moveInfo.physics.velocity));
                    sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_STAND2WALK);
                }
                else
                {
                    sithSoundClass_PlayModeFirst(pThing, SITHSOUNDCLASS_WALK2STAND);
                }
            }
        }

        pKeyframe = pThing->pPuppetClass->aModes[majorMode][submode].pKeyframe;
        if ( pKeyframe )
        {
            pTrack = sithPuppet_FindActiveTrack(pThing, pKeyframe);
            if ( pTrack )
            {
                sithPuppet_SynchMode(pThing, pTrack->submode, submode, -1.0f, 0);
            }
            else
            {
                sithPuppet_SetSubMode(pThing, submode, pCallback);
            }
        }

        pThing->pPuppetState->submode = submode;
        pThing->moveStatus = moveStatus;
    }
}

void J3DAPI sithAIMove_PuppetCallback(SithThing* pThing, int trackNum, rdKeyMarkerType marker)
{
    SithAIMode mode;
    float a;
    float aa;
    SithThingMoveStatus moveStatus;
    SithPhysicsInfo* pPhysics;
    SithAIControlBlock* pLocal;

    pPhysics = (SithPhysicsInfo*)&pThing->moveInfo;
    SITH_ASSERTREL(pThing);
    pLocal = pThing->controlInfo.aiControl.pLocal;
    if ( !pLocal )
    {
    LABEL_48:
        sithPuppet_DefaultCallback(pThing, trackNum, marker);
    }
    else
    {
        switch ( marker )
        {
            case 0:
                sithPuppet_FreeTrackByIndex(pThing, trackNum);
                if ( (pThing->flags & (SITH_TF_DYING | SITH_TF_DESTROYED)) == 0 )
                {
                    switch ( pThing->moveStatus )
                    {
                        case SITHPLAYERMOVE_LAND:
                            pThing->moveStatus = SITHPLAYERMOVE_STILL;
                            goto LABEL_29;

                        case SITHPLAYERMOVE_MOUNTING_WALL:
                            sithAIMove_AIFinalizeSpecialMove(pLocal, SITHACTORSPECIALMOVE_MOUNTWALL);
                            pThing->moveStatus = SITHPLAYERMOVE_WALKING;
                            goto LABEL_29;

                        case SITHPLAYERMOVE_ROLLING_LEFT:
                        case SITHPLAYERMOVE_ROLLING_RIGHT:
                            sithAIMove_AIFinalizeSpecialMove(pLocal, SITHACTORSPECIALMOVE_ROLL);
                            pThing->moveStatus = SITHPLAYERMOVE_STILL;
                            sithAIMove_SetGoalReached(pLocal);
                            goto LABEL_29;

                        case SITHPLAYERMOVE_STRAFING_LEFT:
                        case SITHPLAYERMOVE_STRAFING_RIGHT:
                            pThing->moveStatus = SITHPLAYERMOVE_STILL;
                            sithAIMove_SetGoalReached(pLocal);
                            goto LABEL_29;

                        case SITHPLAYERMOVE_WALK2STAND:
                        case SITHPLAYERMOVE_UNKNOWN_78:
                            pThing->moveStatus = SITHPLAYERMOVE_STILL;
                            goto LABEL_29;

                        case SITHPLAYERMOVE_STAND2WALK:
                        case SITHPLAYERMOVE_STAND2RUN:
                            if ( pThing->moveStatus == SITHPLAYERMOVE_STAND2WALK )
                            {
                                pThing->moveStatus = SITHPLAYERMOVE_WALKING;
                            }
                            else
                            {
                                pThing->moveStatus = SITHPLAYERMOVE_RUNNING;
                            }

                            goto LABEL_29;

                        case SITHPLAYERMOVE_KNOCKEDOUT:
                            pThing->thingInfo.actorInfo.bControlsDisabled = 0;
                            pThing->moveStatus = SITHPLAYERMOVE_STILL;
                            pThing->collide.type = SITH_COLLIDE_FACE;
                            mode = pThing->controlInfo.aiControl.pLocal->mode;
                            mode &= ~0x2000;// 0x2000
                            pThing->controlInfo.aiControl.pLocal->mode = mode;
                            pThing->thingInfo.actorInfo.flags &= ~(SITH_AF_IMMOBILE | SITH_AF_INVULNERABLE);
                            goto LABEL_29;

                        case SITHPLAYERMOVE_TURNING_LEFT_45_DEGREES:
                        case SITHPLAYERMOVE_TURNING_RIGHT_45_DEGREES:
                            sithAIMove_AIFinalizeSpecialMove(pLocal, SITHACTORSPECIALMOVE_TURN45);
                            pThing->moveStatus = SITHPLAYERMOVE_WALKING;
                            goto LABEL_29;

                        case SITHPLAYERMOVE_TURNING_LEFT_90_DEGREES:
                        case SITHPLAYERMOVE_TURNING_RIGHT_90_DEGREES:
                            sithAIMove_AIFinalizeSpecialMove(pLocal, SITHACTORSPECIALMOVE_TURN90);
                            pThing->moveStatus = SITHPLAYERMOVE_WALKING;
                            goto LABEL_29;

                        case SITHPLAYERMOVE_TURNING_LEFT_135_DEGREES:
                        case SITHPLAYERMOVE_TURNING_RIGHT_135_DEGREES:
                            sithAIMove_AIFinalizeSpecialMove(pLocal, SITHACTORSPECIALMOVE_TURN135);
                            pThing->moveStatus = SITHPLAYERMOVE_WALKING;
                            goto LABEL_29;

                        case SITHPLAYERMOVE_TURNING_180_DEGREES:
                            sithAIMove_AIFinalizeSpecialMove(pLocal, SITHACTORSPECIALMOVE_TURN180);
                            pThing->moveStatus = SITHPLAYERMOVE_WALKING;

                        LABEL_29:
                            moveStatus = pThing->moveStatus;
                            switch ( moveStatus )
                            {
                                case SITHPLAYERMOVE_STILL:
                                    sithPuppet_SetSubMode(pThing, SITHPUPPETSUBMODE_STAND, 0);
                                    break;

                                case SITHPLAYERMOVE_WALKING:
                                    if ( pThing->moveInfo.physics.velocity.x == 0.0f
                                        && pThing->moveInfo.physics.velocity.y == 0.0f
                                        && pThing->moveInfo.physics.velocity.z == 0.0f )
                                    {
                                        pThing->moveInfo.physics.velocity.y = 0.059999999f;
                                    }
                                    else
                                    {
                                        a = rdVector_Dot3(&pPhysics->velocity, &pPhysics->velocity);
                                        if ( sqrtf(a) < 0.059999999f )
                                        {
                                            pThing->moveInfo.physics.velocity.x = pThing->moveInfo.physics.velocity.x * 1.0599999f;
                                            pThing->moveInfo.physics.velocity.y = pThing->moveInfo.physics.velocity.y * 1.0599999f;
                                            pThing->moveInfo.physics.velocity.z = pThing->moveInfo.physics.velocity.z * 1.0599999f;
                                        }
                                    }

                                    sithPuppet_SetSubMode(pThing, SITHPUPPETSUBMODE_WALK, 0);
                                    break;

                                case SITHPLAYERMOVE_RUNNING:
                                    if ( pThing->moveInfo.physics.velocity.x == 0.0f
                                        && pThing->moveInfo.physics.velocity.y == 0.0f
                                        && pThing->moveInfo.physics.velocity.z == 0.0f )
                                    {
                                        pThing->moveInfo.physics.velocity.y = 0.27000001f;
                                    }

                                    aa = rdVector_Dot3(&pPhysics->velocity, &pPhysics->velocity);
                                    if ( sqrtf(aa) < 0.27000001f )
                                    {
                                        pThing->moveInfo.physics.velocity.x = pThing->moveInfo.physics.velocity.x * 1.27f;
                                        pThing->moveInfo.physics.velocity.y = pThing->moveInfo.physics.velocity.y * 1.27f;
                                        pThing->moveInfo.physics.velocity.z = pThing->moveInfo.physics.velocity.z * 1.27f;
                                    }

                                    sithPuppet_SetSubMode(pThing, SITHPUPPETSUBMODE_RUN, 0);
                                    break;
                            }

                            break;

                        default:
                            return;
                    }
                }

                return;

            case RDKEYMARKER_LEFTFOOT:
                if ( pThing->moveStatus != SITHPLAYERMOVE_STRAFING_RIGHT )
                {
                    goto LABEL_48;
                }

                goto LABEL_6;

            case RDKEYMARKER_RIGHTFOOT:
                if ( pThing->moveStatus != SITHPLAYERMOVE_STRAFING_LEFT )
                {
                    goto LABEL_48;
                }

            LABEL_6:
                sithAIMove_AIFinalizeSpecialMove(pLocal, SITHACTORSPECIALMOVE_STRAFE);
                break;

            case RDKEYMARKER_DIED:
                switch ( pThing->moveStatus )
                {
                    case SITHPLAYERMOVE_ROLLING_LEFT:
                    case SITHPLAYERMOVE_ROLLING_RIGHT:
                        sithSoundClass_PlayModeRandom(pThing, SITHSOUNDCLASS_RESERVED3);
                        break;

                    case SITHPLAYERMOVE_KNOCKEDOUT:
                        sithSoundClass_PlayModeRandom(pThing, SITHSOUNDCLASS_RESERVED1);
                        break;

                    case SITHPLAYERMOVE_RUNOVER:
                        sithSoundClass_PlayModeRandom(pThing, SITHSOUNDCLASS_RESERVED2);
                        break;

                    default:
                        goto LABEL_48;
                }

                break;

            default:
                goto LABEL_48;
        }
    }
}

void J3DAPI sithAIMove_AISetLookThing(SithAIControlBlock* pLocal, const SithThing* pThing)
{
    rdVector3 eyePos;
    rdVector3 dest;

    SITH_ASSERTREL(pLocal && pLocal->pOwner && pLocal->pClass);
    SITH_ASSERTREL(pThing);
    if ( pThing->type == SITH_THING_ACTOR || pThing->type == SITH_THING_PLAYER )
    {
        rdMatrix_TransformVector34(&dest, &pThing->thingInfo.actorInfo.eyeOffset, &pThing->orient);

        eyePos.x = pThing->pos.x + dest.x;
        eyePos.y = pThing->pos.y + dest.y;
        eyePos.z = pThing->pos.z + dest.z;
    }
    else
    {
        memcpy(&eyePos, &pThing->pos, sizeof(eyePos));
    }

    sithAIMove_AISetLookPos(pLocal, &eyePos);
}

void J3DAPI sithAIMove_AISetLookPos(SithAIControlBlock* pLocal, const rdVector3* targetPos)
{
    double dist;
    SithThing* pOwner;
    float v5;
    rdVector3 eyePos;
    SithActorInfo* pActor;

    SITH_ASSERTREL(pLocal && pLocal->pOwner && pLocal->pClass);
    SITH_ASSERTREL(targetPos);
    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_1) == 0 )
    {
        if ( (pLocal->mode & SITHAI_MODE_DISABLED) != 0 && (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) == 0 )
        {
            SITHLOG_ERROR("WARNING!  Look target issued to '%s'. AI is DISABLED.\n", pLocal->pOwner->aName);
        }

        pOwner = pLocal->pOwner;
        pActor = &pLocal->pOwner->thingInfo.actorInfo;
        memcpy(&eyePos, &pOwner->pos, sizeof(eyePos));
        if ( pOwner->type == SITH_THING_ACTOR || pOwner->type == SITH_THING_PLAYER )
        {
            eyePos.z = eyePos.z + pActor->eyeOffset.z;
        }

        pLocal->goalLVec.x = targetPos->x - eyePos.x;
        pLocal->goalLVec.y = targetPos->y - eyePos.y;
        pLocal->goalLVec.z = targetPos->z - eyePos.z;
        dist = rdVector_Normalize3Acc(&pLocal->goalLVec);

        v5 = dist;
        if ( _isnan(dist) || v5 < 0.001f )
        {
            memcpy(&pLocal->goalLVec, &pOwner->orient.lvec, sizeof(pLocal->goalLVec));
            v5 = 0.0f;
        }

        memcpy(&pLocal->lookPos, targetPos, sizeof(pLocal->lookPos));
        if ( v5 != 0.0f )
        {
            pLocal->mode |= SITHAI_MODE_TURNING;
            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_10;
        }

        pLocal->mode |= SITHAI_MODE_UNKNOWN_80;
        if ( (pLocal->mode & SITHAI_MODE_WANTALLEVENTS) != 0 )
        {
            sithAI_EmitEvent(pLocal, SITHAI_EVENT_UNKNOWN_40000, 0);
        }
    }
}

void J3DAPI sithAIMove_AISetLookPosEyeLevel(SithAIControlBlock* pLocal, const rdVector3* targetPos)
{
    rdVector3 eyePos;
    SithThing* pOwner;
    rdVector3 lookPos;
    float dist;

    pOwner = pLocal->pOwner;

    memcpy(&lookPos, targetPos, sizeof(lookPos));
    eyePos.x = 0.0f;
    eyePos.y = 0.0f;
    eyePos.z = pOwner->thingInfo.actorInfo.eyeOffset.z;
    rdMatrix_TransformVector34Acc(&eyePos, &pOwner->orient);

    eyePos.x = eyePos.x + pOwner->pos.x;
    eyePos.y = eyePos.y + pOwner->pos.y;
    eyePos.z = eyePos.z + pOwner->pos.z;

    // rdMath_DistPointToPlane
    dist = (lookPos.x - eyePos.x) * pOwner->orient.uvec.x + (lookPos.y - eyePos.y) * pOwner->orient.uvec.y + (lookPos.z - eyePos.z) * pOwner->orient.uvec.z;
    lookPos.x = pOwner->orient.uvec.x * -dist + lookPos.x;
    lookPos.y = pOwner->orient.uvec.y * -dist + lookPos.y;
    lookPos.z = pOwner->orient.uvec.z * -dist + lookPos.z;
    sithAIMove_AISetLookPos(pLocal, &lookPos);
}

int J3DAPI sithAIMove_AISetMovePos(SithAIControlBlock* pLocal, const rdVector3* moveToPos, float moveSpeed)
{
    SITH_ASSERTREL(pLocal && pLocal->pOwner && pLocal->pClass);
    SITH_ASSERTREL(moveToPos);

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_1) != 0 )
    {
        return 0;
    }

    if ( (pLocal->mode & SITHAI_MODE_DISABLED) != 0 && (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) == 0 )
    {
        SITHLOG_ERROR("WARNING!  Move target issued to '%s'. AI is DISABLED.\n", pLocal->pOwner->aName);
    }

    rdVector3 newPos = *moveToPos;
    rdVector3 curPos = pLocal->pOwner->pos;

    if ( pLocal->maxHomeDist > 0.0f )
    {
        rdVector3 dir;
        dir.x = moveToPos->x - pLocal->homePos.x;
        dir.y = moveToPos->y - pLocal->homePos.y;
        dir.z = moveToPos->z - pLocal->homePos.z;
        float dist = rdVector_Normalize3Acc(&dir);
        if ( dist > (double)pLocal->maxHomeDist )
        {
            newPos.x = pLocal->maxHomeDist * dir.x + pLocal->homePos.x;
            newPos.y = pLocal->maxHomeDist * dir.y + pLocal->homePos.y;
            newPos.z = pLocal->maxHomeDist * dir.z + pLocal->homePos.z;
        }
    }

    float moveSize = sithAIMove_sub_496200(pLocal, &curPos, &newPos);
    if ( rdVector_Dist3(moveToPos, &pLocal->pOwner->pos) <= moveSize )
    {
        return 0;
    }

    pLocal->moveSpeed = moveSpeed;
    memcpy(&pLocal->movePos, moveToPos, sizeof(pLocal->movePos));
    memcpy(&pLocal->vecUnknown, &pLocal->pOwner->pos, sizeof(pLocal->vecUnknown));

    sithSoundClass_PlayModeFirst(pLocal->pOwner, SITHSOUNDCLASS_MOVING);
    sithAIUtil_AISetMode(pLocal, SITHAI_MODE_MOVING);

    if ( (pLocal->mode & SITHAI_MODE_WANTALLEVENTS) != 0 )
    {
        sithAI_EmitEvent(pLocal, SITHAI_EVENT_MOVE_TARGET_CHANGED, NULL);
    }

    // Found in debug version
#ifdef J3D_DEBUG
    // TODO: Find a way to null the 2 pointers on level close to prevent crashes due to dangling pointers
    if ( sithAIMove_pMoveToDebugMarkThing )
    {
        sithThing_RemoveThing(sithWorld_g_pCurrentWorld, sithAIMove_pMoveToDebugMarkThing);
        sithAIMove_pMoveToDebugMarkThing = NULL;
    }

    if ( sithAIMove_pMoveToPosGroundMarkThing )
    {
        sithThing_RemoveThing(sithWorld_g_pCurrentWorld, sithAIMove_pMoveToPosGroundMarkThing);
        sithAIMove_pMoveToPosGroundMarkThing = NULL;
    }

    if ( !sithAIMove_bDebugMoveToPo )
    {
        return 1;
    }

    SithThing* pMarkTpl = sithTemplate_GetTemplate("+x_mark");
    if ( !pMarkTpl )
    {
        return 1;
    }

    rdVector3 look = rdroid_g_zVector3;
    rdMatrix34 markOrient;
    rdMatrix_BuildFromLook34(&markOrient, &look);

    rdVector3 markPos = *moveToPos;
    markPos.z += 0.001f; // Added: z offset
    sithAIMove_pMoveToDebugMarkThing = sithThing_CreateThingAtPos(pMarkTpl, &markPos, &markOrient, pLocal->pOwner->pInSector, NULL);

    float thingHeight = sithPhysics_GetThingHeight(pLocal->pOwner);
    markPos.z = markPos.z - thingHeight + 0.001f;
    sithAIMove_pMoveToPosGroundMarkThing = sithThing_CreateThingAtPos(pMarkTpl, &markPos, &markOrient, pLocal->pOwner->pInSector, NULL);
#endif

    return 1;
}

int J3DAPI sithAIMove_AISetMoveTargetPos(SithAIControlBlock* pLocal, const rdVector3* moveToPos, float moveSpeed)
{
    SITH_ASSERTREL(pLocal);
    SITH_ASSERTREL(moveToPos);
    if ( (pLocal->mode & SITHAI_MODE_DISABLED) != 0 && (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) == 0 )
    {
        SITHLOG_ERROR("WARNING!  Move target issued to '%s'. AI is DISABLED.\n", pLocal->pOwner->aName);
    }

    pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_1;
    if ( !sithAIMove_AISetMovePos(pLocal, moveToPos, moveSpeed) )
    {
        return 0;
    }

    pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_1;
    return 1;
}

int J3DAPI sithAIMove_AISetMoveTargetPos2(SithAIControlBlock* pLocal, const rdVector3* moveToPos, float moveSpeed)
{
    SITH_ASSERTREL(pLocal);
    SITH_ASSERTREL(moveToPos);
    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_1) != 0 )
    {
        return 0;
    }

    if ( (pLocal->mode & SITHAI_MODE_DISABLED) != 0 && (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_INEDITOR) == 0 )
    {
        SITHLOG_ERROR("WARNING!  Move target issued to '%s'. AI is DISABLED.\n", pLocal->pOwner->aName);
    }

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_2) != 0 )
    {
        return 0;
    }

    memcpy(&pLocal->vecUnknown3, &pLocal->movePos, sizeof(pLocal->vecUnknown3));
    if ( !sithAIMove_AISetMoveTargetPos(pLocal, moveToPos, moveSpeed) )
    {
        return 0;
    }

    pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_2;
    return 1;
}

int J3DAPI sithAIMove_AISpecialMove(SithAIControlBlock* pLocal, SithActorSpecialMoveFlags moveFlags)
{
    double v3;
    double v4;
    float v5;
    BOOL v6;
    float v7;
    float v8;
    BOOL v9;
    float v10;
    float v11;
    float v12;
    float v13;
    float v14;
    SithThing* pThing;
    SithPuppetSubMode rightTurnMode;
    BOOL v17;
    rdVector3 pHitNorm;
    int v19;
    SithPuppetSubMode leftTurnMode;
    int v21;

    rdVector3 newUVec;
    rdVector3 endPos;
    int v25;

    rdVector3 pTarget;

    v17 = 0;

    SITH_ASSERTREL(pLocal && pLocal->pOwner);

    pThing = pLocal->pOwner;

    rightTurnMode = 0;
    if ( (moveFlags & SITHACTORSPECIALMOVE_ROLL) != 0 )
    {
        if ( rdVector_Dot3(&pThing->orient.lvec, &pLocal->toTarget) <= 0.40000001f )
        {
            return 0;
        }

        leftTurnMode = SITHPUPPETSUBMODE_HOPLEFT;
        rightTurnMode = SITHPUPPETSUBMODE_HOPRIGHT;
    }
    else if ( (moveFlags & SITHACTORSPECIALMOVE_STRAFE) != 0 )
    {
        if ( rdVector_Dot3(&pThing->orient.lvec, &pLocal->toTarget) <= 0.40000001f )
        {
            return 0;
        }

        leftTurnMode = SITHPUPPETSUBMODE_STRAFELEFT;
        rightTurnMode = SITHPUPPETSUBMODE_STRAFERIGHT;
    }
    else if ( (moveFlags & SITHACTORSPECIALMOVE_TURN45) != 0 )
    {
        leftTurnMode = SITHPUPPETSUBMODE_WHIPCLIMBUP;
        rightTurnMode = SITHPUPPETSUBMODE_WHIPCLIMBDOWN;
    }
    else if ( (moveFlags & SITHACTORSPECIALMOVE_TURN90) != 0 )
    {
        leftTurnMode = SITHPUPPETSUBMODE_WHIPCLIMBLEFT;
        rightTurnMode = SITHPUPPETSUBMODE_WHIPCLIMBRIGHT;
    }
    else if ( (moveFlags & SITHACTORSPECIALMOVE_TURN135) != 0 )
    {
        leftTurnMode = SITHPUPPETSUBMODE_WHIPCLIMBDISMOUNT;
        rightTurnMode = SITHPUPPETSUBMODE_WHIPSWINGMOUNT;
    }
    else if ( (moveFlags & SITHACTORSPECIALMOVE_TURN180) != 0 )
    {
        leftTurnMode = SITHPUPPETSUBMODE_WHIPCLIMBMOUNT;
        v17 = 1;
    }
    else
    {
        if ( (moveFlags & SITHACTORSPECIALMOVE_MOUNTWALL) == 0 )
        {
            return 0;
        }

        leftTurnMode = SITHPUPPETSUBMODE_MOUNTWALL;
        v17 = 1;
    }

    if ( !v17 )
    {
        v17 = pThing->pPuppetClass->aModes[pThing->pPuppetState->majorMode][rightTurnMode].pKeyframe != 0;
    }

    if ( !pThing->pPuppetClass->aModes[pThing->pPuppetState->majorMode][leftTurnMode].pKeyframe || !v17 )
    {
        return 0;
    }

    bool bRight = false;
    if ( (moveFlags & (SITHACTORSPECIALMOVE_STRAFE | SITHACTORSPECIALMOVE_ROLL)) != 0 )
    {
        if ( (moveFlags & SITHACTORSPECIALMOVE_ROLL) != 0 )
        {
            v12 = 0.21000001f;
        }
        else
        {
            v12 = 0.079999998f;
        }

        float distance = v12;

        bRight = true;
        if ( (moveFlags & SITHACTORSPECIALMOVE_DIR_RANDOM) != 0 )
        {
            bRight = SITH_RAND() >= 0.5f;
        }

        else if ( (moveFlags & SITHACTORSPECIALMOVE_DIR_LEFT) != 0 )// left
        {
            bRight = 0;
        }

        if ( bRight )
        {
            memcpy(&newUVec, &pThing->orient, sizeof(newUVec));
        }
        else
        {
            newUVec.x = -pThing->orient.rvec.x;
            newUVec.y = -pThing->orient.rvec.y;
            newUVec.z = -pThing->orient.rvec.z;
        }

        endPos.x = distance * 0.5f * newUVec.x + pThing->pos.x;
        endPos.y = distance * 0.5f * newUVec.y + pThing->pos.y;
        endPos.z = distance * 0.5f * newUVec.z + pThing->pos.z;

        pTarget.x = newUVec.x * distance + pThing->pos.x;
        pTarget.y = newUVec.y * distance + pThing->pos.y;
        pTarget.z = newUVec.z * distance + pThing->pos.z;

        v25 = 0;
        while ( 1 )
        {
            v19 = sithAIUtil_CheckPathToPoint(pThing, &pTarget, pThing->collide.movesize, &distance, &pHitNorm, 1, 0);
            if ( !v19 || (moveFlags & (SITHACTORSPECIALMOVE_STRAFE | SITHACTORSPECIALMOVE_ROLL)) == 0 )
            {
                break;
            }

            if ( ++v25 >= 2 )
            {
                return 0;
            }

            bRight = 1 - bRight;
            newUVec.x = -newUVec.x;
            newUVec.y = -newUVec.y;
            newUVec.z = -newUVec.z;
        }

        if ( (moveFlags & SITHACTORSPECIALMOVE_ROLL) != 0 )
        {
            v21 = sithAIUtil_CheckPosition(pLocal, &endPos, 0);
            if ( v21 != 1 )
            {
                return 0;
            }
        }

        if ( (moveFlags & (SITHACTORSPECIALMOVE_STRAFE | SITHACTORSPECIALMOVE_ROLL)) != 0 )
        {
            v21 = sithAIUtil_CheckPosition(pLocal, &pTarget, 0);
            if ( v21 != 1 )
            {
                return 0;
            }
        }
    }
    else if ( (moveFlags & (SITHACTORSPECIALMOVE_TURN135 | SITHACTORSPECIALMOVE_TURN90 | SITHACTORSPECIALMOVE_TURN45)) != 0 )
    {
        bRight = (moveFlags & SITHACTORSPECIALMOVE_DIR_LEFT) == 0;
    }

    if ( (moveFlags & SITHACTORSPECIALMOVE_STRAFE) != 0 )
    {
        sithAIMove_StopAIMovement(pLocal);
        if ( bRight )
        {
            pLocal->pOwner->moveStatus = SITHPLAYERMOVE_STRAFING_RIGHT;
            sithPuppet_SetSubMode(pLocal->pOwner, SITHPUPPETSUBMODE_STRAFERIGHT, sithAIMove_PuppetCallback);
        }
        else
        {
            pLocal->pOwner->moveStatus = SITHPLAYERMOVE_STRAFING_LEFT;
            sithPuppet_SetSubMode(pLocal->pOwner, SITHPUPPETSUBMODE_STRAFELEFT, sithAIMove_PuppetCallback);
        }

        if ( pLocal->pTargetThing )
        {
            sithAIMove_AISetLookThing(pLocal, pLocal->pTargetThing);
        }

        sithAIMove_AISetMoveTargetPos(pLocal, &pTarget, 1.0f);
        return 1;
    }

    else if ( (moveFlags & SITHACTORSPECIALMOVE_ROLL) != 0 )
    {
        sithAIMove_StopAIMovement(pLocal);
        sithAIMove_ResetAILook(pLocal);
        if ( bRight )
        {
            pLocal->pOwner->moveStatus = SITHPLAYERMOVE_ROLLING_RIGHT;
            sithPuppet_SetSubMode(pLocal->pOwner, SITHPUPPETSUBMODE_HOPRIGHT, sithAIMove_PuppetCallback);
        }
        else
        {
            pLocal->pOwner->moveStatus = SITHPLAYERMOVE_ROLLING_LEFT;
            sithPuppet_SetSubMode(pLocal->pOwner, SITHPUPPETSUBMODE_HOPLEFT, sithAIMove_PuppetCallback);
        }

        sithAIMove_AISetMoveTargetPos(pLocal, &pTarget, 2.0f);
        return 1;
    }

    else if ( (moveFlags & SITHACTORSPECIALMOVE_TURN45) != 0 )
    {
        if ( bRight )
        {
            pLocal->pOwner->moveStatus = SITHPLAYERMOVE_TURNING_RIGHT_45_DEGREES;
            sithPuppet_SetSubMode(pLocal->pOwner, SITHPUPPETSUBMODE_WHIPCLIMBDOWN, sithAIMove_PuppetCallback);
        }
        else
        {
            pLocal->pOwner->moveStatus = SITHPLAYERMOVE_TURNING_LEFT_45_DEGREES;
            sithPuppet_SetSubMode(pLocal->pOwner, SITHPUPPETSUBMODE_WHIPCLIMBUP, sithAIMove_PuppetCallback);
        }

        return 1;
    }

    else if ( (moveFlags & SITHACTORSPECIALMOVE_TURN90) != 0 )
    {
        if ( bRight )
        {
            pLocal->pOwner->moveStatus = SITHPLAYERMOVE_TURNING_RIGHT_90_DEGREES;
            sithPuppet_SetSubMode(pLocal->pOwner, SITHPUPPETSUBMODE_WHIPCLIMBRIGHT, sithAIMove_PuppetCallback);
        }
        else
        {
            pLocal->pOwner->moveStatus = SITHPLAYERMOVE_TURNING_LEFT_90_DEGREES;
            sithPuppet_SetSubMode(pLocal->pOwner, SITHPUPPETSUBMODE_WHIPCLIMBLEFT, sithAIMove_PuppetCallback);
        }

        return 1;
    }

    else if ( (moveFlags & SITHACTORSPECIALMOVE_TURN135) != 0 )
    {
        if ( bRight )
        {
            pLocal->pOwner->moveStatus = SITHPLAYERMOVE_TURNING_RIGHT_135_DEGREES;
            sithPuppet_SetSubMode(pLocal->pOwner, SITHPUPPETSUBMODE_WHIPSWINGMOUNT, sithAIMove_PuppetCallback);
        }
        else
        {
            pLocal->pOwner->moveStatus = SITHPLAYERMOVE_TURNING_LEFT_135_DEGREES;
            sithPuppet_SetSubMode(pLocal->pOwner, SITHPUPPETSUBMODE_WHIPCLIMBDISMOUNT, sithAIMove_PuppetCallback);
        }

        return 1;
    }
    else                                        // wall mount
    {
        if ( (moveFlags & SITHACTORSPECIALMOVE_TURN180) != 0 )
        {
            pLocal->pOwner->moveStatus = SITHPLAYERMOVE_TURNING_180_DEGREES;
            sithPuppet_SetSubMode(pLocal->pOwner, SITHPUPPETSUBMODE_WHIPCLIMBMOUNT, sithAIMove_PuppetCallback);
            return 1;
        }

        if ( (moveFlags & SITHACTORSPECIALMOVE_MOUNTWALL) == 0 )
        {
            return 0;
        }

        v3 = rdMath_DeltaAngleNormalized(&pLocal->vecUnknown3, &pThing->orient.uvec, &pThing->orient.lvec);
        v14 = v3;
        if ( v3 < 0.0f )
        {
            v11 = -v14;
        }
        else
        {
            v11 = v3;
        }

        if ( v11 < 80.0f )
        {
            v9 = 0;
        }
        else
        {
            if ( v14 < 0.0f )
            {
                v10 = -v14;
            }
            else
            {
                v10 = v3;
            }

            v9 = v10 <= 100.0f;
        }

        if ( v9 )
        {
            pLocal->pOwner->moveStatus = SITHPLAYERMOVE_MOUNTING_WALL;
            sithPuppet_SetSubMode(pLocal->pOwner, SITHPUPPETSUBMODE_MOUNTWALL, sithAIMove_PuppetCallback);
            return 1;
        }

        if ( v14 < 0.0f )
        {
            v8 = -v14;
        }
        else
        {
            v8 = v3;
        }

        if ( v8 < 60.0f )
        {
            v6 = 0;
        }
        else
        {
            if ( v14 < 0.0f )
            {
                v7 = -v14;
            }
            else
            {
                v7 = v3;
            }

            v6 = v7 <= 79.0f;
        }

        if ( v6 )
        {
            newUVec.x = pLocal->vecUnknown3.x * -0.2f + pThing->orient.uvec.x;
            newUVec.y = pLocal->vecUnknown3.y * -0.2f + pThing->orient.uvec.y;
            newUVec.z = pLocal->vecUnknown3.z * -0.2f + pThing->orient.uvec.z;
            rdVector_Normalize3Acc(&newUVec);

            v4 = (pThing->orient.lvec.x - 0.0f) * newUVec.x + (pThing->orient.lvec.y - 0.0f) * newUVec.y + (pThing->orient.lvec.z - 0.0f) * newUVec.z;
            v13 = v4;
            pTarget.x = -v4 * newUVec.x + pThing->orient.lvec.x;
            pTarget.y = -v13 * newUVec.y + pThing->orient.lvec.y;
            pTarget.z = -v13 * newUVec.z + pThing->orient.lvec.z;
            rdVector_Normalize3Acc(&pTarget);

            memcpy(&pThing->orient.uvec, &newUVec, sizeof(pThing->orient.uvec));

            pThing->orient.rvec.x = pThing->orient.lvec.y * pThing->orient.uvec.z - pThing->orient.lvec.z * pThing->orient.uvec.y;
            pThing->orient.rvec.y = pThing->orient.lvec.z * pThing->orient.uvec.x - pThing->orient.lvec.x * pThing->orient.uvec.z;
            pThing->orient.rvec.z = pThing->orient.lvec.x * pThing->orient.uvec.y - pThing->orient.lvec.y * pThing->orient.uvec.x;
            rdVector_Normalize3Acc(&pThing->orient.rvec);

            pThing->orient.lvec.x = pThing->orient.uvec.y * pThing->orient.rvec.z - pThing->orient.uvec.z * pThing->orient.rvec.y;
            pThing->orient.lvec.y = pThing->orient.uvec.z * pThing->orient.rvec.x - pThing->orient.uvec.x * pThing->orient.rvec.z;
            pThing->orient.lvec.z = pThing->orient.uvec.x * pThing->orient.rvec.y - pThing->orient.uvec.y * pThing->orient.rvec.x;

            pLocal->pOwner->moveStatus = SITHPLAYERMOVE_MOUNTING_WALL;
            sithPuppet_SetSubMode(pLocal->pOwner, SITHPUPPETSUBMODE_MOUNTWALL, sithAIMove_PuppetCallback);
            return 1;
        }

        v5 = v14 >= 0.0f ? v3 : -v14;
        if ( v5 <= 40.0f )
        {
            return 0;
        }
        else
        {
            sithAIMove_AIFinalizeSpecialMove(pLocal, SITHACTORSPECIALMOVE_MOUNTWALL);
            return 1;
        }
    }
}

void J3DAPI sithAIMove_AIFinalizeSpecialMove(SithAIControlBlock* pLocal, SithActorSpecialMoveFlags moveFlags)
{
    SithThing* pThing;
    rdVector3 pyr;

    pThing = pLocal->pOwner;
    switch ( moveFlags )
    {
        case SITHACTORSPECIALMOVE_STRAFE:
            sithAIMove_StopAIMovement(pLocal);
            sithAIUtil_ApplyForce(pLocal, &pLocal->moveDirection, -0.050000001f);
            break;

        case SITHACTORSPECIALMOVE_TURN45:
        case SITHACTORSPECIALMOVE_TURN90:
        case SITHACTORSPECIALMOVE_TURN135:
        case SITHACTORSPECIALMOVE_TURN180:
            if ( pThing->moveStatus == SITHPLAYERMOVE_TURNING_LEFT_45_DEGREES )
            {
                pyr.x = 0.0f;
                pyr.y = 45.0f;
                pyr.z = 0.0f;
            }

            if ( pThing->moveStatus == SITHPLAYERMOVE_TURNING_RIGHT_45_DEGREES )
            {
                pyr.x = 0.0f;
                pyr.y = -45.0f;
                pyr.z = 0.0f;
            }

            if ( pThing->moveStatus == SITHPLAYERMOVE_TURNING_LEFT_90_DEGREES )
            {
                pyr.x = 0.0f;
                pyr.y = 90.0f;
                pyr.z = 0.0f;
            }

            if ( pThing->moveStatus == SITHPLAYERMOVE_TURNING_RIGHT_90_DEGREES )
            {
                pyr.x = 0.0f;
                pyr.y = -90.0f;
                pyr.z = 0.0f;
            }

            if ( pThing->moveStatus == SITHPLAYERMOVE_TURNING_LEFT_135_DEGREES )
            {
                pyr.x = 0.0f;
                pyr.y = 135.0f;
                pyr.z = 0.0f;
            }

            if ( pThing->moveStatus == SITHPLAYERMOVE_TURNING_RIGHT_135_DEGREES )
            {
                pyr.x = 0.0f;
                pyr.y = -135.0f;
                pyr.z = 0.0f;
            }

            if ( pThing->moveStatus == SITHPLAYERMOVE_TURNING_180_DEGREES )
            {
                pyr.x = 0.0f;
                pyr.y = 180.0f;
                pyr.z = 0.0f;
            }

            rdMatrix_PreRotate34(&pThing->orient, &pyr);
            rdMatrix_Normalize34(&pThing->orient);
            break;

        case SITHACTORSPECIALMOVE_MOUNTWALL:
            sithThing_DetachThing(pThing);

            memcpy(&pThing->orient.uvec, &pLocal->vecUnknown3, sizeof(pThing->orient.uvec));

            pThing->orient.lvec.x = pThing->orient.uvec.y * pThing->orient.rvec.z - pThing->orient.uvec.z * pThing->orient.rvec.y;
            pThing->orient.lvec.y = pThing->orient.uvec.z * pThing->orient.rvec.x - pThing->orient.uvec.x * pThing->orient.rvec.z;
            pThing->orient.lvec.z = pThing->orient.uvec.x * pThing->orient.rvec.y - pThing->orient.uvec.y * pThing->orient.rvec.x;
            rdVector_Normalize3Acc(&pThing->orient.lvec);

            sithPhysics_FindFloor(pThing, 1);
            if ( pThing->moveStatus == SITHPLAYERMOVE_MOUNTING_WALL )
            {
                sithCollision_MoveThing(pThing, &pThing->orient.lvec, pThing->collide.movesize, 0);
            }

            break;

        default:
            return;
    }
}

void J3DAPI sithAIMove_AIJump(SithAIControlBlock* pLocal, rdVector3* movePos, float jumpDirection)
{
    memcpy(&pLocal->movePos, movePos, sizeof(pLocal->movePos));
    pLocal->moveSpeed = 2.0f;
    if ( sithPuppet_PlayMode(pLocal->pOwner, SITHPUPPETSUBMODE_JUMPUP, 0) < 0 )
    {
        sithPlayerActions_Jump(pLocal->pOwner, jumpDirection, 0);
    }

    pLocal->msecPauseMoveUntil = sithTime_g_msecGameTime + 2000;
    pLocal->mode |= SITHAI_MODE_MOVING;
}

void J3DAPI sithAIMove_AIStop(SithAIControlBlock* pLocal)
{
    SITH_ASSERTREL(pLocal && pLocal->pOwner);
    sithAIMove_StopAIMovement(pLocal);
    sithAIMove_ResetAILook(pLocal);

    memcpy(&pLocal->movePos, &pLocal->pOwner->pos, sizeof(pLocal->movePos));

    sithAI_EmitEvent(pLocal, SITHAI_EVENT_GOAL_UNREACHABLE, NULL);
    sithAIMove_UpdateAIMove(pLocal);
}

void J3DAPI sithAIMove_SetGoalReached(SithAIControlBlock* pLocal)
{
    SITH_ASSERTREL(pLocal && pLocal->pOwner);
    sithAI_EmitEvent(pLocal, SITHAI_EVENT_GOAL_REACHED, NULL);
    if ( (pLocal->mode & SITHAI_MODE_BLOCK) != 0 )
    {
        sithCog_ThingSendMessage(pLocal->pOwner, NULL, SITHCOG_MSG_ARRIVED);
    }

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_4000) == 0 && (pLocal->mode & SITHAI_MODE_TRAVERSEWPNTS) == 0 )
    {
        sithAIMove_StopAIMovement(pLocal);
        if ( !sithAIMove_AIGetMoveState(pLocal) )
        {
            sithAIMove_UpdateAIMove(pLocal);
        }
    }
}

void J3DAPI sithAIMove_Unreachable(SithAIControlBlock* pLocal)
{
    SITH_ASSERTREL(pLocal && pLocal->pOwner);
    if ( (pLocal->mode & SITHAI_MODE_BLOCK) != 0 )
    {
        sithCog_ThingSendMessage(pLocal->pOwner, NULL, SITHCOG_MSG_BLOCKED);
    }
    else
    {
        sithAIMove_StopAIMovement(pLocal);
        sithAI_EmitEvent(pLocal, SITHAI_EVENT_GOAL_UNREACHABLE, NULL);
        sithAIMove_UpdateAIMove(pLocal);
    }
}

void J3DAPI sithAIMove_StopAIMovement(SithAIControlBlock* pLocal)
{
    sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_MOVING);
    sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TRAVERSEWPNTS);

    if ( (pLocal->submode & SITHAI_SUBMODE_CONTINUOUSMOTION) != 0 )
    {
        pLocal->mode |= SITHAI_MODE_MOVING;
    }
    else
    {
        sithSoundClass_StopMode(pLocal->pOwner, SITHSOUNDCLASS_MOVING);
        memset(&pLocal->pOwner->moveInfo.physics.velocity, 0, sizeof(pLocal->pOwner->moveInfo.physics.velocity));
    }
}

void J3DAPI sithAIMove_ResetAILook(SithAIControlBlock* pLocal)
{
    sithActor_SetHeadPYR(pLocal->pOwner, &rdroid_g_zeroVector3);
    memcpy(&pLocal->goalLVec, &pLocal->pOwner->orient.lvec, sizeof(pLocal->goalLVec));
    sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TURNING);
}

void J3DAPI sithAIMove_UpdateBoss(SithAIControlBlock* pLocal, float secDeltaTime)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_UpdateBoss, pLocal, secDeltaTime);

    // Turns a boss toward the look direction; like sithAIMove_sub_4958B0 without special turns and move states
    SithThing* pThing = pLocal->pOwner;
    rdVector3 goalDir = pLocal->goalLVec;
    const rdVector3* pUp = &pThing->orient.uvec;

    if ( fabsf(rdVector_Dot3(pUp, &goalDir)) > 0.99f )
    {
        pLocal->mode &= ~SITHAI_MODE_TURNING;
        SITHLOG_ERROR("WARNING! AI '%s' looking straight up or down.\n", pThing->aName);
        return;
    }

    float dist = -((goalDir.x * pUp->x + goalDir.z * pUp->z) + goalDir.y * pUp->y);
    rdVector3 lookDir;
    lookDir.x = pUp->x * dist + goalDir.x;
    lookDir.y = pUp->y * dist + goalDir.y;
    lookDir.z = pUp->z * dist + goalDir.z;
    rdVector_Normalize3Acc(&lookDir);

    float angle    = rdMath_DeltaAngleNormalized(&lookDir, &pThing->orient.lvec, pUp);
    float turnStep = pThing->thingInfo.actorInfo.maxRotVelocity * secDeltaTime;
    if ( fabsf(angle) > 1.05f * turnStep )
    {
        sithAIMove_TurnStep(pThing, angle, turnStep);
        return;
    }

    sithAIMove_AlignLook(pThing, &lookDir);
    pLocal->mode &= ~SITHAI_MODE_TURNING;
}

void J3DAPI sithAIMove_sub_499090(SithAIControlBlock* pLocal, float secDeltaTime)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_499090, pLocal, secDeltaTime);

    // Turns the head toward the target thing or the look position
    SITH_ASSERTREL(pLocal && pLocal->pOwner);

    SithThing* pThing  = pLocal->pOwner;
    SithThing* pTarget = pLocal->pTargetThing;
    SithActorInfo* pActor = &pThing->thingInfo.actorInfo;
    rdVector3 headPYR = pActor->headPYR;

    if ( (pActor->flags & SITH_AF_IMMOBILE) != 0 || (pActor->flags & SITH_AF_CANROTATEHEAD) == 0 )
    {
        return;
    }

    rdVector3 eyePos;
    eyePos.x = 0.0f;
    eyePos.y = 0.0f;
    eyePos.z = pActor->eyeOffset.z;
    rdMatrix_TransformVector34Acc(&eyePos, &pThing->orient);
    eyePos.x = pThing->pos.x + eyePos.x;
    eyePos.y = pThing->pos.y + eyePos.y;
    eyePos.z = pThing->pos.z + eyePos.z;

    if ( pTarget && (pActor->flags & SITH_AF_SEEINVISIBLE) == 0 )
    {
        if ( (pTarget->type == SITH_THING_ACTOR || pTarget->type == SITH_THING_PLAYER)
            && (pTarget->thingInfo.actorInfo.flags & SITH_AF_INVISIBLE) != 0 )
        {
            pTarget = NULL;
        }

        if ( (pActor->flags & SITH_AF_BLIND) != 0 )
        {
            pTarget = NULL;
        }
    }

    const rdVector3* pUp = &pThing->orient.uvec;
    rdVector3 targetDir  = { 0 };
    rdVector3 targetFlat = { 0 };
    if ( pTarget )
    {
        rdVector3 targetPos;
        if ( pTarget->type == SITH_THING_ACTOR || pTarget->type == SITH_THING_PLAYER )
        {
            rdMatrix_TransformVector34(&targetPos, &pTarget->thingInfo.actorInfo.eyeOffset, &pTarget->orient);
            targetPos.x = pTarget->pos.x + targetPos.x;
            targetPos.y = pTarget->pos.y + targetPos.y;
            targetPos.z = pTarget->pos.z + targetPos.z;
        }
        else
        {
            targetPos = pTarget->pos;
        }

        rdVector_Sub3(&targetDir, &targetPos, &eyePos);
        rdVector_Normalize3Acc(&targetDir);

        float dist = -((targetDir.z * pUp->z + targetDir.y * pUp->y) + targetDir.x * pUp->x);
        targetFlat.x = pUp->x * dist + targetDir.x;
        targetFlat.y = pUp->y * dist + targetDir.y;
        targetFlat.z = pUp->z * dist + targetDir.z;
        rdVector_Normalize3Acc(&targetFlat);
    }

    rdVector3 lookDir;
    rdVector_Sub3(&lookDir, &pLocal->lookPos, &eyePos);
    rdVector_Normalize3Acc(&lookDir);

    rdVector3 lookFlat;
    rdMath_ProjectPointOntoPlaneNormalized(&lookFlat, &lookDir, pUp, &rdroid_g_zeroVector3);

    // UNKNOWN_800: the head follows the target; it starts once the target is in front and stops when it gets behind
    float minAimDot = -0.2f;
    if ( !pTarget )
    {
        pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_800;
    }
    else if ( (pActor->flags & SITH_AF_NOSLOPEMOVE) == 0 && (pLocal->mode & SITHAI_MODE_BLOCK) == 0 )
    {
        float targetDot = rdVector_Dot3(&pThing->orient.lvec, &targetFlat);
        if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_800) != 0 )
        {
            if ( targetDot < -0.2f )
            {
                pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_800;
            }
        }
        else if ( targetDot > sithAIMove_g_flt_58546C )
        {
            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_800;
        }
    }
    else
    {
        minAimDot = -1.0f;
        pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_800;
    }

    const rdVector3* pAimFlat = &lookFlat;
    const rdVector3* pAimDir  = &lookDir;
    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_800) != 0 )
    {
        pAimFlat = &targetFlat;
        pAimDir  = &targetDir;
    }

    const rdVector3* pLook = &pThing->orient.lvec;
    float aimDot    = (pLook->x * pAimFlat->x + pLook->z * pAimFlat->z) + pLook->y * pAimFlat->y;
    float yawStep   = pActor->maxHeadVelocity * secDeltaTime;
    float pitchStep = pActor->maxHeadVelocity * 0.5f * secDeltaTime;

    // Yaw
    float yawDelta = aimDot >= minAimDot ? rdMath_DeltaAngleNormalized(pLook, pAimFlat, pUp) : 0.0f;
    yawDelta = yawDelta - pActor->headPYR.y;
    if ( yawDelta < -yawStep )
    {
        yawDelta = -yawStep;
    }
    else if ( yawDelta > yawStep )
    {
        yawDelta = yawStep;
    }

    headPYR.y = yawDelta + headPYR.y;
    if ( headPYR.y > -pActor->maxHeadYaw && headPYR.y < pActor->maxHeadYaw )
    {
        pLocal->submode |= SITHAI_SUBMODE_HEADTRACKINGMOTION;
    }
    else
    {
        headPYR.y = (headPYR.y < 0.0f ? -1.0f : 1.0f) * pActor->maxHeadYaw;
        pLocal->submode &= ~SITHAI_SUBMODE_HEADTRACKINGMOTION;
    }

    // Pitch
    float pitchDelta = aimDot > 0.0f ? rdMath_DeltaAngleNormalized(pAimFlat, pAimDir, &pThing->orient.rvec) : 0.0f;
    pitchDelta = pitchDelta - pActor->headPYR.x;
    if ( pitchDelta < -pitchStep )
    {
        pitchDelta = -pitchStep;
    }
    else if ( pitchDelta > pitchStep )
    {
        pitchDelta = pitchStep;
    }

    float pitch = pitchDelta + headPYR.x;
    if ( pitch < pActor->minHeadPitch )
    {
        headPYR.x = pActor->minHeadPitch;
    }
    else if ( pitch > pActor->maxHeadPitch )
    {
        headPYR.x = pActor->maxHeadPitch;
    }
    else
    {
        headPYR.x = pitch;
    }

    sithActor_SetHeadPYR(pLocal->pOwner, &headPYR);
}

void J3DAPI sithAIMove_sub_4996C0(SithAIControlBlock* pLocal, float secDeltaTime)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_4996C0, pLocal, secDeltaTime);

    // Turns the head toward the look position
    SITH_ASSERTREL(pLocal && pLocal->pOwner);

    SithThing* pThing = pLocal->pOwner;
    SithActorInfo* pActor = &pThing->thingInfo.actorInfo;
    rdVector3 headPYR = pActor->headPYR;
    rdVector3 lookPos = pLocal->lookPos;

    if ( (pActor->flags & SITH_AF_CANROTATEHEAD) == 0 || (pActor->flags & SITH_AF_IMMOBILE) != 0 )
    {
        return;
    }

    rdVector3 eyePos;
    eyePos.x = 0.0f;
    eyePos.y = 0.0f;
    eyePos.z = pActor->eyeOffset.z;
    rdMatrix_TransformVector34Acc(&eyePos, &pThing->orient);
    eyePos.x = pThing->pos.x + eyePos.x;
    eyePos.y = pThing->pos.y + eyePos.y;
    eyePos.z = pThing->pos.z + eyePos.z;

    rdVector3 lookDir;
    rdVector_Sub3(&lookDir, &lookPos, &eyePos);
    rdVector_Normalize3Acc(&lookDir);

    const rdVector3* pUp = &pThing->orient.uvec;
    float dist = -((lookDir.z * pUp->z + lookDir.x * pUp->x) + lookDir.y * pUp->y);
    rdVector3 lookFlat;
    lookFlat.x = pUp->x * dist + lookDir.x;
    lookFlat.y = pUp->y * dist + lookDir.y;
    lookFlat.z = pUp->z * dist + lookDir.z;
    rdVector_Normalize3Acc(&lookFlat);

    float yawStep   = pActor->maxHeadVelocity * secDeltaTime;
    float pitchStep = pActor->maxHeadVelocity * 0.5f * secDeltaTime;

    // Yaw
    const rdVector3* pLook = &pThing->orient.lvec;
    float yawDelta = rdMath_DeltaAngleNormalized(pLook, &lookFlat, pUp) - pActor->headPYR.y;
    if ( yawDelta < -yawStep )
    {
        yawDelta = -yawStep;
    }
    else if ( yawDelta > yawStep )
    {
        yawDelta = yawStep;
    }

    float yaw = yawDelta + headPYR.y;
    if ( yaw < -pActor->maxHeadYaw )
    {
        headPYR.y = -pActor->maxHeadYaw;
    }
    else if ( yaw > pActor->maxHeadYaw )
    {
        headPYR.y = pActor->maxHeadYaw;
    }
    else
    {
        headPYR.y = yaw;
    }

    // Pitch
    float lookDot = (pLook->x * lookFlat.x + pLook->z * lookFlat.z) + pLook->y * lookFlat.y;
    float pitchDelta = lookDot > 0.0f ? rdMath_DeltaAngleNormalized(&lookFlat, &lookDir, &pThing->orient.rvec) : 0.0f;
    pitchDelta = pitchDelta - pActor->headPYR.x;
    if ( pitchDelta < -pitchStep )
    {
        pitchDelta = -pitchStep;
    }
    else if ( pitchDelta > pitchStep )
    {
        pitchDelta = pitchStep;
    }

    float pitch = pitchDelta + headPYR.x;
    if ( pitch < pActor->minHeadPitch )
    {
        headPYR.x = pActor->minHeadPitch;
    }
    else if ( pitch > pActor->maxHeadPitch )
    {
        headPYR.x = pActor->maxHeadPitch;
    }
    else
    {
        headPYR.x = pitch;
    }

    sithActor_SetHeadPYR(pLocal->pOwner, &headPYR);

    if ( fabsf(yawDelta) <= 0.5f && fabsf(pitchDelta) <= 0.5f )
    {
        pLocal->mode &= ~SITHAI_MODE_UNKNOWN_80;
    }
}

void J3DAPI sithAIMove_sub_499A80(SithAIControlBlock* pLocal, float* pTargetYaw, float* pJoint1Pitch, float* pJoint2YawDelta, float* pJoint2PitchDelta)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_499A80, pLocal, pTargetYaw, pJoint1Pitch, pJoint2YawDelta, pJoint2PitchDelta);

    // Aim of a Quetzalcoatl strike at the target thing (strikes 1 and 2)
    SithThing* pThing  = pLocal->pOwner;
    SithThing* pTarget = pLocal->pTargetThing;

    rdVector3 eyePos = pThing->pos;
    eyePos.z = pThing->thingInfo.actorInfo.eyeOffset.z + eyePos.z;

    rdVector3 targetEye;
    rdMatrix_TransformVector34(&targetEye, &pTarget->thingInfo.actorInfo.eyeOffset, &pTarget->orient);
    rdVector_Add3Acc(&targetEye, &pTarget->pos);
    if ( (pTarget->moveInfo.physics.flags & SITH_PF_CROUCHING) != 0 )
    {
        targetEye.z = targetEye.z - 0.1f;
    }

    rdVector3 toTarget;
    rdVector_Sub3(&toTarget, &targetEye, &eyePos);
    rdVector_Normalize3Acc(&toTarget);

    const rdVector3* pUp = &pThing->orient.uvec;
    float dist = -((toTarget.y * pUp->y + toTarget.z * pUp->z) + toTarget.x * pUp->x);
    rdVector3 targetFlat;
    targetFlat.x = pUp->x * dist + toTarget.x;
    targetFlat.y = pUp->y * dist + toTarget.y;
    targetFlat.z = pUp->z * dist + toTarget.z;
    rdVector_Normalize3Acc(&targetFlat);

    *pTargetYaw      = rdMath_DeltaAngleNormalized(&pThing->orient.lvec, &targetFlat, pUp);
    *pJoint2YawDelta = -pThing->renderData.apTweakedAngles[2].y;

    // The pitches follow the target's horizontal distance
    float dx = pThing->pos.x - pTarget->pos.x;
    float dy = pThing->pos.y - pTarget->pos.y;
    float targetDist = sqrtf(dx * dx + dy * dy);
    if ( targetDist < sithAIMove_strikeMinDist )
    {
        targetDist = sithAIMove_strikeMinDist;
    }
    else if ( targetDist > sithAIMove_strikeMaxDist )
    {
        targetDist = sithAIMove_strikeMaxDist;
    }

    float distRange = sithAIMove_strikeMaxDist - sithAIMove_strikeMinDist;
    *pJoint1Pitch = (targetDist - sithAIMove_strikeMinDist)
        * ((sithAIMove_strikeFarJoint1Pitch - sithAIMove_strikeNearJoint1Pitch) / distRange)
        + sithAIMove_strikeNearJoint1Pitch;

    *pJoint2PitchDelta = (targetDist - sithAIMove_strikeMinDist)
        * ((sithAIMove_strikeFarJoint2Pitch - sithAIMove_g_flt_585470) / distRange)
        + sithAIMove_g_flt_585470
        - pThing->renderData.apTweakedAngles[2].x;
}

void J3DAPI sithAIMove_sub_499CA0(SithAIControlBlock* pLocal, float secDeltaTime)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_499CA0, pLocal, secDeltaTime);

    // Plays Quetzalcoatl strike 1, aimed at the target thing
    SithThing* pThing = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pThing) )
    {
        return;
    }

    SithQuetzStrike* pStrike = &pThing->userblock.pQuetz->strike;
    if ( pStrike->unknown0 != 1 )
    {
        return;
    }

    int snapNum = pStrike->snapShot;
    SithQuetzSnapShot* pNext = &pStrike->aSnapShots[snapNum];
    float secLeft = pNext->unknown0 - pStrike->unknown1;

    // Aim the snapshots once the strike reaches them
    float targetYaw, joint1Pitch, joint2YawDelta, joint2PitchDelta;
    const rdVector3* aAngles = pThing->renderData.apTweakedAngles;
    switch ( snapNum )
    {
        case 1:
            sithAIMove_sub_499A80(pLocal, &targetYaw, &joint1Pitch, &joint2YawDelta, &joint2PitchDelta);
            pNext->angle1 = 20.0f;
            pNext->angle2 = targetYaw * 0.5f;
            pNext->angle3 = aAngles[2].x - 20.0f;
            pNext->angle4 = joint2YawDelta * 0.5f + aAngles[2].y;
            pNext->angle5 = pStrike->aSnapShots[0].angle5;
            break;

        case 2:
            sithAIMove_sub_499A80(pLocal, &targetYaw, &joint1Pitch, &joint2YawDelta, &joint2PitchDelta);
            pNext->angle1 = joint1Pitch * 0.9f;
            pNext->angle2 = targetYaw * 0.9f;
            pNext->angle3 = joint2PitchDelta * 0.9f + aAngles[2].x;
            pNext->angle4 = joint2YawDelta * 0.9f + aAngles[2].y;
            pNext->angle5 = -30.0f;
            break;

        case 3:
            sithAIMove_sub_499A80(pLocal, &targetYaw, &joint1Pitch, &joint2YawDelta, &joint2PitchDelta);
            pNext->angle1 = joint1Pitch;
            pNext->angle2 = targetYaw;
            pNext->angle3 = aAngles[2].x + joint2PitchDelta;
            pNext->angle4 = aAngles[2].y + joint2YawDelta;
            pNext->angle5 = 20.0f;
            break;

        case 4:
            sithAIMove_sub_499A80(pLocal, &targetYaw, &joint1Pitch, &joint2YawDelta, &joint2PitchDelta);
            pNext->angle1 = 10.0f;
            pNext->angle2 = targetYaw * 0.4f;
            pNext->angle3 = aAngles[2].x - 10.0f;
            pNext->angle4 = joint2YawDelta * 0.4f + aAngles[2].y;
            pNext->angle5 = pStrike->aSnapShots[0].angle5;
            break;

        default:
            if ( snapNum == pStrike->unknown3 - 1 )
            {
                // Return the head to where it is now
                pNext->angle3 = aAngles[2].x;
                pNext->angle4 = aAngles[2].y;
            }
            break;
    }

    if ( sithAIMove_PlayStrike(pThing, pStrike, pNext, secDeltaTime, secLeft) )
    {
        sithAIMove_sub_499CA0(pLocal, secDeltaTime - secLeft);
    }
}

void J3DAPI sithAIMove_sub_49A020(SithAIControlBlock* pLocal, float* pTargetYaw, float* pJoint1Pitch, float* pJoint2YawDelta, float* pJoint2PitchDelta)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_49A020, pLocal, pTargetYaw, pJoint1Pitch, pJoint2YawDelta, pJoint2PitchDelta);

    // Aim of Quetzalcoatl strike 2: the yaw toward the target thing and fixed pitches
    SithThing* pThing  = pLocal->pOwner;
    SithThing* pTarget = pLocal->pTargetThing;

    rdVector3 eyePos = pThing->pos;
    eyePos.z = pThing->thingInfo.actorInfo.eyeOffset.z + eyePos.z;

    rdVector3 targetEye;
    rdMatrix_TransformVector34(&targetEye, &pTarget->thingInfo.actorInfo.eyeOffset, &pTarget->orient);
    targetEye.x = pTarget->pos.x + targetEye.x;
    targetEye.y = pTarget->pos.y + targetEye.y;
    targetEye.z = pTarget->pos.z + targetEye.z;

    rdVector3 toTarget;
    rdVector_Sub3(&toTarget, &targetEye, &eyePos);
    rdVector_Normalize3Acc(&toTarget);

    const rdVector3* pUp = &pThing->orient.uvec;
    float dist = -((toTarget.z * pUp->z + toTarget.x * pUp->x) + toTarget.y * pUp->y);
    rdVector3 targetFlat;
    targetFlat.x = pUp->x * dist + toTarget.x;
    targetFlat.y = pUp->y * dist + toTarget.y;
    targetFlat.z = pUp->z * dist + toTarget.z;
    rdVector_Normalize3Acc(&targetFlat);

    *pTargetYaw        = rdMath_DeltaAngleNormalized(&pThing->orient.lvec, &targetFlat, pUp);
    *pJoint2YawDelta   = -pThing->renderData.apTweakedAngles[2].y;
    *pJoint2PitchDelta = 50.0f;
    *pJoint1Pitch      = -30.0f;
}

void J3DAPI sithAIMove_sub_49A1B0(SithAIControlBlock* pLocal, float secDeltaTime)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_49A1B0, pLocal, secDeltaTime);

    // Plays Quetzalcoatl strike 2, aimed at the target thing
    SithThing* pThing = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pThing) )
    {
        return;
    }

    SithQuetzStrike* pStrike = &pThing->userblock.pQuetz->strike;
    if ( pStrike->unknown0 != 2 )
    {
        return;
    }

    int snapNum = pStrike->snapShot;
    SithQuetzSnapShot* pNext = &pStrike->aSnapShots[snapNum];
    float secLeft = pNext->unknown0 - pStrike->unknown1;

    float targetYaw, joint1Pitch, joint2YawDelta, joint2PitchDelta;
    const rdVector3* aAngles = pThing->renderData.apTweakedAngles;
    if ( snapNum == 1 )
    {
        sithAIMove_sub_49A020(pLocal, &targetYaw, &joint1Pitch, &joint2YawDelta, &joint2PitchDelta);
        pNext->angle1 = 20.0f;
        pNext->angle2 = targetYaw * 0.5f;
        pNext->angle3 = aAngles[2].x + 10.0f;
        pNext->angle4 = joint2YawDelta * 0.5f + aAngles[2].y;
        pNext->angle5 = pStrike->aSnapShots[0].angle5;
    }
    else if ( snapNum == 2 )
    {
        sithAIMove_sub_49A020(pLocal, &targetYaw, &joint1Pitch, &joint2YawDelta, &joint2PitchDelta);
        pNext->angle1 = joint1Pitch;
        pNext->angle2 = targetYaw;
        pNext->angle3 = aAngles[2].x + joint2PitchDelta;
        pNext->angle4 = aAngles[2].y + joint2YawDelta;
        pNext->angle5 = -70.0f;
    }
    else if ( snapNum == pStrike->unknown3 - 1 )
    {
        pNext->angle3 = aAngles[2].x;
        pNext->angle4 = aAngles[2].y;
    }

    if ( sithAIMove_PlayStrike(pThing, pStrike, pNext, secDeltaTime, secLeft) )
    {
        sithAIMove_sub_49A1B0(pLocal, secDeltaTime - secLeft);
    }
}

void J3DAPI sithAIMove_sub_49A450(SithAIControlBlock* pLocal, float secDeltaTime)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_49A450, pLocal, secDeltaTime);

    // Plays Quetzalcoatl strike 3
    SithThing* pThing = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pThing) )
    {
        return;
    }

    SithQuetzStrike* pStrike = &pThing->userblock.pQuetz->strike;
    if ( pStrike->unknown0 != 3 )
    {
        return;
    }

    SithQuetzSnapShot* pNext = &pStrike->aSnapShots[pStrike->snapShot];
    float secLeft = pNext->unknown0 - pStrike->unknown1;
    if ( pStrike->snapShot == pStrike->unknown3 - 1 )
    {
        pNext->angle3 = pThing->renderData.apTweakedAngles[2].x;
        pNext->angle4 = pThing->renderData.apTweakedAngles[2].y;
    }

    if ( sithAIMove_PlayStrike(pThing, pStrike, pNext, secDeltaTime, secLeft) )
    {
        sithAIMove_sub_49A450(pLocal, secDeltaTime - secLeft);
    }
}

void J3DAPI sithAIMove_sub_49A630(SithAIControlBlock* pLocal, float secDeltaTime)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_49A630, pLocal, secDeltaTime);

    // Plays Quetzalcoatl strike 5
    SithThing* pThing = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pThing) )
    {
        return;
    }

    SithQuetzStrike* pStrike = &pThing->userblock.pQuetz->strike;
    if ( pStrike->unknown0 != 5 )
    {
        return;
    }

    SithQuetzSnapShot* pNext = &pStrike->aSnapShots[pStrike->snapShot];
    float secLeft = pNext->unknown0 - pStrike->unknown1;
    if ( pStrike->snapShot == pStrike->unknown3 - 1 )
    {
        pNext->angle3 = pThing->renderData.apTweakedAngles[2].x;
        pNext->angle4 = pThing->renderData.apTweakedAngles[2].y;
    }

    if ( sithAIMove_PlayStrike(pThing, pStrike, pNext, secDeltaTime, secLeft) )
    {
        sithAIMove_sub_49A630(pLocal, secDeltaTime - secLeft);
    }
}

void J3DAPI sithAIMove_sub_49A810(SithAIControlBlock* pLocal, float secDeltaTime)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_49A810, pLocal, secDeltaTime);

    // Plays Quetzalcoatl strike 4
    SithThing* pThing = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pThing) )
    {
        return;
    }

    SithQuetzStrike* pStrike = &pThing->userblock.pQuetz->strike;
    if ( pStrike->unknown0 != 4 )
    {
        return;
    }

    SithQuetzSnapShot* pNext = &pStrike->aSnapShots[pStrike->snapShot];
    float secLeft = pNext->unknown0 - pStrike->unknown1;
    if ( sithAIMove_PlayStrike(pThing, pStrike, pNext, secDeltaTime, secLeft) )
    {
        sithAIMove_sub_49A810(pLocal, secDeltaTime - secLeft);
    }
}

void J3DAPI sithAIMove_UpdateQuetzTail(SithAIControlBlock* pLocal, float secDeltaTime)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_UpdateQuetzTail, pLocal, secDeltaTime);

    // Plays the running Quetzalcoatl strike
    SithThing* pThing = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pThing) )
    {
        return;
    }

    switch ( pThing->userblock.pQuetz->strike.unknown0 )
    {
        case 1:
            sithAIMove_sub_499CA0(pLocal, secDeltaTime);
            break;

        case 2:
            sithAIMove_sub_49A1B0(pLocal, secDeltaTime);
            break;

        case 3:
            sithAIMove_sub_49A450(pLocal, secDeltaTime);
            break;

        case 4:
            sithAIMove_sub_49A810(pLocal, secDeltaTime);
            break;

        case 5:
            sithAIMove_sub_49A630(pLocal, secDeltaTime);
            break;

        default:
            break;
    }
}

void J3DAPI sithAIMove_sub_49AA60(SithAIControlBlock* pLocal)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_49AA60, pLocal);

    // Starts Quetzalcoatl strike 1; snapshots 1-4 are aimed while it plays
    SithThing* pThing = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pThing) )
    {
        return;
    }

    SithQuetzStrike* pStrike = &pThing->userblock.pQuetz->strike;

    // Note: the results are unused (but the call needs a target thing)
    float targetYaw, joint1Pitch, joint2YawDelta, joint2PitchDelta;
    sithAIMove_sub_499A80(pLocal, &targetYaw, &joint1Pitch, &joint2YawDelta, &joint2PitchDelta);

    sithAIMove_StartStrike(pStrike, pThing);
    pStrike->aSnapShots[1].unknown0 = 0.11f;
    pStrike->aSnapShots[2].unknown0 = 0.24f;
    pStrike->aSnapShots[3].unknown0 = 0.3f;
    pStrike->aSnapShots[4].unknown0 = 0.5f;
    pStrike->aSnapShots[5].unknown0 = 0.7f;
    sithAIMove_GetStrikePose(&pStrike->aSnapShots[5], pThing);

    pStrike->unknown0 = 1;
    pStrike->unknown3 = 6;
}

void J3DAPI sithAIMove_sub_49AB80(SithAIControlBlock* pLocal)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_49AB80, pLocal);

    // Starts Quetzalcoatl strike 2; snapshots 1 and 2 are aimed while it plays
    SithThing* pThing = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pThing) )
    {
        return;
    }

    SithQuetzStrike* pStrike = &pThing->userblock.pQuetz->strike;
    sithAIMove_StartStrike(pStrike, pThing);
    pStrike->aSnapShots[1].unknown0 = 0.11f;
    pStrike->aSnapShots[2].unknown0 = 0.24f;
    pStrike->aSnapShots[3].unknown0 = 0.5f;
    sithAIMove_GetStrikePose(&pStrike->aSnapShots[3], pThing);

    pStrike->unknown0 = 2;
    pStrike->unknown3 = 4;
}

void J3DAPI sithAIMove_sub_49AC50(SithAIControlBlock* pLocal)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_49AC50, pLocal);

    // Starts Quetzalcoatl strike 3
    SithThing* pThing = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pThing) )
    {
        return;
    }

    SithQuetzStrike* pStrike = &pThing->userblock.pQuetz->strike;
    sithAIMove_StartStrike(pStrike, pThing);
    memcpy(&pStrike->aSnapShots[1], sithAIMove_aStrikeSnapShots, 13 * sizeof(SithQuetzSnapShot));
    pStrike->aSnapShots[14].unknown0 = 5.0f;
    sithAIMove_GetStrikePose(&pStrike->aSnapShots[14], pThing);

    pStrike->unknown0 = 3;
    pStrike->unknown3 = 15;
}

void J3DAPI sithAIMove_sub_49AF80(SithAIControlBlock* pLocal)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_49AF80, pLocal);

    // Starts Quetzalcoatl strike 5
    SithThing* pThing = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pThing) )
    {
        return;
    }

    SithQuetzStrike* pStrike = &pThing->userblock.pQuetz->strike;
    sithAIMove_StartStrike(pStrike, pThing);
    memcpy(&pStrike->aSnapShots[1], sithAIMove_aStrikeSnapShots, 8 * sizeof(SithQuetzSnapShot));
    pStrike->aSnapShots[9].unknown0 = 3.0f;
    sithAIMove_GetStrikePose(&pStrike->aSnapShots[9], pThing);

    pStrike->unknown0 = 5;
    pStrike->unknown3 = 10;
}

void J3DAPI sithAIMove_sub_49B1B0(SithAIControlBlock* pLocal)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_sub_49B1B0, pLocal);

    // Starts Quetzalcoatl strike 4
    SithThing* pThing = pLocal->pOwner;
    if ( !sithThing_CreateQuetzUserBlock(pThing) )
    {
        return;
    }

    SithQuetzStrike* pStrike = &pThing->userblock.pQuetz->strike;
    sithAIMove_StartStrike(pStrike, pThing);

    SithQuetzSnapShot* pSnap = &pStrike->aSnapShots[1];
    pSnap->unknown0 = 1.6f;
    pSnap->angle1   = 0.0f;
    pSnap->angle2   = 0.0f;
    pSnap->angle3   = 0.0f;
    pSnap->angle4   = 0.0f;
    pSnap->angle5   = -60.0f;

    pSnap = &pStrike->aSnapShots[2];
    pSnap->unknown0 = 5.0f;
    pSnap->angle1   = 0.0f;
    pSnap->angle2   = 0.0f;
    pSnap->angle3   = 0.0f;
    pSnap->angle4   = 0.0f;
    pSnap->angle5   = -60.0f;

    pStrike->unknown0 = 4;
    pStrike->unknown3 = 3;
}

void J3DAPI sithAIMove_UpdateMardukTail(SithThing* pThing, float secDeltaTime)
{
    INDY_AB_ORIGINAL_VOID(sithAIMove_UpdateMardukTail, pThing, secDeltaTime);

    // Spins Marduk's abdomen at two turns per second
    if ( stdUtil_StrCmp(pThing->aName, "marduk") && stdUtil_StrCmp(pThing->aName, "marduk2") )
    {
        return;
    }

    int jointNum = sithThing_GetThingJointIndex(pThing, "wmabdomen");
    if ( jointNum == -1 )
    {
        return;
    }

    // Note: only the step is normalized, the joint yaw itself grows without bound
    rdVector3* pPYR = &pThing->renderData.apTweakedAngles[jointNum];
    pPYR->y = stdMath_NormalizeAngle(720.0f * secDeltaTime) + pPYR->y;
}

static void sithAIMove_ProcessWeaponAim(SithThing* pThing, float secDeltaTime)
{
    // TODO(native): sithPlayerControls_ProcessWeaponAim is file-local in sithPlayerControls.c; call it through the exe
    // entry point, which is hooked to the C version (or runs the original with INDY_NOHOOK)
    J3D_CALLFUNCFAR(sithPlayerControls_ProcessWeaponAim_ADDR, sithPlayerControls_ProcessWeaponAim_TYPE, pThing, secDeltaTime);
}

static float sithAIMove_GetTurnStep(const SithThing* pThing, float angle, float secDeltaTime)
{
    float turnStep  = pThing->thingInfo.actorInfo.maxRotVelocity * secDeltaTime;
    float slowAngle = pThing->thingInfo.actorInfo.maxRotVelocity * 0.07f;

    // Slow down close to the goal direction
    if ( fabsf(angle) <= slowAngle && fabsf(angle) >= 1.0f )
    {
        turnStep = fabsf(angle) / slowAngle * turnStep;
    }

    return turnStep;
}

static void sithAIMove_TurnStep(SithThing* pThing, float angle, float turnStep)
{
    rdVector3 pyr;
    pyr.x = 0.0f;
    pyr.y = angle < 0.0f ? turnStep : -turnStep;
    pyr.z = 0.0f;
    rdMatrix_PreRotate34(&pThing->orient, &pyr);
    rdMatrix_Normalize34(&pThing->orient);
}

static void sithAIMove_AlignLook(SithThing* pThing, const rdVector3* pLookDir)
{
    pThing->orient.lvec = *pLookDir;
    rdVector_Cross3(&pThing->orient.rvec, &pThing->orient.lvec, &pThing->orient.uvec);
    rdVector_Normalize3Acc(&pThing->orient.rvec);
    rdVector_Cross3(&pThing->orient.uvec, &pThing->orient.rvec, &pThing->orient.lvec);
}

static void sithAIMove_SetTurnMoveStatus(SithThing* pThing, float angle)
{
    if ( angle < 0.0f )
    {
        pThing->moveStatus = angle < -45.0f ? SITHPLAYERMOVE_UNKNOWN_105 : SITHPLAYERMOVE_TURNING_LEFT;
    }
    else
    {
        pThing->moveStatus = angle > 45.0f ? SITHPLAYERMOVE_UNKNOWN_106 : SITHPLAYERMOVE_TURNING_RIGHT;
    }
}

static void sithAIMove_SetMineCarGunYaw(const SithThing* pThing, rdVector3* pGunPYR, const rdVector3* pDir)
{
    const rdMatrix34* pOrient = &pThing->orient;
    float rightDot = (pOrient->rvec.y * pDir->y + pOrient->rvec.x * pDir->x) + pOrient->rvec.z * pDir->z;
    float lookDot  = (pOrient->lvec.x * pDir->x + pOrient->lvec.z * pDir->z) + pOrient->lvec.y * pDir->y;

    pGunPYR->y = 90.0f - stdMath_ArcSin3(lookDot);
    if ( rightDot > 0.0f )
    {
        pGunPYR->y = -pGunPYR->y;
    }
}

static void sithAIMove_StartStrike(SithQuetzStrike* pStrike, const SithThing* pThing)
{
    pStrike->unknown1 = 0.0f;
    pStrike->snapShot = 0;
    pStrike->aSnapShots[0].unknown0 = 0.0f;
    sithAIMove_GetStrikePose(&pStrike->aSnapShots[0], pThing);
}

static void sithAIMove_GetStrikePose(SithQuetzSnapShot* pSnap, const SithThing* pThing)
{
    const rdVector3* aAngles = pThing->renderData.apTweakedAngles;
    pSnap->angle1 = aAngles[1].x;
    pSnap->angle2 = aAngles[1].y;
    pSnap->angle3 = aAngles[2].x;
    pSnap->angle4 = aAngles[2].y;
    pSnap->angle5 = aAngles[28].x;
}

static void sithAIMove_SetStrikePose(SithThing* pThing, const SithQuetzSnapShot* pSnap)
{
    rdVector3* aAngles = pThing->renderData.apTweakedAngles;
    aAngles[1].x  = pSnap->angle1;
    aAngles[1].y  = pSnap->angle2;
    aAngles[2].x  = pSnap->angle3;
    aAngles[2].y  = pSnap->angle4;
    aAngles[28].x = pSnap->angle5;
}

static bool sithAIMove_PlayStrike(SithThing* pThing, SithQuetzStrike* pStrike, const SithQuetzSnapShot* pNext, float secDeltaTime, float secLeft)
{
    // Returns true when the strike passed snapshot pNext with time left over (secDeltaTime - secLeft)
    if ( secDeltaTime <= secLeft )
    {
        SITH_ASSERTREL(pStrike->snapShot != 0);

        float secTime = secDeltaTime + pStrike->unknown1;
        pStrike->unknown1 = secTime;

        const SithQuetzSnapShot* pPrev = &pStrike->aSnapShots[pStrike->snapShot - 1];
        float t = (secTime - pPrev->unknown0) / (pNext->unknown0 - pPrev->unknown0);
        if ( t < 0.0f )
        {
            t = 0.0f;
        }
        else if ( t > 1.0f )
        {
            t = 1.0f;
        }

        rdVector3* aAngles = pThing->renderData.apTweakedAngles;
        aAngles[1].x  = (pNext->angle1 - pPrev->angle1) * t + pPrev->angle1;
        aAngles[1].y  = (pNext->angle2 - pPrev->angle2) * t + pPrev->angle2;
        aAngles[2].x  = (pNext->angle3 - pPrev->angle3) * t + pPrev->angle3;
        aAngles[2].y  = (pNext->angle4 - pPrev->angle4) * t + pPrev->angle4;
        aAngles[28].x = (pNext->angle5 - pPrev->angle5) * t + pPrev->angle5;
        return false;
    }

    ++pStrike->snapShot;
    pStrike->unknown1 = pNext->unknown0;
    sithAIMove_SetStrikePose(pThing, pNext);
    if ( pStrike->snapShot == pStrike->unknown3 )
    {
        pStrike->unknown0 = 0;
        return false;
    }

    return true;
}
