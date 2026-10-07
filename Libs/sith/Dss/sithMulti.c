#include "sithMulti.h"
#include <j3dcore/j3dhook.h>
#include <sith/RTI/symbols.h>

#include <sith/Devices/sithComm.h>
#include <sith/Dss/sithDSS.h>
#include <sith/Gameplay/sithPlayer.h>
#include <sith/Gameplay/sithEvent.h>
#include <sith/Gameplay/sithTime.h>
#include <sith/World/sithSector.h>
#include <sith/World/sithSurface.h>
#include <sith/World/sithThing.h>

#include <std/Win95/stdComm.h>

#define sithMulti_tickRate J3D_DECL_FAR_VAR(sithMulti_tickRate, size_t)
#define sithMulti_numUpdatedSurfaces J3D_DECL_FAR_VAR(sithMulti_numUpdatedSurfaces, int)
#define sithMulti_lastUpdateIdx J3D_DECL_FAR_VAR(sithMulti_lastUpdateIdx, int)
#define sithMulti_playerWelcomeState J3D_DECL_FAR_VAR(sithMulti_playerWelcomeState, int)
#define sithMulti_numUpdatedSectors J3D_DECL_FAR_VAR(sithMulti_numUpdatedSectors, int)
#define sithMulti_aRemovedStaticThings J3D_DECL_FAR_ARRAYVAR(sithMulti_aRemovedStaticThings, int(*)[256])
#define sithMulti_curWelcomePlayerNum J3D_DECL_FAR_VAR(sithMulti_curWelcomePlayerNum, unsigned int)
#define sithMulti_numUpdatedThings J3D_DECL_FAR_VAR(sithMulti_numUpdatedThings, int)
#define sithMulti_quitGameState J3D_DECL_FAR_VAR(sithMulti_quitGameState, int)
#define sithMulti_checksum J3D_DECL_FAR_VAR(sithMulti_checksum, int)
#define sithMulti_bWelcomingPlayer J3D_DECL_FAR_VAR(sithMulti_bWelcomingPlayer, int)
#define sithMulti_newPlayerId J3D_DECL_FAR_VAR(sithMulti_newPlayerId, DPID)
#define sithMulti_bSyncScores J3D_DECL_FAR_VAR(sithMulti_bSyncScores, int)
#define sithMulti_pfNewPlayerJoinedCallback J3D_DECL_FAR_VAR(sithMulti_pfNewPlayerJoinedCallback, SithMultiNewPlayerJoinedCallback)
#define sithMulti_msecPingStartTime J3D_DECL_FAR_VAR(sithMulti_msecPingStartTime, unsigned int)
#define sithMulti_msecQuitGameTime J3D_DECL_FAR_VAR(sithMulti_msecQuitGameTime, unsigned int)
#define sithMulti_numRemovedStaticThings J3D_DECL_FAR_VAR(sithMulti_numRemovedStaticThings, int)
#define sithMulti_msecLastSyncScoreTime J3D_DECL_FAR_VAR(sithMulti_msecLastSyncScoreTime, unsigned int)
#define sithMulti_msecWelcomeUpdateInterval J3D_DECL_FAR_VAR(sithMulti_msecWelcomeUpdateInterval, unsigned int)
#define sithMulti_dword_17F10EC J3D_DECL_FAR_VAR(sithMulti_dword_17F10EC, int)

// Player welcome (join sync) phases, see sithMulti_Update
#define SITHMULTI_WELCOME_SECTORS  1
#define SITHMULTI_WELCOME_SURFACES 2
#define SITHMULTI_WELCOME_THINGS   3
#define SITHMULTI_WELCOME_REMOVALS 4

#define SITHMULTI_WELCOMESTEP_MSEC 60u

// sithMulti_quitGameState values
#define SITHMULTI_QUIT_STATE1  1
#define SITHMULTI_QUIT_EJECTED 2

void sithMulti_InstallHooks(void)
{
    // Uncomment only lines for functions that have full definition and doesn't call original function (non-thunk functions)

    J3D_HOOKFUNC(sithMulti_CloseGame);
    J3D_HOOKFUNC(sithMulti_CheckPlayers);
    J3D_HOOKFUNC(sithMulti_ProcessPlayerLost);
    J3D_HOOKFUNC(sithMulti_RemovePlayer);
    J3D_HOOKFUNC(sithMulti_SendWelcome);
    J3D_HOOKFUNC(sithMulti_ProcessWelcome);
    J3D_HOOKFUNC(sithMulti_ProcessPlayerJoin);
    J3D_HOOKFUNC(sithMulti_SyncPlayers);
    J3D_HOOKFUNC(sithMulti_ProcessSyncPlayers);
    J3D_HOOKFUNC(sithMulti_ProcessJoinRequest);
    J3D_HOOKFUNC(sithMulti_FinishJoining);
    J3D_HOOKFUNC(sithMulti_ProcessChat);
    J3D_HOOKFUNC(sithMulti_ProcessPing);
    J3D_HOOKFUNC(sithMulti_ProcessPong);
    J3D_HOOKFUNC(sithMulti_QuitPlayer);
    J3D_HOOKFUNC(sithMulti_ProcessQuit);
    J3D_HOOKFUNC(sithMulti_Update);
    J3D_HOOKFUNC(sithMulti_SyncScores);
    J3D_HOOKFUNC(sithMulti_GetPlayerIndexByID);
    J3D_HOOKFUNC(sithMulti_ProcessKilledPlayer);
    J3D_HOOKFUNC(sithMulti_QuitGame);
    J3D_HOOKFUNC(sithMulti_Respawn);
    J3D_HOOKFUNC(sithMulti_RemoveStaticThing);
    J3D_HOOKFUNC(sithMulti_UpdateSurfaces);
    J3D_HOOKFUNC(sithMulti_UpdateSectors);
    J3D_HOOKFUNC(sithMulti_UpdateThings);
    J3D_HOOKFUNC(sithMulti_UpdateRemovals);
    J3D_HOOKFUNC(sithMulti_StartWelcomingPlayer);
    J3D_HOOKFUNC(sithMulti_StopWelcomingPlayer);
}

void sithMulti_ResetGlobals(void)
{
    int sithMulti_tickRate_tmp = 70;
    memcpy(&sithMulti_tickRate, &sithMulti_tickRate_tmp, sizeof(sithMulti_tickRate));

    memset(&sithMulti_numUpdatedSurfaces, 0, sizeof(sithMulti_numUpdatedSurfaces));
    memset(&sithMulti_lastUpdateIdx, 0, sizeof(sithMulti_lastUpdateIdx));
    memset(&sithMulti_playerWelcomeState, 0, sizeof(sithMulti_playerWelcomeState));
    memset(&sithMulti_numUpdatedSectors, 0, sizeof(sithMulti_numUpdatedSectors));
    memset(&sithMulti_aRemovedStaticThings, 0, sizeof(sithMulti_aRemovedStaticThings));
    memset(&sithMulti_curWelcomePlayerNum, 0, sizeof(sithMulti_curWelcomePlayerNum));
    memset(&sithMulti_numUpdatedThings, 0, sizeof(sithMulti_numUpdatedThings));
    memset(&sithMulti_quitGameState, 0, sizeof(sithMulti_quitGameState));
    memset(&sithMulti_checksum, 0, sizeof(sithMulti_checksum));
    memset(&sithMulti_bWelcomingPlayer, 0, sizeof(sithMulti_bWelcomingPlayer));
    memset(&sithMulti_newPlayerId, 0, sizeof(sithMulti_newPlayerId));
    memset(&sithMulti_bSyncScores, 0, sizeof(sithMulti_bSyncScores));
    memset(&sithMulti_pfNewPlayerJoinedCallback, 0, sizeof(sithMulti_pfNewPlayerJoinedCallback));
    memset(&sithMulti_msecPingStartTime, 0, sizeof(sithMulti_msecPingStartTime));
    memset(&sithMulti_msecQuitGameTime, 0, sizeof(sithMulti_msecQuitGameTime));
    memset(&sithMulti_numRemovedStaticThings, 0, sizeof(sithMulti_numRemovedStaticThings));
    memset(&sithMulti_msecLastSyncScoreTime, 0, sizeof(sithMulti_msecLastSyncScoreTime));
    memset(&sithMulti_msecWelcomeUpdateInterval, 0, sizeof(sithMulti_msecWelcomeUpdateInterval));
    memset(&sithMulti_g_serverId, 0, sizeof(sithMulti_g_serverId));
    memset(&sithMulti_dword_17F10EC, 0, sizeof(sithMulti_dword_17F10EC));
    memset(&sithMulti_g_message, 0, sizeof(sithMulti_g_message));
}

void sithMulti_CloseGame()
{
    INDY_AB_ORIGINAL_VOID(sithMulti_CloseGame);

    if ( stdComm_IsGameHost() && sithMulti_bWelcomingPlayer )
    {
        sithMulti_StopWelcomingPlayer(1);
    }

    sithMessage_g_outputstream &= ~SITHMESSAGE_STREAM_NET;
    sithMessage_g_inputstream &= ~SITHMESSAGE_STREAM_NET;
    sithEvent_RegisterTask(SITHMULTI_CHECKPLAYER_TASKID, NULL, 0, SITHEVENT_TASKDISABLED);

    sithMessage_CloseGame();
    sithMulti_g_serverId    = 0;
    sithMulti_dword_17F10EC = 0;
}

int J3DAPI sithMulti_CheckPlayers(int msecTime, SithEventParams* pParam)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)msecTime;
    (void)pParam;
    return 1;
}

void J3DAPI sithMulti_ProcessPlayerLost(DPID idPlayer)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)idPlayer;
}

void J3DAPI sithMulti_RemovePlayer(unsigned int playerNum)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)playerNum;
}

signed int J3DAPI sithMulti_SendWelcome(DPID idPlayer, int playerNum, DPID idTo)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)idPlayer;
    (void)playerNum;
    (void)idTo;
    return 0;
}

int J3DAPI sithMulti_ProcessWelcome(const SithMessage* pMsg)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)pMsg;
    return 1;
}

int J3DAPI sithMulti_ProcessPlayerJoin(int playerNum)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)playerNum;
    return 0;
}

void J3DAPI sithMulti_SyncPlayers(DPID idTo, uint32_t dpFlags)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)idTo;
    (void)dpFlags;
}

signed int J3DAPI sithMulti_ProcessSyncPlayers(const SithMessage* pMsg)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)pMsg;
    return 1;
}

int J3DAPI sithMulti_ProcessJoinRequest(const SithMessage* pMsg)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)pMsg;
    return 1;
}

signed int J3DAPI sithMulti_FinishJoining(SithMultiJoinStatus code, float arg4, int playerId)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)code;
    (void)arg4;
    (void)playerId;
    return 0;
}

int J3DAPI sithMulti_ProcessChat(const SithMessage* pMsg)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)pMsg;
    return 1;
}

int J3DAPI sithMulti_ProcessPing(const SithMessage* pMsg)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)pMsg;
    return 1;
}

int J3DAPI sithMulti_ProcessPong(const SithMessage* pMsg)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)pMsg;
    return 1;
}

int J3DAPI sithMulti_QuitPlayer(DPID id)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)id;
    return 0;
}

int J3DAPI sithMulti_ProcessQuit(const SithMessage* pMsg)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)pMsg;
    return 1;
}

void J3DAPI sithMulti_Update(int msecDeltaTime)
{
    INDY_AB_ORIGINAL_VOID(sithMulti_Update, msecDeltaTime);

    if ( !stdComm_IsGameActive() )
    {
        return;
    }

    sithThing_SyncThings();
    sithSurface_SyncSurfaces();
    sithSector_SyncSectors();

    if ( sithMulti_quitGameState && sithTime_g_msecGameTime > sithMulti_msecQuitGameTime )
    {
        // Note: Both quit states only reset the state when the quit time is reached; nothing else is done here
        if ( sithMulti_quitGameState == SITHMULTI_QUIT_STATE1 || sithMulti_quitGameState == SITHMULTI_QUIT_EJECTED )
        {
            sithMulti_quitGameState = 0;
        }

        return;
    }

    if ( !stdComm_IsGameHost() )
    {
        return;
    }

    if ( sithMulti_bSyncScores )
    {
        sithMulti_bSyncScores = 0;
        sithMulti_SyncPlayers(SITHMESSAGE_SENDTOJOINEDPLAYERS, 0);
    }

    if ( !sithMulti_bWelcomingPlayer )
    {
        return;
    }

    if ( sithMulti_quitGameState )
    {
        sithMulti_StopWelcomingPlayer(1);
        return;
    }

    // The new player is synced one update step per SITHMULTI_WELCOMESTEP_MSEC of game time
    unsigned int msecElapsed = (unsigned int)msecDeltaTime + sithMulti_msecWelcomeUpdateInterval;
    unsigned int numSteps    = msecElapsed / SITHMULTI_WELCOMESTEP_MSEC;
    sithMulti_msecWelcomeUpdateInterval = msecElapsed % SITHMULTI_WELCOMESTEP_MSEC;

    for ( unsigned int i = 0; i < numSteps; ++i )
    {
        switch ( sithMulti_playerWelcomeState )
        {
            case SITHMULTI_WELCOME_SECTORS:
                sithMulti_UpdateSectors();
                ++sithMulti_numUpdatedSectors;
                break;

            case SITHMULTI_WELCOME_SURFACES:
                sithMulti_UpdateSurfaces();
                ++sithMulti_numUpdatedSurfaces;
                break;

            case SITHMULTI_WELCOME_THINGS:
                sithMulti_UpdateThings();
                ++sithMulti_numUpdatedThings;
                break;

            case SITHMULTI_WELCOME_REMOVALS:
                sithMulti_UpdateRemovals();
                break;

            default:
                return;
        }
    }
}

void J3DAPI sithMulti_SyncScores()
{
    INDY_AB_ORIGINAL_VOID(sithMulti_SyncScores);
    sithMulti_bSyncScores = 1;
}

//int J3DAPI sithMulti_GetPlayerIndexByID(DPID idPlayer)
//{
//    return J3D_TRAMPOLINE_CALL(sithMulti_GetPlayerIndexByID, idPlayer);
//}

void J3DAPI sithMulti_ProcessKilledPlayer(const SithPlayer* pPlayer, const SithThing* pPlayerThing, const SithThing* pKiller)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)pPlayer;
    (void)pPlayerThing;
    (void)pKiller;
}

int J3DAPI sithMulti_QuitGame(unsigned int msecTime, int state)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)msecTime;
    (void)state;
    return 0;
}

size_t J3DAPI sithMulti_Respawn(SithThing* pPlayer)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)pPlayer;
    return 0;
}

void J3DAPI sithMulti_RemoveStaticThing(int guid)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)guid;
}

void J3DAPI sithMulti_UpdateSurfaces()
{
    // INDY: multiplayer stub (single-player never gets here)
}

void J3DAPI sithMulti_UpdateSectors()
{
    // INDY: multiplayer stub (single-player never gets here)
}

void J3DAPI sithMulti_UpdateThings()
{
    // INDY: multiplayer stub (single-player never gets here)
}

void J3DAPI sithMulti_UpdateRemovals()
{
    // INDY: multiplayer stub (single-player never gets here)
}

void J3DAPI sithMulti_StartWelcomingPlayer(DPID playerId)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)playerId;
}

void J3DAPI sithMulti_StopWelcomingPlayer(int bError)
{
    // INDY: multiplayer stub (single-player never gets here)
    (void)bError;
}

void sithMulti_OpenGame(void)
{
    // TODO: Add implementation
    SITH_ASSERT(0);
}

size_t J3DAPI sithMulti_GetTickRate(void)
{
    return sithMulti_tickRate;
}

void J3DAPI sithMulti_SetTickRate(size_t tickRate)
{
    sithMulti_tickRate = tickRate;
}

int J3DAPI sithMulti_Ping(DPID idTo)
{
    sithMulti_msecPingStartTime = sithTime_g_msecGameTime;

    SITHDSS_STARTOUT(SITHDSS_PING);
    SITHDSS_PUSHUINT32(sithTime_g_msecGameTime);
    SITHDSS_ENDOUT;
    return sithMessage_SendMessage(&sithMulti_g_message, idTo, 1u, 0);
}

int J3DAPI sithMulti_GetPlayerIndexByID(DPID playerID)
{
    for ( size_t i = 0; i < sithPlayer_g_numPlayers; ++i )
    {
        if ( playerID == sithPlayer_g_aPlayers[i].playerNetId )
        {
            return i;
        }
    }

    return -1;
}

#ifdef J3D_STANDALONE // INDY: Stage 4, the exe's globals of this module, with the exe's initial values
size_t sithMulti_tickRate = 70;
int sithMulti_numUpdatedSurfaces;
int sithMulti_lastUpdateIdx;
int sithMulti_playerWelcomeState;
int sithMulti_numUpdatedSectors;
int sithMulti_aRemovedStaticThings[256];
unsigned int sithMulti_curWelcomePlayerNum;
int sithMulti_numUpdatedThings;
int sithMulti_quitGameState;
int sithMulti_checksum;
int sithMulti_bWelcomingPlayer;
DPID sithMulti_newPlayerId;
int sithMulti_bSyncScores;
SithMultiNewPlayerJoinedCallback sithMulti_pfNewPlayerJoinedCallback;
unsigned int sithMulti_msecPingStartTime;
unsigned int sithMulti_msecQuitGameTime;
int sithMulti_numRemovedStaticThings;
unsigned int sithMulti_msecLastSyncScoreTime;
unsigned int sithMulti_msecWelcomeUpdateInterval;
int sithMulti_dword_17F10EC;
DPID sithMulti_g_serverId;
SithMessage sithMulti_g_message;
#endif
