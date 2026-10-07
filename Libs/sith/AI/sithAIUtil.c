#include "sithAIUtil.h"
#include <j3dcore/j3dhook.h>

#include <indy/indyCrt.h>

#include <rdroid/Engine/rdLight.h>
#include <rdroid/Math/rdMatrix.h>
#include <rdroid/Math/rdVector.h>

#include <sith/AI/sithAI.h>
#include <sith/AI/sithAIMove.h>
#include <sith/Cog/sithCog.h>
#include <sith/Devices/sithComm.h>
#include <sith/Dss/sithDSS.h>
#include <sith/Dss/sithMulti.h>
#include <sith/Engine/sithCollision.h>
#include <sith/Engine/sithPhysics.h>
#include <sith/Engine/sithPuppet.h>
#include <sith/Gameplay/sithInventory.h>
#include <sith/Gameplay/sithPlayer.h>
#include <sith/Gameplay/sithTime.h>
#include <sith/RTI/symbols.h>
#include <sith/World/sithActor.h>
#include <sith/World/sithSoundClass.h>
#include <sith/World/sithWeapon.h>
#include <sith/World/sithWorld.h>

#include <std/General/stdMath.h>
#include <std/General/stdUtil.h>

#include <math.h>
#include <stdlib.h>

#define sithAIUtil_thingsInViewFovY J3D_DECL_FAR_VAR(sithAIUtil_thingsInViewFovY, float)
#define sithAIUtil_numVisitedSectors J3D_DECL_FAR_VAR(sithAIUtil_numVisitedSectors, size_t)
#define sithAIUtil_activeWpntLayer J3D_DECL_FAR_VAR(sithAIUtil_activeWpntLayer, SithAIWaypointLayerFlag)
#define sithAIUtil_numThingsInView J3D_DECL_FAR_VAR(sithAIUtil_numThingsInView, size_t)
#define sithAIUtil_vec_585490 J3D_DECL_FAR_VAR(sithAIUtil_vec_585490, rdVector3)
#define sithAIUtil_pMkPointCurLocal J3D_DECL_FAR_VAR(sithAIUtil_pMkPointCurLocal, SithAIControlBlock*)
#define sithAIUtil_mkPointCurPYR J3D_DECL_FAR_VAR(sithAIUtil_mkPointCurPYR, rdVector3)
#define sithAIUtil_thingsInViewFovX J3D_DECL_FAR_VAR(sithAIUtil_thingsInViewFovX, float)
#define sithAIUtil_aWpntOwners J3D_DECL_FAR_ARRAYVAR(sithAIUtil_aWpntOwners, SithAIWaypointOwner(*)[10])
#define sithAIUtil_aThingsInView J3D_DECL_FAR_VAR(sithAIUtil_aThingsInView, SithThing**)
#define sithAIUtil_aWpntDistances J3D_DECL_FAR_ARRAYVAR(sithAIUtil_aWpntDistances, SithAIWaypointDistance(*)[60])
#define sithAIUtil_sizeThingsInView J3D_DECL_FAR_VAR(sithAIUtil_sizeThingsInView, int)
#define sithAIUtil_vec_585718 J3D_DECL_FAR_VAR(sithAIUtil_vec_585718, rdVector3)
#define sithAIUtil_maxDistanceToThingsInView J3D_DECL_FAR_VAR(sithAIUtil_maxDistanceToThingsInView, float)
#define sithAIUtil_thingInViewTypeMask J3D_DECL_FAR_VAR(sithAIUtil_thingInViewTypeMask, int)
#define sithAIUtil_aAIWpnts J3D_DECL_FAR_ARRAYVAR(sithAIUtil_aAIWpnts, SithAIWaypoint(*)[60])
#define sithAIUtil_secLastUpdate J3D_DECL_FAR_VAR(sithAIUtil_secLastUpdate, float)
#define sithAIUtil_mkPoinCurFlags J3D_DECL_FAR_VAR(sithAIUtil_mkPoinCurFlags, int)

#define SITHAIUTIL_MAXWAYPOINTS  STD_ARRAYLEN(sithAIUtil_aAIWpnts)
#define SITHAIUTIL_MAXWPNTOWNERS STD_ARRAYLEN(sithAIUtil_aWpntOwners)
#define SITHAIUTIL_MAXWPNTLINKS  STD_ARRAYLEN(sithAIUtil_aAIWpnts[0].aWpntLinks)

// The low byte of SithAIWaypoint.flags is the waypoint's rank
#define SITHAIUTIL_WPNTRANK_MASK 0xFFu
#define SITHAIUTIL_WPNTLAYERS (SITH_AIWPNT_LAYER0 | SITH_AIWPNT_LAYER1 | SITH_AIWPNT_LAYER2 | SITH_AIWPNT_LAYER3 | SITH_AIWPNT_LAYER4 | SITH_AIWPNT_LAYER5)

// SithAIWaypoint.goalProximity of the goal waypoint; every link away from it is one less
#define SITHAIUTIL_GOALPROXIMITY 0xFF

// Defined in sithWeapon.c, not declared in its header
SithThing* J3DAPI sithWeapon_WeaponFire(SithThing* pShooter, const SithThing* pProjectileTemplate, const rdVector3* pFireDir, rdVector3* pFirePos, tSoundHandle hFireSnd, SithPuppetSubMode submode, float extra, SithFireProjectileFlags projectileFlags, float secDeltaTime);

void sithAIUtil_InstallHooks(void)
{
    // Uncomment only lines for functions that have full definition and doesn't call original function (non-thunk functions)

    J3D_HOOKFUNC(sithAIUtil_sub_49B2E0);
    J3D_HOOKFUNC(sithAIUtil_sub_49B640);
    J3D_HOOKFUNC(sithAIUtil_GetDistanceToTarget);
    J3D_HOOKFUNC(sithAIUtil_GetDistanceToTargetPos);
    J3D_HOOKFUNC(sithAIUtil_AIFire);
    J3D_HOOKFUNC(sithAIUtil_AIPauseMove);
    J3D_HOOKFUNC(sithAIUtil_AIResetWaypoints);
    J3D_HOOKFUNC(sithAIUtil_SyncWpnts);
    J3D_HOOKFUNC(sithAIUtil_ProcessSyncWpnts);
    J3D_HOOKFUNC(sithAIUtil_AISetWpnt);
    J3D_HOOKFUNC(sithAIUtil_AIGetWpntThing);
    J3D_HOOKFUNC(sithAIUtil_AISetWpntRank);
    J3D_HOOKFUNC(sithAIUtil_AISetWpntFlags);
    J3D_HOOKFUNC(sithAIUtil_AIClearWpntFlags);
    J3D_HOOKFUNC(sithAIUtil_AISetActiveWpntLayer);
    J3D_HOOKFUNC(sithAIUtil_AIConnectWpnts);
    J3D_HOOKFUNC(sithAIUtil_AIMoveToNextWpnt);
    J3D_HOOKFUNC(sithAIUtil_AITraverseWpnts);
    J3D_HOOKFUNC(sithAIUtil_AIAdvanceToNextWpnt);
    J3D_HOOKFUNC(sithAIUtil_sub_49CC60);
    J3D_HOOKFUNC(sithAIUtil_sub_49D170);
    J3D_HOOKFUNC(sithAIUtil_AIFindNearestWpnt);
    J3D_HOOKFUNC(sithAIUtil_sub_49D640);
    J3D_HOOKFUNC(sithAIUtil_sub_49D750);
    J3D_HOOKFUNC(sithAIUtil_CreateAIWaypoint);
    J3D_HOOKFUNC(sithAIUtil_ClearAIWaypoint);
    J3D_HOOKFUNC(sithAIUtil_GetAIWaypointNum);
    J3D_HOOKFUNC(sithAIUtil_CompareAIWpntDistances);
    J3D_HOOKFUNC(sithAIUtil_RenderAIWaypoints);
    J3D_HOOKFUNC(sithAIUtil_CheckPathToPoint);
    J3D_HOOKFUNC(sithAIUtil_CheckPosition);
    J3D_HOOKFUNC(sithAIUtil_CheckFloorAtPos);
    J3D_HOOKFUNC(sithAIUtil_MakeRandPoint);
    J3D_HOOKFUNC(sithAIUtil_RetryMakeRandPoint);
    J3D_HOOKFUNC(sithAIUtil_CheckPathToPos);
    J3D_HOOKFUNC(sithAIUtil_sub_49EE50);
    J3D_HOOKFUNC(sithAIUtil_sub_49F010);
    J3D_HOOKFUNC(sithAIUtil_MakePathPos);
    J3D_HOOKFUNC(sithAIUtil_sub_49F1F0);
    J3D_HOOKFUNC(sithAIUtil_AIPlaySoundMode);
    J3D_HOOKFUNC(sithAIUtil_AISetMode);
    J3D_HOOKFUNC(sithAIUtil_AIClearMode);
    J3D_HOOKFUNC(sithAIUtil_SetWeaponFireFlags);
    J3D_HOOKFUNC(sithAIUtil_AIGetMovePos);
    J3D_HOOKFUNC(sithAIUtil_GetXYHeadingVector);
    J3D_HOOKFUNC(sithAIUtil_GetXYZHeadingVector);
    J3D_HOOKFUNC(sithAIUtil_CanAttackTarget);
    J3D_HOOKFUNC(sithAIUtil_IsThingMoving);
    J3D_HOOKFUNC(sithAIUtil_ApplyForce);
    J3D_HOOKFUNC(sithAIUtil_RandomRotate);
    J3D_HOOKFUNC(sithAIUtil_GetThingsInView);
    J3D_HOOKFUNC(sithAIUtil_GetNextThingsInView);
    J3D_HOOKFUNC(sithAIUtil_CanSeeTarget);
}

void sithAIUtil_ResetGlobals(void)
{
    memset(&sithAIUtil_thingsInViewFovY, 0, sizeof(sithAIUtil_thingsInViewFovY));
    memset(&sithAIUtil_numVisitedSectors, 0, sizeof(sithAIUtil_numVisitedSectors));
    memset(&sithAIUtil_g_bRenderAIWpnts, 0, sizeof(sithAIUtil_g_bRenderAIWpnts));
    memset(&sithAIUtil_activeWpntLayer, 0, sizeof(sithAIUtil_activeWpntLayer));
    memset(&sithAIUtil_numThingsInView, 0, sizeof(sithAIUtil_numThingsInView));
    memset(&sithAIUtil_vec_585490, 0, sizeof(sithAIUtil_vec_585490));
    memset(&sithAIUtil_pMkPointCurLocal, 0, sizeof(sithAIUtil_pMkPointCurLocal));
    memset(&sithAIUtil_mkPointCurPYR, 0, sizeof(sithAIUtil_mkPointCurPYR));
    memset(&sithAIUtil_thingsInViewFovX, 0, sizeof(sithAIUtil_thingsInViewFovX));
    memset(&sithAIUtil_aWpntOwners, 0, sizeof(sithAIUtil_aWpntOwners));
    memset(&sithAIUtil_aThingsInView, 0, sizeof(sithAIUtil_aThingsInView));
    memset(&sithAIUtil_aWpntDistances, 0, sizeof(sithAIUtil_aWpntDistances));
    memset(&sithAIUtil_sizeThingsInView, 0, sizeof(sithAIUtil_sizeThingsInView));
    memset(&sithAIUtil_vec_585718, 0, sizeof(sithAIUtil_vec_585718));
    memset(&sithAIUtil_maxDistanceToThingsInView, 0, sizeof(sithAIUtil_maxDistanceToThingsInView));
    memset(&sithAIUtil_thingInViewTypeMask, 0, sizeof(sithAIUtil_thingInViewTypeMask));
    memset(&sithAIUtil_aAIWpnts, 0, sizeof(sithAIUtil_aAIWpnts));
    memset(&sithAIUtil_secLastUpdate, 0, sizeof(sithAIUtil_secLastUpdate));
    memset(&sithAIUtil_mkPoinCurFlags, 0, sizeof(sithAIUtil_mkPoinCurFlags));
}

// Random number from 0 to 1
static inline float sithAIUtil_RandFraction(void)
{
    return (float)rand() * (1.0f / 32767.0f);
}

static inline float sithAIUtil_ZeroIfTiny(float value)
{
    return fabsf(value) > 0.00001f ? value : 0.0f;
}

static inline bool sithAIUtil_IsWpntActive(size_t wpntNum)
{
    SithAIWaypointLayerFlag flags = sithAIUtil_aAIWpnts[wpntNum].flags;
    return (flags & SITH_AIWPNT_DISABLED) == 0 && (flags & sithAIUtil_activeWpntLayer) != 0;
}

static inline SithThing* sithAIUtil_GetWpntThing(size_t wpntNum)
{
    return &sithWorld_g_pCurrentWorld->aThings[sithAIUtil_aAIWpnts[wpntNum].thingNum];
}

// SithAIWaypointOwner.waypointNum holds two waypoint numbers: the waypoint passed last in bits 0-7 and the one being
// moved to in bits 8-15
static inline size_t sithAIUtil_GetPrevWpnt(const SithAIWaypointOwner* pEntry)
{
    return (uint32_t)pEntry->waypointNum & 0xFFu;
}

static inline size_t sithAIUtil_GetCurWpnt(const SithAIWaypointOwner* pEntry)
{
    return ((uint32_t)pEntry->waypointNum >> 8) & 0xFFu;
}

static inline void sithAIUtil_SetPrevWpnt(SithAIWaypointOwner* pEntry, size_t wpntNum)
{
    pEntry->waypointNum = (int)(((uint32_t)pEntry->waypointNum & ~0xFFu) | (uint32_t)wpntNum);
}

static inline void sithAIUtil_SetCurWpnt(SithAIWaypointOwner* pEntry, size_t wpntNum)
{
    pEntry->waypointNum = (int)(((uint32_t)pEntry->waypointNum & ~0xFF00u) | ((uint32_t)wpntNum << 8));
}

// Updates the AI's view of its target (pTargetThing, or targetPos when there's none), once per frame unless a fire
// flag asks for it again: fire position, direction and distance to the target and whether it's in sight.
void J3DAPI sithAIUtil_sub_49B2E0(SithAIControlBlock* pLocal, SithAIUtilFireFlags flags)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_sub_49B2E0, pLocal, flags);

    float collideSize = 0.0f;
    int prevSightState = pLocal->targetSightState;
    SithThing* pOwner = pLocal->pOwner;

    if ( (flags & (SITHAIUTIL_FIRE_LOB | SITHAIUTIL_FIRE_PRIMARY | SITHAIUTIL_FIRE_ALT)) == 0 && (size_t)pLocal->unknown124 == sithMain_g_frameNumber )
    {
        return;
    }

    pLocal->unknown124 = (int)sithMain_g_frameNumber;

    if ( sithWeapon_HasWeaponSelected(pOwner) )
    {
        collideSize = sithWeapon_GetWeaponCollideSize((SithWeaponId)pOwner->thingInfo.actorInfo.weaponInfo.curWeaponID);
    }
    else if ( pOwner->thingInfo.actorInfo.pWeaponTemplate )
    {
        collideSize = pOwner->thingInfo.actorInfo.pWeaponTemplate->collide.movesize;
    }

    rdMatrix_TransformVector34(&pLocal->weaponFirePos, &pOwner->thingInfo.actorInfo.fireOffset, &pOwner->orient);
    rdVector_Add3Acc(&pLocal->weaponFirePos, &pOwner->pos);

    rdVector3 viewPos = pLocal->weaponFirePos;
    if ( (pLocal->submode & SITHAI_SUBMODE_FIREADDEYEOFFSET) != 0 && !rdVector_IsZero3(&pOwner->thingInfo.actorInfo.eyeOffset) )
    {
        viewPos.z = pOwner->thingInfo.actorInfo.eyeOffset.z * 0.5f + viewPos.z;
    }

    pLocal->mode &= ~SITHAI_MODE_TARGETVISIBLE;

    SithThing* pTarget = pLocal->pTargetThing;
    if ( pTarget )
    {
        if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_SEEINVISIBLE) == 0 )
        {
            if ( (pTarget->type == SITH_THING_ACTOR || pTarget->type == SITH_THING_PLAYER) && (pTarget->thingInfo.actorInfo.flags & SITH_AF_INVISIBLE) != 0 )
            {
                prevSightState = 3;
            }

            if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_BLIND) != 0 )
            {
                prevSightState = 3;
            }
        }

        pLocal->targetPos = pTarget->pos;
        pLocal->targetSightState = sithAIUtil_GetDistanceToTarget(pOwner, &viewPos, pTarget, pLocal->pClass->fov, pLocal->pClass->sightDistance, collideSize, &pLocal->toTarget, &pLocal->distance);
        if ( pLocal->targetSightState == 0 )
        {
            // A target that wasn't in sight (or is hard to see) is noticed only by chance
            if ( prevSightState != 0 && !sithAIUtil_CanSeeTarget(pLocal, pTarget, pLocal->distance) )
            {
                pLocal->targetSightState = 3;
                return;
            }

            pLocal->vecUnknown122 = pTarget->pos;
            pLocal->msecAttackStart = sithTime_g_msecGameTime;
            pLocal->mode |= SITHAI_MODE_TARGETVISIBLE;
        }
    }
    else
    {
        pLocal->targetSightState = sithAIUtil_GetDistanceToTargetPos(pOwner, &viewPos, &pLocal->targetPos, pLocal->pClass->fov, pLocal->pClass->sightDistance, collideSize, &pLocal->toTarget, &pLocal->distance);
        if ( pLocal->targetSightState == 0 )
        {
            pLocal->vecUnknown122 = pLocal->targetPos;
            pLocal->msecAttackStart = sithTime_g_msecGameTime;
            pLocal->mode |= SITHAI_MODE_TARGETVISIBLE;
        }
    }

    // The eye offset only raised the sight check; aim from the fire position
    if ( (pLocal->submode & SITHAI_SUBMODE_FIREADDEYEOFFSET) != 0 && !rdVector_IsZero3(&pOwner->thingInfo.actorInfo.eyeOffset) )
    {
        rdVector_Sub3(&pLocal->toTarget, &pLocal->targetPos, &pLocal->weaponFirePos);
        if ( pTarget )
        {
            pLocal->distance = rdVector_Normalize3Acc(&pLocal->toTarget) - pTarget->collide.size;
        }
        else
        {
            pLocal->distance = rdVector_Normalize3Acc(&pLocal->toTarget);
        }

        if ( pLocal->distance <= 0.0f )
        {
            pLocal->distance = 0.0f;
        }
    }
}

// Updates the AI's view of its goal (goalThing, or movePos when there's none) once per frame: direction, distance and
// whether it's in sight.
void J3DAPI sithAIUtil_sub_49B640(SithAIControlBlock* pLocal)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_sub_49B640, pLocal);

    int prevSightState = pLocal->targetSightState2;
    SithThing* pOwner = pLocal->pOwner;
    if ( (size_t)pLocal->unknown141 == sithMain_g_frameNumber )
    {
        return;
    }

    pLocal->unknown141 = (int)sithMain_g_frameNumber;

    SithThing* pGoal = pLocal->goalThing;
    if ( pGoal )
    {
        if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_SEEINVISIBLE) == 0 )
        {
            if ( (pGoal->type == SITH_THING_ACTOR || pGoal->type == SITH_THING_PLAYER) && (pGoal->thingInfo.actorInfo.flags & SITH_AF_INVISIBLE) != 0 )
            {
                prevSightState = 3;
            }

            if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_BLIND) != 0 )
            {
                prevSightState = 3;
            }
        }

        pLocal->targetSightState2 = sithAIUtil_GetDistanceToTarget(pOwner, &pOwner->pos, pGoal, -1.0f, pLocal->pClass->sightDistance, 0.0f, &pLocal->vecUnknown0, &pLocal->targetDistance);
        if ( pLocal->targetSightState2 != 0 )
        {
            return;
        }

        if ( prevSightState != 0 && !sithAIUtil_CanSeeTarget(pLocal, pGoal, pLocal->targetDistance) )
        {
            pLocal->targetSightState2 = 3;
            return;
        }

        pLocal->vecUnknown5 = pGoal->pos;
        pLocal->unknown150  = (int)sithTime_g_msecGameTime;
    }
    else
    {
        pLocal->vecUnknown6 = pLocal->movePos;
        pLocal->targetSightState2 = sithAIUtil_GetDistanceToTargetPos(pOwner, &pOwner->pos, &pLocal->vecUnknown6, -1.0f, pLocal->pClass->sightDistance, 0.0f, &pLocal->vecUnknown0, &pLocal->targetDistance);
        if ( pLocal->targetSightState2 == 0 )
        {
            pLocal->vecUnknown5 = pLocal->vecUnknown6;
            pLocal->unknown150  = (int)sithTime_g_msecGameTime;
        }
    }
}

// fov limits the view: from 0 on, targets behind the viewer or with a side component (along rvec) above 1 - fov are
// out of view; between -1 and 0 only targets behind with a side component below fov + 1; -1 or less sees all around.
int J3DAPI sithAIUtil_GetDistanceToTarget(SithThing* pViewer, rdVector3* startPos, const SithThing* pTarget, float fov, float sightDistance, float collisionSize, rdVector3* toTarget, float* pDist)
{
    INDY_AB_ORIGINAL(sithAIUtil_GetDistanceToTarget, pViewer, startPos, pTarget, fov, sightDistance, collisionSize, toTarget, pDist);
    // Note: collisionSize isn't used

    SITH_ASSERTREL(toTarget && pDist);
    SITH_ASSERTREL(pViewer && pTarget && startPos);

    rdVector_Sub3(toTarget, &pTarget->pos, startPos);
    *pDist = rdVector_Normalize3Acc(toTarget) - pTarget->collide.size;
    if ( *pDist <= 0.0f )
    {
        *pDist = 0.0f;
    }

    bool bSeeInvisible = (pViewer->thingInfo.actorInfo.flags & SITH_AF_SEEINVISIBLE) != 0;
    if ( !bSeeInvisible )
    {
        if ( (pTarget->type == SITH_THING_ACTOR || pTarget->type == SITH_THING_PLAYER) && (pTarget->thingInfo.actorInfo.flags & SITH_AF_INVISIBLE) != 0 )
        {
            return 3;
        }

        if ( (pViewer->thingInfo.actorInfo.flags & SITH_AF_BLIND) != 0 )
        {
            return 3;
        }
    }

    // No sight through the water surface, except of a target swimming at the surface
    bool bViewerSubmerged = (pViewer->flags & SITH_TF_SUBMERGED) != 0;
    if ( !bViewerSubmerged && (pTarget->flags & SITH_TF_SUBMERGED) != 0 )
    {
        if ( pTarget->moveType != SITH_MT_PHYSICS || (pTarget->moveInfo.physics.flags & SITH_PF_ONWATERSURFACE) == 0 )
        {
            return 3;
        }
    }
    else if ( bViewerSubmerged && (pTarget->flags & SITH_TF_SUBMERGED) == 0 )
    {
        return 3;
    }

    // Note: the target's collide size is subtracted a second time
    if ( *pDist - pTarget->collide.size > sightDistance )
    {
        return 1;
    }

    if ( fov > -1.0f )
    {
        const rdMatrix34* pOrient = &pViewer->orient;
        float lookDot = pOrient->lvec.z * toTarget->z + pOrient->lvec.y * toTarget->y + pOrient->lvec.x * toTarget->x;
        float sideDot = fabsf(pOrient->rvec.z * toTarget->z + pOrient->rvec.y * toTarget->y + pOrient->rvec.x * toTarget->x);
        if ( fov >= 0.0f && (lookDot < 0.0f || sideDot > 1.0f - fov) )
        {
            return 2;
        }

        if ( fov < 0.0f && lookDot < 0.0f && sideDot < fov + 1.0f )
        {
            return 2;
        }
    }

    // Note: SEEINVISIBLE also skips the line of sight check
    if ( bSeeInvisible )
    {
        return 0;
    }

    SithSector* pSector = sithCollision_FindSectorInRadius(pViewer->pInSector, &pViewer->pos, startPos, 0.0f);
    return sithCollision_CheckLOS(pSector, startPos, &pTarget->pos, 0.0f) ? 0 : 3;
}

int J3DAPI sithAIUtil_GetDistanceToTargetPos(SithThing* pViewer, rdVector3* startPos, rdVector3* endPos, float fov, float sightDistance, float collisionSize, rdVector3* toTarget, float* pDist)
{
    INDY_AB_ORIGINAL(sithAIUtil_GetDistanceToTargetPos, pViewer, startPos, endPos, fov, sightDistance, collisionSize, toTarget, pDist);
    // Note: collisionSize isn't used

    SITH_ASSERTREL(toTarget && pDist);
    SITH_ASSERTREL(pViewer && startPos);

    rdVector_Sub3(toTarget, endPos, startPos);
    float dist = rdVector_Normalize3Acc(toTarget);
    *pDist = dist;
    if ( dist > sightDistance )
    {
        return 1;
    }

    if ( fov > -1.0f )
    {
        const rdMatrix34* pOrient = &pViewer->orient;
        float lookDot = pOrient->lvec.y * toTarget->y + pOrient->lvec.z * toTarget->z + pOrient->lvec.x * toTarget->x;
        float sideDot = fabsf(pOrient->rvec.y * toTarget->y + pOrient->rvec.z * toTarget->z + pOrient->rvec.x * toTarget->x);
        if ( fov >= 0.0f && (lookDot < 0.0f || sideDot > 1.0f - fov) )
        {
            return 2;
        }

        if ( fov < 0.0f && lookDot < 0.0f && sideDot < fov + 1.0f )
        {
            return 2;
        }
    }

    if ( (pViewer->thingInfo.actorInfo.flags & SITH_AF_SEEINVISIBLE) != 0 )
    {
        return 0;
    }

    SithSector* pSector = sithCollision_FindSectorInRadius(pViewer->pInSector, &pViewer->pos, startPos, 0.0f);
    return sithCollision_CheckLOS(pSector, startPos, endPos, 0.0f) ? 0 : 3;
}

int J3DAPI sithAIUtil_AIFire(SithAIControlBlock* pLocal, float minDist, float maxDist, float fireDot, float aimError, int weaponNum, SithAIUtilFireFlags flags, int burstCount)
{
    INDY_AB_ORIGINAL(sithAIUtil_AIFire, pLocal, minDist, maxDist, fireDot, aimError, weaponNum, flags, burstCount);

    float velocityScale = 1.0f;
    SithPuppetSubMode fireSubmode = (SithPuppetSubMode)0;
    SithFireProjectileFlags projectileFlags = (SithFireProjectileFlags)0;
    int bFired = 0;

    SITH_ASSERTREL(pLocal->pOwner && pLocal->pClass);
    SithThing* pOwner = pLocal->pOwner;

    if ( (sithMain_g_sith_mode.debugModeFlags & SITHDEBUG_AIDISABLED) != 0 )
    {
        return 0;
    }

    if ( (pOwner->flags & (SITH_TF_DESTROYED | SITH_TF_DYING)) != 0 )
    {
        return 0;
    }

    if ( !pLocal->pTargetThing )
    {
        return 0;
    }

    if ( (pOwner->pInSector->flags & SITH_SECTOR_UNDERWATER) != 0 && (pOwner->thingInfo.actorInfo.flags & SITH_AF_NOUNDERWATERFIRE) != 0 )
    {
        return 0;
    }

    sithAI_EmitEvent(pLocal, SITHAI_EVENT_PREFIRE, (void*)flags);

    // Without a weapon template the shot is fired by the cog of the selected inventory weapon
    SithThing* pWeaponTemplate = pOwner->thingInfo.actorInfo.pWeaponTemplate;
    unsigned int curWeaponId   = (unsigned int)pOwner->thingInfo.actorInfo.weaponInfo.curWeaponID;
    float projectileSpeed;
    if ( pWeaponTemplate )
    {
        projectileSpeed = pWeaponTemplate->moveInfo.physics.velocity.y;
    }
    else if ( curWeaponId >= SITHWEAPON_COMFISTS && curWeaponId <= SITHWEAPON_COMSHOTGUN )
    {
        projectileSpeed = 5.0f;
    }
    else
    {
        goto done;
    }

    sithAIUtil_sub_49B2E0(pLocal, flags);
    rdVector3 fireDir = pLocal->toTarget;

    // A delayed shot (actors with SITH_AF_DELAYFIRE) passed the checks when its fire animation was started
    bool bDelayedShot = (flags & SITHAIUTIL_FIRE_UNKNOWN_8) != 0;
    if ( !bDelayedShot )
    {
        if ( pLocal->msecFireWaitTime > sithTime_g_msecGameTime || pLocal->targetSightState != 0 )
        {
            goto done;
        }

        if ( pLocal->distance < minDist || pLocal->distance > maxDist )
        {
            goto done;
        }

        if ( (pLocal->submode & SITHAI_SUBMODE_SKIPCHECKFIREFOV) == 0 && rdVector_Dot3(&pOwner->orient.lvec, &fireDir) < fireDot )
        {
            goto done;
        }

        if ( (flags & SITHAIUTIL_FIRE_UNKNOWN_40) != 0 )
        {
            fireSubmode = SITHPUPPETSUBMODE_FIRE;
        }
        else if ( (flags & SITHAIUTIL_FIRE_UNKNOWN_80) != 0 )
        {
            fireSubmode = SITHPUPPETSUBMODE_FIRE2;
        }
        else if ( (flags & SITHAIUTIL_FIRE_UNKNOWN_100) != 0 )
        {
            fireSubmode = SITHPUPPETSUBMODE_FIRE3;
        }
        else if ( (flags & SITHAIUTIL_FIRE_UNKNOWN_200) != 0 )
        {
            fireSubmode = SITHPUPPETSUBMODE_FIRE4;
        }

        if ( (flags & SITHAIUTIL_FIRE_STOP_ON_FIRE) != 0 )
        {
            sithAIMove_AIStop(pLocal);
        }
    }

    if ( weaponNum >= 0 )
    {
        pLocal->fireDot     = fireDot;
        pLocal->minFireDist = minDist;
        pLocal->maxFireDist = maxDist;

        if ( !bDelayedShot && (pOwner->thingInfo.actorInfo.flags & SITH_AF_DELAYFIRE) != 0 )
        {
            // The puppet fires the shot (AIFire with the stored parameters) at the fire key of the animation
            pLocal->fireFlags     = flags | SITHAIUTIL_FIRE_UNKNOWN_8;
            pLocal->aimError      = aimError;
            pLocal->fireWeaponNum = weaponNum;
            sithPuppet_PlayMode(pOwner, fireSubmode, NULL);

            sithAI_EmitEvent(pLocal, SITHAI_EVENT_FIRE, pLocal->pTargetThing);
            bFired = 1;
            goto done;
        }
    }

    // Lead a moving target, unless that turns the aim too far away from it
    SithThing* pTarget = pLocal->pTargetThing;
    if ( (flags & SITHAIUTIL_FIRE_LEAD) != 0 && pTarget->moveType == SITH_MT_PHYSICS && !rdVector_IsZero3(&pTarget->moveInfo.physics.velocity) )
    {
        rdVector3 leadDir;
        rdVector_Scale3(&leadDir, &fireDir, projectileSpeed);
        rdVector_Add3Acc(&leadDir, &pTarget->moveInfo.physics.velocity);
        rdVector_Normalize3Acc(&leadDir);
        if ( rdVector_Dot3(&leadDir, &fireDir) > 0.5f )
        {
            fireDir = leadDir;
        }
    }

    // Lob: aim higher, so gravity brings the projectile down at the target
    if ( (flags & SITHAIUTIL_FIRE_LOB) != 0 )
    {
        rdVector3 throwVel;
        throwVel.x = pLocal->toTarget.x * projectileSpeed;
        throwVel.y = pLocal->toTarget.y * projectileSpeed;
        throwVel.z = pLocal->distance / projectileSpeed * 0.5f * sithWorld_g_pCurrentWorld->gravity + pLocal->toTarget.z * projectileSpeed;
        velocityScale   = rdVector_Normalize3(&fireDir, &throwVel) / projectileSpeed;
        projectileFlags = SITHFIREPROJECTILE_SCALE_VELOCITY;
    }

    float accuracy = sithGetHitAccuarancyScalar() * pLocal->pClass->accurancy;
    if ( aimError != 0.0f && pLocal->distance != 0.0f && sithAIUtil_RandFraction() > accuracy )
    {
        sithAIUtil_RandomRotate(&fireDir, sithGetCombatDamageScalar() * aimError);
    }

    if ( pWeaponTemplate )
    {
        if ( pOwner->pAttachedThing && (pOwner->pAttachedThing->attach.flags & SITH_ATTACH_TAIL) != 0 )
        {
            sithAIMove_sub_49AA60(pLocal);
        }

        if ( weaponNum < 0 )
        {
            weaponNum = 0;
        }

        sithSoundClass_PlayModeRandom(pOwner, (SithSoundClassMode)(SITHSOUNDCLASS_FIRE1 + weaponNum));
        pOwner->thingInfo.actorInfo.weaponInfo.vecUnknown0 = fireDir;

        SithThing* pProjectile = sithWeapon_WeaponFire(pOwner, pWeaponTemplate, &fireDir, &pLocal->weaponFirePos, 0, fireSubmode, velocityScale, projectileFlags, 0.0f);
        if ( !pProjectile )
        {
            return 0; // Note: without SITHAI_EVENT_POSTFIRE
        }

        sithCog_ThingSendMessageEx(pOwner, pProjectile, SITHCOG_MSG_FIRE, (int)pWeaponTemplate->thingInfo.weaponInfo.damage, pWeaponTemplate->thingInfo.weaponInfo.damageType, weaponNum, 0);
    }
    else
    {
        SithInventoryType* pWeaponType = sithInventory_GetType((size_t)pOwner->thingInfo.actorInfo.weaponInfo.curWeaponID);
        if ( !pWeaponType || !pWeaponType->pCog )
        {
            goto done;
        }

        rdVector_Sub3(&pOwner->thingInfo.actorInfo.weaponInfo.vecUnknown0, &fireDir, &pLocal->toTarget);
        if ( (sithWeapon_HasWeaponSelected(pOwner) && !sithWeapon_IsAiming(pOwner)) || sithWeapon_IsMountingWeapon(pOwner) )
        {
            goto done;
        }

        sithCog_SendMessageEx(pWeaponType->pCog, SITHCOG_MSG_ACTIVATE, SITHCOG_SYM_REF_NONE, 0, SITHCOG_SYM_REF_THING, pOwner->idx, 0, burstCount, 0, 0, 0);
    }

    if ( !bDelayedShot )
    {
        sithAI_EmitEvent(pLocal, SITHAI_EVENT_FIRE, pLocal->pTargetThing);
    }

    bFired = 1;

done:
    sithAI_EmitEvent(pLocal, SITHAI_EVENT_POSTFIRE, (void*)flags);
    return bFired;
}

void J3DAPI sithAIUtil_AIPauseMove(SithAIControlBlock* pLocal, int msecPause)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_AIPauseMove, pLocal, msecPause);
    pLocal->msecPauseMoveUntil = sithTime_g_msecGameTime + msecPause;
}

void sithAIUtil_AIResetWaypoints(void)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_AIResetWaypoints);

    for ( size_t i = 0; i < SITHAIUTIL_MAXWAYPOINTS; i++ )
    {
        SithAIWaypoint* pWpnt = &sithAIUtil_aAIWpnts[i];
        pWpnt->thingNum      = -1;
        pWpnt->flags         = SITH_AIWPNT_LAYER0;
        pWpnt->numUsedLinks  = 0;
        pWpnt->goalProximity = 0;
    }

    for ( size_t i = 0; i < SITHAIUTIL_MAXWPNTOWNERS; i++ )
    {
        SithAIWaypointOwner* pEntry = &sithAIUtil_aWpntOwners[i];
        pEntry->thingNum          = -1;
        pEntry->waypointNum       = 0;
        pEntry->cosTurnAlignAngle = 0.0f;
    }

    sithAIUtil_activeWpntLayer   = SITH_AIWPNT_LAYER0;
    sithAIUtil_g_bRenderAIWpnts = 0;
}

// Also writes the waypoints to savegames
int J3DAPI sithAIUtil_SyncWpnts(DPID idTo, unsigned int outstream)
{
    INDY_AB_ORIGINAL(sithAIUtil_SyncWpnts, idTo, outstream);

    SITHDSS_STARTOUT(SITHDSS_SYNCWPNT);

    for ( size_t i = 0; i < SITHAIUTIL_MAXWAYPOINTS; i++ )
    {
        const SithAIWaypoint* pWpnt = &sithAIUtil_aAIWpnts[i];
        SITHDSS_PUSHINT32(pWpnt->thingNum);
        SITHDSS_PUSHINT32(pWpnt->flags);
        SITHDSS_PUSHINT32(pWpnt->numUsedLinks);
        for ( size_t j = 0; j < SITHAIUTIL_MAXWPNTLINKS; j++ )
        {
            SITHDSS_PUSHINT32(pWpnt->aWpntLinks[j]);
        }

        SITHDSS_PUSHINT32(pWpnt->goalProximity);
    }

    for ( size_t i = 0; i < SITHAIUTIL_MAXWPNTOWNERS; i++ )
    {
        const SithAIWaypointOwner* pEntry = &sithAIUtil_aWpntOwners[i];
        SITHDSS_PUSHINT32(pEntry->thingNum);
        SITHDSS_PUSHINT32(pEntry->waypointNum);
        SITHDSS_PUSHFLOAT(pEntry->cosTurnAlignAngle);
    }

    SITHDSS_PUSHINT32(sithAIUtil_activeWpntLayer);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, (SithMessageStream)outstream, 1);
}

int J3DAPI sithAIUtil_ProcessSyncWpnts(const SithMessage* pMsg)
{
    INDY_AB_ORIGINAL(sithAIUtil_ProcessSyncWpnts, pMsg);

    SITH_ASSERTREL(pMsg);

    uint8_t* pData = (uint8_t*)pMsg->data; // the sithDSS readers take a non-const cursor
    for ( size_t i = 0; i < SITHAIUTIL_MAXWAYPOINTS; i++ )
    {
        SithAIWaypoint* pWpnt = &sithAIUtil_aAIWpnts[i];
        pWpnt->thingNum     = sithDSS_ReadInt32(&pData);
        pWpnt->flags        = (SithAIWaypointLayerFlag)sithDSS_ReadInt32(&pData);
        pWpnt->numUsedLinks = sithDSS_ReadInt32(&pData);
        for ( size_t j = 0; j < SITHAIUTIL_MAXWPNTLINKS; j++ )
        {
            pWpnt->aWpntLinks[j] = sithDSS_ReadInt32(&pData);
        }

        pWpnt->goalProximity = sithDSS_ReadInt32(&pData);
    }

    for ( size_t i = 0; i < SITHAIUTIL_MAXWPNTOWNERS; i++ )
    {
        SithAIWaypointOwner* pEntry = &sithAIUtil_aWpntOwners[i];
        pEntry->thingNum          = sithDSS_ReadInt32(&pData);
        pEntry->waypointNum       = sithDSS_ReadInt32(&pData);
        pEntry->cosTurnAlignAngle = sithDSS_ReadFloat(&pData);
    }

    sithAIUtil_activeWpntLayer = (SithAIWaypointLayerFlag)sithDSS_ReadInt32(&pData);
    return 1;
}

void J3DAPI sithAIUtil_AISetWpnt(SithThing* pGhost, unsigned int wpntIdx)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_AISetWpnt, pGhost, wpntIdx);

    if ( wpntIdx >= SITHAIUTIL_MAXWAYPOINTS )
    {
        SITHLOG_ERROR("AISetWpnt: Index (%d) out of allowed range (0 - %d).\n", wpntIdx, (int)SITHAIUTIL_MAXWAYPOINTS);
        return;
    }

    if ( !pGhost || pGhost->type != SITH_THING_GHOST )
    {
        SITHLOG_ERROR("AISetWpnt: Non-GHOST type THING passed as waypoint.\n");
        return;
    }

    SithAIWaypoint* pWpnt = &sithAIUtil_aAIWpnts[wpntIdx];
    pWpnt->thingNum     = pGhost->idx;
    pWpnt->flags        = SITH_AIWPNT_LAYER0;
    pWpnt->numUsedLinks = 0;
}

SithThing* J3DAPI sithAIUtil_AIGetWpntThing(int wpntIdx)
{
    INDY_AB_ORIGINAL(sithAIUtil_AIGetWpntThing, wpntIdx);
    return sithAIUtil_GetWpntThing(wpntIdx); // Note: no range check
}

void J3DAPI sithAIUtil_AISetWpntRank(unsigned int wpntIdx, unsigned int rank)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_AISetWpntRank, wpntIdx, rank);

    if ( wpntIdx >= SITHAIUTIL_MAXWAYPOINTS )
    {
        SITHLOG_ERROR("AISetWpnt: Index (%d) out of allowed range (0 - %d).\n", wpntIdx, (int)SITHAIUTIL_MAXWAYPOINTS);
        return;
    }

    if ( rank > SITHAIUTIL_WPNTRANK_MASK )
    {
        rank = SITHAIUTIL_WPNTRANK_MASK;
    }

    SithAIWaypoint* pWpnt = &sithAIUtil_aAIWpnts[wpntIdx];
    pWpnt->flags = (SithAIWaypointLayerFlag)(((uint32_t)pWpnt->flags & ~SITHAIUTIL_WPNTRANK_MASK) | rank);
}

void J3DAPI sithAIUtil_AISetWpntFlags(unsigned int wpntIdx, SithAIWaypointLayerFlag flags)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_AISetWpntFlags, wpntIdx, flags);

    if ( wpntIdx >= SITHAIUTIL_MAXWAYPOINTS )
    {
        SITHLOG_ERROR("AISetWpnt: Index (%d) out of allowed range (0 - %d).\n", wpntIdx, (int)SITHAIUTIL_MAXWAYPOINTS);
        return;
    }

    // Clamped above the rank byte
    uint32_t setFlags = (uint32_t)flags;
    if ( setFlags < 0x100u )
    {
        setFlags = 0x100u;
    }
    else if ( setFlags > 0xFFFFFF00u )
    {
        setFlags = 0xFFFFFF00u;
    }

    SithAIWaypoint* pWpnt = &sithAIUtil_aAIWpnts[wpntIdx];
    pWpnt->flags = (SithAIWaypointLayerFlag)((uint32_t)pWpnt->flags | setFlags);
}

void J3DAPI sithAIUtil_AIClearWpntFlags(unsigned int wpntIdx, SithAIWaypointLayerFlag flags)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_AIClearWpntFlags, wpntIdx, flags);

    if ( wpntIdx >= SITHAIUTIL_MAXWAYPOINTS )
    {
        SITHLOG_ERROR("AISetWpnt: Index (%d) out of allowed range (0 - %d).\n", wpntIdx, (int)SITHAIUTIL_MAXWAYPOINTS);
        return;
    }

    uint32_t clearFlags = (uint32_t)flags;
    if ( clearFlags < 0x100u )
    {
        clearFlags = 0x100u;
    }
    else if ( clearFlags > 0xFFFFFF00u )
    {
        clearFlags = 0xFFFFFF00u;
    }

    SithAIWaypoint* pWpnt = &sithAIUtil_aAIWpnts[wpntIdx];
    pWpnt->flags = (SithAIWaypointLayerFlag)((uint32_t)pWpnt->flags & ~clearFlags);
}

void J3DAPI sithAIUtil_AISetActiveWpntLayer(SithAIWaypointLayerFlag layer)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_AISetActiveWpntLayer, layer);

    if ( (layer & SITHAIUTIL_WPNTLAYERS) == 0 )
    {
        SITHLOG_ERROR("AISetWpntLayer: Invalid layer flag (%x).\n", layer);
        return;
    }

    sithAIUtil_activeWpntLayer = layer;
}

void J3DAPI sithAIUtil_AIConnectWpnts(unsigned int wpntIdx1, unsigned int wpntIdx2, int bBidirectional)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_AIConnectWpnts, wpntIdx1, wpntIdx2, bBidirectional);

    if ( wpntIdx1 >= SITHAIUTIL_MAXWAYPOINTS || wpntIdx2 >= SITHAIUTIL_MAXWAYPOINTS )
    {
        SITHLOG_ERROR("AISetWpnt: Index (%d or %d) out of allowed range (0 - %d).\n", wpntIdx1, wpntIdx2, (int)SITHAIUTIL_MAXWAYPOINTS);
        return;
    }

    SithAIWaypoint* pWpnt1 = &sithAIUtil_aAIWpnts[wpntIdx1];
    SithAIWaypoint* pWpnt2 = &sithAIUtil_aAIWpnts[wpntIdx2];
    if ( pWpnt1->thingNum == -1 )
    {
        SITHLOG_ERROR("AIConnectWpnts: Invalid waypoint (#%d).\n", wpntIdx1);
        return;
    }

    if ( pWpnt2->thingNum == -1 )
    {
        SITHLOG_ERROR("AIConnectWpnts: Invalid waypoint (#%d).\n", wpntIdx2);
        return;
    }

    if ( (unsigned int)pWpnt1->numUsedLinks < SITHAIUTIL_MAXWPNTLINKS )
    {
        pWpnt1->aWpntLinks[pWpnt1->numUsedLinks++] = (int)wpntIdx2;
    }
    else
    {
        SITHLOG_ERROR("AIConnectWpnts: Cant connect (#%d) to (#%d). Too many links\n", wpntIdx2, wpntIdx1);
    }

    if ( bBidirectional != 1 )
    {
        return;
    }

    if ( (unsigned int)pWpnt2->numUsedLinks >= SITHAIUTIL_MAXWPNTLINKS )
    {
        SITHLOG_ERROR("AIConnectWpnts: Cannot connect (#%d) to (#%d). Too many links.\n", wpntIdx1, wpntIdx2);
        return;
    }

    pWpnt2->aWpntLinks[pWpnt2->numUsedLinks++] = (int)wpntIdx1;
}

int J3DAPI sithAIUtil_AIMoveToNextWpnt(SithAIControlBlock* pLocal, float moveSpeed, float degTurn, int mode)
{
    INDY_AB_ORIGINAL(sithAIUtil_AIMoveToNextWpnt, pLocal, moveSpeed, degTurn, mode);

    int wpntNum = sithAIUtil_AIFindNearestWpnt(pLocal->pOwner, 1);
    if ( wpntNum <= -1 )
    {
        return 0;
    }

    if ( (pLocal->submode & SITHAI_SUBMODE_UNKNOWN_100) != 0 )
    {
        degTurn = 180.0f;
    }

    sithAIUtil_AITraverseWpnts(pLocal, wpntNum, moveSpeed, degTurn, mode);
    return 1;
}

// mode: 0 stops at every waypoint, 1 moves semi-continuously, anything else continuously
void J3DAPI sithAIUtil_AITraverseWpnts(SithAIControlBlock* pLocal, int wpntIdx, float moveSpeed, float degTurn, int mode)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_AITraverseWpnts, pLocal, wpntIdx, moveSpeed, degTurn, mode);

    SithWorld* pWorld = sithWorld_g_pCurrentWorld;
    SITH_ASSERTREL(pLocal);

    if ( !pWorld || (unsigned int)wpntIdx >= SITHAIUTIL_MAXWAYPOINTS )
    {
        SITHLOG_ERROR("AITraverseWpnts: Index out of allowed range.\n");
        return;
    }

    // Note: an unset waypoint (thingNum -1) passes the range check and reads aThings[-1]
    int thingNum = sithAIUtil_aAIWpnts[wpntIdx].thingNum;
    if ( thingNum > pWorld->lastThingIdx || pWorld->aThings[thingNum].type != SITH_THING_GHOST )
    {
        SITHLOG_ERROR("AITraverseWpnts: Invalid waypoint (#%d).\n", wpntIdx);
        return;
    }

    if ( degTurn == 0.0f )
    {
        degTurn = pLocal->pClass->degTurnAlign;
        if ( degTurn == 0.0f )
        {
            degTurn = 45.0f;
        }
    }

    int ownerNum = sithAIUtil_CreateAIWaypoint(pLocal, degTurn);
    if ( ownerNum == -1 )
    {
        SITHLOG_ERROR("AITraverseWpnts: Too many AI's using waypoints (max = %d).\n", (int)SITHAIUTIL_MAXWPNTOWNERS);
        sithAIMove_AIStop(pLocal);
        return;
    }

    pLocal->mode |= SITHAI_MODE_TRAVERSEWPNTS;
    if ( mode == 1 )
    {
        pLocal->submode |= SITHAI_SUBMODE_SEMICONTINUOUSWPNTMOTION;
    }
    else if ( (unsigned int)mode > 1 )
    {
        pLocal->submode |= SITHAI_SUBMODE_CONTINUOUSWPNTMOTION;
    }

    sithAIUtil_SetCurWpnt(&sithAIUtil_aWpntOwners[ownerNum], (size_t)wpntIdx);

    const rdVector3* pWpntPos = &pWorld->aThings[thingNum].pos;
    if ( sithAIMove_AISetMovePos(pLocal, pWpntPos, moveSpeed) )
    {
        sithAIMove_AISetLookPosEyeLevel(pLocal, pWpntPos);
        sithAIUtil_sub_49D170(pLocal);
        return;
    }

    // Already at the waypoint
    pLocal->moveSpeed = moveSpeed;
    sithAIUtil_AIAdvanceToNextWpnt(pLocal, 0);
}

int J3DAPI sithAIUtil_AIAdvanceToNextWpnt(SithAIControlBlock* pLocal, int bNotify)
{
    INDY_AB_ORIGINAL(sithAIUtil_AIAdvanceToNextWpnt, pLocal, bNotify);

    SithWorld* pWorld = sithWorld_g_pCurrentWorld;
    SITH_ASSERTREL(pLocal && pLocal->pOwner);

    int ownerNum = sithAIUtil_GetAIWaypointNum(pLocal);
    if ( ownerNum == -1 )
    {
        SITHLOG_ERROR("AIAdvanceToNextWpnt: AI '%s' not in waypoint tracking list.\n", pLocal->pOwner->aName);
        sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TRAVERSEWPNTS);
        return 0;
    }

    SithAIWaypointOwner* pEntry = &sithAIUtil_aWpntOwners[ownerNum];
    size_t arrivedWpnt = sithAIUtil_GetCurWpnt(pEntry);
    sithCog_ThingSendMessageEx(pLocal->pOwner, NULL, SITHCOG_MSG_UPDATEWPNTS, 0, 0, 0, 0);

    if ( arrivedWpnt < SITHAIUTIL_MAXWAYPOINTS )
    {
        SithAIWaypoint* pWpnt = &sithAIUtil_aAIWpnts[arrivedWpnt];
        if ( pWpnt->numUsedLinks != 0 )
        {
            bool bHasActiveLink = false;
            int linkNum = 0;
            for ( ; linkNum < pWpnt->numUsedLinks; linkNum++ )
            {
                if ( sithAIUtil_IsWpntActive(pWpnt->aWpntLinks[linkNum]) )
                {
                    bHasActiveLink = true;
                    break;
                }
            }

            // At a dead end only continuous motion turns around
            if ( bHasActiveLink
                && (pWpnt->numUsedLinks != 1
                    || (size_t)pWpnt->aWpntLinks[0] != sithAIUtil_GetPrevWpnt(pEntry)
                    || (pLocal->submode & (SITHAI_SUBMODE_CONTINUOUSWPNTMOTION | SITHAI_SUBMODE_SEMICONTINUOUSWPNTMOTION)) != 0) )
            {
                int nextWpnt = sithAIUtil_sub_49CC60(pLocal, (unsigned int)pWpnt->numUsedLinks, pWpnt->aWpntLinks, linkNum, ownerNum);
                const rdVector3* pNextPos = &pWorld->aThings[sithAIUtil_aAIWpnts[nextWpnt].thingNum].pos;
                if ( sithAIMove_AISetMovePos(pLocal, pNextPos, pLocal->moveSpeed) )
                {
                    sithAIMove_AISetLookPosEyeLevel(pLocal, pNextPos);
                    sithAIUtil_sub_49D170(pLocal);

                    sithAIUtil_SetPrevWpnt(pEntry, arrivedWpnt);
                    sithAIUtil_SetCurWpnt(pEntry, (size_t)nextWpnt);
                    if ( bNotify )
                    {
                        sithCog_ThingSendMessageEx(pLocal->pOwner, NULL, SITHCOG_MSG_ARRIVEDWPNT, (int)arrivedWpnt, nextWpnt, 0, 0);
                    }

                    return 1;
                }
            }
        }
    }

    // End of the route
    sithCog_ThingSendMessageEx(pLocal->pOwner, NULL, SITHCOG_MSG_ARRIVEDWPNT, (int)arrivedWpnt, -1, 0, 0);
    sithCog_ThingSendMessage(pLocal->pOwner, NULL, SITHCOG_MSG_ARRIVED);
    sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TRAVERSEWPNTS);
    return 0;
}

// Chooses the next waypoint among the links from linkNum on (the first one at linkNum is known to be active). Hunting
// and fleeing AIs score the links by their distance to the player or to the waypoint to flee to; other AIs by rank,
// with chance. wpntOwnerNum's previous waypoint scores lower.
int J3DAPI sithAIUtil_sub_49CC60(SithAIControlBlock* pLocal, unsigned int numLinks, int* aWypointLinks, int linkNum, int wpntOwnerNum)
{
    INDY_AB_ORIGINAL(sithAIUtil_sub_49CC60, pLocal, numLinks, aWypointLinks, linkNum, wpntOwnerNum);

    if ( (pLocal->mode & SITHAI_MODE_HUNTING) != 0 )
    {
        sithAIUtil_sub_49D640();
    }
    else if ( (pLocal->mode & SITHAI_MODE_FLEEING) != 0 )
    {
        if ( (pLocal->mode & SITHAI_MODE_FLEEINGTOWAYPOINT) != 0 )
        {
            sithAIUtil_sub_49D750(pLocal->pFleeFromThing);
        }
        else
        {
            sithAIUtil_sub_49D640();
        }
    }

    int bestWpnt = aWypointLinks[linkNum];
    for ( unsigned int i = (unsigned int)linkNum + 1; i < numLinks; i++ )
    {
        int candWpnt = aWypointLinks[i];
        if ( !sithAIUtil_IsWpntActive(candWpnt) )
        {
            continue;
        }

        const SithAIWaypoint* pCand = &sithAIUtil_aAIWpnts[candWpnt];
        const SithAIWaypoint* pBest = &sithAIUtil_aAIWpnts[bestWpnt];
        int candScore = (int)((uint32_t)pCand->flags & SITHAIUTIL_WPNTRANK_MASK);
        int bestScore = (int)((uint32_t)pBest->flags & SITHAIUTIL_WPNTRANK_MASK);

        size_t prevWpnt = SITHAIUTIL_MAXWAYPOINTS;
        if ( wpntOwnerNum != -1 )
        {
            prevWpnt = sithAIUtil_GetPrevWpnt(&sithAIUtil_aWpntOwners[wpntOwnerNum]);
        }

        bool bPrevValid = prevWpnt < SITHAIUTIL_MAXWAYPOINTS;
        bool bFleeing   = (pLocal->mode & SITHAI_MODE_FLEEING) != 0 && pLocal->pFleeFromThing;
        if ( bFleeing && (pLocal->mode & SITHAI_MODE_FLEEINGTOWAYPOINT) == 0 )
        {
            // Away from the player
            if ( pBest->goalProximity == SITHAIUTIL_GOALPROXIMITY )
            {
                bestScore -= 14;
            }
            else if ( pCand->goalProximity == SITHAIUTIL_GOALPROXIMITY )
            {
                candScore -= 14;
            }

            // Not toward the threat; the nearer it is, the wider the avoided cone
            rdVector3 threatDir;
            rdVector_Sub3(&threatDir, &pLocal->pFleeFromThing->pos, &pLocal->pOwner->pos);
            float threatDist = rdVector_Normalize3Acc(&threatDir);
            float minAvoidDot = (threatDist - 0.5f) * ((0.984f - 0.923f) / (1.5f - 0.5f)) + 0.923f;
            if ( minAvoidDot < 0.923f )
            {
                minAvoidDot = 0.923f;
            }
            else if ( minAvoidDot > 0.984f )
            {
                minAvoidDot = 0.984f;
            }

            rdVector3 toWpnt;
            rdVector_Sub3(&toWpnt, &sithAIUtil_GetWpntThing(bestWpnt)->pos, &pLocal->pOwner->pos);
            rdVector_Normalize3Acc(&toWpnt);
            if ( rdVector_Dot3(&toWpnt, &threatDir) > minAvoidDot )
            {
                bestScore -= 10;
            }

            rdVector_Sub3(&toWpnt, &sithAIUtil_GetWpntThing(candWpnt)->pos, &pLocal->pOwner->pos);
            rdVector_Normalize3Acc(&toWpnt);
            if ( rdVector_Dot3(&toWpnt, &threatDir) > minAvoidDot )
            {
                candScore -= 10;
            }

            int proximityDiff = pCand->goalProximity - pBest->goalProximity;
            if ( proximityDiff < 0 )
            {
                candScore += -3 * proximityDiff;
            }
            else if ( proximityDiff > 0 )
            {
                bestScore += 3 * proximityDiff;
            }

            if ( bPrevValid )
            {
                if ( (size_t)candWpnt == prevWpnt )
                {
                    candScore--;
                }

                if ( (size_t)bestWpnt == prevWpnt )
                {
                    bestScore--;
                }
            }
        }
        else if ( (bFleeing && (pLocal->mode & SITHAI_MODE_FLEEINGTOWAYPOINT) != 0) || ((pLocal->mode & SITHAI_MODE_HUNTING) != 0 && pLocal->pTargetThing) )
        {
            // Toward the goal, not back
            candScore = 0;
            bestScore = 0;
            if ( bPrevValid )
            {
                if ( (size_t)candWpnt == prevWpnt )
                {
                    candScore = -6;
                }

                if ( (size_t)bestWpnt == prevWpnt )
                {
                    bestScore = -6;
                }
            }

            if ( (unsigned int)pCand->goalProximity > (unsigned int)pBest->goalProximity )
            {
                candScore += 7;
            }
            else if ( (unsigned int)pCand->goalProximity < (unsigned int)pBest->goalProximity )
            {
                bestScore += 7;
            }
        }
        else if ( (pLocal->mode & (SITHAI_MODE_ATTACKING | SITHAI_MODE_SEARCHING)) != 0 )
        {
            if ( bPrevValid )
            {
                if ( (size_t)candWpnt == prevWpnt )
                {
                    candScore -= 6;
                }

                if ( (size_t)bestWpnt == prevWpnt )
                {
                    bestScore -= 6;
                }
            }

            // A lower score may still win, the more likely the closer it is
            if ( candScore < bestScore )
            {
                float randValue = sithAIUtil_RandFraction();
                if ( (float)(bestScore - candScore) * 0.1f + 0.5f < randValue )
                {
                    bestScore = -bestScore;
                }
            }
        }

        if ( candScore > bestScore || (candScore == bestScore && sithAIUtil_RandFraction() > 0.5f) )
        {
            bestWpnt = candWpnt;
        }
    }

    return bestWpnt;
}

// Waits for the AI to face its next waypoint: while the look direction is off by more than the waypoint turn angle
// and the AI is turning, it stands still
void J3DAPI sithAIUtil_sub_49D170(SithAIControlBlock* pLocal)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_sub_49D170, pLocal);

    int ownerNum = sithAIUtil_GetAIWaypointNum(pLocal);
    if ( ownerNum <= -1 )
    {
        sithAIUtil_AIClearMode(pLocal, SITHAI_MODE_TRAVERSEWPNTS);
        return;
    }

    // Goal direction in the plane the AI stands on
    SithThing* pOwner     = pLocal->pOwner;
    const rdVector3* pUp  = &pOwner->orient.uvec;
    const rdVector3* pGoal = &pLocal->goalLVec;
    float upDist = pGoal->x * pUp->x + pGoal->z * pUp->z + pGoal->y * pUp->y;

    rdVector3 goalDir;
    goalDir.x = pUp->x * -upDist + pGoal->x;
    goalDir.y = pUp->y * -upDist + pGoal->y;
    goalDir.z = pUp->z * -upDist + pGoal->z;
    rdVector_Normalize3Acc(&goalDir);

    float alignDot;
    if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) != 0 )
    {
        alignDot = rdVector_Dot3(&pOwner->orient.lvec, &goalDir);
    }
    else
    {
        alignDot = pOwner->orient.lvec.x * goalDir.x + pOwner->orient.lvec.y * goalDir.y;
    }

    if ( alignDot < sithAIUtil_aWpntOwners[ownerNum].cosTurnAlignAngle && (pLocal->mode & SITHAI_MODE_TURNING) != 0 )
    {
        rdVector_Zero3(&pOwner->moveInfo.physics.velocity);
        return;
    }

    pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_10;
}

// Returns the nearest active waypoint, -1 if none (or none reachable with bCheckPath). A thing standing at a waypoint
// gets that one. For other things than the player the nearest reachable waypoint competes with its reachable linked
// waypoints in sithAIUtil_sub_49CC60.
int J3DAPI sithAIUtil_AIFindNearestWpnt(SithThing* pThing, int bCheckPath)
{
    INDY_AB_ORIGINAL(sithAIUtil_AIFindNearestWpnt, pThing, bCheckPath);

    SithWorld* pWorld = sithWorld_g_pCurrentWorld;
    bool bFound = false;

    if ( pThing != sithPlayer_g_pLocalPlayerThing )
    {
        sithCog_ThingSendMessageEx(pThing, NULL, SITHCOG_MSG_UPDATEWPNTS, 0, 0, 0, 0);
    }

    size_t numDistances = 0;
    size_t wpntNum = 0;
    for ( ; wpntNum < SITHAIUTIL_MAXWAYPOINTS; wpntNum++ )
    {
        const SithAIWaypoint* pWpnt = &sithAIUtil_aAIWpnts[wpntNum];
        if ( !sithAIUtil_IsWpntActive(wpntNum) || pWpnt->thingNum == -1 )
        {
            continue;
        }

        if ( pWpnt->thingNum > pWorld->lastThingIdx || pWorld->aThings[pWpnt->thingNum].type != SITH_THING_GHOST )
        {
            SITHLOG_ERROR("AIFindNearestWpnt: Invalid waypoint (#%d).\n", (int)wpntNum);
            continue;
        }

        SithAIWaypointDistance* pDistance = &sithAIUtil_aWpntDistances[numDistances];
        pDistance->num = (int)wpntNum;
        bFound = true;

        rdVector3 offset;
        rdVector_Sub3(&offset, &pThing->pos, &pWorld->aThings[pWpnt->thingNum].pos);
        pDistance->distance = rdVector_Dot3(&offset, &offset);

        float reach = pThing->collide.movesize + 0.03f;
        if ( pDistance->distance <= reach * reach )
        {
            return (int)wpntNum;
        }

        numDistances++;
    }

    if ( !bFound )
    {
        return -1;
    }

    indyCrt_Qsort(sithAIUtil_aWpntDistances, numDistances, sizeof(SithAIWaypointDistance), (int (__cdecl*)(const void*, const void*))sithAIUtil_CompareAIWpntDistances);

    int nearestWpnt = sithAIUtil_aWpntDistances[0].num;

    // Note: without bCheckPath sortedIdx keeps the end of the waypoint scan, so no linked waypoints compete below
    size_t sortedIdx = wpntNum;
    if ( bCheckPath )
    {
        bool bReachable = false;
        for ( sortedIdx = 0; sortedIdx < numDistances; sortedIdx++ )
        {
            nearestWpnt = sithAIUtil_aWpntDistances[sortedIdx].num;

            float hitDist;
            rdVector3 hitNorm;
            if ( !sithAIUtil_CheckPathToPoint(pThing, &sithAIUtil_GetWpntThing(nearestWpnt)->pos, pThing->collide.movesize, &hitDist, &hitNorm, 1, 0) )
            {
                bReachable = true;
                break;
            }
        }

        if ( !bReachable )
        {
            return -1;
        }
    }

    if ( pThing == sithPlayer_g_pLocalPlayerThing )
    {
        return nearestWpnt;
    }

    int aCandidates[1 + SITHAIUTIL_MAXWPNTLINKS];
    unsigned int numCandidates = 1;
    aCandidates[0] = nearestWpnt;

    const SithAIWaypoint* pNearest = &sithAIUtil_aAIWpnts[nearestWpnt];
    for ( size_t i = sortedIdx + 1; i < numDistances; i++ )
    {
        int wpnt = sithAIUtil_aWpntDistances[i].num;
        for ( unsigned int link = 0; link < (unsigned int)pNearest->numUsedLinks; link++ )
        {
            float hitDist;
            rdVector3 hitNorm;
            if ( pNearest->aWpntLinks[link] == wpnt
                && !sithAIUtil_CheckPathToPoint(pThing, &sithAIUtil_GetWpntThing(wpnt)->pos, pThing->collide.movesize, &hitDist, &hitNorm, 1, 0) )
            {
                aCandidates[numCandidates++] = wpnt;
                break;
            }
        }

        if ( numCandidates >= (unsigned int)pNearest->numUsedLinks + 1 )
        {
            break;
        }
    }

    return sithAIUtil_sub_49CC60(pThing->controlInfo.aiControl.pLocal, numCandidates, aCandidates, 0, -1);
}

static void sithAIUtil_ClearGoalProximity(void)
{
    for ( size_t i = 0; i < SITHAIUTIL_MAXWAYPOINTS; i++ )
    {
        sithAIUtil_aAIWpnts[i].goalProximity = 0;
    }
}

// Spreads SithAIWaypoint.goalProximity from the goal waypoint over the active waypoints: one less per link of the
// shortest route found. Dead ends get a value but don't pass it on.
static void sithAIUtil_SpreadGoalProximity(size_t goalWpnt)
{
    // Note: a waypoint is pushed again whenever its value improves; like the original, the stack size isn't checked
    int aPending[SITHAIUTIL_MAXWAYPOINTS];
    int top = 0;
    aPending[0] = (int)goalWpnt;
    sithAIUtil_aAIWpnts[goalWpnt].goalProximity = SITHAIUTIL_GOALPROXIMITY;

    do
    {
        const SithAIWaypoint* pWpnt = &sithAIUtil_aAIWpnts[aPending[top--]];
        unsigned int linkProximity = (unsigned int)pWpnt->goalProximity - 1;
        for ( unsigned int i = 0; i < (unsigned int)pWpnt->numUsedLinks; i++ )
        {
            int linkWpnt = pWpnt->aWpntLinks[i];
            SithAIWaypoint* pLink = &sithAIUtil_aAIWpnts[linkWpnt];
            if ( sithAIUtil_IsWpntActive(linkWpnt) && (unsigned int)pLink->goalProximity < linkProximity )
            {
                pLink->goalProximity = (int)linkProximity;
                if ( (unsigned int)pLink->numUsedLinks > 1 )
                {
                    aPending[++top] = linkWpnt;
                }
            }
        }
    } while ( top >= 0 );
}

// Sets the waypoints' goal proximity to the player
void J3DAPI sithAIUtil_sub_49D640(void)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_sub_49D640);

    SithThing* pPlayer = sithPlayer_g_pLocalPlayerThing;
    if ( !pPlayer || (pPlayer->thingInfo.actorInfo.flags & SITH_AF_INVISIBLE) != 0 )
    {
        return;
    }

    sithAIUtil_ClearGoalProximity();

    int nearestWpnt = sithAIUtil_AIFindNearestWpnt(pPlayer, 0);
    if ( nearestWpnt != -1 )
    {
        sithAIUtil_SpreadGoalProximity((size_t)nearestWpnt);
    }
}

// Sets the waypoints' goal proximity to the waypoint pThing
void J3DAPI sithAIUtil_sub_49D750(SithThing* pThing)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_sub_49D750, pThing);

    sithAIUtil_ClearGoalProximity();

    for ( size_t i = 0; i < SITHAIUTIL_MAXWAYPOINTS; i++ )
    {
        if ( sithAIUtil_aAIWpnts[i].thingNum == pThing->idx )
        {
            sithAIUtil_SpreadGoalProximity(i);
            return;
        }
    }
}

// Returns the AI's waypoint tracking entry, a new one if it has none, -1 if all are taken
int J3DAPI sithAIUtil_CreateAIWaypoint(SithAIControlBlock* pLocal, float degTurn)
{
    INDY_AB_ORIGINAL(sithAIUtil_CreateAIWaypoint, pLocal, degTurn);

    int ownerIdx = pLocal->pOwner->idx;
    int freeNum  = -1;
    int ownerNum = -1;
    for ( int i = 0; i < (int)SITHAIUTIL_MAXWPNTOWNERS; i++ )
    {
        if ( sithAIUtil_aWpntOwners[i].thingNum == -1 && freeNum == -1 )
        {
            freeNum = i;
        }

        if ( sithAIUtil_aWpntOwners[i].thingNum == ownerIdx )
        {
            ownerNum = i;
            break;
        }
    }

    if ( ownerNum == -1 )
    {
        ownerNum = freeNum;
    }

    if ( ownerNum > -1 )
    {
        if ( degTurn < 5.0f )
        {
            degTurn = 5.0f;
        }
        else if ( degTurn > 180.0f )
        {
            degTurn = 180.0f;
        }

        float sinTurn, cosTurn;
        stdMath_SinCos(degTurn, &sinTurn, &cosTurn);

        sithAIUtil_aWpntOwners[ownerNum].thingNum          = pLocal->pOwner->idx;
        sithAIUtil_aWpntOwners[ownerNum].cosTurnAlignAngle = cosTurn;
    }

    return ownerNum;
}

void J3DAPI sithAIUtil_ClearAIWaypoint(SithAIControlBlock* pLocal)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_ClearAIWaypoint, pLocal);

    int ownerNum = sithAIUtil_GetAIWaypointNum(pLocal);
    if ( ownerNum > -1 )
    {
        SithAIWaypointOwner* pEntry = &sithAIUtil_aWpntOwners[ownerNum];
        pEntry->thingNum          = -1;
        pEntry->waypointNum       = 0;
        pEntry->cosTurnAlignAngle = 0.0f;
    }
}

int J3DAPI sithAIUtil_GetAIWaypointNum(SithAIControlBlock* pLocal)
{
    INDY_AB_ORIGINAL(sithAIUtil_GetAIWaypointNum, pLocal);

    for ( int i = 0; i < (int)SITHAIUTIL_MAXWPNTOWNERS; i++ )
    {
        if ( sithAIUtil_aWpntOwners[i].thingNum == pLocal->pOwner->idx )
        {
            return i;
        }
    }

    return -1;
}

int J3DAPI sithAIUtil_CompareAIWpntDistances(const SithAIWaypointDistance* pDist1, const SithAIWaypointDistance* pDist2)
{
    INDY_AB_ORIGINAL(sithAIUtil_CompareAIWpntDistances, pDist1, pDist2);

    if ( pDist1 && pDist2 )
    {
        if ( pDist1->distance < pDist2->distance )
        {
            return -1;
        }

        if ( pDist1->distance > pDist2->distance )
        {
            return 1;
        }
    }

    return 0;
}

// INDY: stub (debug only)
void sithAIUtil_RenderAIWaypoints(void)
{
}

// Returns 1 and the hit when something blocks the straight way from pViewer to pTarget, 0 when it's clear. Floors
// block only with bCollideFloor; adjoins that can be passed don't.
int J3DAPI sithAIUtil_CheckPathToPoint(SithThing* pViewer, const rdVector3* pTarget, float radius, float* pDistance, rdVector3* pHitNorm, int bDetectThings, int bCollideFloor)
{
    INDY_AB_ORIGINAL(sithAIUtil_CheckPathToPoint, pViewer, pTarget, radius, pDistance, pHitNorm, bDetectThings, bCollideFloor);

    int bBlocked = 0;
    SITH_ASSERTREL(pViewer && pTarget);

    rdVector3 dir;
    rdVector_Sub3(&dir, pTarget, &pViewer->pos);
    float dist = rdVector_Normalize3Acc(&dir);
    *pDistance = dist;

    sithCollision_SearchForCollisions(pViewer->pInSector, pViewer, &pViewer->pos, &dir, dist, radius, bDetectThings ? 0x102 : 0x103);

    SithCollision* pCollision = sithCollision_PopStack();
    if ( pCollision )
    {
        while ( (!bCollideFloor && (pCollision->type & SITHCOLLISION_WORLD) != 0 && (pCollision->pSurfaceCollided->flags & SITH_SURFACE_ISFLOOR) != 0)
            || ((pCollision->type & (SITHCOLLISION_ADJOINTOUCH | SITHCOLLISION_ADJOINCROSS)) != 0 && (pCollision->pSurfaceCollided->pAdjoin->flags & SITH_ADJOIN_MOVE) != 0) )
        {
            pCollision = sithCollision_PopStack();
            if ( !pCollision )
            {
                sithCollision_DecreaseStackLevel();
                return bBlocked;
            }
        }

        *pDistance = pCollision->distance;
        bBlocked = 1;
        *pHitNorm = pCollision->hitNorm;
    }

    sithCollision_DecreaseStackLevel();
    return bBlocked;
}

// Can the AI move to endPos? See sithAIUtil_CheckFloorAtPos for the result.
int J3DAPI sithAIUtil_CheckPosition(const SithAIControlBlock* pLocal, rdVector3* endPos, int* pbNoCollision)
{
    INDY_AB_ORIGINAL(sithAIUtil_CheckPosition, pLocal, endPos, pbNoCollision);

    SithThing* pThing = pLocal->pOwner;
    SITH_ASSERTREL(pThing);

    SithSector* pSector = sithCollision_FindSectorInRadius(pThing->pInSector, &pThing->pos, endPos, 0.0f);
    if ( !pSector )
    {
        return 0;
    }

    if ( (pSector->flags & SITH_SECTOR_NOACTORENTER) != 0 )
    {
        return 0;
    }

    if ( (pThing->pInSector->flags & SITH_SECTOR_UNDERWATER) == 0 && (pSector->flags & SITH_SECTOR_UNDERWATER) != 0 && (pThing->flags & SITH_TF_WATERDESTROYED) != 0 )
    {
        return 0;
    }

    return sithAIUtil_CheckFloorAtPos(pLocal, endPos, pSector, pbNoCollision);
}

// What's below startPos within the AI's step height: 0 a place the AI must not enter, 1 floor (or a thing to stand
// on), 2 a wall or a floor too steep, 4 a thing in the way, 8 nothing. pbNoCollision is set to 0 when it's what the
// AI already stands on.
int J3DAPI sithAIUtil_CheckFloorAtPos(const SithAIControlBlock* pLocal, const rdVector3* startPos, SithSector* pStartSector, int* pbNoCollision)
{
    INDY_AB_ORIGINAL(sithAIUtil_CheckFloorAtPos, pLocal, startPos, pStartSector, pbNoCollision);

    SithThing* pThing = pLocal->pOwner;
    int result = 8;
    SITH_ASSERTREL(pThing);

    rdVector3 downDir;
    if ( (pLocal->mode & SITHAI_MODE_WALLCRAWLING) != 0 )
    {
        rdVector_Neg3(&downDir, &pThing->orient.uvec);
    }
    else if ( rdVector_Dot3(&pThing->orient.uvec, &rdroid_g_zVector3) <= -0.707f )
    {
        downDir = rdroid_g_zVector3; // upside down
    }
    else
    {
        rdVector_Neg3(&downDir, &rdroid_g_zVector3);
    }

    float height = sithPhysics_GetThingHeight(pThing);
    sithCollision_SearchForCollisions(pStartSector, pThing, startPos, &downDir, height + pLocal->pClass->maxStep, 0.001f, 0);

    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (pCollision->type & (SITHCOLLISION_ADJOINTOUCH | SITHCOLLISION_ADJOINCROSS)) != 0 )
        {
            const SithSector* pAdjoinSector = pCollision->pSurfaceCollided->pAdjoin->pAdjoinSector;
            if ( (pAdjoinSector->flags & SITH_SECTOR_NOACTORENTER) != 0
                || ((pThing->pInSector->flags & SITH_SECTOR_UNDERWATER) == 0 && (pAdjoinSector->flags & SITH_SECTOR_UNDERWATER) != 0 && (pThing->flags & SITH_TF_WATERDESTROYED) != 0) )
            {
                result = 0;
                break;
            }
        }

        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            SITH_ASSERTREL(pCollision->pSurfaceCollided);
            const SithSurface* pSurf = pCollision->pSurfaceCollided;
            if ( (pSurf->flags & SITH_SURFACE_NOAIMOVE) != 0 )
            {
                result = 0;
                break;
            }

            if ( pLocal->allowedSurfaceTypes )
            {
                if ( (pSurf->flags & pLocal->allowedSurfaceTypes) == 0 )
                {
                    result = 0;
                    break;
                }
            }
            else if ( (pSurf->flags & SITH_SURFACE_LAVA) != 0 )
            {
                result = 0;
                break;
            }

            if ( (pSurf->flags & SITH_SURFACE_ISFLOOR) == 0 )
            {
                result = 2;
            }
            else
            {
                result = 1;

                // Too steep to step onto from the floor the AI stands on
                const rdVector3* pNormal = &pSurf->face.normal;
                if ( (pThing->moveInfo.physics.flags & SITH_PF_ALIGNUP) != 0
                    && (pThing->attach.flags & SITH_ATTACH_SURFACE) != 0
                    && pThing->attach.attachedToStructure.pSurfaceAttached != pSurf
                    && pNormal->y * rdroid_g_zVector3.y + pNormal->z * rdroid_g_zVector3.z + pNormal->x * rdroid_g_zVector3.x < 0.788f )
                {
                    result = 2;
                }
            }

            if ( pbNoCollision )
            {
                *pbNoCollision = !((pThing->attach.flags & SITH_ATTACH_SURFACE) != 0 && pThing->attach.attachedToStructure.pSurfaceAttached == pSurf);
            }

            break;
        }

        if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
        {
            SITH_ASSERTREL(pCollision->pThingCollided);
            const SithThing* pHitThing = pCollision->pThingCollided;

            // Note: the type is tested as a bit mask (0xA matches actors, players, ghosts and others)
            bool bCanStandOn = ((pHitThing->attach.flags & SITH_ATTACH_THING) != 0 && pHitThing->attach.attachedToStructure.pThingAttached == pThing)
                || (pThing->pAttachedThing && (pThing->pAttachedThing->attach.flags & SITH_ATTACH_TAIL) != 0)
                || (pHitThing->flags & SITH_TF_STANDON) != 0
                || (pHitThing->type & 0xA) != 0;
            if ( !bCanStandOn )
            {
                result = 4;
                break;
            }

            result = 1;
            if ( pbNoCollision )
            {
                *pbNoCollision = !((pThing->attach.flags & SITH_ATTACH_THINGFACE) != 0 && pThing->attach.attachedToStructure.pThingAttached == pHitThing);
            }

            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    return result;
}

// Makes a point pDistance away from pPos in a direction derived from pDir and stores both. flags: 1 sideways
// (perpendicular to pDir), 2 rotate by angle (yaw), 4 rotate by angle (pitch, pDir keeps its z), 0x4000 random part of
// angle, 0x8000 random side, 0x800 sideways on the side of the move direction; toward (0x100500) or away from (0x1200)
// the position of sithAIUtil_AIGetMovePos, 0x10000 distance along the direction up to that position.
// sithAIUtil_RetryMakeRandPoint mirrors the last point.
int J3DAPI sithAIUtil_MakeRandPoint(SithAIControlBlock* pLocal, rdVector3* pPos, int flags, float angle, float* pDistance, rdVector3* pDir, rdVector3* pDestPoint)
{
    INDY_AB_ORIGINAL(sithAIUtil_MakeRandPoint, pLocal, pPos, flags, angle, pDistance, pDir, pDestPoint);

    SITH_ASSERTREL(pLocal && pLocal->pClass && pLocal->pOwner);
    SithThing* pOwner = pLocal->pOwner;

    if ( !pPos || !pDir || !pDestPoint )
    {
        SITHLOG_ERROR("sithAIUtil_MakeRandPoint: Bad parameters.\n");
        return 0;
    }

    sithAIUtil_mkPoinCurFlags   = 0;
    sithAIUtil_pMkPointCurLocal = NULL;

    if ( (flags & 4) == 0 )
    {
        pDir->z = 0.0f;
        rdVector_Normalize3Acc(pDir);
    }

    rdVector3 newDir;
    rdVector3 pyr = { 0 };
    if ( (flags & (2 | 4)) != 0 )
    {
        float pitch = (flags & 4) != 0 ? angle : 0.0f;
        float yaw   = (flags & 2) != 0 ? angle : 0.0f;
        if ( (flags & 0x4000) != 0 )
        {
            if ( pitch != 0.0f )
            {
                pitch = sithAIUtil_RandFraction() * pitch;
            }

            if ( yaw != 0.0f )
            {
                yaw = sithAIUtil_RandFraction() * yaw;
            }
        }

        rdVector_Set3(&pyr, pitch, yaw, 0.0f);
        if ( (flags & 0x8000) != 0 && sithAIUtil_RandFraction() > 0.5f )
        {
            rdVector_Neg3Acc(&pyr);
        }

        rdVector_Rotate3(&newDir, pDir, &pyr);
    }
    else if ( (flags & 1) != 0 )
    {
        rdVector_Cross3(&newDir, &pOwner->orient.uvec, pDir);
        if ( (flags & 0x800) != 0 )
        {
            const rdVector3* pMoveDir = &pLocal->moveDirection;
            if ( pMoveDir->z * newDir.z + pMoveDir->y * newDir.y + pMoveDir->x * newDir.x < 0.0f )
            {
                rdVector_Neg3Acc(&newDir);
            }
        }
        else
        {
            if ( (flags & 0x8000) != 0 && sithAIUtil_RandFraction() > 0.5f )
            {
                rdVector_Neg3Acc(&newDir);
            }

            if ( (flags & 0x4000) != 0 )
            {
                pyr.x = 0.0f;
                pyr.y = sithAIUtil_RandFraction() * angle;
                pyr.z = 0.0f;
                if ( sithAIUtil_RandFraction() > 0.5f )
                {
                    rdVector_Neg3Acc(&pyr);
                }

                rdVector_Rotate3Acc(&newDir, &pyr);
            }
        }
    }
    else
    {
        return 0;
    }

    if ( (flags & 0x101700) != 0 )
    {
        rdVector3 movePos = { 0 }; // Note: the original leaves it uninitialized when sithAIUtil_AIGetMovePos sets nothing
        sithAIUtil_AIGetMovePos(pLocal, flags, &movePos);

        rdVector3 toMovePos;
        rdVector_Sub3(&toMovePos, &movePos, &pOwner->pos);
        rdVector_Normalize3Acc(&toMovePos);
        bool bTowardMovePos = rdVector_Dot3(&toMovePos, &newDir) > 0.0f;
        if ( ((flags & 0x100500) != 0 && !bTowardMovePos) || ((flags & 0x1200) != 0 && bTowardMovePos) )
        {
            if ( (flags & 2) != 0 )
            {
                rdVector_Neg3Acc(&pyr);
                rdVector_Rotate3(&newDir, pDir, &pyr);
            }
            else
            {
                rdVector_Neg3Acc(&newDir);
            }
        }

        if ( (flags & 0x10000) != 0 )
        {
            *pDistance = -((pPos->x - movePos.x) * newDir.x + (pPos->z - movePos.z) * newDir.z + (pPos->y - movePos.y) * newDir.y);
        }
    }

    if ( fabsf(*pDistance) <= 0.00001f )
    {
        return 0;
    }

    sithAIUtil_mkPoinCurFlags   = flags;
    sithAIUtil_pMkPointCurLocal = pLocal;
    sithAIUtil_vec_585490       = *pDir;
    if ( (flags & 2) != 0 )
    {
        sithAIUtil_mkPointCurPYR = pyr;
    }
    else
    {
        sithAIUtil_vec_585718 = newDir;
    }

    rdVector_ScaleAdd3(pDestPoint, &newDir, *pDistance, pPos);
    *pDir = newDir;
    *pDistance = fabsf(*pDistance);
    return 1;
}

// Makes the point mirrored to the last one of sithAIUtil_MakeRandPoint (once)
int J3DAPI sithAIUtil_RetryMakeRandPoint(SithAIControlBlock* pLocal, rdVector3* pPos, float distance, rdVector3* pDirection, rdVector3* pDestPoint)
{
    INDY_AB_ORIGINAL(sithAIUtil_RetryMakeRandPoint, pLocal, pPos, distance, pDirection, pDestPoint);

    if ( !sithAIUtil_mkPoinCurFlags || !sithAIUtil_pMkPointCurLocal || !pLocal )
    {
        return 0;
    }

    if ( pLocal != sithAIUtil_pMkPointCurLocal || !pPos || !pDirection || !pDestPoint )
    {
        SITHLOG_ERROR("sithAIUtil_RetryMakeRandPoint: Bad parameters.\n");
        return 0;
    }

    rdVector3 newDir;
    if ( (sithAIUtil_mkPoinCurFlags & 0x10400) != 0 )
    {
        sithAIUtil_mkPoinCurFlags   = 0;
        sithAIUtil_pMkPointCurLocal = NULL;
        return 0;
    }

    if ( (sithAIUtil_mkPoinCurFlags & (2 | 4)) != 0 )
    {
        rdVector_Neg3Acc(&sithAIUtil_mkPointCurPYR);
        rdVector_Rotate3(&newDir, &sithAIUtil_vec_585490, &sithAIUtil_mkPointCurPYR);
        sithAIUtil_mkPoinCurFlags &= ~(2 | 4);
    }
    else if ( (sithAIUtil_mkPoinCurFlags & 1) != 0 )
    {
        rdVector_Neg3(&newDir, &sithAIUtil_vec_585718);
        sithAIUtil_mkPoinCurFlags &= ~1;
    }
    else
    {
        sithAIUtil_mkPoinCurFlags   = 0;
        sithAIUtil_pMkPointCurLocal = NULL;
        return 0;
    }

    rdVector_ScaleAdd3(pDestPoint, &newDir, distance, pPos);
    *pDirection = newDir;
    return 1;
}

// sithAIUtil_CheckPathToPos with flag 0x10
static int sithAIUtil_CheckPathAhead(SithAIControlBlock* pLocal, SithSector* pStartSector, const rdVector3* startPos, int flags, float minMoveDist, float moveDist, float radius, const rdVector3* pDirection, rdVector3* pOutPos)
{
    SithThing* pOwner = pLocal->pOwner;
    int result = 1;

    rdVector3 endPos;
    rdVector_ScaleAdd3(&endPos, pDirection, moveDist, startPos);

    sithCollision_SearchForCollisions(pStartSector, pOwner, startPos, pDirection, moveDist, radius, 0x102);
    for ( SithCollision* pCollision = sithCollision_PopStack(); pCollision; pCollision = sithCollision_PopStack() )
    {
        if ( (flags & 0x20000) != 0 && (pCollision->type & SITHCOLLISION_THING) != 0 && pCollision->pThingCollided->type == SITH_THING_PLAYER )
        {
            continue;
        }

        if ( (flags & 0x40000) != 0 && (pCollision->type & SITHCOLLISION_THING) != 0 )
        {
            continue;
        }

        if ( (flags & 0x80000) != 0 && (pCollision->type & SITHCOLLISION_WORLD) != 0 && (pCollision->pSurfaceCollided->flags & SITH_SURFACE_ISFLOOR) != 0 )
        {
            continue;
        }

        if ( pCollision->distance < minMoveDist )
        {
            result = 0;
            break;
        }

        rdVector_ScaleAdd3(&endPos, pDirection, pCollision->distance, startPos);
        if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
        {
            result = 4;
            break;
        }

        if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
        {
            result = 2;
            break;
        }
    }

    sithCollision_DecreaseStackLevel();
    if ( !result )
    {
        return 0;
    }

    // The end position has to stay in sight of the goal (0x2000) or target (0x800000)
    SithThing* pWatcher = NULL;
    if ( (flags & 0x2000) != 0 && pLocal->goalThing )
    {
        pWatcher = pLocal->goalThing;
    }

    if ( (flags & 0x800000) != 0 && pLocal->pTargetThing )
    {
        pWatcher = pLocal->pTargetThing;
    }

    if ( pWatcher )
    {
        rdVector3 toEndPos;
        float dist;
        if ( sithAIUtil_GetDistanceToTargetPos(pWatcher, &pWatcher->pos, &endPos, -1.0f, pLocal->pClass->sightDistance, 0.0f, &toEndPos, &dist) == 3 )
        {
            return 0;
        }
    }

    if ( pOutPos )
    {
        *pOutPos = endPos;
    }

    return result;
}

// sithAIUtil_CheckPathToPos with flag 0x20: steps sideways (first along uvec x pDirection, then the other way) until
// the way ahead along pDirection is clear, then picks a side
static int sithAIUtil_CheckPathSideways(SithAIControlBlock* pLocal, SithSector* pStartSector, const rdVector3* startPos, int flags, float minMoveDist, float moveDist, float radius, rdVector3* pDirection, rdVector3* pOutPos)
{
    SithThing* pOwner = pLocal->pOwner;
    int aResults[2] = { 0 };
    int aScores[2]  = { 0 };
    rdVector3 aPositions[2] = { 0 }; // Note: the original leaves a side's position uninitialized when it takes no step

    rdVector3 sideDir;
    rdVector_Cross3(&sideDir, &pOwner->orient.uvec, pDirection);
    sideDir.x = sithAIUtil_ZeroIfTiny(sideDir.x);
    sideDir.y = sithAIUtil_ZeroIfTiny(sideDir.y);
    sideDir.z = sithAIUtil_ZeroIfTiny(sideDir.z);
    if ( rdVector_Normalize3Acc(&sideDir) == 0.0f )
    {
        // pDirection is parallel to the up vector
        rdVector_Cross3(&sideDir, pDirection, &pOwner->orient.rvec);
        sideDir.x = sithAIUtil_ZeroIfTiny(sideDir.x);
        sideDir.y = sithAIUtil_ZeroIfTiny(sideDir.y);
        sideDir.z = sithAIUtil_ZeroIfTiny(sideDir.z);
        rdVector_Normalize3Acc(&sideDir);
    }

    for ( int side = 1; side >= 0; side-- )
    {
        if ( side == 0 )
        {
            rdVector_Neg3Acc(&sideDir);
        }

        rdVector3* pPos = &aPositions[side];
        int* pResult    = &aResults[side];
        float stepDist  = minMoveDist;
        for ( ;; )
        {
            stepDist = stepDist + 0.03f;
            if ( stepDist > moveDist )
            {
                break;
            }

            rdVector_ScaleAdd3(pPos, &sideDir, stepDist, startPos);
            SithSector* pSector = sithCollision_FindSectorInRadius(pStartSector, startPos, pPos, radius);
            if ( !pSector )
            {
                break;
            }

            sithCollision_SearchForCollisions(pStartSector, pOwner, startPos, &sideDir, stepDist, radius, 0x102);
            SithCollision* pCollision = sithCollision_PopStack();
            sithCollision_DecreaseStackLevel();
            if ( pCollision )
            {
                rdVector_ScaleAdd3(pPos, &sideDir, pCollision->distance, startPos);
                if ( (pCollision->type & SITHCOLLISION_THING) != 0 )
                {
                    *pResult = 4;
                }
                else if ( (pCollision->type & SITHCOLLISION_WORLD) != 0 )
                {
                    *pResult = 2;
                }

                break;
            }

            *pResult = sithAIUtil_CheckPathToPos(pLocal, pSector, pPos, 0x80010, pOwner->collide.movesize + 0.03f, radius, 0.0f, pDirection, NULL);
            if ( *pResult == 1 )
            {
                break;
            }
        }
    }

    if ( pLocal->goalThing )
    {
        // Prefer the side in sight of the goal ...
        for ( size_t i = 0; i < 2; i++ )
        {
            if ( sithAIUtil_GetDistanceToTargetPos(pLocal->goalThing, &pLocal->goalThing->pos, &aPositions[i], -1.0f, pLocal->pClass->sightDistance, 0.0f, &pLocal->vecUnknown0, &pLocal->targetDistance) != 3 )
            {
                aScores[i]++;
            }
            else if ( (flags & 0x2000) != 0 )
            {
                aScores[i]--;
            }
        }

        // ... and nearer to it (0x100) or farther from it (0x200), measured in the XY plane
        float dx = aPositions[0].x - pLocal->goalThing->pos.x;
        float dy = aPositions[0].y - pLocal->goalThing->pos.y;
        float sqrDist0 = dx * dx + dy * dy;
        float dist0 = sqrtf(sqrDist0);

        dx = aPositions[1].x - pLocal->goalThing->pos.x;
        dy = aPositions[1].y - pLocal->goalThing->pos.y;
        float sqrDist1 = dx * dx + dy * dy;
        float dist1 = sqrtf(sqrDist1);

        int nearerSide = dist1 <= dist0 ? 1 : 0;
        if ( (flags & 0x100) != 0 )
        {
            aScores[nearerSide]++;
        }
        else if ( (flags & 0x200) != 0 )
        {
            aScores[nearerSide]--;
        }
    }

    int side = aScores[0] < aScores[1] ? 1 : 0;
    if ( pOutPos )
    {
        *pOutPos = aPositions[side];
    }

    return aResults[side];
}

// Checks the way from startPos: ahead along pDirection (flag 0x10) or by stepping sideways (flag 0x20). Returns 0 when
// blocked, 1 when clear, 2 when a wall and 4 when a thing ends the way early. More flags: 0x20000 players, 0x40000
// things and 0x80000 floors don't block.
int J3DAPI sithAIUtil_CheckPathToPos(SithAIControlBlock* pLocal, SithSector* pStartSector, rdVector3* startPos, int flags, float minMoveDist, float moveDist, float radius, rdVector3* pDirection, rdVector3* pOutPos)
{
    INDY_AB_ORIGINAL(sithAIUtil_CheckPathToPos, pLocal, pStartSector, startPos, flags, minMoveDist, moveDist, radius, pDirection, pOutPos);

    SITH_ASSERTREL(pLocal && pLocal->pOwner);

    if ( !pDirection )
    {
        SITHLOG_ERROR("sithAIUtil_CheckPathToPos: Bad direction vector.\n");
        return 0;
    }

    if ( radius == 0.0f )
    {
        radius = pLocal->pOwner->collide.movesize;
    }

    if ( (flags & 0x10) != 0 )
    {
        return sithAIUtil_CheckPathAhead(pLocal, pStartSector, startPos, flags, minMoveDist, moveDist, radius, pDirection, pOutPos);
    }

    if ( (flags & 0x20) != 0 )
    {
        return sithAIUtil_CheckPathSideways(pLocal, pStartSector, startPos, flags, minMoveDist, moveDist, radius, pDirection, pOutPos);
    }

    return 0;
}

// Turns from the direction to pPos by angle, angle + deltaAngle, ... up to maxAngle (both sides) and moves the AI to
// the first random point whose straight way from the AI is blocked by something
int J3DAPI sithAIUtil_sub_49EE50(SithAIControlBlock* pLocal, rdVector3* pPos, float angle, float maxAngle, float deltaAngle, int flags)
{
    INDY_AB_ORIGINAL(sithAIUtil_sub_49EE50, pLocal, pPos, angle, maxAngle, deltaAngle, flags);

    rdVector3 heading;
    heading.x = pPos->x - pLocal->pOwner->pos.x;
    heading.y = pPos->y - pLocal->pOwner->pos.y;
    heading.z = 0.0f;
    float distance = rdVector_Normalize3Acc(&heading);

    int pointFlags = (flags & 0x0FFF7F00) | 0x8000 | 2;
    for ( ; angle <= maxAngle; angle += deltaAngle )
    {
        rdVector3 dir = heading;
        rdVector3 destPoint;
        if ( !sithAIUtil_MakeRandPoint(pLocal, &pLocal->pOwner->pos, pointFlags, angle, &distance, &dir, &destPoint) )
        {
            return 0;
        }

        float hitDist;
        rdVector3 hitNorm;
        int bBlocked = sithAIUtil_CheckPathToPoint(pLocal->pOwner, &destPoint, pLocal->pOwner->collide.movesize, &hitDist, &hitNorm, 1, 0);
        while ( !bBlocked && angle != 0.0f && angle != 180.0f )
        {
            // Note: the mirrored direction is stored in heading, which the next angle starts from
            if ( !sithAIUtil_RetryMakeRandPoint(pLocal, &pLocal->pOwner->pos, distance, &heading, &destPoint) )
            {
                break;
            }

            bBlocked = sithAIUtil_CheckPathToPoint(pLocal->pOwner, &destPoint, pLocal->pOwner->collide.movesize, &hitDist, &hitNorm, 1, 0);
        }

        if ( bBlocked )
        {
            sithAIMove_StopAIMovement(pLocal);
            sithAIMove_AISetLookPosEyeLevel(pLocal, &destPoint);
            sithAIMove_AISetMoveTargetPos(pLocal, &destPoint, pLocal->moveSpeed);
            return 1;
        }
    }

    return 0;
}

// Looks for a random point in any direction (with bAllowPitch also up and down) that the AI can walk to, at most
// maxRetries times (at least once)
int J3DAPI sithAIUtil_sub_49F010(SithAIControlBlock* pLocal, rdVector3* pPos, float minDist, float moveDist, int bAllowPitch, int maxRetries, rdVector3* pDestPos)
{
    INDY_AB_ORIGINAL(sithAIUtil_sub_49F010, pLocal, pPos, minDist, moveDist, bAllowPitch, maxRetries, pDestPos);

    rdVector3 dir = pLocal->pOwner->orient.lvec;
    unsigned int numTries = 0;
    do
    {
        if ( !sithAIUtil_MakeRandPoint(pLocal, pPos, bAllowPitch ? 0xC006 : 0xC002, 360.0f, &moveDist, &dir, pDestPos) )
        {
            return 0;
        }

        do
        {
            if ( sithAIUtil_CheckPathToPos(pLocal, pLocal->pOwner->pInSector, &pLocal->pOwner->pos, bAllowPitch ? 0x10 : 0x80010, minDist, moveDist, 0.0f, &dir, pDestPos) )
            {
                return 1;
            }
        } while ( sithAIUtil_RetryMakeRandPoint(pLocal, &pLocal->pOwner->pos, moveDist, &dir, pDestPos) );
    } while ( ++numTries < (unsigned int)maxRetries );

    return 0;
}

int J3DAPI sithAIUtil_MakePathPos(SithAIControlBlock* pLocal, rdVector3* pPos, float minDist, float moveDist, float angle, int flags, rdVector3* pDirection, rdVector3* pDestPos)
{
    INDY_AB_ORIGINAL(sithAIUtil_MakePathPos, pLocal, pPos, minDist, moveDist, angle, flags, pDirection, pDestPos);

    if ( !sithAIUtil_MakeRandPoint(pLocal, pPos, flags, angle, &moveDist, pDirection, pDestPos) )
    {
        return 0;
    }

    int checkFlags = (flags & 0x8E2000) | ((flags & 4) != 0 ? 0x10 : 0x80010);
    if ( sithAIUtil_CheckPathToPos(pLocal, pLocal->pOwner->pInSector, &pLocal->pOwner->pos, checkFlags, minDist, moveDist, 0.0f, pDirection, pDestPos) )
    {
        return 1;
    }

    while ( sithAIUtil_RetryMakeRandPoint(pLocal, &pLocal->pOwner->pos, moveDist, pDirection, pDestPos) )
    {
        if ( sithAIUtil_CheckPathToPos(pLocal, pLocal->pOwner->pInSector, &pLocal->pOwner->pos, checkFlags, minDist, moveDist, 0.0f, pDirection, pDestPos) )
        {
            return 1;
        }
    }

    return 0;
}

// Returns 0 when an armed AI at pPos would be in the line of fire of another human AI shooting at its visible target
int J3DAPI sithAIUtil_sub_49F1F0(const SithAIControlBlock* pLocal, const rdVector3* pPos)
{
    INDY_AB_ORIGINAL(sithAIUtil_sub_49F1F0, pLocal, pPos);

    if ( !pLocal || !pLocal->pOwner || pLocal->pOwner->controlType != SITH_CT_AI || !sithWeapon_HasWeaponSelected(pLocal->pOwner) || !pLocal->pTargetThing )
    {
        return 1;
    }

    rdVector3 toTarget;
    rdVector_Sub3(&toTarget, &pLocal->pTargetThing->pos, pPos);
    float dist = rdVector_Normalize3Acc(&toTarget);

    for ( int i = 0; i <= sithAI_g_lastUsedAIIndex; i++ )
    {
        const SithAIControlBlock* pOther = &sithAI_g_aControlBlocks[i];
        if ( pOther->pOwner
            && pOther->pOwner->controlType == SITH_CT_AI
            && (pOther->pOwner->thingInfo.actorInfo.flags & SITH_AF_HUMAN) != 0
            && (pOther->mode & SITHAI_MODE_TARGETVISIBLE) != 0
            && pOther != pLocal
            && pOther->toTarget.z * toTarget.z + pOther->toTarget.y * toTarget.y + pOther->toTarget.x * toTarget.x >= 0.966f
            && dist > pOther->distance )
        {
            return 0;
        }
    }

    return 1;
}

void J3DAPI sithAIUtil_AIPlaySoundMode(SithAIControlBlock* pLocal, SithSoundClassMode mode)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_AIPlaySoundMode, pLocal, mode);

    if ( !pLocal || !pLocal->pOwner )
    {
        return;
    }

    // AIs that talk say the voice modes through their talk instinct
    if ( mode >= SITHSOUNDCLASS_CURIOUS && mode <= SITHSOUNDCLASS_RESERVED8 && sithAI_HasInstinct(pLocal, "talk") )
    {
        sithAI_EmitEvent(pLocal, SITHAI_EVENT_TALK, (void*)mode);
        return;
    }

    sithSoundClass_PlayModeRandom(pLocal->pOwner, mode);
}

void J3DAPI sithAIUtil_AISetMode(SithAIControlBlock* pLocal, SithAIMode mode)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_AISetMode, pLocal, mode);

    switch ( mode )
    {
        case SITHAI_MODE_MOVING:
            pLocal->mode |= SITHAI_MODE_MOVING;
            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_4000;
            break;

        case SITHAI_MODE_ATTACKING:
            pLocal->mode &= ~(SITHAI_MODE_SEARCHING | SITHAI_MODE_NOCHECKFORCLIFF | SITHAI_MODE_BLOCK | SITHAI_MODE_FLEEING | SITHAI_MODE_CHASE_GOAL | SITHAI_MODE_FLEEINGTOWAYPOINT);
            pLocal->mode |= SITHAI_MODE_ATTACKING | SITHAI_MODE_ACTIVE;
            break;

        case SITHAI_MODE_SEARCHING:
            pLocal->mode &= ~(SITHAI_MODE_ATTACKING | SITHAI_MODE_NOCHECKFORCLIFF | SITHAI_MODE_BLOCK | SITHAI_MODE_ACTIVE | SITHAI_MODE_TARGETVISIBLE | SITHAI_MODE_FLEEING
                | SITHAI_MODE_CIRCLESTRAFING | SITHAI_MODE_CHASE_GOAL | SITHAI_MODE_UNKNOWN_1000000 | SITHAI_MODE_FLEEINGTOWAYPOINT);
            pLocal->mode |= SITHAI_MODE_SEARCHING;
            sithActor_SetHeadPYR(pLocal->pOwner, &rdroid_g_zeroVector3);
            break;

        case SITHAI_MODE_BLOCK:
            pLocal->mode &= ~(SITHAI_MODE_ATTACKING | SITHAI_MODE_ACTIVE | SITHAI_MODE_TARGETVISIBLE | SITHAI_MODE_CIRCLESTRAFING | SITHAI_MODE_CHASE_GOAL | SITHAI_MODE_UNKNOWN_1000000);
            pLocal->mode |= SITHAI_MODE_SEARCHING | SITHAI_MODE_NOCHECKFORCLIFF | SITHAI_MODE_BLOCK;
            break;

        case SITHAI_MODE_FLEEING:
            pLocal->mode &= ~(SITHAI_MODE_ATTACKING | SITHAI_MODE_SEARCHING | SITHAI_MODE_NOCHECKFORCLIFF | SITHAI_MODE_BLOCK | SITHAI_MODE_ACTIVE | SITHAI_MODE_TARGETVISIBLE
                | SITHAI_MODE_SLEEPING | SITHAI_MODE_CIRCLESTRAFING | SITHAI_MODE_CHASE_GOAL | SITHAI_MODE_UNKNOWN_1000000);
            pLocal->mode |= SITHAI_MODE_FLEEING;
            pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_1;
            break;

        case SITHAI_MODE_WALLCRAWLING:
            pLocal->mode |= SITHAI_MODE_WALLCRAWLING;
            pLocal->submode |= SITHAI_SUBMODE_UNKNOWN_100;
            break;

        case SITHAI_MODE_HUNTING:
            pLocal->mode &= ~(SITHAI_MODE_NOCHECKFORCLIFF | SITHAI_MODE_BLOCK | SITHAI_MODE_FLEEING | SITHAI_MODE_SLEEPING | SITHAI_MODE_FLEEINGTOWAYPOINT);
            pLocal->mode |= SITHAI_MODE_HUNTING;
            break;

        case SITHAI_MODE_FLEEINGTOWAYPOINT:
            pLocal->mode &= ~(SITHAI_MODE_ATTACKING | SITHAI_MODE_SEARCHING | SITHAI_MODE_NOCHECKFORCLIFF | SITHAI_MODE_BLOCK | SITHAI_MODE_ACTIVE | SITHAI_MODE_TARGETVISIBLE
                | SITHAI_MODE_SLEEPING | SITHAI_MODE_CIRCLESTRAFING | SITHAI_MODE_CHASE_GOAL | SITHAI_MODE_HUNTING | SITHAI_MODE_UNKNOWN_1000000);
            pLocal->mode |= SITHAI_MODE_FLEEING | SITHAI_MODE_FLEEINGTOWAYPOINT;
            break;

        default:
            break;
    }
}

void J3DAPI sithAIUtil_AIClearMode(SithAIControlBlock* pLocal, SithAIMode mode)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_AIClearMode, pLocal, mode);

    switch ( mode )
    {
        case SITHAI_MODE_MOVING:
            pLocal->mode &= ~(SITHAI_MODE_MOVING | SITHAI_MODE_UNKNOWN_10);
            pLocal->submode &= ~(SITHAI_SUBMODE_UNKNOWN_1 | SITHAI_SUBMODE_UNKNOWN_2 | SITHAI_SUBMODE_UNKNOWN_4 | SITHAI_SUBMODE_UNKNOWN_8 | SITHAI_SUBMODE_UNKNOWN_10 | SITHAI_SUBMODE_UNKNOWN_4000);
            break;

        case SITHAI_MODE_TURNING:
            pLocal->mode &= ~(SITHAI_MODE_TURNING | SITHAI_MODE_UNKNOWN_80);
            pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_10;
            break;

        case SITHAI_MODE_WALLCRAWLING:
            pLocal->mode &= ~SITHAI_MODE_WALLCRAWLING;
            pLocal->submode &= ~SITHAI_SUBMODE_UNKNOWN_100;
            break;

        case SITHAI_MODE_HUNTING:
            pLocal->mode &= ~SITHAI_MODE_HUNTING;
            if ( (pLocal->submode & SITHAI_SUBMODE_CONTINUOUSWPNTMOTION) == 0 )
            {
                pLocal->mode &= ~(SITHAI_MODE_TRAVERSEWPNTS | SITHAI_MODE_FLEEINGTOWAYPOINT);
                pLocal->submode &= ~SITHAI_SUBMODE_SEMICONTINUOUSWPNTMOTION;
                sithAIUtil_ClearAIWaypoint(pLocal);
            }
            break;

        case SITHAI_MODE_TRAVERSEWPNTS:
            pLocal->mode &= ~(SITHAI_MODE_HUNTING | SITHAI_MODE_TRAVERSEWPNTS | SITHAI_MODE_FLEEINGTOWAYPOINT);
            pLocal->submode &= ~SITHAI_SUBMODE_SEMICONTINUOUSWPNTMOTION;
            sithAIUtil_ClearAIWaypoint(pLocal);
            break;

        default:
            break;
    }
}

void J3DAPI sithAIUtil_SetWeaponFireFlags(int weaponNum, SithAIUtilFireFlags* pFlags)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_SetWeaponFireFlags, weaponNum, pFlags);

    switch ( weaponNum )
    {
        case 0:
            *pFlags |= SITHAIUTIL_FIRE_UNKNOWN_40;
            break;

        case 1:
            *pFlags |= SITHAIUTIL_FIRE_UNKNOWN_80;
            break;

        case 2:
            *pFlags |= SITHAIUTIL_FIRE_UNKNOWN_100;
            break;

        case 3:
            *pFlags |= SITHAIUTIL_FIRE_UNKNOWN_200;
            break;

        default:
            break;
    }
}

// flags: 0x100000 own position, 0x400 move position, 0x200000 home, 0x300 goal, 0x1000 where the thing to flee from
// will be next frame. Returns 0 for the latter (and when nothing matches).
int J3DAPI sithAIUtil_AIGetMovePos(const SithAIControlBlock* pLocal, int flags, rdVector3* pDestPos)
{
    INDY_AB_ORIGINAL(sithAIUtil_AIGetMovePos, pLocal, flags, pDestPos);

    if ( (flags & 0x100000) != 0 )
    {
        *pDestPos = pLocal->pOwner->pos;
        return 1;
    }

    if ( (flags & 0x400) != 0 )
    {
        *pDestPos = pLocal->movePos;
        return 1;
    }

    if ( (flags & 0x200000) != 0 )
    {
        *pDestPos = pLocal->homePos;
        return 1;
    }

    if ( (flags & 0x300) != 0 )
    {
        if ( pLocal->goalThing )
        {
            *pDestPos = pLocal->goalThing->pos;
        }
        else
        {
            *pDestPos = pLocal->vecUnknown6;
        }

        return 1;
    }

    if ( (flags & 0x1000) != 0 && pLocal->pFleeFromThing )
    {
        const SithThing* pFleeFrom = pLocal->pFleeFromThing;
        rdVector_ScaleAdd3(pDestPos, &pFleeFrom->moveInfo.physics.velocity, sithTime_g_frameTimeFlex, &pFleeFrom->pos);
    }

    return 0;
}

signed int J3DAPI sithAIUtil_GetXYHeadingVector(const SithThing* pThing, rdVector3* pDest)
{
    INDY_AB_ORIGINAL(sithAIUtil_GetXYHeadingVector, pThing, pDest);

    if ( !pThing || !pDest )
    {
        SITHLOG_ERROR("Bad args passed to sithAIUtil_GetXYHeadingVector().\n");
        return 0;
    }

    if ( rdVector_IsZero3(&pThing->moveInfo.physics.velocity) )
    {
        *pDest = pThing->orient.lvec;
    }
    else
    {
        *pDest = pThing->moveInfo.physics.velocity;
    }

    pDest->z = 0.0f;
    pDest->x = sithAIUtil_ZeroIfTiny(pDest->x);
    pDest->y = sithAIUtil_ZeroIfTiny(pDest->y);
    pDest->z = 0.0f;
    rdVector_Normalize3Acc(pDest);
    return 1;
}

signed int J3DAPI sithAIUtil_GetXYZHeadingVector(const SithThing* pThing, rdVector3* pDest)
{
    INDY_AB_ORIGINAL(sithAIUtil_GetXYZHeadingVector, pThing, pDest);

    if ( !pThing || !pDest )
    {
        SITHLOG_ERROR("Bad args passed to sithAIUtil_GetXYHeadingVector().\n");
        return 0;
    }

    if ( rdVector_IsZero3(&pThing->moveInfo.physics.velocity) )
    {
        *pDest = pThing->orient.lvec;
        return 1;
    }

    *pDest = pThing->moveInfo.physics.velocity;
    pDest->x = sithAIUtil_ZeroIfTiny(pDest->x);
    pDest->y = sithAIUtil_ZeroIfTiny(pDest->y);
    pDest->z = sithAIUtil_ZeroIfTiny(pDest->z);
    rdVector_Normalize3Acc(pDest);
    return 1;
}

int J3DAPI sithAIUtil_CanAttackTarget(const SithThing* pThing)
{
    INDY_AB_ORIGINAL(sithAIUtil_CanAttackTarget, pThing);

    if ( !pThing )
    {
        return 0;
    }

    if ( (pThing->flags & (SITH_TF_DESTROYED | SITH_TF_DYING | SITH_TF_DISABLED)) != 0 )
    {
        return 0;
    }

    if ( pThing->type == SITH_THING_PLAYER && (pThing->thingInfo.actorInfo.flags & SITH_AF_CONTROLSDISABLED) != 0 )
    {
        return 0;
    }

    return 1;
}

int J3DAPI sithAIUtil_IsThingMoving(const SithThing* pThing)
{
    INDY_AB_ORIGINAL(sithAIUtil_IsThingMoving, pThing);

    if ( !pThing || pThing->type == SITH_THING_FREE )
    {
        return 0;
    }

    if ( pThing->type == SITH_THING_ACTOR )
    {
        const SithAIControlBlock* pLocal = pThing->controlInfo.aiControl.pLocal;
        if ( !pLocal )
        {
            return 0;
        }

        return pLocal->mode & SITHAI_MODE_MOVING;
    }

    if ( pThing->moveType == SITH_MT_PHYSICS )
    {
        return !rdVector_IsZero3(&pThing->moveInfo.physics.velocity) || !rdVector_IsZero3(&pThing->moveInfo.physics.angularVelocity);
    }

    if ( pThing->moveType == SITH_MT_PATH )
    {
        return pThing->moveInfo.pathMovement.mode & SITH_PATHMOVE_MOVE;
    }

    return 0;
}

void J3DAPI sithAIUtil_ApplyForce(SithAIControlBlock* pLocal, const rdVector3* pDirection, float force)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_ApplyForce, pLocal, pDirection, force);

    SithThing* pOwner = pLocal->pOwner;
    rdVector3 forceVec;
    rdVector_Scale3(&forceVec, pDirection, pOwner->moveInfo.physics.mass * force);
    sithPhysics_ApplyForce(pOwner, &forceVec);
}

// Turns the direction pOutPYR randomly, by up to scalar / 2 per axis
void J3DAPI sithAIUtil_RandomRotate(rdVector3* pOutPYR, float scalar)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_RandomRotate, pOutPYR, scalar);

    pOutPYR->x = (sithAIUtil_RandFraction() - 0.5f) * scalar + pOutPYR->x;
    pOutPYR->y = (sithAIUtil_RandFraction() - 0.5f) * scalar + pOutPYR->y;
    pOutPYR->z = (sithAIUtil_RandFraction() - 0.5f) * scalar + pOutPYR->z;
    rdVector_Normalize3Acc(pOutPYR);
}

// Collects up to sizeResult things of the types in thingTypeMask in the view cone (fovX, fovY in degrees) of orient,
// through the sectors visible from pStartSector up to maxDistance
size_t J3DAPI sithAIUtil_GetThingsInView(const SithSector* pStartSector, const rdMatrix34* orient, float fovX, float fovY, int sizeResult, SithThing** pResult, int thingTypeMask, float maxDistance)
{
    INDY_AB_ORIGINAL(sithAIUtil_GetThingsInView, pStartSector, orient, fovX, fovY, sizeResult, pResult, thingTypeMask, maxDistance);

    SITH_ASSERTREL(pStartSector != NULL);

    if ( fovX < 0.0f || fovY < 0.0f )
    {
        return 0;
    }

    sithAIUtil_thingInViewTypeMask       = thingTypeMask;
    sithAIUtil_sizeThingsInView          = sizeResult;
    sithAIUtil_maxDistanceToThingsInView = maxDistance;
    sithAIUtil_aThingsInView             = pResult;

    // Sines of the half FOV angles: the limits for the dot products with the side and up axes
    float cosHalfFov;
    stdMath_SinCos(90.0f - fovX * 0.5f, &cosHalfFov, &sithAIUtil_thingsInViewFovX);
    stdMath_SinCos(90.0f - fovY * 0.5f, &cosHalfFov, &sithAIUtil_thingsInViewFovY);

    sithAdvanceRenderTick();
    sithAIUtil_numThingsInView   = 0;
    sithAIUtil_numVisitedSectors = 0;
    sithAIUtil_GetNextThingsInView(pStartSector, orient, 0.0f);
    return sithAIUtil_numThingsInView;
}

void J3DAPI sithAIUtil_GetNextThingsInView(const SithSector* pSector, const rdMatrix34* pOrient, float distance)
{
    INDY_AB_ORIGINAL_VOID(sithAIUtil_GetNextThingsInView, pSector, pOrient, distance);

    if ( pSector->renderTick == sithMain_g_curRenderTick )
    {
        return;
    }

    ((SithSector*)pSector)->renderTick = sithMain_g_curRenderTick; // marks the sector visited
    if ( sithAIUtil_numVisitedSectors >= 128 )
    {
        return;
    }

    sithAIUtil_numVisitedSectors++;

    for ( SithThing* pThing = pSector->pFirstThingInSector; pThing && sithAIUtil_numThingsInView < (size_t)sithAIUtil_sizeThingsInView; pThing = pThing->pNextThingInSector )
    {
        if ( ((1 << pThing->type) & sithAIUtil_thingInViewTypeMask) == 0 || (pThing->flags & (SITH_TF_DESTROYED | SITH_TF_DYING | SITH_TF_DISABLED)) != 0 )
        {
            continue;
        }

        rdVector3 dir;
        rdVector_Sub3(&dir, &pThing->pos, &pOrient->dvec);
        rdVector_Normalize3Acc(&dir);

        rdVector3 up, right, look;
        rdVector_Normalize3(&up, &pOrient->uvec);
        rdVector_Normalize3(&right, &pOrient->rvec);
        rdVector_Normalize3(&look, &pOrient->lvec);

        float upDot    = rdVector_Dot3(&up, &dir);
        float rightDot = rdVector_Dot3(&right, &dir);
        float lookDot  = rdVector_Dot3(&look, &dir);
        if ( upDot <= sithAIUtil_thingsInViewFovY && upDot >= -sithAIUtil_thingsInViewFovY
            && rightDot <= sithAIUtil_thingsInViewFovX && rightDot >= -sithAIUtil_thingsInViewFovX
            && lookDot >= 0.0f )
        {
            if ( sithAIUtil_numThingsInView >= (size_t)sithAIUtil_sizeThingsInView )
            {
                return;
            }

            sithAIUtil_aThingsInView[sithAIUtil_numThingsInView++] = pThing;
        }
    }

    if ( distance > sithAIUtil_maxDistanceToThingsInView )
    {
        return;
    }

    for ( const SithSurfaceAdjoin* pAdjoin = pSector->pFirstAdjoin; pAdjoin; pAdjoin = pAdjoin->pNextAdjoin )
    {
        if ( (pAdjoin->flags & SITH_ADJOIN_VISIBLE) == 0 )
        {
            continue;
        }

        // Only through adjoins that can be seen through, and in the view direction
        const SithSurface* pSurf = pAdjoin->pAdjoinSurface;
        const rdMaterial* pMaterial = pSurf->face.pMaterial;
        if ( pMaterial && pSurf->face.geometryMode != RD_GEOMETRY_NONE && (pSurf->face.flags & RD_FF_TEX_TRANSLUCENT) == 0 && pMaterial->formatType == STDCOLOR_FORMAT_RGB )
        {
            continue;
        }

        const rdVector3* pNormal = &pSurf->face.normal;
        if ( pNormal->y * pOrient->lvec.y + pNormal->z * pOrient->lvec.z + pNormal->x * pOrient->lvec.x < 0.0f )
        {
            sithAIUtil_GetNextThingsInView(pAdjoin->pAdjoinSector, pOrient, pAdjoin->distance + pAdjoin->pMirrorAdjoin->distance + distance);
        }
    }
}

// Whether the AI notices pTarget, by chance: the farther, darker, stiller or more hidden the target, the less likely
int J3DAPI sithAIUtil_CanSeeTarget(const SithAIControlBlock* pLocal, SithThing* pTarget, float distance)
{
    INDY_AB_ORIGINAL(sithAIUtil_CanSeeTarget, pLocal, pTarget, distance);

    const SithThing* pOwner = pLocal->pOwner;
    float chance = 1.0f;
    if ( !pTarget || (pTarget->type != SITH_THING_ACTOR && pTarget->type != SITH_THING_PLAYER) )
    {
        return 1;
    }

    if ( distance >= 2.0f )
    {
        if ( (pLocal->mode & SITHAI_MODE_ACTIVE) == 0 )
        {
            chance = 0.5f;
        }

        if ( (pTarget->thingInfo.actorInfo.flags & SITH_AF_HEADLIGHT) == 0 && (pTarget->unknownFlags & 1) == 0 )
        {
            float distFactor = (distance - 2.0f) * 0.1f;
            if ( distFactor < 0.0f )
            {
                distFactor = 0.0f;
            }
            else if ( distFactor > 0.6f )
            {
                distFactor = 0.6f;
            }

            chance = (1.0f - distFactor) * chance;

            if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_SEEINDARK) == 0 && rdLight_GetIntensity(&pTarget->pInSector->ambientLight) < 0.5f )
            {
                chance = (rdLight_GetIntensity(&pTarget->pInSector->ambientLight) + 0.2f) * chance;
            }

            if ( pTarget->moveType == SITH_MT_PHYSICS )
            {
                if ( (pTarget->moveInfo.physics.flags & SITH_PF_CROUCHING) != 0 )
                {
                    chance = chance * 0.75f;
                }

                if ( rdVector_IsZero3(&pTarget->moveInfo.physics.velocity) )
                {
                    chance = chance * 0.5f;
                }
            }
        }
    }

    if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_SEEINVISIBLE) == 0 )
    {
        if ( (pTarget->thingInfo.actorInfo.flags & SITH_AF_INVISIBLE) != 0 )
        {
            chance = chance * 0.05f;
        }

        if ( (pOwner->thingInfo.actorInfo.flags & SITH_AF_BLIND) != 0 )
        {
            chance = chance * 0.05f;
        }
    }

    if ( chance < 0.05f )
    {
        chance = 0.05f;
    }
    else if ( chance > 1.0f )
    {
        chance = 1.0f;
    }

    return sithAIUtil_RandFraction() < chance;
}

#ifdef J3D_STANDALONE // INDY: Stage 4, the exe's globals of this module, with the exe's initial values
float sithAIUtil_thingsInViewFovY;
size_t sithAIUtil_numVisitedSectors;
SithAIWaypointLayerFlag sithAIUtil_activeWpntLayer;
size_t sithAIUtil_numThingsInView;
rdVector3 sithAIUtil_vec_585490;
SithAIControlBlock* sithAIUtil_pMkPointCurLocal;
rdVector3 sithAIUtil_mkPointCurPYR;
float sithAIUtil_thingsInViewFovX;
SithAIWaypointOwner sithAIUtil_aWpntOwners[10];
SithThing** sithAIUtil_aThingsInView;
SithAIWaypointDistance sithAIUtil_aWpntDistances[60];
int sithAIUtil_sizeThingsInView;
rdVector3 sithAIUtil_vec_585718;
float sithAIUtil_maxDistanceToThingsInView;
int sithAIUtil_thingInViewTypeMask;
SithAIWaypoint sithAIUtil_aAIWpnts[60];
float sithAIUtil_secLastUpdate;
int sithAIUtil_mkPoinCurFlags;
int sithAIUtil_g_bRenderAIWpnts;
#endif
