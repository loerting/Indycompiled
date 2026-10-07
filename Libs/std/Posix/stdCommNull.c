// Native builds: no multiplayer (DirectPlay exists only on Windows). No session is ever active; every call fails.
#include <std/Win95/stdComm.h>

void stdComm_CloseGame(void) {}
HRESULT J3DAPI stdComm_CreateGame(const StdCommGame* pSettings) { J3D_UNUSED(pSettings); return E_FAIL; }
DPID J3DAPI stdComm_CreatePlayer(const wchar_t* pPlayerName) { J3D_UNUSED(pPlayerName); return 0; }
void J3DAPI stdComm_DestroyPlayer(DPID playerId) { J3D_UNUSED(playerId); }
size_t stdComm_GetNumPlayers(void) { return 0; }
DPID J3DAPI stdComm_GetPlayerID(size_t playerNum) { J3D_UNUSED(playerNum); return 0; }
int J3DAPI stdComm_IsGameActive(void) { return 0; }
int J3DAPI stdComm_IsGameHost(void) { return 0; }
int J3DAPI stdComm_JoinGame(size_t gameNum, const wchar_t* pPassword) { J3D_UNUSED(gameNum); J3D_UNUSED(pPassword); return 1; }
int32_t J3DAPI stdComm_Receive(DPID* pSender, void* pData, size_t* pLength) { J3D_UNUSED(pSender); J3D_UNUSED(pData); J3D_UNUSED(pLength); return -1; }
int J3DAPI stdComm_RejoinSession(DPID* pPlayerId, const wchar_t* pPlayerName) { J3D_UNUSED(pPlayerId); J3D_UNUSED(pPlayerName); return 1; }
int32_t J3DAPI stdComm_Send(DPID idFrom, DPID idTo, const void* pData, uint32_t size, uint32_t flags)
{
    J3D_UNUSED(idFrom); J3D_UNUSED(idTo); J3D_UNUSED(pData); J3D_UNUSED(size); J3D_UNUSED(flags);
    return -1;
}
HRESULT J3DAPI stdComm_SetGameParams(StdCommGame* pSettings) { J3D_UNUSED(pSettings); return E_FAIL; }
int J3DAPI stdComm_UpdatePlayers(size_t gameNum) { J3D_UNUSED(gameNum); return 1; }
int J3DAPI stdComm_VerifyPlayer(DPID id) { J3D_UNUSED(id); return 0; }
