// Indycompiled: touch controls (Android). See indyTouch.h.
#include "indyTouch.h"
#include "indyDraw.h"

#include <sith/Devices/sithControl.h>
#include <std/General/std.h>
#include <std/General/stdUtil.h>
#include <std/Win95/std3D.h>
#include <std/Win95/stdControl.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

// Sizes are fractions of the screen height (the short side in landscape)
#define INDY_TOUCH_MAXFINGERS         8
#define INDY_TOUCH_STICK_RADIUS       0.13f
#define INDY_TOUCH_STICK_KEY          0.4f   // stick deflection that counts as a classic key (climbing, swimming)
#define INDY_TOUCH_TAP_MSEC           250u
#define INDY_TOUCH_TAP_MOVE           0.03f
#define INDY_TOUCH_FLICK_MSEC         300u
#define INDY_TOUCH_FLICK_DIST         0.10f
#define INDY_TOUCH_CAMERA_WAIT_MSEC   150u   // a drag turns the camera after this (a quick flick down crouches instead)
#define INDY_TOUCH_YAW_PER_HEIGHT     180.0f // camera degrees per screen height of drag
#define INDY_TOUCH_PITCH_PER_HEIGHT   90.0f
#define INDY_TOUCH_MENU_STEP          0.08f  // menu swipe: one step per this distance
#define INDY_TOUCH_PULSE_MSEC         100u   // a tap or flick holds its control function this long
#define INDY_TOUCH_KEY_MSEC           80u    // injected keys (Enter, Escape) stay down this long

typedef enum eIndyTouchRole
{
    INDY_TOUCH_ROLE_NONE,
    INDY_TOUCH_ROLE_STICK,
    INDY_TOUCH_ROLE_CAMERA,
    INDY_TOUCH_ROLE_BUTTON,
    INDY_TOUCH_ROLE_MENU,
    INDY_TOUCH_ROLE_HUD,
    INDY_TOUCH_ROLE_UI, // native menus: taps and scrolling only
} IndyTouchRole;

typedef enum eIndyTouchButton
{
    INDY_TOUCH_BUTTON_JUMP,
    INDY_TOUCH_BUTTON_ACTION,
    INDY_TOUCH_BUTTON_MENU,
    INDY_TOUCH_BUTTON_HOLSTER, // put away what Indy holds
    INDY_TOUCH_BUTTON_COUNT
} IndyTouchButton;

typedef struct sIndyTouchFinger
{
    bool bUsed;
    uint64_t id;
    IndyTouchRole role;
    IndyTouchButton button;
    float downX, downY; // pixels
    float x, y;
    float camX, camY;   // camera: position already turned into camera motion
    float menuX, menuY; // menu: origin of the current swipe step
    int numMenuSteps;
    uint32_t msecDown;
} IndyTouchFinger;

static IndyTouchFinger indyTouch_aFingers[INDY_TOUCH_MAXFINGERS];
static IndyTouchFrame indyTouch_frame;
static bool indyTouch_bFrameValid;
static bool indyTouch_bVisible;           // touch used, no keyboard or gamepad since
static IndyTouchKeyFunc indyTouch_pfKey;
static uint32_t indyTouch_msecNow;

static float indyTouch_stickX, indyTouch_stickY; // stick centre (pixels); follows the thumb past the rim
static float indyTouch_camYaw, indyTouch_camPitch;
static bool indyTouch_bMenuTap;                  // a tap in the HUD menu, for JonesHud (indyTouch_TakeMenuTap)
static bool indyTouch_bUiMode;                   // native menus (indyTouch_SetUiMode)
static bool indyTouch_bUiTap;
static float indyTouch_uiTapX, indyTouch_uiTapY, indyTouch_uiScroll;
static float indyTouch_menuTapX, indyTouch_menuTapY;

static uint32_t indyTouch_aPulseUntil[SITHCONTROL_MAXFUNCTIONS];
static int indyTouch_aPendingPresses[SITHCONTROL_MAXFUNCTIONS];
static int indyTouch_aFramePresses[SITHCONTROL_MAXFUNCTIONS];

typedef struct sIndyTouchKey
{
    uint8_t dik;
    uint32_t msecUp;
} IndyTouchKey;
static IndyTouchKey indyTouch_aKeys[4];

// ---- layout ------------------------------------------------------------------------------------------------------

static void indyTouch_GetButton(IndyTouchButton button, float* pX, float* pY, float* pRadius)
{
    const float w = (float)indyTouch_frame.width, h = (float)indyTouch_frame.height;
    switch ( button )
    {
        case INDY_TOUCH_BUTTON_JUMP:
            *pX = w - 0.17f * h; *pY = h - 0.20f * h; *pRadius = 0.10f * h;
            break;
        case INDY_TOUCH_BUTTON_ACTION:
            *pX = w - 0.42f * h; *pY = h - 0.13f * h; *pRadius = 0.085f * h;
            break;
        case INDY_TOUCH_BUTTON_HOLSTER:
            *pX = w - 0.33f * h; *pY = h - 0.36f * h; *pRadius = 0.06f * h;
            break;
        default: // menu
            *pX = w - 0.09f * h; *pY = 0.10f * h; *pRadius = 0.06f * h;
            break;
    }
}

static bool indyTouch_HitButton(float x, float y, IndyTouchButton* pButton)
{
    for ( int i = 0; i < INDY_TOUCH_BUTTON_COUNT; ++i )
    {
        if ( (indyTouch_frame.bMenuOpen && i != INDY_TOUCH_BUTTON_MENU) || (i == INDY_TOUCH_BUTTON_HOLSTER && !indyTouch_frame.bHolding) )
        {
            continue;
        }
        float bx, by, r;
        indyTouch_GetButton((IndyTouchButton)i, &bx, &by, &r);
        if ( hypotf(x - bx, y - by) <= r * 1.25f )
        {
            *pButton = (IndyTouchButton)i;
            return true;
        }
    }
    return false;
}

static bool indyTouch_HitHud(float x, float y)
{
    const IndyTouchFrame* pF = &indyTouch_frame;
    const float mx = pF->hudWidth * 0.25f, my = pF->hudHeight * 0.25f;
    return pF->hudWidth > 0.0f && x >= pF->hudX - mx && x <= pF->hudX + pF->hudWidth + mx && y >= pF->hudY - my
        && y <= pF->hudY + pF->hudHeight + my;
}

// ---- output: pulses of control functions and injected keys --------------------------------------------------------

static void indyTouch_Pulse(SithControlFunction function)
{
    indyTouch_aPulseUntil[function] = indyTouch_msecNow + INDY_TOUCH_PULSE_MSEC;
    ++indyTouch_aPendingPresses[function];
}

static void indyTouch_PressKey(uint8_t dik)
{
    if ( !indyTouch_pfKey )
    {
        return;
    }
    for ( size_t i = 0; i < STD_ARRAYLEN(indyTouch_aKeys); ++i )
    {
        if ( !indyTouch_aKeys[i].dik )
        {
            indyTouch_aKeys[i].dik    = dik;
            indyTouch_aKeys[i].msecUp = indyTouch_msecNow + INDY_TOUCH_KEY_MSEC;
            indyTouch_pfKey(dik, true);
            return;
        }
    }
}

void indyTouch_SetKeyFunc(IndyTouchKeyFunc pfKey)
{
    indyTouch_pfKey = pfKey;
}

void indyTouch_Update(uint32_t msecTime)
{
    indyTouch_msecNow = msecTime;
    for ( size_t i = 0; i < STD_ARRAYLEN(indyTouch_aKeys); ++i )
    {
        if ( indyTouch_aKeys[i].dik && (int32_t)(msecTime - indyTouch_aKeys[i].msecUp) >= 0 )
        {
            if ( indyTouch_pfKey )
            {
                indyTouch_pfKey(indyTouch_aKeys[i].dik, false);
            }
            indyTouch_aKeys[i].dik = 0;
        }
    }
}

void indyTouch_OnOtherInput(void)
{
    indyTouch_bVisible = false;
}

// ---- input ------------------------------------------------------------------------------------------------------

static IndyTouchFinger* indyTouch_FindFinger(uint64_t id)
{
    for ( int i = 0; i < INDY_TOUCH_MAXFINGERS; ++i )
    {
        if ( indyTouch_aFingers[i].bUsed && indyTouch_aFingers[i].id == id )
        {
            return &indyTouch_aFingers[i];
        }
    }
    return NULL;
}

static bool indyTouch_HasRole(IndyTouchRole role)
{
    for ( int i = 0; i < INDY_TOUCH_MAXFINGERS; ++i )
    {
        if ( indyTouch_aFingers[i].bUsed && indyTouch_aFingers[i].role == role )
        {
            return true;
        }
    }
    return false;
}

static void indyTouch_StartStick(IndyTouchFinger* pFinger)
{
    pFinger->role     = INDY_TOUCH_ROLE_STICK;
    indyTouch_stickX  = pFinger->downX;
    indyTouch_stickY  = pFinger->downY;
}

static void indyTouch_FingerDown(IndyTouchFinger* pFinger)
{
    const float w = (float)indyTouch_frame.width;
    IndyTouchButton button;
    if ( indyTouch_bUiMode )
    {
        pFinger->role = INDY_TOUCH_ROLE_UI;
        pFinger->camY = pFinger->y; // scrolling: position already reported
    }
    else if ( indyTouch_HitButton(pFinger->x, pFinger->y, &button) )
    {
        pFinger->role   = INDY_TOUCH_ROLE_BUTTON;
        pFinger->button = button;
        if ( button == INDY_TOUCH_BUTTON_JUMP )
        {
            ++indyTouch_aPendingPresses[SITHCONTROL_JUMP];
        }
        else if ( button == INDY_TOUCH_BUTTON_ACTION )
        {
            ++indyTouch_aPendingPresses[SITHCONTROL_ACT2]; // the game's action key (Ctrl): use, grab, attack
        }
        else if ( button == INDY_TOUCH_BUTTON_HOLSTER )
        {
            indyTouch_Pulse(SITHCONTROL_WEAPONTOGGLE);
        }
    }
    else if ( indyTouch_frame.bMenuOpen )
    {
        pFinger->role  = INDY_TOUCH_ROLE_MENU;
        pFinger->menuX = pFinger->x;
        pFinger->menuY = pFinger->y;
    }
    else if ( indyTouch_frame.bCinematic )
    {
        pFinger->role = INDY_TOUCH_ROLE_NONE;
    }
    else if ( indyTouch_HitHud(pFinger->x, pFinger->y) )
    {
        pFinger->role = INDY_TOUCH_ROLE_HUD; // a tap opens the menu; a drag becomes the stick
    }
    else if ( pFinger->x < w * 0.5f && !indyTouch_HasRole(INDY_TOUCH_ROLE_STICK) )
    {
        indyTouch_StartStick(pFinger);
    }
    else if ( pFinger->x >= w * 0.5f && !indyTouch_HasRole(INDY_TOUCH_ROLE_CAMERA) )
    {
        pFinger->role = INDY_TOUCH_ROLE_CAMERA;
        pFinger->camX = pFinger->x;
        pFinger->camY = pFinger->y;
    }
}

static void indyTouch_FingerMotion(IndyTouchFinger* pFinger)
{
    const float h = (float)indyTouch_frame.height;
    switch ( pFinger->role )
    {
        case INDY_TOUCH_ROLE_HUD:
            if ( hypotf(pFinger->x - pFinger->downX, pFinger->y - pFinger->downY) > INDY_TOUCH_TAP_MOVE * h
                && !indyTouch_HasRole(INDY_TOUCH_ROLE_STICK) )
            {
                indyTouch_StartStick(pFinger);
            }
            break;

        case INDY_TOUCH_ROLE_STICK:
        {
            // past the rim the stick follows the thumb
            const float r  = INDY_TOUCH_STICK_RADIUS * h;
            const float dx = pFinger->x - indyTouch_stickX, dy = pFinger->y - indyTouch_stickY;
            const float d  = hypotf(dx, dy);
            if ( d > r )
            {
                indyTouch_stickX = pFinger->x - dx * (r / d);
                indyTouch_stickY = pFinger->y - dy * (r / d);
            }
        } break;

        case INDY_TOUCH_ROLE_CAMERA:
            // a quick flick down may still become crouching: the camera waits a moment before it follows
            if ( indyTouch_msecNow - pFinger->msecDown >= INDY_TOUCH_CAMERA_WAIT_MSEC
                || fabsf(pFinger->x - pFinger->downX) > fabsf(pFinger->y - pFinger->downY) )
            {
                indyTouch_camYaw   += (pFinger->x - pFinger->camX) / h * INDY_TOUCH_YAW_PER_HEIGHT;
                indyTouch_camPitch += (pFinger->camY - pFinger->y) / h * INDY_TOUCH_PITCH_PER_HEIGHT; // drag up: like stick up
                pFinger->camX = pFinger->x;
                pFinger->camY = pFinger->y;
            }
            break;

        case INDY_TOUCH_ROLE_UI:
            indyTouch_uiScroll += pFinger->y - pFinger->camY;
            pFinger->camY = pFinger->y;
            break;

        case INDY_TOUCH_ROLE_MENU:
        {
            const float step = INDY_TOUCH_MENU_STEP * h;
            const float dx = pFinger->x - pFinger->menuX, dy = pFinger->y - pFinger->menuY;
            if ( fabsf(dx) >= step && fabsf(dx) >= fabsf(dy) )
            {
                indyTouch_Pulse(dx > 0.0f ? SITHCONTROL_TURNRIGHT : SITHCONTROL_TURNLEFT);
                pFinger->menuX += dx > 0.0f ? step : -step;
                pFinger->menuY  = pFinger->y;
                ++pFinger->numMenuSteps;
            }
            else if ( fabsf(dy) >= step )
            {
                indyTouch_Pulse(dy < 0.0f ? SITHCONTROL_FORWARD : SITHCONTROL_BACK);
                pFinger->menuY += dy > 0.0f ? step : -step;
                pFinger->menuX  = pFinger->x;
                ++pFinger->numMenuSteps;
            }
        } break;

        default:
            break;
    }
}

static void indyTouch_FingerUp(IndyTouchFinger* pFinger)
{
    const float h       = (float)indyTouch_frame.height;
    const uint32_t msec = indyTouch_msecNow - pFinger->msecDown;
    const float dx = pFinger->x - pFinger->downX, dy = pFinger->y - pFinger->downY;
    const bool bTap = msec <= INDY_TOUCH_TAP_MSEC && hypotf(dx, dy) <= INDY_TOUCH_TAP_MOVE * h;

    switch ( pFinger->role )
    {
        case INDY_TOUCH_ROLE_BUTTON:
            if ( pFinger->button == INDY_TOUCH_BUTTON_MENU )
            {
                indyTouch_PressKey(DIK_ESCAPE); // opens / closes the HUD menu
            }
            break;

        case INDY_TOUCH_ROLE_HUD:
            if ( bTap )
            {
                indyTouch_PressKey(DIK_ESCAPE);
            }
            break;

        case INDY_TOUCH_ROLE_CAMERA:
            if ( bTap )
            {
                indyTouch_Pulse(SITHCONTROL_ACT2); // attack (the action key fires a drawn weapon)
            }
            else if ( msec <= INDY_TOUCH_FLICK_MSEC && dy >= INDY_TOUCH_FLICK_DIST * h && dy > 2.0f * fabsf(dx) )
            {
                indyTouch_Pulse(SITHCONTROL_CRAWLTOGGLE);
            }
            break;

        case INDY_TOUCH_ROLE_UI:
            if ( msec <= 2u * INDY_TOUCH_TAP_MSEC && hypotf(dx, dy) <= INDY_TOUCH_TAP_MOVE * h )
            {
                indyTouch_bUiTap  = true;
                indyTouch_uiTapX  = pFinger->downX;
                indyTouch_uiTapY  = pFinger->downY;
            }
            break;

        case INDY_TOUCH_ROLE_MENU:
            if ( bTap && pFinger->numMenuSteps == 0 )
            {
                // JonesHud selects the tapped item, or uses it when it is selected already
                indyTouch_bMenuTap  = true;
                indyTouch_menuTapX  = pFinger->downX;
                indyTouch_menuTapY  = pFinger->downY;
            }
            break;

        default:
            break;
    }
    pFinger->bUsed = false;
}

void indyTouch_OnFinger(IndyTouchEvent event, uint64_t fingerId, float x, float y, uint32_t msecTime)
{
    indyTouch_msecNow = msecTime;
    if ( !indyTouch_bFrameValid )
    {
        return; // no game frame yet: no layout
    }

    IndyTouchFinger* pFinger = indyTouch_FindFinger(fingerId);
    const float px = x * (float)indyTouch_frame.width, py = y * (float)indyTouch_frame.height;
    if ( event == INDY_TOUCH_DOWN )
    {
        indyTouch_bVisible = true;
        if ( !pFinger )
        {
            for ( int i = 0; i < INDY_TOUCH_MAXFINGERS && !pFinger; ++i )
            {
                if ( !indyTouch_aFingers[i].bUsed )
                {
                    pFinger = &indyTouch_aFingers[i];
                }
            }
            if ( !pFinger )
            {
                return;
            }
        }
        memset(pFinger, 0, sizeof(*pFinger));
        pFinger->bUsed    = true;
        pFinger->id       = fingerId;
        pFinger->downX    = pFinger->x = px;
        pFinger->downY    = pFinger->y = py;
        pFinger->msecDown = msecTime;
        indyTouch_FingerDown(pFinger);
        return;
    }

    if ( !pFinger )
    {
        return;
    }
    pFinger->x = px;
    pFinger->y = py;
    if ( event == INDY_TOUCH_MOTION )
    {
        indyTouch_FingerMotion(pFinger);
    }
    else
    {
        indyTouch_FingerUp(pFinger);
    }
}

// ---- game side ----------------------------------------------------------------------------------------------------

void indyTouch_BeginControlFrame(void)
{
    memcpy(indyTouch_aFramePresses, indyTouch_aPendingPresses, sizeof(indyTouch_aFramePresses));
    memset(indyTouch_aPendingPresses, 0, sizeof(indyTouch_aPendingPresses));
}

bool indyTouch_GetStick(float* pX, float* pY)
{
    *pX = *pY = 0.0f;
    if ( indyTouch_frame.bMenuOpen )
    {
        return false;
    }
    for ( int i = 0; i < INDY_TOUCH_MAXFINGERS; ++i )
    {
        const IndyTouchFinger* pFinger = &indyTouch_aFingers[i];
        if ( pFinger->bUsed && pFinger->role == INDY_TOUCH_ROLE_STICK )
        {
            const float r = INDY_TOUCH_STICK_RADIUS * (float)indyTouch_frame.height;
            *pX = fminf(fmaxf((pFinger->x - indyTouch_stickX) / r, -1.0f), 1.0f);
            *pY = fminf(fmaxf((indyTouch_stickY - pFinger->y) / r, -1.0f), 1.0f); // up: positive
            return true;
        }
    }
    return false;
}

void indyTouch_SetUiMode(bool bUiMode)
{
    indyTouch_bUiMode  = bUiMode;
    indyTouch_bUiTap   = false;
    indyTouch_uiScroll = 0.0f;
    for ( int i = 0; i < INDY_TOUCH_MAXFINGERS; ++i )
    {
        indyTouch_aFingers[i].bUsed = false; // fingers down now belong to the old mode
    }
}

bool indyTouch_TakeUiTap(float* pX, float* pY)
{
    if ( !indyTouch_bUiTap )
    {
        return false;
    }
    indyTouch_bUiTap = false;
    *pX = indyTouch_uiTapX;
    *pY = indyTouch_uiTapY;
    return true;
}

float indyTouch_TakeUiScroll(void)
{
    const float scroll = indyTouch_uiScroll;
    indyTouch_uiScroll = 0.0f;
    return scroll;
}

bool indyTouch_GetUiPointer(float* pX, float* pY)
{
    for ( int i = 0; i < INDY_TOUCH_MAXFINGERS; ++i )
    {
        if ( indyTouch_aFingers[i].bUsed && indyTouch_aFingers[i].role == INDY_TOUCH_ROLE_UI )
        {
            *pX = indyTouch_aFingers[i].x;
            *pY = indyTouch_aFingers[i].y;
            return true;
        }
    }
    return false;
}

bool indyTouch_TakeMenuTap(float* pX, float* pY)
{
    if ( !indyTouch_bMenuTap )
    {
        return false;
    }
    indyTouch_bMenuTap = false;
    *pX = indyTouch_menuTapX;
    *pY = indyTouch_menuTapY;
    return true;
}

bool indyTouch_TakeCameraDelta(float* pYaw, float* pPitch)
{
    *pYaw              = indyTouch_camYaw;
    *pPitch            = indyTouch_camPitch;
    indyTouch_camYaw   = 0.0f;
    indyTouch_camPitch = 0.0f;
    return indyTouch_HasRole(INDY_TOUCH_ROLE_CAMERA);
}

static bool indyTouch_IsButtonHeld(IndyTouchButton button)
{
    for ( int i = 0; i < INDY_TOUCH_MAXFINGERS; ++i )
    {
        const IndyTouchFinger* pFinger = &indyTouch_aFingers[i];
        if ( pFinger->bUsed && pFinger->role == INDY_TOUCH_ROLE_BUTTON && pFinger->button == button )
        {
            return true;
        }
    }
    return false;
}

bool J3DAPI indyTouch_GetKey(SithControlFunction function, int* pValue, int* pNumPressed)
{
    *pValue      = 0;
    *pNumPressed = 0;
    if ( (unsigned)function >= SITHCONTROL_MAXFUNCTIONS )
    {
        return false;
    }

    int value = (int32_t)(indyTouch_msecNow - indyTouch_aPulseUntil[function]) < 0;
    switch ( function )
    {
        case SITHCONTROL_JUMP:
            value |= indyTouch_IsButtonHeld(INDY_TOUCH_BUTTON_JUMP);
            break;

        case SITHCONTROL_ACT2:
            value |= indyTouch_IsButtonHeld(INDY_TOUCH_BUTTON_ACTION); // held: grab a block and push/pull with the stick
            break;

        case SITHCONTROL_FORWARD:
        case SITHCONTROL_BACK:
        case SITHCONTROL_TURNLEFT:
        case SITHCONTROL_TURNRIGHT:
        {
            // the stick as classic keys where the modern controls don't apply (climbing, swimming, ...)
            float x, y;
            if ( indyTouch_GetStick(&x, &y) )
            {
                value |= (function == SITHCONTROL_FORWARD && y > INDY_TOUCH_STICK_KEY) || (function == SITHCONTROL_BACK && y < -INDY_TOUCH_STICK_KEY)
                    || (function == SITHCONTROL_TURNRIGHT && x > INDY_TOUCH_STICK_KEY) || (function == SITHCONTROL_TURNLEFT && x < -INDY_TOUCH_STICK_KEY);
            }
        } break;

        default:
            break;
    }

    *pValue      = value;
    *pNumPressed = indyTouch_aFramePresses[function];
    return value || *pNumPressed;
}

// ---- overlay ------------------------------------------------------------------------------------------------------

static void indyTouch_DrawButton(IndyTouchButton button)
{
    float x, y, r;
    indyTouch_GetButton(button, &x, &y, &r);
    const bool bHeld   = indyTouch_IsButtonHeld(button);
    const D3DCOLOR fill = D3DCOLOR_ARGB(bHeld ? 90 : 45, 255, 255, 255);
    const D3DCOLOR line = D3DCOLOR_ARGB(140, 255, 255, 255);
    indyDraw_Disc(x, y, r, fill);
    indyDraw_Ring(x, y, r, r * 0.06f, line);

    switch ( button )
    {
        case INDY_TOUCH_BUTTON_JUMP: // up arrow
            indyDraw_Triangle(x, y - r * 0.45f, x + r * 0.42f, y + r * 0.25f, x - r * 0.42f, y + r * 0.25f, line);
            break;

        case INDY_TOUCH_BUTTON_ACTION: // a dot: "use"
            indyDraw_Disc(x, y, r * 0.28f, line);
            break;

        case INDY_TOUCH_BUTTON_HOLSTER: // down arrow: put away
            indyDraw_Triangle(x - r * 0.42f, y - r * 0.22f, x + r * 0.42f, y - r * 0.22f, x, y + r * 0.42f, line);
            break;

        default: // menu: three bars
            for ( int i = -1; i <= 1; ++i )
            {
                indyDraw_Rect(x - r * 0.45f, y + (float)i * r * 0.32f - r * 0.07f, x + r * 0.45f, y + (float)i * r * 0.32f + r * 0.07f, line);
            }
            break;
    }
}

void indyTouch_Frame(const IndyTouchFrame* pFrame)
{
    indyTouch_frame       = *pFrame;
    indyTouch_bFrameValid = pFrame->width && pFrame->height;

    static int bForceVisible = -1; // tests: INDY_TOUCH_OVERLAY=1 shows the overlay without a touch screen
    if ( bForceVisible < 0 )
    {
        bForceVisible = getenv("INDY_TOUCH_OVERLAY") != NULL;
    }
    if ( !indyTouch_bFrameValid || indyTouch_bUiMode || (!indyTouch_bVisible && !bForceVisible) )
    {
        return;
    }

    indyDraw_Begin();
    const float h = (float)pFrame->height;

    indyTouch_DrawButton(INDY_TOUCH_BUTTON_MENU);
    if ( !pFrame->bMenuOpen && !pFrame->bCinematic )
    {
        indyTouch_DrawButton(INDY_TOUCH_BUTTON_ACTION);
        indyTouch_DrawButton(INDY_TOUCH_BUTTON_JUMP);
        if ( pFrame->bHolding )
        {
            indyTouch_DrawButton(INDY_TOUCH_BUTTON_HOLSTER);
        }

        // the stick where it is held, else a faint hint where it usually is
        const float r = INDY_TOUCH_STICK_RADIUS * h;
        float x, y;
        if ( indyTouch_GetStick(&x, &y) )
        {
            indyDraw_Disc(indyTouch_stickX, indyTouch_stickY, r, D3DCOLOR_ARGB(40, 255, 255, 255));
            indyDraw_Ring(indyTouch_stickX, indyTouch_stickY, r, r * 0.05f, D3DCOLOR_ARGB(110, 255, 255, 255));
            indyDraw_Disc(indyTouch_stickX + x * r, indyTouch_stickY - y * r, r * 0.42f, D3DCOLOR_ARGB(120, 255, 255, 255));
        }
        else
        {
            indyDraw_Ring(0.30f * h, h - 0.30f * h, r, r * 0.04f, D3DCOLOR_ARGB(50, 255, 255, 255));
        }
    }

    indyDraw_End();
}
