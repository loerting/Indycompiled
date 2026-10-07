// Native builds: the dialogs of jonesConfig.c. Loading, saving, game over, exit and messages are engine-drawn menus
// (JonesMenuSDL.c); the others still answer the way the most common button would until they get one too: level end
// continues (no store purchases), settings dialogs cancel. Without a game frame (e.g. loading at startup) the menus fall
// back to the same: the last save, one save per level, restart from the autosave.
#include "jonesConfig.h"
#include "JonesMenuSDL.h"
#include <Jones3D/Main/jonesString.h>
#include <rdroid/Primitives/rdFont.h>
#include <sith/Devices/sithSoundMixer.h>
#include <sith/Dss/sithGamesave.h>
#include <sith/Main/sithMain.h>
#include <sith/World/sithWorld.h>
#include <sith/Gameplay/sithTime.h>
#include <Jones3D/Display/JonesHud.h>
#include <Jones3D/Display/JonesHudConstants.h>
#include <Jones3D/Main/JonesLevel.h>
#include <Jones3D/Play/jonesCog.h>
#include <sith/Engine/sithCamera.h>
#include <std/General/std.h>
#include <std/General/stdConfig.h>
#include <std/General/stdFileUtil.h>
#include <std/General/stdFnames.h>
#include <std/General/stdUtil.h>

#include <ctype.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>

#define JONESCONFIGSDL_OK     1
#define JONESCONFIGSDL_CANCEL 2

#define JONESCONFIGSDL_MAXSAVES 128
#define JONESCONFIGSDL_BACK     -2 // a menu was left with Back
#define JONESCONFIGSDL_TOPITEM  -1 // the list's extra first entry (e.g. "New save")

typedef struct sJonesConfigSaveEntry
{
    char aPath[JONESCONFIG_GAMESAVE_FILEPATHSIZE];
    char aTitle[64];
    char aInfo[96];
    uint32_t msecTime; // file time (seconds), for sorting
    bool bAuto;        // the autosave made when a level starts
} JonesConfigSaveEntry;

static JonesConfigSaveEntry jonesConfigSDL_aSaves[JONESCONFIGSDL_MAXSAVES];

// A localized string from the game's text, else the English one. jonesString_GetString returns a shared buffer: the
// text is copied into one of a few buffers of its own, so a menu can hold several at once.
static const char* jonesConfigSDL_Text(const char* pKey, const char* pDefault)
{
    static char aaBuffers[16][128];
    static size_t nextBuffer;

    const char* pText = pKey ? jonesString_GetString(pKey) : NULL;
    if ( !pText || !*pText )
    {
        return pDefault;
    }
    char* pCopy = aaBuffers[nextBuffer++ % STD_ARRAYLEN(aaBuffers)];
    stdUtil_StringCopy(pCopy, sizeof(aaBuffers[0]), pText);
    return pCopy;
}

// "00_cyn.cnd" -> the level's name from the game's text (JONES_STR_CYN), else the file name
static void jonesConfigSDL_GetLevelTitle(const char* pLevelFilename, char* pTitle, size_t titleSize)
{
    stdUtil_StringCopy(pTitle, titleSize, pLevelFilename);
    const char* pCode = strchr(pLevelFilename, '_');
    if ( !pCode || strlen(pCode) < 4 )
    {
        return;
    }

    char aKey[32] = "JONES_STR_";
    size_t len = strlen(aKey);
    for ( int i = 1; i <= 3; ++i )
    {
        aKey[len++] = (char)toupper((unsigned char)pCode[i]);
    }
    aKey[len] = 0;
    const char* pName = jonesString_GetString(aKey);
    if ( pName && *pName )
    {
        stdUtil_StringCopy(pTitle, titleSize, pName);
    }
}

static int jonesConfigSDL_CompareSaves(const void* pA, const void* pB)
{
    const JonesConfigSaveEntry* pSaveA = (const JonesConfigSaveEntry*)pA;
    const JonesConfigSaveEntry* pSaveB = (const JonesConfigSaveEntry*)pB;
    return pSaveA->msecTime < pSaveB->msecTime ? 1 : (pSaveA->msecTime > pSaveB->msecTime ? -1 : 0);
}

// The savegames, newest first; level-start autosaves only if asked for
static size_t jonesConfigSDL_ListSaves(bool bWithAutosaves)
{
    size_t numSaves = 0;
    const char* pAutoPrefix = sithGetAutoSaveFilePrefix();
    FindFileData* pFind = stdFileUtil_NewFind(sithGetSaveGamesDir(), 3, "nds");
    tFoundFileInfo fileInfo;
    while ( pFind && numSaves < JONESCONFIGSDL_MAXSAVES && stdFileUtil_FindNext(pFind, &fileInfo) )
    {
        if ( fileInfo.bIsDirectory )
        {
            continue;
        }

        JonesConfigSaveEntry* pSave = &jonesConfigSDL_aSaves[numSaves];
        pSave->bAuto = pAutoPrefix && strncasecmp(fileInfo.aName, pAutoPrefix, strlen(pAutoPrefix)) == 0;
        if ( pSave->bAuto && !bWithAutosaves )
        {
            continue;
        }

        stdFnames_MakePath(pSave->aPath, sizeof(pSave->aPath), sithGetSaveGamesDir(), fileInfo.aName);
        char aLevelFilename[64] = { 0 };
        if ( sithGamesave_LoadLevelFilename(pSave->aPath, aLevelFilename) )
        {
            continue; // not a savegame of this version
        }

        jonesConfigSDL_GetLevelTitle(aLevelFilename, pSave->aTitle, sizeof(pSave->aTitle));
        pSave->msecTime = fileInfo.lastChanged;

        char aDate[48] = { 0 };
        const time_t fileTime = (time_t)fileInfo.lastChanged;
        const struct tm* pTime = localtime(&fileTime);
        if ( pTime )
        {
            strftime(aDate, sizeof(aDate), "%Y-%m-%d  %H:%M", pTime);
        }
        STD_FORMAT(pSave->aInfo, "%s%s", aDate, pSave->bAuto ? jonesConfigSDL_Text(NULL, "   (level start)") : "");
        ++numSaves;
    }
    if ( pFind )
    {
        stdFileUtil_DisposeFind(pFind);
    }

    qsort(jonesConfigSDL_aSaves, numSaves, sizeof(JonesConfigSaveEntry), jonesConfigSDL_CompareSaves);
    return numSaves;
}

// A panel with a title, a text and buttons in a row; returns the chosen button, or backResult on Back (if >= 0)
static int jonesConfigSDL_RunChoice(const char* pTitle, const char* pText, const char* const* apButtons, int numButtons, int backResult)
{
    while ( JonesMenu_BeginFrame() )
    {
        const float u = JonesMenu_GetUnit(), w = JonesMenu_GetWidth(), h = JonesMenu_GetHeight();
        const float panelW = fminf(w * 0.92f, 330.0f * u * (float)(numButtons > 2 ? 1.6f : 1.2f));
        const float buttonW = (panelW - 20.0f * u - (float)(numButtons - 1) * 10.0f * u) / (float)numButtons;
        const JonesMenuRect panel = { (w - panelW) * 0.5f, h * 0.5f - 105.0f * u, panelW, 210.0f * u };
        JonesMenu_Panel(&panel);
        JonesMenu_Text(pTitle, w * 0.5f, panel.y + 16.0f * u, 24.0f, RDFONT_ALIGNCENTER, JONESMENU_TEXT_TITLE);
        JonesMenu_Text(pText, w * 0.5f, panel.y + 74.0f * u, 15.0f, RDFONT_ALIGNCENTER, JONESMENU_TEXT_NORMAL);

        int chosen = -1;
        for ( int i = 0; i < numButtons; ++i )
        {
            const JonesMenuRect button = { panel.x + 10.0f * u + (float)i * (buttonW + 10.0f * u), panel.y + panel.height - 62.0f * u, buttonW, 50.0f * u };
            if ( JonesMenu_Button(&button, apButtons[i], NULL) )
            {
                chosen = i;
            }
        }
        if ( chosen < 0 && backResult >= 0 && JonesMenu_IsBack() )
        {
            chosen = backResult;
        }
        JonesMenu_EndFrame();
        if ( chosen >= 0 )
        {
            return chosen;
        }
    }
    return backResult >= 0 ? backResult : 0; // the window closes
}

// A scrolling list of savegames under a title, with an optional first entry; returns the chosen save's index,
// JONESCONFIGSDL_TOPITEM or JONESCONFIGSDL_BACK
static int jonesConfigSDL_RunSaveList(const char* pTitle, const char* pTopItem, size_t numSaves)
{
    float scroll = 0.0f;
    JonesMenu_SetFocus(0);
    while ( JonesMenu_BeginFrame() )
    {
        const float u = JonesMenu_GetUnit(), w = JonesMenu_GetWidth(), h = JonesMenu_GetHeight();
        const float panelW = fminf(w * 0.92f, 480.0f * u);
        const JonesMenuRect panel = { (w - panelW) * 0.5f, 14.0f * u, panelW, h - 28.0f * u };
        JonesMenu_Panel(&panel);
        JonesMenu_Text(pTitle, w * 0.5f, panel.y + 10.0f * u, 24.0f, RDFONT_ALIGNCENTER, JONESMENU_TEXT_TITLE);

        // the list between the title and the Back button; rows only drawn when completely visible
        const float rowH = 52.0f * u, gap = 6.0f * u;
        const float listTop = panel.y + 50.0f * u, listBottom = panel.y + panel.height - 66.0f * u;
        const size_t numRows = numSaves + (pTopItem ? 1 : 0);
        const float contentH = (float)numRows * (rowH + gap);
        const float maxScroll = fmaxf(0.0f, contentH - (listBottom - listTop));

        // keys and gamepads: keep the focused row in view; touch: drag
        scroll -= JonesMenu_TakeScroll();
        const int focus = JonesMenu_GetFocus();
        if ( focus < (int)numRows )
        {
            const float focusTop = (float)focus * (rowH + gap);
            if ( focusTop < scroll ) scroll = focusTop;
            if ( focusTop + rowH > scroll + (listBottom - listTop) ) scroll = focusTop + rowH - (listBottom - listTop);
        }
        scroll = fminf(fmaxf(scroll, 0.0f), maxScroll);

        int chosen = INT32_MIN;
        for ( size_t row = 0; row < numRows; ++row )
        {
            const JonesMenuRect rect = { panel.x + 12.0f * u, listTop + (float)row * (rowH + gap) - scroll, panel.width - 24.0f * u, rowH };
            const bool bVisible = rect.y >= listTop - 0.5f && rect.y + rect.height <= listBottom + 0.5f;
            if ( !bVisible )
            {
                // keeps the button order for the focus, without drawing
                JonesMenuRect hidden = { -10000.0f, -10000.0f, 0.0f, 0.0f };
                if ( JonesMenu_Button(&hidden, NULL, NULL) ) chosen = pTopItem && row == 0 ? JONESCONFIGSDL_TOPITEM : (int)row - (pTopItem ? 1 : 0);
                continue;
            }

            if ( pTopItem && row == 0 )
            {
                if ( JonesMenu_Button(&rect, pTopItem, NULL) ) chosen = JONESCONFIGSDL_TOPITEM;
            }
            else
            {
                const size_t i = row - (pTopItem ? 1 : 0);
                if ( JonesMenu_Button(&rect, jonesConfigSDL_aSaves[i].aTitle, jonesConfigSDL_aSaves[i].aInfo) ) chosen = (int)i;
            }
        }
        if ( !numRows )
        {
            JonesMenu_Text(jonesConfigSDL_Text(NULL, "No saved games"), w * 0.5f, listTop + 20.0f * u, 14.0f, RDFONT_ALIGNCENTER, JONESMENU_TEXT_DIM);
        }

        const JonesMenuRect back = { (w - 180.0f * u) * 0.5f, panel.y + panel.height - 58.0f * u, 180.0f * u, 48.0f * u };
        if ( JonesMenu_Button(&back, jonesConfigSDL_Text(NULL, "Back"), NULL) || JonesMenu_IsBack() )
        {
            chosen = JONESCONFIGSDL_BACK;
        }
        JonesMenu_EndFrame();
        if ( chosen != INT32_MIN )
        {
            return chosen;
        }
    }
    return JONESCONFIGSDL_BACK;
}

// A new savegame's path: <save games dir>\<level save name>_<date>_<time>.nds
static void jonesConfigSDL_MakeNewSavePath(char* pOutFilePath)
{
    char aStamp[32] = { 0 };
    const time_t now = time(NULL);
    const struct tm* pTime = localtime(&now);
    if ( pTime )
    {
        strftime(aStamp, sizeof(aStamp), "%Y-%m-%d_%H-%M-%S", pTime);
    }

    char aLevelName[64] = { 0 };
    stdUtil_StringCopy(aLevelName, sizeof(aLevelName), sithWorld_g_pCurrentWorld ? sithGetCurrentWorldSaveName() : "save");
    stdFnames_StripExtAndDot(aLevelName); // "01_CANYON.nds"

    char aFilename[128] = { 0 };
    STD_FORMAT(aFilename, "%s_%s.nds", aLevelName, aStamp);
    stdFnames_MakePath(pOutFilePath, JONESCONFIG_GAMESAVE_FILEPATHSIZE, sithGetSaveGamesDir(), aFilename);
}

// Lets the player pick a savegame (with level-start autosaves); false: Back
static bool jonesConfigSDL_PickSaveToLoad(char* pDestNdsPath)
{
    const size_t numSaves = jonesConfigSDL_ListSaves(/*bWithAutosaves=*/true);
    const int chosen = jonesConfigSDL_RunSaveList(jonesConfigSDL_Text("JONES_STR_LOADGM", "Load Game"), NULL, numSaves);
    if ( chosen < 0 )
    {
        return false;
    }
    stdUtil_StringCopy(pDestNdsPath, JONESCONFIG_GAMESAVE_FILEPATHSIZE, jonesConfigSDL_aSaves[chosen].aPath);
    return true;
}

// Native start menu (JonesMain_Startup): continue from the newest save, load one, or start a new game. True with the
// save to load in pNdsPath; false for a new game. No menu without saves.
bool jonesConfigSDL_ShowStartMenu(char* pNdsPath)
{
    const size_t numSaves = jonesConfigSDL_ListSaves(/*bWithAutosaves=*/true);
    if ( !numSaves || !JonesMenu_Begin() )
    {
        return false;
    }

    bool bLoad = false;
    for ( ;; )
    {
        int chosen = -1;
        if ( !JonesMenu_BeginFrame() )
        {
            break;
        }

        const float u = JonesMenu_GetUnit(), w = JonesMenu_GetWidth(), h = JonesMenu_GetHeight();
        JonesMenu_Text("Indiana Jones", w * 0.5f, h * 0.12f, 34.0f, RDFONT_ALIGNCENTER, JONESMENU_TEXT_TITLE);
        JonesMenu_Text(jonesConfigSDL_Text(NULL, "and the Infernal Machine"), w * 0.5f, h * 0.12f + 40.0f * u, 18.0f, RDFONT_ALIGNCENTER, JONESMENU_TEXT_TITLE);

        const float buttonW = fminf(w * 0.8f, 300.0f * u), buttonH = 54.0f * u, x = (w - buttonW) * 0.5f;
        char aContinue[160] = { 0 };
        STD_FORMAT(aContinue, "%s,  %s", jonesConfigSDL_aSaves[0].aTitle, jonesConfigSDL_aSaves[0].aInfo);
        const JonesMenuRect continueRect = { x, h * 0.42f, buttonW, buttonH };
        const JonesMenuRect loadRect     = { x, continueRect.y + buttonH + 10.0f * u, buttonW, buttonH };
        const JonesMenuRect newRect      = { x, loadRect.y + buttonH + 10.0f * u, buttonW, buttonH };
        if ( JonesMenu_Button(&continueRect, jonesConfigSDL_Text(NULL, "Continue"), aContinue) ) chosen = 0;
        if ( JonesMenu_Button(&loadRect, jonesConfigSDL_Text("JONES_STR_LOADGM", "Load Game"), NULL) ) chosen = 1;
        if ( JonesMenu_Button(&newRect, jonesConfigSDL_Text(NULL, "New game"), NULL) ) chosen = 2;
        JonesMenu_EndFrame();

        if ( chosen == 0 )
        {
            stdUtil_StringCopy(pNdsPath, JONESCONFIG_GAMESAVE_FILEPATHSIZE, jonesConfigSDL_aSaves[0].aPath);
            bLoad = true;
            break;
        }
        if ( chosen == 1 && jonesConfigSDL_PickSaveToLoad(pNdsPath) )
        {
            bLoad = true;
            break;
        }
        if ( chosen == 2 )
        {
            break;
        }
        if ( chosen == 1 )
        {
            jonesConfigSDL_ListSaves(/*bWithAutosaves=*/true); // the load menu reused the list
        }
    }

    JonesMenu_End();
    return bLoad;
}

int J3DAPI jonesConfig_GetLoadGameFilePath(HWND hWnd, char* pDestNdsPath)
{
    J3D_UNUSED(hWnd);

    if ( JonesMenu_Begin() )
    {
        const bool bChosen = jonesConfigSDL_PickSaveToLoad(pDestNdsPath);
        JonesMenu_End();
        return bChosen ? JONESCONFIGSDL_OK : JONESCONFIGSDL_CANCEL;
    }

    // No game frame (loading at startup): the last saved game, if it still exists
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

    if ( JonesMenu_Begin() )
    {
        int result = JONESCONFIGSDL_CANCEL;
        for ( ;; )
        {
            const size_t numSaves = jonesConfigSDL_ListSaves(/*bWithAutosaves=*/false);
            const int chosen = jonesConfigSDL_RunSaveList(jonesConfigSDL_Text("JONES_STR_SAVEGM", "Save Game"), jonesConfigSDL_Text(NULL, "New save"), numSaves);
            if ( chosen == JONESCONFIGSDL_BACK )
            {
                break;
            }
            if ( chosen == JONESCONFIGSDL_TOPITEM )
            {
                jonesConfigSDL_MakeNewSavePath(pOutFilePath);
                result = JONESCONFIGSDL_OK;
                break;
            }

            // overwrite an existing save?
            const char* apButtons[] = { jonesConfigSDL_Text(NULL, "Overwrite"), jonesConfigSDL_Text(NULL, "Cancel") };
            char aText[160] = { 0 };
            STD_FORMAT(aText, "%s,  %s", jonesConfigSDL_aSaves[chosen].aTitle, jonesConfigSDL_aSaves[chosen].aInfo);
            if ( jonesConfigSDL_RunChoice(jonesConfigSDL_Text(NULL, "Overwrite this save?"), aText, apButtons, 2, 1) == 0 )
            {
                stdUtil_StringCopy(pOutFilePath, JONESCONFIG_GAMESAVE_FILEPATHSIZE, jonesConfigSDL_aSaves[chosen].aPath);
                result = JONESCONFIGSDL_OK;
                break;
            }
        }
        JonesMenu_End();
        return result;
    }

    // No game frame: one slot per level, <save games dir>\<level save name>.nds
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
    if ( !JonesMenu_Begin() )
    {
        return JONESCONFIGSDL_OK; // exit without saving
    }

    const char* apButtons[] = { jonesConfigSDL_Text(NULL, "Save and exit"), jonesConfigSDL_Text(NULL, "Exit"), jonesConfigSDL_Text(NULL, "Cancel") };
    const int chosen = jonesConfigSDL_RunChoice(jonesConfigSDL_Text("JONES_STR_EXIT", "Exit"), jonesConfigSDL_Text(NULL, "Leave the game?"), apButtons, 3, 2);
    JonesMenu_End();

    switch ( chosen )
    {
        case 0:
            jonesConfigSDL_MakeNewSavePath(pSaveGameFilePath);
            return 1187; // save, then exit (JonesHud)
        case 1:
            return JONESCONFIGSDL_OK;
        default:
            return JONESCONFIGSDL_CANCEL;
    }
}

// The level's autosave (made when the level started), or an empty path
static void jonesConfigSDL_GetLevelAutosave(char* pRestoreFilename)
{
    pRestoreFilename[0] = '\0';
    if ( sithWorld_g_pCurrentWorld )
    {
        char aFilename[128] = { 0 };
        STD_FORMAT(aFilename, "%s%s", sithGetAutoSaveFilePrefix(), sithGetCurrentWorldSaveName());
        stdFnames_ChangeExtEx(aFilename, STD_ARRAYLEN(aFilename), "nds");

        LPSTR pFilePart;
        SearchPath(sithGetSaveGamesDir(), aFilename, NULL, JONESCONFIG_GAMESAVE_FILEPATHSIZE, pRestoreFilename, &pFilePart);
    }
}

int J3DAPI jonesConfig_ShowGameOverDialog(HWND hWnd, char* pRestoreFilename, tSoundHandle hSndGameOVerMus, tSoundChannelHandle* pSndChnlMus)
{
    J3D_UNUSED(hWnd);
    if ( pSndChnlMus )
    {
        *pSndChnlMus = 0;
    }

    if ( !JonesMenu_Begin() )
    {
        jonesConfigSDL_GetLevelAutosave(pRestoreFilename); // restart from the level's autosave, as the Restart button
        return strlen(pRestoreFilename) ? 1177 : 1179;     // restart, else quit
    }

    if ( hSndGameOVerMus && pSndChnlMus )
    {
        *pSndChnlMus = sithSoundMixer_PlaySound(hSndGameOVerMus, 1.0f, 0.0f, (SoundPlayFlag)0); // as the dialog does
    }

    // the newest save first: dying usually means going back to it
    const size_t numSaves = jonesConfigSDL_ListSaves(/*bWithAutosaves=*/true);
    int result = 1179;
    for ( ;; )
    {
        const char* apButtons[] = { jonesConfigSDL_Text(NULL, "Last save"), jonesConfigSDL_Text(NULL, "Load game"), jonesConfigSDL_Text(NULL, "Quit") };
        char aText[160] = "";
        if ( numSaves )
        {
            STD_FORMAT(aText, "%s,  %s", jonesConfigSDL_aSaves[0].aTitle, jonesConfigSDL_aSaves[0].aInfo);
        }
        const int chosen = jonesConfigSDL_RunChoice(jonesConfigSDL_Text(NULL, "Indy is dead"), aText, apButtons, 3, -1);
        if ( chosen == 0 )
        {
            if ( numSaves )
            {
                stdUtil_StringCopy(pRestoreFilename, JONESCONFIG_GAMESAVE_FILEPATHSIZE, jonesConfigSDL_aSaves[0].aPath);
            }
            else
            {
                jonesConfigSDL_GetLevelAutosave(pRestoreFilename);
            }
            if ( strlen(pRestoreFilename) )
            {
                result = 1177; // restart from there
                break;
            }
        }
        else if ( chosen == 1 )
        {
            if ( jonesConfigSDL_PickSaveToLoad(pRestoreFilename) )
            {
                result = 1178; // load game
                break;
            }
        }
        else
        {
            const char* apConfirm[] = { jonesConfigSDL_Text(NULL, "Quit"), jonesConfigSDL_Text(NULL, "Cancel") };
            if ( jonesConfigSDL_RunChoice(jonesConfigSDL_Text(NULL, "Quit"), jonesConfigSDL_Text(NULL, "Leave the game?"), apConfirm, 2, 1) == 0 )
            {
                pRestoreFilename[0] = '\0';
                result = 1179;
                break;
            }
        }
    }

    JonesMenu_End();
    return result;
}

int J3DAPI jonesConfig_ShowGamePlayOptions(HWND hWnd)
{
    J3D_UNUSED(hWnd);
    return JONESCONFIGSDL_CANCEL;
}

// The shop after a level (as jonesConfig_ShowStoreDialog): apItemsState[i] has the item's menu ID in the high word,
// what Indy owns in bits 4..15 and whether it can be bought in bit 0; on return the low word is the number bought.
static void jonesConfigSDL_RunShop(int* pBalance, int* apItemsState)
{
    int aOwned[STD_ARRAYLEN(JonesHud_aStoreItems)] = { 0 };
    int aAvailable[STD_ARRAYLEN(JonesHud_aStoreItems)] = { 0 };
    int numAvailable = 0;
    for ( size_t i = 0; i < STD_ARRAYLEN(JonesHud_aStoreItems); ++i )
    {
        aAvailable[i] = (apItemsState[i] & 0xF) != 0;
        aOwned[i]     = (apItemsState[i] & 0xFFF0) >> 4;
        apItemsState[i] &= ~0xFFFF; // from here: the number bought
        numAvailable += aAvailable[i];
    }

    int balance = *pBalance;
    float scroll = 0.0f;
    JonesMenu_SetFocus(0);
    while ( JonesMenu_BeginFrame() )
    {
        const float u = JonesMenu_GetUnit(), w = JonesMenu_GetWidth(), h = JonesMenu_GetHeight();
        const float panelW = fminf(w * 0.94f, 520.0f * u);
        const JonesMenuRect panel = { (w - panelW) * 0.5f, 14.0f * u, panelW, h - 28.0f * u };
        JonesMenu_Panel(&panel);
        JonesMenu_Text(jonesConfigSDL_Text("JONES_STR_STORE", "Store"), w * 0.5f, panel.y + 10.0f * u, 24.0f, RDFONT_ALIGNCENTER, JONESMENU_TEXT_TITLE);

        char aBalance[64] = { 0 };
        STD_FORMAT(aBalance, "%s %d", jonesConfigSDL_Text(NULL, "Treasure value:"), balance);
        JonesMenu_Text(aBalance, w * 0.5f, panel.y + 40.0f * u, 15.0f, RDFONT_ALIGNCENTER, JONESMENU_TEXT_NORMAL);

        const float rowH = 50.0f * u, gap = 6.0f * u, minusW = 56.0f * u;
        const float listTop = panel.y + 66.0f * u, listBottom = panel.y + panel.height - 66.0f * u;
        const float contentH = (float)numAvailable * (rowH + gap);
        scroll -= JonesMenu_TakeScroll();
        scroll = fminf(fmaxf(scroll, 0.0f), fmaxf(0.0f, contentH - (listBottom - listTop)));

        int buy = -1, giveBack = -1, row = 0;
        for ( size_t i = 0; i < STD_ARRAYLEN(JonesHud_aStoreItems); ++i )
        {
            if ( !aAvailable[i] )
            {
                continue;
            }

            const int numBought = (uint16_t)apItemsState[i];
            JonesMenuRect rect = { panel.x + 12.0f * u, listTop + (float)row * (rowH + gap) - scroll, panel.width - 24.0f * u, rowH };
            ++row;
            if ( rect.y < listTop - 0.5f || rect.y + rect.height > listBottom + 0.5f )
            {
                continue; // scrolled out of view
            }

            char aInfo[96] = { 0 };
            if ( JonesHud_aStoreItems[i].menuID == JONESHUD_MENU_INVITEM_BONUSMAP )
            {
                STD_FORMAT(aInfo, "%d%s", JonesHud_aStoreItems[i].price, numBought ? "    +1" : "");
            }
            else
            {
                STD_FORMAT(aInfo, "%d    (%d)%s", JonesHud_aStoreItems[i].price, aOwned[i], numBought ? "" : "");
                if ( numBought )
                {
                    char aCart[24] = { 0 };
                    STD_FORMAT(aCart, "    +%d", numBought);
                    stdUtil_StringCat(aInfo, sizeof(aInfo), aCart);
                }
            }

            if ( numBought )
            {
                rect.width -= minusW + 8.0f * u;
            }
            if ( JonesMenu_Button(&rect, jonesConfigSDL_Text(JonesHud_aStoreItems[i].aClipName, JonesHud_aStoreItems[i].aClipName), aInfo) )
            {
                buy = (int)i;
            }
            if ( numBought )
            {
                const JonesMenuRect minus = { rect.x + rect.width + 8.0f * u, rect.y, minusW, rowH };
                if ( JonesMenu_Button(&minus, "-", NULL) )
                {
                    giveBack = (int)i;
                }
            }
        }

        const JonesMenuRect done = { (w - 180.0f * u) * 0.5f, panel.y + panel.height - 58.0f * u, 180.0f * u, 48.0f * u };
        const bool bDone = JonesMenu_Button(&done, jonesConfigSDL_Text(NULL, "Done"), NULL) || JonesMenu_IsBack();
        JonesMenu_EndFrame();

        if ( giveBack >= 0 )
        {
            apItemsState[giveBack] = ((uint16_t)apItemsState[giveBack] - 1) | (apItemsState[giveBack] & ~0xFFFF);
            balance += JonesHud_aStoreItems[giveBack].price;
        }
        else if ( buy >= 0 )
        {
            const bool bMap = JonesHud_aStoreItems[buy].menuID == JONESHUD_MENU_INVITEM_BONUSMAP;
            const char* apOk[] = { "OK" };
            if ( balance < JonesHud_aStoreItems[buy].price )
            {
                jonesConfigSDL_RunChoice(jonesConfigSDL_Text("JONES_STR_STORE", "Store"),
                    jonesConfigSDL_Text(bMap ? "JONES_STR_NOPERU" : "JONES_STR_CANTBUY1", "Not enough treasure."), apOk, 1, 0);
            }
            else if ( !(bMap && (uint16_t)apItemsState[buy]) ) // the map only once
            {
                if ( bMap )
                {
                    jonesConfigSDL_RunChoice(jonesConfigSDL_Text("JONES_STR_STORE", "Store"), jonesConfigSDL_Text("JONES_STR_PERU", ""), apOk, 1, 0);
                }
                apItemsState[buy] = ((uint16_t)apItemsState[buy] + 1) | (apItemsState[buy] & ~0xFFFF);
                balance -= JonesHud_aStoreItems[buy].price;
            }
        }
        else if ( bDone )
        {
            break;
        }
    }
    *pBalance = balance;
}

int J3DAPI jonesConfig_ShowLevelCompletedDialog(HWND hWnd, int* pBalance, int* apItemsState, int a4, int elapsedTime, int qiPoints, int numFoundTrasures, int foundTrasureValue, int totalTreasureValue)
{
    J3D_UNUSED(hWnd);
    J3D_UNUSED(a4);
    STDLOG_STATUS("Level completed: time %d, IQ points %d, treasures %d (value %d of %d).\n",
        elapsedTime, qiPoints, numFoundTrasures, foundTrasureValue, totalTreasureValue);
    if ( !JonesMenu_Begin() )
    {
        return JONESCONFIGSDL_OK; // continue, nothing bought
    }

    char aLevel[64] = { 0 };
    jonesConfigSDL_GetLevelTitle(sithWorld_g_pCurrentWorld ? sithWorld_g_pCurrentWorld->aName : "", aLevel, sizeof(aLevel));

    // As the dialog: hours in the high bits, minutes in the low byte
    char aLines[5][96] = { 0 };
    STD_FORMAT(aLines[0], "%s  %d:%02d", jonesConfigSDL_Text(NULL, "Time:"), elapsedTime >> 8, elapsedTime & 0xFF);
    STD_FORMAT(aLines[1], "%s  %d", jonesConfigSDL_Text(NULL, "IQ points:"), qiPoints);
    STD_FORMAT(aLines[2], "%s  %d", jonesConfigSDL_Text(NULL, "Treasures found:"), numFoundTrasures);
    STD_FORMAT(aLines[3], "%s  %d", jonesConfigSDL_Text(NULL, "Value found:"), foundTrasureValue);
    STD_FORMAT(aLines[4], "%s  %d", jonesConfigSDL_Text(NULL, "Total value:"), totalTreasureValue);

    while ( JonesMenu_BeginFrame() )
    {
        const float u = JonesMenu_GetUnit(), w = JonesMenu_GetWidth(), h = JonesMenu_GetHeight();
        const float panelW = fminf(w * 0.9f, 400.0f * u);
        const JonesMenuRect panel = { (w - panelW) * 0.5f, h * 0.5f - 175.0f * u, panelW, 350.0f * u };
        JonesMenu_Panel(&panel);
        JonesMenu_Text(jonesConfigSDL_Text(NULL, "Level completed"), w * 0.5f, panel.y + 12.0f * u, 24.0f, RDFONT_ALIGNCENTER, JONESMENU_TEXT_TITLE);
        JonesMenu_Text(aLevel, w * 0.5f, panel.y + 44.0f * u, 18.0f, RDFONT_ALIGNCENTER, JONESMENU_TEXT_TITLE);
        for ( int i = 0; i < 5; ++i )
        {
            JonesMenu_Text(aLines[i], panel.x + 40.0f * u, panel.y + (90.0f + 32.0f * (float)i) * u, 16.0f, RDFONT_ALIGNLEFT, JONESMENU_TEXT_NORMAL);
        }

        const JonesMenuRect next = { (w - 200.0f * u) * 0.5f, panel.y + panel.height - 62.0f * u, 200.0f * u, 50.0f * u };
        const bool bContinue = JonesMenu_Button(&next, jonesConfigSDL_Text(NULL, "Continue"), NULL);
        JonesMenu_EndFrame();
        if ( bContinue )
        {
            break;
        }
    }

    // Then the shop, except after the bonus level (as the dialog)
    const SithGameStatistics* pStatistics = sithGamesave_GetGameStatistics();
    if ( !pStatistics || pStatistics->curLevelNum + 1 < JONESLEVEL_BONUSLEVELNUM )
    {
        jonesConfigSDL_RunShop(pBalance, apItemsState);
    }

    JonesMenu_End();
    return JONESCONFIGSDL_OK;
}

int J3DAPI jonesConfig_ShowMessageDialog(HWND hWnd, const char* pTitle, const char* pText, int iconID)
{
    J3D_UNUSED(hWnd);
    J3D_UNUSED(iconID);
    const char* pTitleText = pTitle ? jonesString_GetString(pTitle) : NULL;
    STDLOG_STATUS("%s: %s\n", pTitleText ? pTitleText : (pTitle ? pTitle : ""), pText ? pText : "");

    if ( JonesMenu_Begin() )
    {
        const char* apButtons[] = { "OK" };
        jonesConfigSDL_RunChoice(pTitleText ? pTitleText : (pTitle ? pTitle : ""), pText ? pText : "", apButtons, 1, 0);
        JonesMenu_End();
    }
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

// Headless tests: INDY_MENU_TEST=load|save|gameover|exit|levelend opens that menu once, 5 s into the game (JonesMain calls this
// every game frame); the result goes to the log
void jonesConfigSDL_RunTestMenu(void)
{
    static int bDone = -1;
    if ( bDone < 0 )
    {
        bDone = getenv("INDY_MENU_TEST") == NULL;
    }
    // in play: 5 s in and no cutscene running
    if ( bDone || sithTime_g_msecGameTime < 5000 || sithCamera_g_pCurCamera == &sithCamera_g_aCameras[SITHCAMERA_CINEMACAMERANUM] )
    {
        return;
    }
    bDone = 1;

    const char* pMenu = getenv("INDY_MENU_TEST");
    char aPath[JONESCONFIG_GAMESAVE_FILEPATHSIZE] = { 0 };
    int result = 0;
    if ( streq(pMenu, "load") )
    {
        result = jonesConfig_GetLoadGameFilePath(NULL, aPath);
    }
    else if ( streq(pMenu, "save") )
    {
        result = jonesConfig_GetSaveGameFilePath(NULL, aPath);
    }
    else if ( streq(pMenu, "exit") )
    {
        result = jonesConfig_ShowExitGameDialog(NULL, aPath);
    }
    else if ( streq(pMenu, "levelend") )
    {
        jonesCog_EndLevel(); // as the console's endlevel: the level's end with statistics and shop
    }
    else if ( streq(pMenu, "gameover") )
    {
        JonesHud_ShowGameOverDialog(/*bPlayDiedMusic=*/1); // the whole flow: restores the chosen save
    }
    STDLOG_STATUS("INDY_MENU_TEST %s: result %d, path '%s'\n", pMenu, result, aPath);
}
