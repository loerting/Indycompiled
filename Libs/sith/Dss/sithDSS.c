#include "sithDSS.h"
#include <j3dcore/j3dhook.h>

#include <rdroid/Engine/rdPuppet.h>

#include <sith/AI/sithAIMove.h>
#include <sith/Cog/sithCog.h>
#include <sith/Devices/sithComm.h>
#include <sith/Dss/sithMulti.h>
#include <sith/Engine/sithAnimate.h>
#include <sith/Engine/sithCamera.h>
#include <sith/Engine/sithPuppet.h>
#include <sith/Engine/sithRender.h>
#include <sith/Gameplay/sithEvent.h>
#include <sith/Gameplay/sithFX.h>
#include <sith/Gameplay/sithInventory.h>
#include <sith/Gameplay/sithPlayer.h>
#include <sith/Gameplay/sithPlayerActions.h>
#include <sith/Gameplay/sithPlayerControls.h>
#include <sith/Gameplay/sithTime.h>
#include <sith/Gameplay/sithVehicleControls.h>
#include <sith/Gameplay/sithWhip.h>
#include <sith/RTI/symbols.h>
#include <sith/World/sithMaterial.h>
#include <sith/World/sithModel.h>
#include <sith/World/sithSector.h>
#include <sith/World/sithSurface.h>
#include <sith/World/sithThing.h>
#include <sith/World/sithWorld.h>

#include <std/General/stdMemory.h>
#include <std/General/stdUtil.h>

#define VANILLACB(callback) J3D_EXE_FUNC(callback, rdPuppetTrackCallback) // INDY: NULL standalone (Stage 4)

// Index of pElem in aArray, computed like the original: the signed byte distance divided by the element size.
// The distance doesn't have to be a multiple of the element size (e.g. a ledge face of another mesh than the first).
static inline int sithDSS_GetArrayIndex(const void* pElem, const void* aArray, size_t elemSize)
{
    return (int)((intptr_t)pElem - (intptr_t)aArray) / (int)elemSize;
}

static const rdPuppetTrackCallback sithDSS_aPuppetCallbacks[7][2] = {
    { &sithPuppet_DefaultCallback,           VANILLACB(sithPuppet_DefaultCallback)           },
    { &sithPlayerControls_PuppetCallback,    VANILLACB(sithPlayerControls_PuppetCallback)    },
    { &sithVehicleControls_PuppetCallback,   VANILLACB(sithVehicleControls_PuppetCallback)   },
    { &sithAIMove_PuppetCallback,            VANILLACB(sithAIMove_PuppetCallback)            },
    { &sithWhip_WhipClimbPuppetCallback,     VANILLACB(sithWhip_WhipClimbPuppetCallback)     },
    { &sithWhip_ClimbDismountPuppetCallback, VANILLACB(sithWhip_ClimbDismountPuppetCallback) },
    { &sithWhip_WhipFirePuppetCallback,      VANILLACB(sithWhip_WhipFirePuppetCallback)      }
};

// TODO: When all functions that are using following callbacks are defined, uncomment below const and remove previous definition above
//static const rdPuppetTrackCallback sithDSS_aPuppetCallbacks[7] = {
//    &sithPuppet_DefaultCallback,
//    &sithPlayerControls_PuppetCallback,
//    &sithVehicleControls_PuppetCallback,
//    &sithAIMove_PuppetCallback,
//    &sithWhip_WhipClimbPuppetCallback,
//    &sithWhip_ClimbDismountPuppetCallback,
//    &sithWhip_WhipFirePuppetCallback
//};

void sithDSS_InstallHooks(void)
{
    // Uncomment only lines for functions that have full definition and doesn't call original function (non-thunk functions)

    J3D_HOOKFUNC(sithDSS_SurfaceStatus);
    J3D_HOOKFUNC(sithDSS_ProcessSurfaceStatus);
    J3D_HOOKFUNC(sithDSS_SectorStatus);
    J3D_HOOKFUNC(sithDSS_SyncSeenSectors);
    J3D_HOOKFUNC(sithDSS_ProcessSyncSeenSecors);
    J3D_HOOKFUNC(sithDSS_ProcessSectorStatus);
    J3D_HOOKFUNC(sithDSS_SectorFlags);
    J3D_HOOKFUNC(sithDSS_ProcessSectorFlags);
    J3D_HOOKFUNC(sithDSS_AIStatus);
    J3D_HOOKFUNC(sithDSS_ProcessAIStatus);
    J3D_HOOKFUNC(sithDSS_Inventory);
    J3D_HOOKFUNC(sithDSS_ProcessInventory);
    J3D_HOOKFUNC(sithDSS_AnimStatus);
    J3D_HOOKFUNC(sithDSS_ProcessAnimStatus);
    J3D_HOOKFUNC(sithDSS_PuppetStatus);
    J3D_HOOKFUNC(sithDSS_ProcessPuppetStatus);
    J3D_HOOKFUNC(sithDSS_SyncTaskEvents);
    J3D_HOOKFUNC(sithDSS_ProcessSyncTaskEvents);
    J3D_HOOKFUNC(sithDSS_SyncCameras);
    J3D_HOOKFUNC(sithDSS_ProcessSyncCameras);
    J3D_HOOKFUNC(sithDSS_SyncGameState);
    J3D_HOOKFUNC(sithDSS_ProcessSyncGameState);
    J3D_HOOKFUNC(sithDSS_SyncVehicleControlState);
    J3D_HOOKFUNC(sithDSS_ProcessVehicleControlsState);
    J3D_HOOKFUNC(sithDSS_sub_4B3760);
    J3D_HOOKFUNC(sithDSS_sub_4B3790);
}

void sithDSS_ResetGlobals(void)
{}

int J3DAPI sithDSS_SurfaceStatus(const SithSurface* pSurf, DPID idTo, unsigned int outstream)
{
    INDY_AB_ORIGINAL(sithDSS_SurfaceStatus, pSurf, idTo, outstream);

    SITH_ASSERTREL(pSurf);
    SITHDSS_STARTOUT(SITHDSS_SURFACESTATUS);

    SITHDSS_PUSHINT16(sithSurface_GetSurfaceIndex(pSurf));
    SITHDSS_PUSHUINT32(pSurf->flags);
    SITHDSS_PUSHINT32(pSurf->face.pMaterial ? pSurf->face.pMaterial->num : -1);
    SITHDSS_PUSHINT16(pSurf->face.matCelNum);
    SITHDSS_PUSHVEC2(&pSurf->face.texVertOffset);
    SITHDSS_PUSHVEC3((const rdVector3*)&pSurf->face.extraLight); // Note: alpha is not serialized
    SITHDSS_PUSHUINT32(pSurf->face.flags);
    SITHDSS_PUSHUINT32(pSurf->face.geometryMode);
    SITHDSS_PUSHUINT32(pSurf->face.lightingMode);

    if ( pSurf->pAdjoin )
    {
        SITHDSS_PUSHUINT32(pSurf->pAdjoin->flags);
    }

    // Only the alpha of each vertex intensity is serialized
    for ( size_t i = 0; i < pSurf->face.numVertices; ++i )
    {
        SITHDSS_PUSHFLOAT(pSurf->aIntensities[i].alpha);
    }

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, DPSEND_GUARANTEED);
}

int J3DAPI sithDSS_ProcessSurfaceStatus(const SithMessage* pMsg)
{
    INDY_AB_ORIGINAL(sithDSS_ProcessSurfaceStatus, pMsg);

    SITH_ASSERTREL(pMsg && sithWorld_g_pCurrentWorld);
    SITHDSS_STARTIN(pMsg);

    size_t surfIdx = SITHDSS_POPUINT16();
    if ( surfIdx >= sithWorld_g_pCurrentWorld->numSurfaces )
    {
        SITHLOG_ERROR("DSS::SurfaceStatus..Surface %d out of range.\n", surfIdx);
        SITHDSS_ENDIN;
        return 0;
    }

    SithSurface* pSurf = &sithWorld_g_pCurrentWorld->aSurfaces[surfIdx];
    pSurf->flags = SITHDSS_POPUINT32();

    rdMaterial* pMaterial = sithMaterial_GetMaterialByIndex(SITHDSS_POPINT32());
    pSurf->face.pMaterial = pMaterial;

    int celNum = SITHDSS_POPUINT16();
    if ( pMaterial && celNum < (int)pMaterial->numCels )
    {
        pSurf->face.matCelNum = celNum;
    }
    else
    {
        pSurf->face.matCelNum = -1;
    }

    SITHDSS_POPVEC2(&pSurf->face.texVertOffset);
    SITHDSS_POPVEC3((rdVector3*)&pSurf->face.extraLight);
    pSurf->face.flags        = SITHDSS_POPUINT32();
    pSurf->face.geometryMode = SITHDSS_POPUINT32();
    pSurf->face.lightingMode = SITHDSS_POPUINT32();

    if ( pSurf->pAdjoin )
    {
        pSurf->pAdjoin->flags = SITHDSS_POPUINT32();
    }

    for ( size_t i = 0; i < pSurf->face.numVertices; ++i )
    {
        pSurf->aIntensities[i].alpha = SITHDSS_POPFLOAT();
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSS_SectorStatus(const SithSector* pSector, DPID idTo, unsigned int outstream)
{
    INDY_AB_ORIGINAL(sithDSS_SectorStatus, pSector, idTo, outstream);

    SITH_ASSERTREL(pSector);
    SITHDSS_STARTOUT(SITHDSS_SECTORSTATUS);

    SITHDSS_PUSHINT16(sithSector_GetSectorIndex(pSector));
    SITHDSS_PUSHUINT32(pSector->flags);
    SITHDSS_PUSHVEC3((const rdVector3*)&pSector->ambientLight); // Note: alpha is not serialized
    SITHDSS_PUSHVEC3((const rdVector3*)&pSector->extraLight);   // Note: alpha is not serialized

    if ( (pSector->flags & SITH_SECTOR_USETHRUST) != 0 )
    {
        SITHDSS_PUSHVEC3(&pSector->thrust);
    }

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, DPSEND_GUARANTEED);
}

int J3DAPI sithDSS_SyncSeenSectors(DPID idTo, unsigned int outstream)
{
    INDY_AB_ORIGINAL(sithDSS_SyncSeenSectors, idTo, outstream);

    SithWorld* pWorld = sithWorld_g_pCurrentWorld;
    if ( !pWorld )
    {
        return 1;
    }

    // Note: one byte per sector, the message size isn't checked against the number of sectors
    SITHDSS_STARTOUT(SITHDSS_SYNCSEENSECTORS);
    for ( size_t i = 0; i < pWorld->numSectors; ++i )
    {
        SITHDSS_PUSHUINT8((pWorld->aSectors[i].flags & SITH_SECTOR_SEEN) != 0);
    }

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, DPSEND_GUARANTEED);
}

int J3DAPI sithDSS_ProcessSyncSeenSecors(const SithMessage* pMsg)
{
    INDY_AB_ORIGINAL(sithDSS_ProcessSyncSeenSecors, pMsg);

    SITHDSS_STARTIN(pMsg);
    if ( !sithWorld_g_pCurrentWorld )
    {
        SITHDSS_ENDIN;
        return 0;
    }

    for ( size_t i = 0; i < sithWorld_g_pCurrentWorld->numSectors; ++i )
    {
        SithSector* pSector = &sithWorld_g_pCurrentWorld->aSectors[i];
        if ( SITHDSS_POPUINT8() )
        {
            pSector->flags |= SITH_SECTOR_SEEN;
        }
        else
        {
            pSector->flags &= ~SITH_SECTOR_SEEN;
        }
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSS_ProcessSectorStatus(const SithMessage* pMsg)
{
    INDY_AB_ORIGINAL(sithDSS_ProcessSectorStatus, pMsg);

    SITHDSS_STARTIN(pMsg);

    size_t sectorIdx = SITHDSS_POPUINT16();
    if ( sectorIdx >= sithWorld_g_pCurrentWorld->numSectors )
    {
        SITHLOG_ERROR("Sector %d out of range for DSS:SectorStatus.\n", sectorIdx);
        SITHDSS_ENDIN;
        return 0;
    }

    SithSector* pSector = &sithWorld_g_pCurrentWorld->aSectors[sectorIdx];

    SithSectorFlag prevFlags = pSector->flags;
    SithSectorFlag newFlags  = SITHDSS_POPUINT32();
    pSector->flags = newFlags;

    if ( (newFlags & SITH_SECTOR_ADJOINSOFF) != 0 && (prevFlags & SITH_SECTOR_ADJOINSOFF) == 0 )
    {
        sithSector_HideSectorAdjoins(pSector);
    }
    else if ( (newFlags & SITH_SECTOR_ADJOINSOFF) == 0 && (prevFlags & SITH_SECTOR_ADJOINSOFF) != 0 )
    {
        sithSector_ShowSectorAdjoins(pSector);
    }

    SITHDSS_POPVEC3((rdVector3*)&pSector->ambientLight);
    pSector->ambientLight.alpha = 0.0f;

    SITHDSS_POPVEC3((rdVector3*)&pSector->extraLight);
    pSector->extraLight.alpha = 0.0f;

    if ( (pSector->flags & SITH_SECTOR_USETHRUST) != 0 )
    {
        SITHDSS_POPVEC3(&pSector->thrust);
    }
    else
    {
        rdVector_Zero3(&pSector->thrust);
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSS_SectorFlags(const SithSector* pSector, DPID idTo, unsigned int outstream)
{
    // INDY: multiplayer stub (sector flag changes are only queued while a message output stream is open, i.e. in a network game;
    //       in single-player the original only builds the message and the send layer drops it, returning 1)
    SITH_ASSERTREL(pSector);
    return 1;
}

int J3DAPI sithDSS_ProcessSectorFlags(const SithMessage* pMsg)
{
    // INDY: multiplayer stub (handler for a message that is only received over the network, never written to savegames)
    return 1;
}

int J3DAPI sithDSS_AIStatus(const SithAIControlBlock* pLocal, DPID idTo, signed int outstream)
{
    INDY_AB_ORIGINAL(sithDSS_AIStatus, pLocal, idTo, outstream);

    SITH_ASSERTREL(pLocal && pLocal->pOwner && pLocal->pClass);
    SITHDSS_STARTOUT(SITHDSS_AISTATUS);

    SITHDSS_PUSHINT16(pLocal->pOwner->idx);
    SITHDSS_PUSHINT16(sithDSS_GetArrayIndex(pLocal->pClass, sithWorld_g_pCurrentWorld->aAIClasses, sizeof(SithAIClass)));
    SITHDSS_PUSHUINT32(pLocal->mode);
    SITHDSS_PUSHUINT32(pLocal->submode);
    SITHDSS_PUSHUINT32(pLocal->msecNextUpdate);

    SITHDSS_PUSHINT16(sithThing_GetThingIndex(pLocal->goalThing));
    SITHDSS_PUSHVEC3(&pLocal->vecUnknown6);
    SITHDSS_PUSHVEC3(&pLocal->vecUnknown5);
    SITHDSS_PUSHINT32(pLocal->unknown150);

    SITHDSS_PUSHINT16(sithThing_GetThingIndex(pLocal->pTargetThing));
    SITHDSS_PUSHVEC3(&pLocal->targetPos);
    SITHDSS_PUSHVEC3(&pLocal->vecUnknown122);
    SITHDSS_PUSHUINT32(pLocal->msecAttackStart);
    SITHDSS_PUSHFLOAT(pLocal->moveSpeed);

    if ( (pLocal->mode & SITHAI_MODE_MOVING) != 0 )
    {
        SITHDSS_PUSHVEC3(&pLocal->movePos);
    }

    if ( (pLocal->mode & SITHAI_MODE_TURNING) != 0 )
    {
        SITHDSS_PUSHVEC3(&pLocal->goalLVec);
    }

    if ( (pLocal->mode & SITHAI_MODE_FLEEING) != 0 )
    {
        SITHDSS_PUSHINT16(sithThing_GetThingIndex(pLocal->pFleeFromThing));
    }

    SITHDSS_PUSHVEC3(&pLocal->homePos);
    SITHDSS_PUSHVEC3(&pLocal->homeOrient);

    for ( size_t i = 0; i < pLocal->numInstincts; ++i )
    {
        const SithAIInstinctState* pState = &pLocal->aInstinctStates[i];
        SITHDSS_PUSHUINT32(pState->flags);
        SITHDSS_PUSHUINT32(pState->msecNextUpdate);
        for ( size_t j = 0; j < STD_ARRAYLEN(pState->aParams); ++j )
        {
            SITHDSS_PUSHFLOAT(pState->aParams[j]);
        }
    }

    SITHDSS_PUSHUINT32(pLocal->msecFireWaitTime);
    SITHDSS_PUSHUINT32(pLocal->msecPauseMoveUntil);

    SITHDSS_PUSHUINT32(pLocal->numFrames);
    for ( size_t i = 0; i < pLocal->numFrames; ++i )
    {
        SITHDSS_PUSHVEC3(&pLocal->aFrames[i]);
    }

    SITHDSS_PUSHINT32(pLocal->allowedSurfaceTypes);
    SITHDSS_PUSHVEC3(&pLocal->vecUnknown3);
    SITHDSS_PUSHVEC3(&pLocal->vecUnknown);
    SITHDSS_PUSHFLOAT(pLocal->maxHomeDist);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, DPSEND_GUARANTEED);
}

int J3DAPI sithDSS_ProcessAIStatus(const SithMessage* pMsg)
{
    INDY_AB_ORIGINAL(sithDSS_ProcessAIStatus, pMsg);

    SithWorld* pWorld = sithWorld_g_pCurrentWorld;
    SITH_ASSERTREL(pWorld && pMsg);
    SITHDSS_STARTIN(pMsg);

    int thingIdx = SITHDSS_POPUINT16();
    SithThing* pThing = sithThing_GetThingByIndex(thingIdx);
    if ( !pThing )
    {
        SITHLOG_ERROR("DSS::AiStatus - Bad thing index %d.\n", thingIdx);
        SITHDSS_ENDIN;
        return 0;
    }

    SithAIControlBlock* pLocal = pThing->controlInfo.aiControl.pLocal;
    if ( pThing->controlType != SITH_CT_AI || !pLocal )
    {
        SITHLOG_ERROR("DSS::AiStatus - Non-AI thing index %d.\n", thingIdx);
        SITHDSS_ENDIN;
        return 0;
    }

    size_t classIdx = SITHDSS_POPUINT16();
    if ( classIdx >= pWorld->numAIClasses )
    {
        SITHDSS_ENDIN;
        return 0;
    }

    pLocal->pClass       = &pWorld->aAIClasses[classIdx];
    pLocal->numInstincts = pLocal->pClass->numInstincts;

    pLocal->mode           = SITHDSS_POPUINT32();
    pLocal->submode        = SITHDSS_POPUINT32();
    pLocal->msecNextUpdate = SITHDSS_POPUINT32();

    pLocal->goalThing = sithThing_GetThingByIndex(SITHDSS_POPUINT16());
    SITHDSS_POPVEC3(&pLocal->vecUnknown6);
    SITHDSS_POPVEC3(&pLocal->vecUnknown5);
    pLocal->unknown150 = SITHDSS_POPINT32();
    pLocal->unknown141 = 0;

    pLocal->pTargetThing = sithThing_GetThingByIndex(SITHDSS_POPUINT16());
    SITHDSS_POPVEC3(&pLocal->targetPos);
    SITHDSS_POPVEC3(&pLocal->vecUnknown122);
    pLocal->msecAttackStart = SITHDSS_POPUINT32();
    pLocal->unknown124      = 0;
    pLocal->moveSpeed       = SITHDSS_POPFLOAT();

    if ( (pLocal->mode & SITHAI_MODE_MOVING) != 0 )
    {
        SITHDSS_POPVEC3(&pLocal->movePos);
    }

    if ( (pLocal->mode & SITHAI_MODE_TURNING) != 0 )
    {
        SITHDSS_POPVEC3(&pLocal->goalLVec);
    }

    if ( (pLocal->mode & SITHAI_MODE_FLEEING) != 0 )
    {
        pLocal->pFleeFromThing = sithThing_GetThingByIndex(SITHDSS_POPUINT16());
    }

    SITHDSS_POPVEC3(&pLocal->homePos);
    SITHDSS_POPVEC3(&pLocal->homeOrient);

    for ( size_t i = 0; i < pLocal->numInstincts; ++i )
    {
        SithAIInstinctState* pState = &pLocal->aInstinctStates[i];
        pState->flags          = SITHDSS_POPUINT32();
        pState->msecNextUpdate = SITHDSS_POPUINT32();
        for ( size_t j = 0; j < STD_ARRAYLEN(pState->aParams); ++j )
        {
            pState->aParams[j] = SITHDSS_POPFLOAT();
        }
    }

    pLocal->msecFireWaitTime   = SITHDSS_POPUINT32();
    pLocal->msecPauseMoveUntil = SITHDSS_POPUINT32();

    // Note: the previous frame buffer is not freed (leaked), and when there are no frames the old pointer is kept.
    //       If the allocation fails, the frame data is not skipped and the fields below are read from it.
    pLocal->numFrames = SITHDSS_POPUINT32();
    if ( pLocal->numFrames && (pLocal->aFrames = (rdVector3*)STDMALLOC(sizeof(rdVector3) * pLocal->numFrames)) != NULL )
    {
        pLocal->sizeFrames = pLocal->numFrames;
        for ( size_t i = 0; i < pLocal->numFrames; ++i )
        {
            SITHDSS_POPVEC3(&pLocal->aFrames[i]);
        }
    }
    else
    {
        pLocal->sizeFrames = 0;
        pLocal->numFrames  = 0;
    }

    pLocal->allowedSurfaceTypes = SITHDSS_POPINT32();
    SITHDSS_POPVEC3(&pLocal->vecUnknown3);
    SITHDSS_POPVEC3(&pLocal->vecUnknown);
    pLocal->maxHomeDist = SITHDSS_POPFLOAT();

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSS_Inventory(const SithThing* pThing, unsigned int inventoryId, DPID idTo, unsigned int outstream)
{
    INDY_AB_ORIGINAL(sithDSS_Inventory, pThing, inventoryId, idTo, outstream);

    SITH_ASSERTREL(pThing);
    SITH_ASSERTREL(inventoryId < SITHINVENTORY_MAXTYPES);

    if ( pThing->type != SITH_THING_PLAYER && pThing->type != SITH_THING_ACTOR )
    {
        return 0;
    }

    const SithPlayer* pPlayer = pThing->thingInfo.actorInfo.pPlayer;
    if ( !pPlayer )
    {
        return 0;
    }

    SITHDSS_STARTOUT(SITHDSS_INVENTORY);
    SITHDSS_PUSHINT16(sithThing_GetThingIndex(pThing));
    SITHDSS_PUSHUINT16(inventoryId);
    SITHDSS_PUSHFLOAT(pPlayer->aItems[inventoryId].amount);
    SITHDSS_PUSHUINT32(pPlayer->aItems[inventoryId].status);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, DPSEND_GUARANTEED);
}

int J3DAPI sithDSS_ProcessInventory(const SithMessage* pMsg)
{
    INDY_AB_ORIGINAL(sithDSS_ProcessInventory, pMsg);

    SITH_ASSERTREL(pMsg);
    SITHDSS_STARTIN(pMsg);

    SithThing* pThing = sithThing_GetThingByIndex(SITHDSS_POPUINT16());
    if ( pThing && (pThing->type == SITH_THING_ACTOR || pThing->type == SITH_THING_PLAYER) && pThing->thingInfo.actorInfo.pPlayer )
    {
        SithPlayer* pPlayer = pThing->thingInfo.actorInfo.pPlayer;
        size_t typeId = SITHDSS_POPUINT16();
        if ( typeId < SITHINVENTORY_MAXTYPES )
        {
            pPlayer->aItems[typeId].amount = SITHDSS_POPFLOAT();
            pPlayer->aItems[typeId].status = SITHDSS_POPUINT32();
            SITHDSS_ENDIN;
            return 1;
        }
    }

    SITHLOG_ERROR("Error processing DSS::Inventory message.\n");
    SITHDSS_ENDIN;
    return 0;
}

int J3DAPI sithDSS_AnimStatus(const SithAnimationSlot* pAnim, DPID idTo, unsigned int outstream)
{
    INDY_AB_ORIGINAL(sithDSS_AnimStatus, pAnim, idTo, outstream);

    SITH_ASSERTREL(pAnim);
    SITHDSS_STARTOUT(SITHDSS_ANIMSTATUS);

    SITHDSS_PUSHINT32(pAnim->animID);
    SITHDSS_PUSHUINT32(pAnim->flags);

    if ( (pAnim->flags & (SITHANIMATE_THING | SITHANIMATE_SPRITE)) != 0 )
    {
        SITHDSS_PUSHINT16(sithThing_GetThingIndex(pAnim->pThing));
        SITHDSS_PUSHUINT32(pAnim->thingSignature);
    }

    if ( (pAnim->flags & SITHANIMATE_CAMERA_ZOOM) != 0 )
    {
        SITHDSS_PUSHFLOAT(pAnim->deltaValue);
        SITHDSS_PUSHFLOAT(pAnim->endValue);
        SITHDSS_PUSHFLOAT(pAnim->curValue);
    }

    if ( (pAnim->flags & SITHANIMATE_THING_MOVE) != 0 )
    {
        SITHDSS_PUSHUINT32(pAnim->thingSignature);
        SITHDSS_PUSHFLOAT(pAnim->deltaValue);
        SITHDSS_PUSHFLOAT(pAnim->endValue);
        SITHDSS_PUSHFLOAT(pAnim->curValue);
    }

    if ( (pAnim->flags & SITHANIMATE_THING_MOVEPOS) != 0 )
    {
        SITHDSS_PUSHUINT32(pAnim->thingSignature);
        SITHDSS_PUSHFLOAT(pAnim->secTimeRemaining);
        SITHDSS_PUSHVEC3(&pAnim->thingEndPosition);
    }

    if ( (pAnim->flags & (SITHANIMATE_PUSHITEM | SITHANIMATE_PULLITEM)) != 0 )
    {
        SITHDSS_PUSHINT16(sithThing_GetThingIndex(pAnim->pItemThing));
        SITHDSS_PUSHUINT32(pAnim->thingSignature);
        SITHDSS_PUSHFLOAT(pAnim->deltaValue);
        SITHDSS_PUSHFLOAT(pAnim->endValue);
        SITHDSS_PUSHFLOAT(pAnim->curValue);
        SITHDSS_PUSHUINT32(pAnim->curFrame);
        SITHDSS_PUSHVEC4(&pAnim->deltaVector);
        SITHDSS_PUSHVEC4(&pAnim->startVector);
        SITHDSS_PUSHVEC4(&pAnim->endVector);
        SITHDSS_PUSHINT32(pAnim->trackNum);
        SITHDSS_PUSHUINT32(pAnim->msecPerFrame);
        SITHDSS_PUSHINT32(sithSurface_GetSurfaceIndex(pAnim->pSurface));
        SITHDSS_PUSHVEC3(&pAnim->direction3);
    }

    if ( (pAnim->flags & SITHANIMATE_SPRITE_SIZE) != 0 )
    {
        SITHDSS_PUSHVEC4(&pAnim->startVector);
        SITHDSS_PUSHVEC4(&pAnim->curVector);
        SITHDSS_PUSHVEC4(&pAnim->deltaVector);
        SITHDSS_PUSHVEC4(&pAnim->endVector);
    }

    if ( (pAnim->flags & SITHANIMATE_THING_QUICKTURN) != 0 )
    {
        SITHDSS_PUSHFLOAT(pAnim->curValue);
        SITHDSS_PUSHFLOAT(pAnim->endValue);
        SITHDSS_PUSHFLOAT(pAnim->deltaValue);
    }

    if ( (pAnim->flags & SITHANIMATE_SURFACE) != 0 )
    {
        SITH_ASSERTREL(pAnim->pSurface);
        SITHDSS_PUSHINT32(sithSurface_GetSurfaceIndex(pAnim->pSurface));
    }

    if ( (pAnim->flags & SITHANIMATE_SCROLL) != 0 )
    {
        SITHDSS_PUSHVEC3(&pAnim->direction3);
        SITHDSS_PUSHVEC2(&pAnim->direction2);
    }

    if ( (pAnim->flags & SITHANIMATE_PAGEFLIP) != 0 )
    {
        SITHDSS_PUSHUINT32(pAnim->msecNextFrameTime);
        SITHDSS_PUSHUINT32(pAnim->msecPerFrame);
        SITHDSS_PUSHUINT32(pAnim->curFrame);
    }

    if ( (pAnim->flags & SITHANIMATE_LIGHT) != 0 )
    {
        SITHDSS_PUSHVEC4(&pAnim->deltaVector);
        SITHDSS_PUSHVEC4(&pAnim->startVector);
        SITHDSS_PUSHVEC4(&pAnim->endVector);
        SITHDSS_PUSHVEC4(&pAnim->curVector);
    }

    if ( (pAnim->flags & SITHANIMATE_MATERIAL) != 0 )
    {
        SITHDSS_PUSHINT32(pAnim->pMaterial->num);
    }

    if ( (pAnim->flags & SITHANIMATE_SECTOR) != 0 )
    {
        SITHDSS_PUSHINT32(sithSector_GetSectorIndex(pAnim->pSector));
    }

    if ( (pAnim->flags & SITHANIMATE_THING_FADE) != 0 )
    {
        SITHDSS_PUSHINT32(sithThing_GetThingIndex(pAnim->pThing));
        SITHDSS_PUSHUINT32(pAnim->thingSignature);
        SITHDSS_PUSHVEC4(&pAnim->startVector);
        SITHDSS_PUSHVEC4(&pAnim->curVector);
        SITHDSS_PUSHVEC4(&pAnim->deltaVector);
        SITHDSS_PUSHVEC4(&pAnim->endVector);
    }

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, DPSEND_GUARANTEED);
}

int J3DAPI sithDSS_ProcessAnimStatus(const SithMessage* pMsg)
{
    INDY_AB_ORIGINAL(sithDSS_ProcessAnimStatus, pMsg);

    SITHDSS_STARTIN(pMsg);

    // Note: when the anim doesn't exist anymore a new slot is used, which keeps its own animID
    SithAnimationSlot* pAnim = sithAnimate_GetAnim(SITHDSS_POPINT32());
    if ( !pAnim )
    {
        pAnim = sithAnimate_Create();
        if ( !pAnim )
        {
            SITHDSS_ENDIN;
            return 0;
        }
    }

    pAnim->flags = SITHDSS_POPUINT32();
    if ( !pAnim->flags )
    {
        sithAnimate_Stop(pAnim);
        SITHDSS_ENDIN;
        return 1;
    }

    if ( (pAnim->flags & (SITHANIMATE_THING | SITHANIMATE_SPRITE)) != 0 )
    {
        pAnim->pThing = sithThing_GetThingByIndex(SITHDSS_POPINT16());
        if ( pAnim->pThing && pAnim->pThing->renderData.type == RD_THING_SPRITE3 )
        {
            pAnim->pMaterial = pAnim->pThing->renderData.data.pSprite3->face.pMaterial;
        }

        pAnim->thingSignature = SITHDSS_POPUINT32();
    }

    if ( (pAnim->flags & SITHANIMATE_CAMERA_ZOOM) != 0 )
    {
        pAnim->deltaValue = SITHDSS_POPFLOAT();
        pAnim->endValue   = SITHDSS_POPFLOAT();
        pAnim->curValue   = SITHDSS_POPFLOAT();
    }

    if ( (pAnim->flags & SITHANIMATE_THING_MOVE) != 0 )
    {
        pAnim->thingSignature = SITHDSS_POPUINT32();
        pAnim->deltaValue     = SITHDSS_POPFLOAT();
        pAnim->endValue       = SITHDSS_POPFLOAT();
        pAnim->curValue       = SITHDSS_POPFLOAT();
    }

    if ( (pAnim->flags & SITHANIMATE_THING_MOVEPOS) != 0 )
    {
        pAnim->thingSignature   = SITHDSS_POPUINT32();
        pAnim->secTimeRemaining = SITHDSS_POPFLOAT();
        SITHDSS_POPVEC3(&pAnim->thingEndPosition);
    }

    if ( (pAnim->flags & (SITHANIMATE_PUSHITEM | SITHANIMATE_PULLITEM)) != 0 )
    {
        pAnim->pItemThing     = sithThing_GetThingByIndex(SITHDSS_POPINT16());
        pAnim->thingSignature = SITHDSS_POPUINT32();
        pAnim->deltaValue     = SITHDSS_POPFLOAT();
        pAnim->endValue       = SITHDSS_POPFLOAT();
        pAnim->curValue       = SITHDSS_POPFLOAT();
        pAnim->curFrame       = SITHDSS_POPUINT32();
        SITHDSS_POPVEC4(&pAnim->deltaVector);
        SITHDSS_POPVEC4(&pAnim->startVector);
        SITHDSS_POPVEC4(&pAnim->endVector);
        pAnim->trackNum     = SITHDSS_POPINT32();
        pAnim->msecPerFrame = SITHDSS_POPUINT32();
        pAnim->pSurface     = sithSurface_GetSurfaceEx(sithWorld_g_pCurrentWorld, SITHDSS_POPINT32());
        SITHDSS_POPVEC3(&pAnim->direction3);
    }

    if ( (pAnim->flags & SITHANIMATE_SPRITE_SIZE) != 0 )
    {
        SITHDSS_POPVEC4(&pAnim->startVector);
        SITHDSS_POPVEC4(&pAnim->curVector);
        SITHDSS_POPVEC4(&pAnim->deltaVector);
        SITHDSS_POPVEC4(&pAnim->endVector);
    }

    if ( (pAnim->flags & SITHANIMATE_THING_QUICKTURN) != 0 )
    {
        pAnim->curValue   = SITHDSS_POPFLOAT();
        pAnim->endValue   = SITHDSS_POPFLOAT();
        pAnim->deltaValue = SITHDSS_POPFLOAT();
    }

    if ( (pAnim->flags & SITHANIMATE_SURFACE) != 0 )
    {
        pAnim->pSurface = sithSurface_GetSurfaceEx(sithWorld_g_pCurrentWorld, SITHDSS_POPINT32());
        if ( pAnim->pSurface )
        {
            pAnim->pMaterial = pAnim->pSurface->face.pMaterial;
        }
    }

    if ( (pAnim->flags & SITHANIMATE_SCROLL) != 0 )
    {
        SITHDSS_POPVEC3(&pAnim->direction3);
        SITHDSS_POPVEC2(&pAnim->direction2);
    }

    if ( (pAnim->flags & SITHANIMATE_PAGEFLIP) != 0 )
    {
        pAnim->msecNextFrameTime = SITHDSS_POPUINT32();
        pAnim->msecPerFrame      = SITHDSS_POPUINT32();
        pAnim->curFrame          = SITHDSS_POPUINT32();
    }

    if ( (pAnim->flags & SITHANIMATE_LIGHT) != 0 )
    {
        SITHDSS_POPVEC4(&pAnim->deltaVector);
        SITHDSS_POPVEC4(&pAnim->startVector);
        SITHDSS_POPVEC4(&pAnim->endVector);
        SITHDSS_POPVEC4(&pAnim->curVector);
    }

    if ( (pAnim->flags & SITHANIMATE_MATERIAL) != 0 )
    {
        pAnim->pMaterial = sithMaterial_GetMaterialByIndex(SITHDSS_POPINT32());
    }

    if ( (pAnim->flags & SITHANIMATE_SECTOR) != 0 )
    {
        pAnim->pSector = sithSector_GetSectorEx(sithWorld_g_pCurrentWorld, SITHDSS_POPINT32());
    }

    if ( (pAnim->flags & SITHANIMATE_THING_FADE) != 0 )
    {
        pAnim->pThing         = sithThing_GetThingByIndex(SITHDSS_POPINT32());
        pAnim->thingSignature = SITHDSS_POPUINT32();
        SITHDSS_POPVEC4(&pAnim->startVector);
        SITHDSS_POPVEC4(&pAnim->curVector);
        SITHDSS_POPVEC4(&pAnim->deltaVector);
        SITHDSS_POPVEC4(&pAnim->endVector);
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSS_PuppetStatus(const SithThing* pThing, DPID idTo, unsigned int outstream)
{
    SITH_ASSERTREL(pThing && pThing->renderData.pPuppet);
    rdPuppet* pPuppet = pThing->renderData.pPuppet;

    SITHDSS_STARTOUT(SITHDSS_PUPPETSTATUS);

    SITHDSS_PUSHINT32(sithThing_GetThingIndex(pThing));
    //*(int32_t*)sithMulti_g_message.data = sithThing_GetThingIndex(pThing);

    //uint8_t* pCurOut = &sithMulti_g_message.data[4];

    size_t trackCount = 0;
    for ( size_t i = 0; i < STD_ARRAYLEN(pPuppet->aTracks); ++i )
    {
        SITHDSS_PUSHINT32(pPuppet->aTracks[i].status);
        /**(int32_t*)pCurOut = pPuppet->aTracks[i].status;
        pCurOut += 4;*/

        if ( pPuppet->aTracks[i].status )
        {
            SITH_ASSERTREL(pPuppet->aTracks[i].pKFTrack);

            SITHDSS_PUSHINT32(pPuppet->aTracks[i].pKFTrack->idx);
            /**(int32_t*)pCurOut = pPuppet->aTracks[i].pKFTrack->idx;
            pCurOut += 4;*/

            // Altered: Changed to serialize 32bit float type, as struct field was repurposed.
            //          Originally this field was not used but was serialized as 32 bit integer
            SITHDSS_PUSHFLOAT(pPuppet->aTracks[i].playbackSpeed);
           // SITHDSS_PUSHINT32(pPuppet->aTracks[i].unknown0);
            /**(int32_t*)pCurOut = pPuppet->aTracks[i].unknown0;
            pCurOut += 4;*/

            SITHDSS_PUSHINT16(pPuppet->aTracks[i].lowPriority);
            /**(int16_t*)pCurOut = (uint16_t)pPuppet->aTracks[i].lowPriority;
            pCurOut += 2;*/

            SITHDSS_PUSHINT16(pPuppet->aTracks[i].highPriority);

            /**(int16_t*)pCurOut = (uint16_t)pPuppet->aTracks[i].highPriority;
            pCurOut += 2;*/

            SITHDSS_PUSHFLOAT(pPuppet->aTracks[i].fps);
            /**(float*)pCurOut = pPuppet->aTracks[i].fps;
            pCurOut += 4;*/

            SITHDSS_PUSHFLOAT(pPuppet->aTracks[i].blendWeight);
            /* *(float*)pCurOut = pPuppet->aTracks[i].blendWeight;
             pCurOut += 4;*/

            SITHDSS_PUSHFLOAT(pPuppet->aTracks[i].curFrame);
            /**(float*)pCurOut = pPuppet->aTracks[i].curFrame;
            pCurOut += 4;*/

            SITHDSS_PUSHFLOAT(pPuppet->aTracks[i].prevFrame);
            /**(float*)pCurOut = pPuppet->aTracks[i].prevFrame;
            pCurOut += 4;*/

            if ( pPuppet->aTracks[i].pfCallback )
            {
                size_t cbIdx = 0;
                for ( ; cbIdx < STD_ARRAYLEN(sithDSS_aPuppetCallbacks); ++cbIdx )
                {
                    if ( pPuppet->aTracks[i].pfCallback == sithDSS_aPuppetCallbacks[cbIdx][0]
                        || pPuppet->aTracks[i].pfCallback == sithDSS_aPuppetCallbacks[cbIdx][1] )
                    {
                        SITHDSS_PUSHUINT8(cbIdx);
                        //*pCurOut++ = (uint8_t)cbIdx;
                        break;
                    }
                }

                if ( cbIdx == STD_ARRAYLEN(sithDSS_aPuppetCallbacks) )
                {
                    SITHLOG_ERROR("Saving Unknown callback for %s\n", pPuppet->aTracks[i].pKFTrack->aName);
                    SITHDSS_PUSHUINT8(STD_ARRAYLEN(sithDSS_aPuppetCallbacks)); // mark there is no callback
                    //*pCurOut++ = STD_ARRAYLEN(sithDSS_aPuppetCallbacks);
                }
            }
            else // mark there is no callback
            {
                SITHDSS_PUSHUINT8(STD_ARRAYLEN(sithDSS_aPuppetCallbacks));
                //*pCurOut++ = STD_ARRAYLEN(sithDSS_aPuppetCallbacks);
            }

            ++trackCount;
        }
    }

    if ( pThing->pPuppetState )
    {
        SITHDSS_PUSHINT16(pThing->pPuppetState->armedMode);
        /**(int16_t*)pCurOut = (int16_t)pThing->pPuppetState->armedMode;
        pCurOut += 2;*/

        SITHDSS_PUSHINT16(pThing->pPuppetState->moveMode);
        /* *(int16_t*)pCurOut = (int16_t)pThing->pPuppetState->moveMode;
         pCurOut += 2;*/

        SITHDSS_PUSHINT16(trackCount);

        /**(int16_t*)pCurOut = (int16_t)trackCount;
        pCurOut += 2;*/

        SithPuppetTrack* pCurTrack = pThing->pPuppetState->pFirstTrack;
        while ( trackCount )
        {
            if ( pCurTrack )
            {
                SITHDSS_PUSHINT32(pCurTrack->trackNum);
                /**(int32_t*)pCurOut = pCurTrack->trackNum;
                pCurOut += 4;*/

                SITHDSS_PUSHUINT32(pCurTrack->submode);
                /**(uint32_t*)pCurOut = pCurTrack->submode;
                pCurOut += 4;*/

                pCurTrack = pCurTrack->pNextTrack;
            }
            else // If no track
            {
                SITHDSS_PUSHUINT32(STD_ARRAYLEN(pPuppet->aTracks)); // mark null track
                /**(uint32_t*)pCurOut = STD_ARRAYLEN(pPuppet->aTracks);
                pCurOut += 4;*/

                SITHDSS_PUSHUINT32(SITH_PUPPET_NUMSUBMODES); // mark no mode
                /**(uint32_t*)pCurOut = SITH_PUPPET_NUMSUBMODES;
                pCurOut += 4;*/
            }

            --trackCount;
        }
    }

    /*sithMulti_g_message.type = SITHDSS_PUPPETSTATUS;
    sithMulti_g_message.length = pCurOut - sithMulti_g_message.data;*/
    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, 1u);
}

int J3DAPI sithDSS_ProcessPuppetStatus(const SithMessage* pMsg)
{
    SITH_ASSERTREL(pMsg);

    SITHDSS_STARTIN(pMsg);

    SithThing* pThing = sithThing_GetThingByIndex(SITHDSS_POPINT32());
    //uint8_t* pCurIn   = (uint8_t*)&pMsg->data[4];
    //SithThing* pThing = sithThing_GetThingByIndex(*(int32_t*)pMsg->data);

    if ( !pThing || !pThing->pPuppetClass || !pThing->pPuppetState || !pThing->renderData.pPuppet )
    {
        SITHLOG_ERROR("Error in DSS::PuppetStatus message format.\n");
        SITHDSS_ENDIN;
        return 0;
    }

    rdPuppet* pPuppet = pThing->renderData.pPuppet;
    rdPuppet_NewEntry(pPuppet, &pThing->renderData);

    for ( size_t i = 0; i < STD_ARRAYLEN(pPuppet->aTracks); ++i )
    {
        pPuppet->aTracks[i].status = SITHDSS_POPINT32();
        /*pPuppet->aTracks[i].status = *(int32_t*)pCurIn;
        pCurIn += 4;*/

        if ( pPuppet->aTracks[i].status )
        {
            int kfIdx = SITHDSS_POPINT32();
            /*int kfIdx = *(int32_t*)pCurIn;
            pCurIn += 4;*/

            pPuppet->aTracks[i].pKFTrack = sithPuppet_GetKeyframeByIndex(kfIdx);

            // Altered: Changed to deserialize 32bit float type, as struct field was repurposed.
            //          Originally this field was not used but was desrialized as 32 bit integer
            pPuppet->aTracks[i].playbackSpeed = SITHDSS_POPFLOAT();
            if ( pPuppet->aTracks[i].playbackSpeed <= 0.0f ) // Added: Added check for 0, and in this case set it to 1.0f
            {
                pPuppet->aTracks[i].playbackSpeed = 1.0f;
            }

           // pPuppet->aTracks[i].unknown0 = SITHDSS_POPINT32();
            /*pPuppet->aTracks[i].unknown0 = *(int32_t*)pCurIn;
            pCurIn += 4;*/

            pPuppet->aTracks[i].lowPriority = SITHDSS_POPINT16();
            /*pPuppet->aTracks[i].lowPriority = *(int16_t*)pCurIn;
            pCurIn += 2;*/

            pPuppet->aTracks[i].highPriority = SITHDSS_POPINT16();
            /* pPuppet->aTracks[i].highPriority = *(int16_t*)pCurIn;
             pCurIn += 2;*/

            pPuppet->aTracks[i].fps = SITHDSS_POPFLOAT();
            /*pPuppet->aTracks[i].fps = *(float*)pCurIn;
            pCurIn += 4;*/

            pPuppet->aTracks[i].blendWeight = SITHDSS_POPFLOAT();
            /*pPuppet->aTracks[i].blendWeight = *(float*)pCurIn;
            pCurIn += 4;*/

            pPuppet->aTracks[i].curFrame = SITHDSS_POPFLOAT();
            /*pPuppet->aTracks[i].curFrame = *(float*)pCurIn;
            pCurIn += 4;*/

            pPuppet->aTracks[i].prevFrame = SITHDSS_POPFLOAT();
            /*pPuppet->aTracks[i].prevFrame = *(float*)pCurIn;
            pCurIn += 4;*/

            size_t cbIdx = SITHDSS_POPUINT8();
            //size_t cbIdx = *pCurIn++;
            if ( cbIdx < STD_ARRAYLEN(sithDSS_aPuppetCallbacks) )
            {
                pPuppet->aTracks[i].pfCallback = sithDSS_aPuppetCallbacks[cbIdx][0];
            }
            else
            {
                pPuppet->aTracks[i].pfCallback = NULL;
            }
        }
    }

    if ( !pThing->pPuppetState )
    {
        SITHDSS_ENDIN;
        return 1;
    }

    uint32_t armMode = SITHDSS_POPINT16();
    /*uint32_t armMode = *(int16_t*)pCurIn;
    pCurIn += 2;*/

    // Fixed: Reject save/network puppet modes before they reach mode-indexed arrays.
    if ( armMode >= SITH_PUPPET_NUMARMEDMODES )
    {
        SITHLOG_ERROR("DSS::PuppetStatus received invalid armed mode %d.\n", armMode);
        SITHDSS_ENDIN;
        return 0;
    }

    sithPuppet_SetArmedMode(pThing, armMode);

    SithPuppetMoveMode moveMode = SITHDSS_POPUINT16();
    /*SithPuppetMoveMode moveMode = *(uint16_t*)pCurIn;
    pCurIn += 2;*/

    // Fixed: Reject save/network puppet move modes before sithPuppet_SetMoveMode asserts or indexes with them.
    if ( moveMode >= SITH_PUPPET_NUMMOVEMODES )
    {
        SITHLOG_ERROR("DSS::PuppetStatus received invalid move mode %d.\n", moveMode);
        SITHDSS_ENDIN;
        return 0;
    }

    int bControlsDisabled = false;
    if ( pThing->type == SITH_THING_PLAYER || pThing->type == SITH_THING_ACTOR )
    {
        bControlsDisabled = pThing->thingInfo.actorInfo.bControlsDisabled;
    }

    sithPuppet_SetMoveMode(pThing, moveMode);
    if ( pThing->type == SITH_THING_PLAYER || pThing->type == SITH_THING_ACTOR )
    {
        pThing->thingInfo.actorInfo.bControlsDisabled = bControlsDisabled;
    }

    size_t trackCount = SITHDSS_POPUINT16();
    /*size_t trackCount = *(uint16_t*)pCurIn;
    pCurIn += 2;*/

    // Fixed: The stream only writes one entry per puppet track, so larger counts are corrupt.
    if ( trackCount > STD_ARRAYLEN(pPuppet->aTracks) )
    {
        SITHLOG_ERROR("DSS::PuppetStatus received invalid track count %d.\n", (int)trackCount);
        SITHDSS_ENDIN;
        return 0;
    }

    // Fixed: Prevent restored state from indexing outside the puppet mode table.
    if ( (size_t)pThing->pPuppetState->majorMode >= SITH_PUPPET_MAXMODES )
    {
        SITHLOG_ERROR("DSS::PuppetStatus received invalid major mode %d.\n", pThing->pPuppetState->majorMode);
        SITHDSS_ENDIN;
        return 0;
    }

    while ( trackCount )
    {
        size_t trackNum = SITHDSS_POPINT32();
        SithPuppetSubMode submode = SITHDSS_POPUINT32();

        // Note: trackNum == STD_ARRAYLEN(pPuppet->aTracks) && submode == SITH_PUPPET_NUMSUBMODES is used to mark null track,
        //       so these values are valid but indicate no track rather than a real track index or submode.
        if ( trackNum < STD_ARRAYLEN(pPuppet->aTracks)
            && submode < SITH_PUPPET_NUMSUBMODES // Fixed: Added submode bounds check to prevent indexing outside the mode table
            && sithPuppet_NewTrack(pThing, &pThing->pPuppetClass->aModes[pThing->pPuppetState->majorMode][submode], trackNum, submode) )
        {
            SITHDSS_ENDIN;
            return 0;
        }

        --trackCount;
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSS_SyncTaskEvents(const SithEvent* pEvent, DPID idTo, unsigned int outstream)
{
    INDY_AB_ORIGINAL(sithDSS_SyncTaskEvents, pEvent, idTo, outstream);

    SITHDSS_STARTOUT(SITHDSS_SYNCTASKEVENTS);

    // Event time is stored relative to the current game time
    SITHDSS_PUSHUINT32(pEvent->msecEventTime - sithTime_g_msecGameTime);
    SITHDSS_PUSHINT32(pEvent->params.idx);
    SITHDSS_PUSHINT32(pEvent->params.param1);
    SITHDSS_PUSHINT32(pEvent->params.param2);
    SITHDSS_PUSHINT32(pEvent->params.param3);
    SITHDSS_PUSHINT16(pEvent->taskNum);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, DPSEND_GUARANTEED);
}

int J3DAPI sithDSS_ProcessSyncTaskEvents(const SithMessage* pMsg)
{
    INDY_AB_ORIGINAL(sithDSS_ProcessSyncTaskEvents, pMsg);

    SITH_ASSERTREL(pMsg);
    SITHDSS_STARTIN(pMsg);

    uint32_t msecDelay = SITHDSS_POPUINT32();

    SithEventParams params;
    params.idx    = SITHDSS_POPINT32();
    params.param1 = SITHDSS_POPINT32();
    params.param2 = SITHDSS_POPINT32();
    params.param3 = SITHDSS_POPINT32();

    size_t taskNum = SITHDSS_POPUINT16();
    sithEvent_CreateEvent(taskNum, &params, msecDelay);

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSS_SyncCameras(DPID idTo, unsigned int outstream)
{
    INDY_AB_ORIGINAL(sithDSS_SyncCameras, idTo, outstream);

    SITHDSS_STARTOUT(SITHDSS_SYNCCAMERAS);

    SITHDSS_PUSHINT16(sithDSS_GetArrayIndex(sithCamera_g_pCurCamera, sithCamera_g_aCameras, sizeof(SithCamera)));
    SITHDSS_PUSHINT32(sithCamera_g_bCurCameraSet);
    SITHDSS_PUSHUINT32(sithCamera_g_curCycleCamNum);
    SITHDSS_PUSHVEC3(&sithCamera_g_vecCameraPosOffset);
    SITHDSS_PUSHVEC3(&sithCamera_g_vecCameraAngleOffset);
    SITHDSS_PUSHFLOAT(sithCamera_g_cameraPosDelta);
    SITHDSS_PUSHFLOAT(sithCamera_g_cameraAngleDelta);

    for ( size_t i = 0; i < STD_ARRAYLEN(sithCamera_g_aCameras); ++i )
    {
        const SithCamera* pCamera = &sithCamera_g_aCameras[i];
        SITHDSS_PUSHINT32(sithThing_GetThingIndex(pCamera->pPrimaryFocusThing));
        SITHDSS_PUSHINT32(sithThing_GetThingIndex(pCamera->pSecondaryFocusThing));
        SITHDSS_PUSHFLOAT(pCamera->fov);
    }

    SITH_ASSERTREL(sithPlayer_g_pLocalPlayer);

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, DPSEND_GUARANTEED);
}

int J3DAPI sithDSS_ProcessSyncCameras(const SithMessage* pMsg)
{
    INDY_AB_ORIGINAL(sithDSS_ProcessSyncCameras, pMsg);

    SITH_ASSERTREL(pMsg);
    SITHDSS_STARTIN(pMsg);

    size_t camNum = SITHDSS_POPUINT16();
    if ( camNum > STD_ARRAYLEN(sithCamera_g_aCameras) - 1 )
    {
        camNum = STD_ARRAYLEN(sithCamera_g_aCameras) - 1;
    }

    sithCamera_g_pCurCamera     = &sithCamera_g_aCameras[camNum];
    sithCamera_g_bCurCameraSet  = SITHDSS_POPINT32();
    sithCamera_g_curCycleCamNum = SITHDSS_POPUINT32();
    SITHDSS_POPVEC3(&sithCamera_g_vecCameraPosOffset);
    SITHDSS_POPVEC3(&sithCamera_g_vecCameraAngleOffset);
    sithCamera_g_cameraPosDelta   = SITHDSS_POPFLOAT();
    sithCamera_g_cameraAngleDelta = SITHDSS_POPFLOAT();

    for ( size_t i = 0; i < STD_ARRAYLEN(sithCamera_g_aCameras); ++i )
    {
        SithCamera* pCamera = &sithCamera_g_aCameras[i];
        pCamera->pSector              = NULL;
        pCamera->pPrimaryFocusThing   = sithThing_GetThingByIndex(SITHDSS_POPINT32());
        pCamera->pSecondaryFocusThing = sithThing_GetThingByIndex(SITHDSS_POPINT32());
        pCamera->fov                  = SITHDSS_POPFLOAT();
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSS_SyncGameState(DPID idTo, unsigned int outstream)
{
    INDY_AB_ORIGINAL(sithDSS_SyncGameState, idTo, outstream);

    SITH_ASSERTREL(sithWorld_g_pStaticWorld && sithWorld_g_pCurrentWorld);
    SITHDSS_STARTOUT(SITHDSS_SYNCGAMESTATE);

    SITHDSS_PUSHINT32(sithCog_g_pMasterCog ? sithCog_g_pMasterCog->idx : -1);

    SITH_ASSERTREL(sithPlayer_g_pLocalPlayer);
    SITHDSS_PUSHINT32(sithPlayer_g_pLocalPlayer->curItemID);
    SITHDSS_PUSHINT32(sithPlayer_g_pLocalPlayer->curWeaponID);

    // Inventory state
    for ( size_t i = 0; i < STD_ARRAYLEN(sithInventory_g_aUnknown); ++i )
    {
        SITHDSS_PUSHINT32(sithInventory_g_aUnknown[i].unknown3);
    }

    SITHDSS_PUSHINT32(sithInventory_g_bSendDeactivateMessage);
    SITHDSS_PUSHINT32(sithInventory_g_bInitInventory);
    SITHDSS_PUSHINT32(sithInventory_g_dword_56B750);
    SITHDSS_PUSHINT32(sithInventory_g_dword_56B754);

    for ( size_t weaponId = SITHWEAPON_WHIP; weaponId <= SITHWEAPON_BAZOOKA; ++weaponId )
    {
        const rdModel3* pHolsterModel = sithInventory_g_aTypes[weaponId].pHolsterModel;
        SITHDSS_PUSHINT16(pHolsterModel ? sithModel_GetModelIndex(pHolsterModel) : -1);
    }

    // Player state
    SITHDSS_PUSHINT32(sithRender_GetResetCameraAspect());
    SITHDSS_PUSHINT32(sithThing_GetThingIndex(sithPlayerControls_GetVehicleBoardedThing()));

    SITHDSS_PUSHINT16(sithSurface_GetSurfaceIndex(sithPlayerActions_g_pCurLedgeSurface));
    SITHDSS_PUSHINT16(sithModel_GetModelIndex(sithPlayerActions_g_pCurLedgeThingModel));
    if ( sithPlayerActions_g_pCurLedgeThingModel )
    {
        // Note: the face index is relative to the first mesh even if the ledge face belongs to another mesh
        const rdFace* aFaces = sithPlayerActions_g_pCurLedgeThingModel->aGeos[0].aMeshes->aFaces;
        SITHDSS_PUSHINT16(sithDSS_GetArrayIndex(sithPlayerActions_g_pCurLedgeThingModelFace, aFaces, sizeof(rdFace)));
    }

    SITHDSS_PUSHINT32(sithPlayer_g_bPlayerInPor);
    SITHDSS_PUSHFLOAT(sithPlayer_g_impState);
    SITHDSS_PUSHUINT8(sithPlayer_g_bInAetheriumSector);
    SITHDSS_PUSHUINT8(sithPlayer_g_bGuybrush);
    SITHDSS_PUSHINT16(sithPlayer_g_impFireType);
    SITHDSS_PUSHINT16(sithPlayer_g_curLevelNum);
    SITHDSS_PUSHUINT8(sithPlayer_g_bBonusMapBought);

    SITHDSS_PUSHINT32(sithThing_GetThingIndex(sithPlayerActions_g_pPlasma));
    SITHDSS_PUSHINT16(sithPlayerActions_g_jewelFlyingPuppetTrackNum);
    SITHDSS_PUSHUINT8(sithPlayerActions_g_bJewelFlying);
    SITHDSS_PUSHUINT8(sithPlayerActions_g_bPlayerInvisible);
    SITHDSS_PUSHUINT8(sithPlayerControls_g_bCutsceneMode);
    SITHDSS_PUSHUINT8(sithPuppet_g_bPlayerLeapForward);

    // Chalk marks
    SITHDSS_PUSHINT32(sithFX_g_numChalkMarks);
    SITHDSS_PUSHINT32(sithFX_g_lastChalkMarkNum);
    for ( int i = 0; i < (int)sithFX_g_numChalkMarks; ++i )
    {
        SITHDSS_PUSHINT32(sithThing_GetThingIndex(sithFX_g_aChalkMarks[i]));
    }

    // Current cel of the animated materials of the static and the current world
    const SithWorld* apWorlds[2] = { sithWorld_g_pStaticWorld, sithWorld_g_pCurrentWorld };
    for ( size_t w = 0; w < STD_ARRAYLEN(apWorlds); ++w )
    {
        const SithWorld* pWorld = apWorlds[w];
        for ( int i = 0; i < (int)pWorld->numMaterials; ++i )
        {
            if ( pWorld->aMaterials[i].numCels > 1 )
            {
                SITHDSS_PUSHUINT8(pWorld->aMaterials[i].curCelNum);
            }
        }
    }

    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, DPSEND_GUARANTEED);
}

int J3DAPI sithDSS_ProcessSyncGameState(const SithMessage* pMsg)
{
    INDY_AB_ORIGINAL(sithDSS_ProcessSyncGameState, pMsg);

    SITH_ASSERTREL(pMsg && sithWorld_g_pStaticWorld && sithWorld_g_pCurrentWorld);
    SITHDSS_STARTIN(pMsg);

    sithCog_g_pMasterCog = sithCog_GetCogByIndex(SITHDSS_POPINT32());

    SITH_ASSERTREL(sithPlayer_g_pLocalPlayer);
    sithPlayer_g_pLocalPlayer->curItemID   = SITHDSS_POPINT32();
    sithPlayer_g_pLocalPlayer->curWeaponID = SITHDSS_POPINT32();

    // Inventory state
    for ( size_t i = 0; i < STD_ARRAYLEN(sithInventory_g_aUnknown); ++i )
    {
        sithInventory_g_aUnknown[i].unknown3 = SITHDSS_POPINT32();
    }

    sithInventory_g_bSendDeactivateMessage = SITHDSS_POPINT32();
    sithInventory_g_bInitInventory         = SITHDSS_POPINT32();
    sithInventory_g_dword_56B750           = SITHDSS_POPINT32();
    sithInventory_g_dword_56B754           = SITHDSS_POPINT32();

    for ( size_t weaponId = SITHWEAPON_WHIP; weaponId <= SITHWEAPON_BAZOOKA; ++weaponId )
    {
        uint16_t modelIdx = SITHDSS_POPUINT16();
        sithInventory_g_aTypes[weaponId].pHolsterModel = modelIdx == 0xFFFF ? NULL : sithModel_GetModelByIndex(modelIdx);
    }

    // Player state
    sithRender_SetResetCameraAspect(SITHDSS_POPINT32() != 0);
    sithPlayerControls_SetVehicleBoardedThing(sithThing_GetThingByIndex(SITHDSS_POPINT32()));

    sithPlayerActions_g_pCurLedgeSurface    = sithSurface_GetSurfaceEx(sithWorld_g_pCurrentWorld, SITHDSS_POPINT16());
    sithPlayerActions_g_pCurLedgeThingModel = sithModel_GetModelByIndex(SITHDSS_POPUINT16());
    if ( sithPlayerActions_g_pCurLedgeThingModel )
    {
        int faceIdx = SITHDSS_POPINT16();
        sithPlayerActions_g_pCurLedgeThingModelFace = &sithPlayerActions_g_pCurLedgeThingModel->aGeos[0].aMeshes->aFaces[faceIdx];
    }

    sithPlayer_g_bPlayerInPor       = SITHDSS_POPINT32();
    sithPlayer_g_impState           = SITHDSS_POPFLOAT();
    sithPlayer_g_bInAetheriumSector = SITHDSS_POPUINT8();
    sithPlayer_g_bGuybrush          = SITHDSS_POPUINT8();
    sithPlayer_g_impFireType        = SITHDSS_POPUINT16();
    sithPlayer_g_curLevelNum        = SITHDSS_POPUINT16();
    sithPlayer_g_bBonusMapBought    = SITHDSS_POPUINT8();

    sithPlayerActions_g_pPlasma = sithThing_GetThingByIndex(SITHDSS_POPINT32());
    sithPlayerActions_g_jewelFlyingPuppetTrackNum = SITHDSS_POPUINT16(); // Note: zero-extended, a saved -1 is restored as 65535
    sithPlayerActions_g_bJewelFlying     = SITHDSS_POPUINT8();
    sithPlayerActions_g_bPlayerInvisible = SITHDSS_POPUINT8();
    sithPlayerControls_g_bCutsceneMode   = SITHDSS_POPUINT8();
    sithPuppet_g_bPlayerLeapForward      = SITHDSS_POPUINT8();

    // Chalk marks
    // Note: the saved count isn't checked against the size of sithFX_g_aChalkMarks,
    //       and a mark with an invalid thing index keeps its previous pointer
    int numChalkMarks         = SITHDSS_POPINT32();
    sithFX_g_lastChalkMarkNum = SITHDSS_POPINT32();
    sithFX_g_numChalkMarks    = numChalkMarks;
    for ( int i = 0; i < numChalkMarks; ++i )
    {
        int thingIdx = SITHDSS_POPINT32();
        if ( thingIdx >= 0 && thingIdx < (int)sithWorld_g_pCurrentWorld->numThings )
        {
            sithFX_g_aChalkMarks[i] = &sithWorld_g_pCurrentWorld->aThings[thingIdx];
        }
    }

    // Current cel of the animated materials of the static and the current world
    SithWorld* apWorlds[2] = { sithWorld_g_pStaticWorld, sithWorld_g_pCurrentWorld };
    for ( size_t w = 0; w < STD_ARRAYLEN(apWorlds); ++w )
    {
        SithWorld* pWorld = apWorlds[w];
        for ( int i = 0; i < (int)pWorld->numMaterials; ++i )
        {
            if ( pWorld->aMaterials[i].numCels > 1 )
            {
                pWorld->aMaterials[i].curCelNum = SITHDSS_POPUINT8();
            }
        }
    }

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSS_SyncVehicleControlState(DPID idTo, unsigned int outstream)
{
    INDY_AB_ORIGINAL(sithDSS_SyncVehicleControlState, idTo, outstream);

    const SithMineCarControlsState* pMineCar = sithVehicleControls_GetMineCarState();
    const SithRaftControlsState* pRaft       = sithVehicleControls_GetRaftState();

    SITHDSS_STARTOUT(SITHDSS_VEHICLECONTROLSTATE);

    SITHDSS_PUSHFLOAT(pMineCar->secElapsedStoppingTime);
    SITHDSS_PUSHUINT32(pMineCar->moveState);
    SITHDSS_PUSHFLOAT(pMineCar->secDuckTime);
    SITHDSS_PUSHINT32(pMineCar->curDuckPuppetTrackNum);
    SITHDSS_PUSHINT32(pMineCar->bCanDuck);
    SITHDSS_PUSHUINT32(pMineCar->leanState);
    SITHDSS_PUSHINT32(pMineCar->curPuppetTrack);
    SITHDSS_PUSHINT32(pMineCar->hBrakeSnd);
    SITHDSS_PUSHFLOAT(pMineCar->secElapsedBrakingTime);
    SITHDSS_PUSHFLOAT(pMineCar->secUnboardingElapsedTime);
    SITHDSS_PUSHUINT32(pMineCar->unboardState);

    SITHDSS_PUSHUINT32(pRaft->nextMoveStatus);
    SITHDSS_PUSHUINT32(pRaft->nextPuppetMode);
    SITHDSS_PUSHVEC3(&pRaft->unboardPos);
    SITHDSS_PUSHVEC3(&pRaft->unboardNorm);
    SITHDSS_PUSHFLOAT(pRaft->moveSize);
    SITHDSS_PUSHFLOAT(pRaft->wakeTimer);
    SITHDSS_PUSHINT32(pRaft->bRowing);
    SITHDSS_PUSHFLOAT(pRaft->secRowStartTime);
    SITHDSS_PUSHFLOAT(pRaft->secUnboardTime);

    SITHDSS_ENDOUT;
    SITH_ASSERT(sithMulti_g_message.length == sizeof(SithMineCarControlsState) + sizeof(SithRaftControlsState)); // Added: Sanity check
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, DPSEND_GUARANTEED);
}

int J3DAPI sithDSS_ProcessVehicleControlsState(const SithMessage* pMsg)
{
    INDY_AB_ORIGINAL(sithDSS_ProcessVehicleControlsState, pMsg);

    SITH_ASSERTREL(pMsg);
    SITHDSS_STARTIN(pMsg);

    SithMineCarControlsState mineCar;
    mineCar.secElapsedStoppingTime   = SITHDSS_POPFLOAT();
    mineCar.moveState                = SITHDSS_POPUINT32();
    mineCar.secDuckTime              = SITHDSS_POPFLOAT();
    mineCar.curDuckPuppetTrackNum    = SITHDSS_POPINT32();
    mineCar.bCanDuck                 = SITHDSS_POPINT32();
    mineCar.leanState                = SITHDSS_POPUINT32();
    mineCar.curPuppetTrack           = SITHDSS_POPINT32();
    mineCar.hBrakeSnd                = SITHDSS_POPINT32();
    mineCar.secElapsedBrakingTime    = SITHDSS_POPFLOAT();
    mineCar.secUnboardingElapsedTime = SITHDSS_POPFLOAT();
    mineCar.unboardState             = SITHDSS_POPUINT32();

    SithRaftControlsState raft;
    raft.nextMoveStatus = SITHDSS_POPUINT32();
    raft.nextPuppetMode = SITHDSS_POPUINT32();
    SITHDSS_POPVEC3(&raft.unboardPos);
    SITHDSS_POPVEC3(&raft.unboardNorm);
    raft.moveSize        = SITHDSS_POPFLOAT();
    raft.wakeTimer       = SITHDSS_POPFLOAT();
    raft.bRowing         = SITHDSS_POPINT32();
    raft.secRowStartTime = SITHDSS_POPFLOAT();
    raft.secUnboardTime  = SITHDSS_POPFLOAT();

    sithVehicleControls_SetMineCarState(&mineCar);
    sithVehicleControls_SetRaftState(&raft);

    SITHDSS_ENDIN;
    return 1;
}

int J3DAPI sithDSS_sub_4B3760(DPID idTo, unsigned int outstream)
{
    INDY_AB_ORIGINAL(sithDSS_sub_4B3760, idTo, outstream);

    // Empty message, written as the last section of a savegame
    SITHDSS_STARTOUT(SITHDSS_UNKNOWN_44);
    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, outstream, DPSEND_GUARANTEED);
}

int J3DAPI sithDSS_sub_4B3790(const SithMessage* pMsg)
{
    INDY_AB_ORIGINAL(sithDSS_sub_4B3790, pMsg);

    SITH_ASSERTREL(pMsg);
    return 1;
}