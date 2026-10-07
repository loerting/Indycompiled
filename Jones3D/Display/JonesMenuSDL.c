// Native builds: engine-drawn menus. See JonesMenuSDL.h.
#include "JonesMenuSDL.h"

#include <indy/indyDraw.h>
#include <indy/indyTouch.h>
#include <rdroid/Main/rdroid.h>
#include <rdroid/Primitives/rdFont.h>
#include <rdroid/Raster/rdCache.h>
#include <sith/World/sithWorld.h>
#include <std/General/std.h>
#include <std/SDL/stdGLES.h>
#include <std/Win95/std3D.h>
#include <std/Win95/stdControl.h>
#include <std/Win95/stdDisplay.h>
#include <wkernel/wkernel.h>

#include <SDL3/SDL.h>
#include <math.h>

#define JONESMENU_STICK_THRESHOLD    0.6f
#define JONESMENU_STICK_REPEAT_MSEC  180u
#define JONESMENU_STICK_DELAY_MSEC   400u
#define JONESMENU_RELEASE_WAIT_MSEC  600u

static bool JonesMenu_bActive;
static bool JonesMenu_bBackdrop; // the game frame behind the menu (else black: e.g. the start menu)
static bool JonesMenu_bOpenedControls; // the start menu runs before the game opens the controls
static rdFont* JonesMenu_pFont;
static float JonesMenu_width, JonesMenu_height;

// Input of the current frame
static bool JonesMenu_bConfirm, JonesMenu_bBack;
static int JonesMenu_move; // -1 up, +1 down
static bool JonesMenu_bTap;
static float JonesMenu_tapX, JonesMenu_tapY;
static bool JonesMenu_bPointer;
static float JonesMenu_pointerX, JonesMenu_pointerY;
static float JonesMenu_scroll;
static bool JonesMenu_bShowFocus; // keys or a gamepad were used: show the focus

static int JonesMenu_focus;
static int JonesMenu_numFocusable, JonesMenu_numFocusablePrev;
static int JonesMenu_stickDir;
static uint32_t JonesMenu_msecStickNext;

// ---- input ------------------------------------------------------------------------------------------------------

static bool JonesMenu_KeyPressed(size_t keyId)
{
    int numPressed = 0;
    stdControl_ReadKey(keyId, &numPressed);
    return numPressed > 0;
}

static bool JonesMenu_KeyHeld(size_t keyId)
{
    return stdControl_ReadKey(keyId, NULL) != 0;
}

static void JonesMenu_ReadInput(void)
{
    JonesMenu_bConfirm = JonesMenu_KeyPressed(DIK_RETURN) || JonesMenu_KeyPressed(DIK_NUMPADENTER) || JonesMenu_KeyPressed(DIK_SPACE);
    JonesMenu_bBack    = JonesMenu_KeyPressed(DIK_ESCAPE);
    JonesMenu_move     = JonesMenu_KeyPressed(DIK_UP) ? -1 : (JonesMenu_KeyPressed(DIK_DOWN) ? 1 : 0);

    // Gamepads: A, B, D-pad, left stick (with repeat)
    int stickDir = 0;
    for ( int joyNum = 0; joyNum < (int)stdControl_GetNumJoysticks(); ++joyNum )
    {
        if ( !stdControl_IsGamePad(joyNum) )
        {
            continue;
        }
        JonesMenu_bConfirm |= JonesMenu_KeyPressed(STDCONTROL_JOYSTICK_GETBUTTON(joyNum, 0));
        JonesMenu_bBack    |= JonesMenu_KeyPressed(STDCONTROL_JOYSTICK_GETBUTTON(joyNum, 1));
        if ( JonesMenu_KeyPressed(STDCONTROL_JOYSTICK_GETPOVUP(joyNum, 0)) ) JonesMenu_move = -1;
        if ( JonesMenu_KeyPressed(STDCONTROL_JOYSTICK_GETPOVDOWN(joyNum, 0)) ) JonesMenu_move = 1;

        const size_t axisY = STDCONTROL_GET_JOYSTICK_AXIS_Y(joyNum);
        if ( !stdControl_TestAxisFlag(axisY, STDCONTROL_AXIS_ENABLED) )
        {
            stdControl_EnableAxis((int)axisY);
        }
        const float y = stdControl_ReadAxis(axisY); // positive: down
        if ( fabsf(y) > JONESMENU_STICK_THRESHOLD )
        {
            stickDir = y > 0.0f ? 1 : -1;
        }
    }

    const uint32_t msecNow = (uint32_t)SDL_GetTicks();
    if ( stickDir && stickDir != JonesMenu_stickDir )
    {
        JonesMenu_move          = stickDir;
        JonesMenu_msecStickNext = msecNow + JONESMENU_STICK_DELAY_MSEC;
    }
    else if ( stickDir && (int32_t)(msecNow - JonesMenu_msecStickNext) >= 0 )
    {
        JonesMenu_move          = stickDir;
        JonesMenu_msecStickNext = msecNow + JONESMENU_STICK_REPEAT_MSEC;
    }
    JonesMenu_stickDir = stickDir;

    if ( JonesMenu_bConfirm || JonesMenu_move )
    {
        JonesMenu_bShowFocus = true;
    }

    // Touch
    JonesMenu_bTap = indyTouch_TakeUiTap(&JonesMenu_tapX, &JonesMenu_tapY);
    JonesMenu_scroll += indyTouch_TakeUiScroll();
    JonesMenu_bPointer = indyTouch_GetUiPointer(&JonesMenu_pointerX, &JonesMenu_pointerY);
    if ( JonesMenu_bTap || JonesMenu_bPointer )
    {
        JonesMenu_bShowFocus = false;
    }
}

// ---- session and frames -------------------------------------------------------------------------------------------

bool JonesMenu_Begin(void)
{
    uint32_t width = 0, height = 0;
    stdDisplay_GetBackBufferSize(&width, &height);
    if ( JonesMenu_bActive || !stdDisplay_IsOpen() || !width || !height )
    {
        return false; // no display yet
    }

    if ( !JonesMenu_pFont )
    {
        JonesMenu_pFont = rdFont_Load("mat\\jonesCalisto MT20.gcf");
        if ( !JonesMenu_pFont )
        {
            return false;
        }
    }

    JonesMenu_width  = (float)width;
    JonesMenu_height = (float)height;
    JonesMenu_bActive = true;
    JonesMenu_focus = JonesMenu_numFocusable = JonesMenu_numFocusablePrev = 0;
    JonesMenu_scroll = 0.0f;
    JonesMenu_bShowFocus = false;
    JonesMenu_stickDir = 0;

    JonesMenu_bOpenedControls = !stdControl_IsOpen();
    if ( JonesMenu_bOpenedControls )
    {
        stdControl_Open();
    }

    JonesMenu_bBackdrop = sithWorld_g_pCurrentWorld != NULL;
    if ( JonesMenu_bBackdrop )
    {
        stdDisplay_CaptureBackdrop(); // the game as it is now, behind the menu
    }
    indyTouch_SetUiMode(true);
    wkernel_SetModal(true);
    return true;
}

void JonesMenu_End(void)
{
    // the key that closed the menu must not reach the game (it would e.g. reopen the menu or close the HUD menu)
    const uint32_t msecEnd = (uint32_t)SDL_GetTicks() + JONESMENU_RELEASE_WAIT_MSEC;
    while ( (int32_t)((uint32_t)SDL_GetTicks() - msecEnd) < 0 && !wkernel_PeekProcessEvents() )
    {
        stdControl_ReadControls();
        bool bHeld = JonesMenu_KeyHeld(DIK_RETURN) || JonesMenu_KeyHeld(DIK_NUMPADENTER) || JonesMenu_KeyHeld(DIK_SPACE) || JonesMenu_KeyHeld(DIK_ESCAPE);
        for ( int joyNum = 0; joyNum < (int)stdControl_GetNumJoysticks(); ++joyNum )
        {
            bHeld |= JonesMenu_KeyHeld(STDCONTROL_JOYSTICK_GETBUTTON(joyNum, 0)) || JonesMenu_KeyHeld(STDCONTROL_JOYSTICK_GETBUTTON(joyNum, 1));
        }
        if ( !bHeld )
        {
            break;
        }
        SDL_Delay(10);
    }

    if ( JonesMenu_bOpenedControls )
    {
        stdControl_Close();
        JonesMenu_bOpenedControls = false;
    }
    indyTouch_SetUiMode(false);
    wkernel_SetModal(false);
    JonesMenu_bActive = false;
}

bool JonesMenu_BeginFrame(void)
{
    if ( wkernel_PeekProcessEvents() )
    {
        return false;
    }

    stdControl_ReadControls();
    JonesMenu_ReadInput();

    // focus moves through last frame's buttons, wrapping around
    if ( JonesMenu_move && JonesMenu_numFocusablePrev > 0 )
    {
        JonesMenu_focus = (JonesMenu_focus + JonesMenu_move + JonesMenu_numFocusablePrev) % JonesMenu_numFocusablePrev;
    }

    // the touch layer needs the screen size (it draws nothing in menus)
    IndyTouchFrame touchFrame = { 0 };
    touchFrame.width  = (uint32_t)JonesMenu_width;
    touchFrame.height = (uint32_t)JonesMenu_height;
    indyTouch_Frame(&touchFrame);

    std3D_ClearZBuffer();
    rdCache_AdvanceFrame();
    std3D_StartScene();
    if ( JonesMenu_bBackdrop )
    {
        stdDisplay_DrawBackdrop(0.3f);
    }
    else
    {
        stdDisplay_BackBufferFill(0, NULL);
    }
    rdFont_SetKeepAspect(true);
    JonesMenu_numFocusable = 0;
    return true;
}

void JonesMenu_EndFrame(void)
{
    rdCache_Flush();
    rdCache_FlushAlpha();
    rdFont_SetKeepAspect(false);
    std3D_EndScene();
    stdDisplay_Update();
    JonesMenu_numFocusablePrev = JonesMenu_numFocusable;
    if ( JonesMenu_focus >= JonesMenu_numFocusable )
    {
        JonesMenu_focus = JonesMenu_numFocusable > 0 ? JonesMenu_numFocusable - 1 : 0;
    }
}

// ---- layout and widgets -------------------------------------------------------------------------------------------

float JonesMenu_GetWidth(void)
{
    return JonesMenu_width;
}

float JonesMenu_GetHeight(void)
{
    return JonesMenu_height;
}

float JonesMenu_GetUnit(void)
{
    return JonesMenu_height / 480.0f;
}

bool JonesMenu_IsBack(void)
{
    return JonesMenu_bBack;
}

int JonesMenu_GetFocus(void)
{
    return JonesMenu_focus;
}

void JonesMenu_SetFocus(int index)
{
    JonesMenu_focus = index;
}

float JonesMenu_TakeScroll(void)
{
    const float scroll = JonesMenu_scroll;
    JonesMenu_scroll = 0.0f;
    return scroll;
}

static bool JonesMenu_IsInside(const JonesMenuRect* pRect, float x, float y)
{
    return x >= pRect->x && x <= pRect->x + pRect->width && y >= pRect->y && y <= pRect->y + pRect->height;
}

// Shapes go out after the text drawn so far, so later widgets cover earlier text and not the other way round
static void JonesMenu_BeginShapes(void)
{
    rdCache_FlushAlpha();
    indyDraw_Begin();
}

void JonesMenu_Panel(const JonesMenuRect* pRect)
{
    const float u = JonesMenu_GetUnit();
    JonesMenu_BeginShapes();
    indyDraw_Rect(pRect->x, pRect->y, pRect->x + pRect->width, pRect->y + pRect->height, D3DCOLOR_ARGB(215, 24, 18, 12));
    indyDraw_Frame(pRect->x, pRect->y, pRect->x + pRect->width, pRect->y + pRect->height, 1.5f * u, D3DCOLOR_ARGB(255, 176, 132, 64));
    indyDraw_End();
}

void JonesMenu_Text(const char* pText, float x, float y, float sizePt, int alignFlags, JonesMenuTextStyle style)
{
    if ( !pText || !*pText || !JonesMenu_pFont )
    {
        return;
    }

    rdFontColor color;
    switch ( style )
    {
        case JONESMENU_TEXT_TITLE: // gold, as the game's titles
            rdVector_Set4(&color[0], 1.0f, 0.86f, 0.45f, 1.0f);
            rdVector_Set4(&color[1], 1.0f, 0.86f, 0.45f, 1.0f);
            rdVector_Set4(&color[2], 0.86f, 0.55f, 0.18f, 1.0f);
            rdVector_Set4(&color[3], 0.86f, 0.55f, 0.18f, 1.0f);
            break;

        case JONESMENU_TEXT_DIM:
            for ( int i = 0; i < 4; ++i ) rdVector_Set4(&color[i], 0.72f, 0.68f, 0.6f, 1.0f);
            break;

        default:
            for ( int i = 0; i < 4; ++i ) rdVector_Set4(&color[i], 1.0f, 0.96f, 0.88f, 1.0f);
            break;
    }
    rdFont_SetFontColor(color);
    rdFont_DrawTextLineEx(pText, x / JonesMenu_width, y / JonesMenu_height, RD_FIXEDPOINT_RHW_SCALE_X1, JonesMenu_pFont, alignFlags, sizePt);
}

bool JonesMenu_Button(const JonesMenuRect* pRect, const char* pText, const char* pSubText)
{
    const int index    = JonesMenu_numFocusable++;
    const bool bFocus  = JonesMenu_bShowFocus && index == JonesMenu_focus;
    const bool bHover  = JonesMenu_bPointer && JonesMenu_IsInside(pRect, JonesMenu_pointerX, JonesMenu_pointerY);
    bool bChosen       = false;

    if ( JonesMenu_bTap && JonesMenu_IsInside(pRect, JonesMenu_tapX, JonesMenu_tapY) )
    {
        JonesMenu_focus = index;
        JonesMenu_bTap  = false;
        bChosen         = true;
    }
    else if ( index == JonesMenu_focus && JonesMenu_bConfirm )
    {
        JonesMenu_bConfirm = false;
        bChosen            = true;
    }

    const float u = JonesMenu_GetUnit();
    const float x1 = pRect->x + pRect->width, y1 = pRect->y + pRect->height;
    JonesMenu_BeginShapes();
    indyDraw_Rect(pRect->x, pRect->y, x1, y1, bHover ? D3DCOLOR_ARGB(230, 110, 78, 36) : (bFocus ? D3DCOLOR_ARGB(225, 84, 60, 28) : D3DCOLOR_ARGB(200, 44, 34, 22)));
    indyDraw_Frame(pRect->x, pRect->y, x1, y1, (bFocus ? 1.5f : 1.0f) * u, bFocus ? D3DCOLOR_ARGB(255, 255, 214, 120) : D3DCOLOR_ARGB(255, 120, 92, 50));
    indyDraw_End();

    const float textSize = 17.0f, subSize = 13.0f, pad = 10.0f * u; // finger-friendly sizes
    if ( pSubText && *pSubText )
    {
        const float textY = pRect->y + (pRect->height - (textSize + subSize + 2.0f) * u) * 0.5f;
        JonesMenu_Text(pText, pRect->x + pad, textY, textSize, RDFONT_ALIGNLEFT, JONESMENU_TEXT_NORMAL);
        JonesMenu_Text(pSubText, pRect->x + pad, textY + (textSize + 2.0f) * u, subSize, RDFONT_ALIGNLEFT, JONESMENU_TEXT_DIM);
    }
    else
    {
        JonesMenu_Text(pText, pRect->x + pRect->width * 0.5f, pRect->y + (pRect->height - textSize * u) * 0.5f, textSize, RDFONT_ALIGNCENTER, JONESMENU_TEXT_NORMAL);
    }
    return bChosen;
}
