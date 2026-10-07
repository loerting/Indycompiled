// Native builds: stand-ins for the Win32 dialogs of jonesConfig.c until the engine draws its own. Each one answers
// the way the most common button would: exit leaves, game over restarts from the level's autosave, level end
// continues (no store purchases), settings dialogs cancel. Messages go to the log.
#include "jonesConfig.h"
#include <Jones3D/Main/jonesString.h>
#include <sith/Dss/sithGamesave.h>
#include <sith/Main/sithMain.h>
#include <sith/World/sithWorld.h>
#include <std/General/std.h>
#include <std/General/stdConfig.h>
#include <std/General/stdFnames.h>
#include <std/General/stdUtil.h>

#define JONESCONFIGSDL_OK     1
#define JONESCONFIGSDL_CANCEL 2

int J3DAPI jonesConfig_GetLoadGameFilePath(HWND hWnd, char* pDestNdsPath)
{
    J3D_UNUSED(hWnd);

    // The last saved game, if it still exists
    char aPath[JONESCONFIG_GAMESAVE_FILEPATHSIZE] = { 0 };
    stdConfig_GetString(SITHSAVEGAME_CFG_GAMEPLAY_LASTSAVEGAME, aPath, sizeof(aPath), "");
    if ( strlen(aPath) == 0 || !stdUtil_FileExists(aPath) )
    {
        return JONESCONFIGSDL_CANCEL;
    }

    stdUtil_StringCopy(pDestNdsPath, JONESCONFIG_GAMESAVE_FILEPATHSIZE, aPath);
    return JONESCONFIGSDL_OK;
}

int J3DAPI jonesConfig_GetSaveGameFilePath(HWND hWnd, char* pOutFilePath)
{
    J3D_UNUSED(hWnd);

    // One slot per level: <save games dir>\<level save name>.nds
    char aFilename[64] = { 0 };
    stdUtil_StringCopy(aFilename, sizeof(aFilename), sithWorld_g_pCurrentWorld ? sithGetCurrentWorldSaveName() : "save");
    stdFnames_ChangeExtEx(aFilename, STD_ARRAYLEN(aFilename), "nds");
    stdFnames_MakePath(pOutFilePath, JONESCONFIG_GAMESAVE_FILEPATHSIZE, sithGetSaveGamesDir(), aFilename);
    return JONESCONFIGSDL_OK;
}

int J3DAPI jonesConfig_ShowControlOptions(HWND hWnd)
{
    J3D_UNUSED(hWnd);
    return JONESCONFIGSDL_CANCEL;
}

int J3DAPI jonesConfig_ShowDialogInsertCD(HWND hWnd, LPARAM dwInitParam)
{
    J3D_UNUSED(hWnd);
    J3D_UNUSED(dwInitParam);
    STDLOG_ERROR("Game data not found (insert CD %d).\n", (int)dwInitParam);
    return JONESCONFIGSDL_CANCEL;
}

int J3DAPI jonesConfig_ShowDisplaySettingsDialog(HWND hWnd, StdDisplayEnvironment* pDisplayEnv, JonesDisplaySettings* pDSettings)
{
    J3D_UNUSED(hWnd);
    J3D_UNUSED(pDisplayEnv);
    J3D_UNUSED(pDSettings);
    return JONESCONFIGSDL_CANCEL;
}

int J3DAPI jonesConfig_ShowExitGameDialog(HWND hWnd, char* pSaveGameFilePath)
{
    J3D_UNUSED(hWnd);
    J3D_UNUSED(pSaveGameFilePath);
    return JONESCONFIGSDL_OK; // exit without saving
}

int J3DAPI jonesConfig_ShowGameOverDialog(HWND hWnd, char* pRestoreFilename, tSoundHandle hSndGameOVerMus, tSoundChannelHandle* pSndChnlMus)
{
    J3D_UNUSED(hWnd);
    J3D_UNUSED(hSndGameOVerMus);
    if ( pSndChnlMus )
    {
        *pSndChnlMus = 0;
    }

    // Restart from the level's autosave, as the dialog's Restart button
    pRestoreFilename[0] = '\0';
    if ( sithWorld_g_pCurrentWorld )
    {
        char aFilename[128] = { 0 };
        STD_FORMAT(aFilename, "%s%s", sithGetAutoSaveFilePrefix(), sithGetCurrentWorldSaveName());
        stdFnames_ChangeExtEx(aFilename, STD_ARRAYLEN(aFilename), "nds");

        LPSTR pFilePart;
        SearchPath(sithGetSaveGamesDir(), aFilename, NULL, JONESCONFIG_GAMESAVE_FILEPATHSIZE, pRestoreFilename, &pFilePart);
    }

    return strlen(pRestoreFilename) ? 1177 : 1179; // restart, else quit
}

int J3DAPI jonesConfig_ShowGamePlayOptions(HWND hWnd)
{
    J3D_UNUSED(hWnd);
    return JONESCONFIGSDL_CANCEL;
}

int J3DAPI jonesConfig_ShowLevelCompletedDialog(HWND hWnd, int* pBalance, int* apItemsState, int a4, int elapsedTime, int qiPoints, int numFoundTrasures, int foundTrasureValue, int totalTreasureValue)
{
    J3D_UNUSED(hWnd);
    J3D_UNUSED(pBalance);
    J3D_UNUSED(apItemsState);
    J3D_UNUSED(a4);
    STDLOG_STATUS("Level completed: time %d, IQ points %d, treasures %d (value %d of %d).\n",
        elapsedTime, qiPoints, numFoundTrasures, foundTrasureValue, totalTreasureValue);
    return JONESCONFIGSDL_OK; // continue, nothing bought
}

int J3DAPI jonesConfig_ShowMessageDialog(HWND hWnd, const char* pTitle, const char* pText, int iconID)
{
    J3D_UNUSED(hWnd);
    J3D_UNUSED(iconID);
    const char* pTitleText = pTitle ? jonesString_GetString(pTitle) : NULL;
    STDLOG_STATUS("%s: %s\n", pTitleText ? pTitleText : (pTitle ? pTitle : ""), pText ? pText : "");
    return JONESCONFIGSDL_OK;
}

int J3DAPI jonesConfig_ShowSoundSettingsDialog(HWND hWnd, JonesSoundSettings* pData)
{
    J3D_UNUSED(hWnd);
    J3D_UNUSED(pData);
    return JONESCONFIGSDL_CANCEL;
}

int J3DAPI jonesConfig_ShowStatisticsDialog(HWND hWnd, SithGameStatistics* pStatistics)
{
    J3D_UNUSED(hWnd);
    J3D_UNUSED(pStatistics);
    return JONESCONFIGSDL_OK;
}
