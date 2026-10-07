#ifndef SITH_SITHAIINSTINCT_H
#define SITH_SITHAIINSTINCT_H
#include <j3dcore/j3d.h>
#include <rdroid/types.h>
#include <sith/types.h>
#include <sith/Main/sithMain.h>
#include <sith/RTI/addresses.h>
#include <std/types.h>

J3D_EXTERN_C_START

void J3DAPI sithAIInstinct_InitInstincts(void);
int J3DAPI sithAIInstinct_Listen(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing);
int J3DAPI sithAIInstinct_LookForTarget(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithAIMode prevMode);
int J3DAPI sithAIInstinct_SenseDanger(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing);
signed int J3DAPI sithAIInstinct_FearGunshot(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_Hop(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_HoverDrift(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_Talk(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_RandomTurn(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_OpenDoors(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_Jump(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_LobFire(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_PrimaryFire(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_AlternateFire(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_CircleStrafe(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing);
int J3DAPI sithAIInstinct_Dodge(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing);
int J3DAPI sithAIInstinct_HitAndRun(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_Retreat(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_RandomMove(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_HumanCombatMove(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_Roam(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_ReturnHome(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_Flee(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_BasicFallow(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, void* pObject);
int J3DAPI sithAIInstinct_WallCrawl(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithSurface* pSurface);
void J3DAPI sithAIInstinct_sub_494360(SithAIControlBlock* pLocal, rdVector3* pMovePos, rdVector3* pAltMovePos);
int J3DAPI sithAIInstinct_SnakeMungeTestCheck(SithAIControlBlock* pLocal, rdVector3* pDirection, float distance, rdVector3* pOutPoint);
signed int J3DAPI sithAIInstinct_SnakeFollow(SithAIControlBlock* pLocal, SithAIInstinct* pInstinct, SithAIInstinctState* pState, SithAIEventType event, SithThing* pThing);
void J3DAPI sithAIInstinct_sub_494FF0(const SithThing* pThing, float sideMove);

// Helper hooking functions
void sithAIInstinct_InstallHooks(void);
void sithAIInstinct_ResetGlobals(void);

#ifdef J3D_STANDALONE // INDY: Stage 4, the exe's globals are ours (defined in sithAIInstinct.c)
// used only by sithAIInstinct.c
extern float sithAIInstinct_flt_539AB8;
#endif

J3D_EXTERN_C_END
#endif // SITH_SITHAIINSTINCT_H
