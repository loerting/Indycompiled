// Native builds: the game window and main loop on SDL3. SDL events are translated into the Win32 messages the
// engine's window procedures handle (WM_KEYDOWN, WM_ACTIVATEAPP, WM_PAINT, WM_TIMER ...), so the game code stays the
// same on every platform. Also provides the small window API those procedures call (j3dcore/j3dwin32.h).
#include "wkernel.h"
#include <std/Win95/stdWin95.h>

#include <SDL3/SDL.h>
#include <dinput.h>

#include <indy/indyTouch.h> // INDY: touch controls

#define WKERNEL_MAXTIMERS   8
#define WKERNEL_MAXMESSAGES 64

typedef struct sWKernelTimer
{
    UINT_PTR id;
    UINT interval;
    uint64_t nextTick;
} WKernelTimer;

typedef struct sWKernelMessage
{
    UINT msg;
    WPARAM wParam;
    LPARAM lParam;
} WKernelMessage;

static SDL_Window* wkernel_pWindow = NULL;
static bool wkernel_bQuit          = false;
static int wkernel_numCursorShows  = 0;

static WKERNELPROC wkernel_pfProcess            = NULL;
static WKERNELSTARTUPPROC wkernel_pfOnStartup   = NULL;
static WKERNELSHUTDOWNPROC wkernel_pfOnShutdown = NULL;
static WKERNELWNDPROC wkernel_pfWndProc         = NULL;

static WKernelTimer wkernel_aTimers[WKERNEL_MAXTIMERS];
static WKernelMessage wkernel_aMessages[WKERNEL_MAXMESSAGES];
static size_t wkernel_numMessages = 0;

void wkernel_InstallHooks(void) {}
void wkernel_ResetGlobals(void) {}

// As wkernel.c's main window procedure
static void wkernel_Dispatch(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if ( msg == WM_DESTROY || msg == WM_QUIT )
    {
        wkernel_bQuit = true;
        return;
    }

    if ( msg == WM_CLOSE )
    {
        if ( wkernel_pfOnShutdown )
        {
            wkernel_pfOnShutdown();
        }
        wkernel_bQuit = true; // DefWindowProc destroys the window
        return;
    }

    int retValue;
    if ( wkernel_pfWndProc )
    {
        wkernel_pfWndProc((HWND)wkernel_pWindow, msg, wParam, lParam, &retValue);
    }
}

static WPARAM wkernel_GetVirtualKey(SDL_Keycode key)
{
    if ( key >= 'a' && key <= 'z' ) return (WPARAM)(key - 'a' + 'A');
    if ( key >= '0' && key <= '9' ) return (WPARAM)key;
    if ( key >= SDLK_F1 && key <= SDLK_F12 ) return (WPARAM)(VK_F1 + (key - SDLK_F1));
    switch ( key )
    {
        case SDLK_ESCAPE:    return VK_ESCAPE;
        case SDLK_AC_BACK:   return VK_ESCAPE; // Android Back
        case SDLK_RETURN:    return VK_RETURN;
        case SDLK_KP_ENTER:  return VK_RETURN;
        case SDLK_SPACE:     return VK_SPACE;
        case SDLK_BACKSPACE: return VK_BACK;
        case SDLK_TAB:       return VK_TAB;
        case SDLK_UP:        return VK_UP;
        case SDLK_DOWN:      return VK_DOWN;
        case SDLK_LEFT:      return VK_LEFT;
        case SDLK_RIGHT:     return VK_RIGHT;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:    return VK_SHIFT;
        case SDLK_LCTRL:
        case SDLK_RCTRL:     return VK_CONTROL;
        case SDLK_LALT:
        case SDLK_RALT:      return VK_MENU;
        case SDLK_PAUSE:     return VK_PAUSE;
        default:             return 0;
    }
}

static void wkernel_HandleEvent(const SDL_Event* pEvent)
{
    switch ( pEvent->type )
    {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            wkernel_Dispatch(WM_CLOSE, 0, 0);
            break;

        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
        {
            const IndyTouchEvent event = pEvent->type == SDL_EVENT_FINGER_DOWN ? INDY_TOUCH_DOWN
                : (pEvent->type == SDL_EVENT_FINGER_MOTION ? INDY_TOUCH_MOTION : INDY_TOUCH_UP);
            indyTouch_OnFinger(event, (uint64_t)pEvent->tfinger.fingerID, pEvent->tfinger.x, pEvent->tfinger.y, (uint32_t)SDL_GetTicks());
        } break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            indyTouch_OnOtherInput(); // the touch overlay hides while a gamepad is used
            break;

        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if ( pEvent->gaxis.value > 16000 || pEvent->gaxis.value < -16000 )
            {
                indyTouch_OnOtherInput();
            }
            break;

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        {
            WPARAM vk = wkernel_GetVirtualKey(pEvent->key.key);
            if ( pEvent->type == SDL_EVENT_KEY_DOWN && vk && pEvent->key.key != SDLK_AC_BACK )
            {
                indyTouch_OnOtherInput(); // a keyboard (not the phone's Back or volume buttons)
            }
            if ( !vk )
            {
                break;
            }

            // lParam as Win32: repeat count, scan code, KF_REPEAT (previous key state), KF_UP (transition state)
            LPARAM flags = 1 | ((LPARAM)(pEvent->key.scancode & 0xFF) << 16);
            if ( pEvent->type == SDL_EVENT_KEY_DOWN )
            {
                if ( pEvent->key.repeat ) flags |= (LPARAM)KF_REPEAT << 16;
                wkernel_Dispatch(WM_KEYDOWN, vk, flags);
                if ( vk == VK_RETURN || vk == VK_ESCAPE || vk == VK_BACK || vk == VK_SPACE || vk == VK_TAB )
                {
                    wkernel_Dispatch(WM_CHAR, vk, flags);
                }
            }
            else
            {
                flags |= ((LPARAM)KF_REPEAT | KF_UP) << 16;
                wkernel_Dispatch(WM_KEYUP, vk, flags);
            }
        } break;

        case SDL_EVENT_TEXT_INPUT:
            for ( const char* p = pEvent->text.text; *p; ++p )
            {
                if ( (unsigned char)*p >= 0x20 && (unsigned char)*p < 0x80 && *p != ' ' )
                {
                    wkernel_Dispatch(WM_CHAR, (WPARAM)(unsigned char)*p, 1);
                }
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            LPARAM pos = MAKELONG((int)pEvent->button.x, (int)pEvent->button.y);
            if ( pEvent->button.button == SDL_BUTTON_LEFT ) wkernel_Dispatch(WM_LBUTTONUP, 0, pos);
            else if ( pEvent->button.button == SDL_BUTTON_RIGHT ) wkernel_Dispatch(WM_RBUTTONUP, 0, pos);
            else if ( pEvent->button.button == SDL_BUTTON_MIDDLE ) wkernel_Dispatch(WM_MBUTTONUP, 0, pos);
        } break;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            wkernel_Dispatch(WM_ACTIVATEAPP, 1, 0);
            wkernel_Dispatch(WM_ACTIVATE, WA_ACTIVE, 0);
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            wkernel_Dispatch(WM_ACTIVATE, WA_INACTIVE, 0);
            wkernel_Dispatch(WM_ACTIVATEAPP, 0, 0);
            break;

        case SDL_EVENT_WINDOW_EXPOSED:
            wkernel_Dispatch(WM_PAINT, 0, 0);
            break;

        default:
            break;
    }
}

static void wkernel_ProcessTimersAndMessages(void)
{
    // posted messages first, in order; handlers may post new ones
    WKernelMessage aPending[WKERNEL_MAXMESSAGES];
    size_t numPending = wkernel_numMessages;
    memcpy(aPending, wkernel_aMessages, numPending * sizeof(WKernelMessage));
    wkernel_numMessages = 0;
    for ( size_t i = 0; i < numPending; ++i )
    {
        wkernel_Dispatch(aPending[i].msg, aPending[i].wParam, aPending[i].lParam);
    }

    uint64_t now = SDL_GetTicks();
    for ( size_t i = 0; i < WKERNEL_MAXTIMERS; ++i )
    {
        WKernelTimer* pTimer = &wkernel_aTimers[i];
        if ( pTimer->id && now >= pTimer->nextTick )
        {
            pTimer->nextTick = now + pTimer->interval;
            wkernel_Dispatch(WM_TIMER, pTimer->id, 0);
        }
    }
}

int J3DAPI wkernel_Run(HINSTANCE hinstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd, LPCSTR lpWindowName)
{
    J3D_UNUSED(hPrevInstance);
    J3D_UNUSED(nShowCmd);

    if ( wkernel_pfProcess == NULL )
    {
        fprintf(stderr, "ERROR: wkernel_Run: No main process set!\n");
        return -1;
    }

    // INDY: touch controls get the finger events; no mouse clicks or motion made up from them (they would turn Indy)
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1"); // Android Back: Escape (menu), not leaving the app
    indyTouch_SetKeyFunc(DInputSDL_SetVirtualKey);

    if ( !SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD) )
    {
        fprintf(stderr, "ERROR: wkernel_Run: SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    // The display module sets the size and mode and creates the GL context: OpenGL ES 3.0 with depth and stencil,
    // chosen here because the window's pixel format depends on it
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    wkernel_pWindow = SDL_CreateWindow(lpWindowName, 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if ( !wkernel_pWindow )
    {
        fprintf(stderr, "ERROR: wkernel_Run: SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    stdWin95_SetWindow((HWND)wkernel_pWindow);
    stdWin95_SetInstance(hinstance);

    if ( wkernel_pfOnStartup && wkernel_pfOnStartup(lpCmdLine) )
    {
        SDL_DestroyWindow(wkernel_pWindow);
        SDL_Quit();
        return -1;
    }

    int result = 0;
    while ( result != -1 )
    {
        if ( result == 1 ) // Finish
        {
            break;
        }

        result = wkernel_pfProcess();
    }

    SDL_DestroyWindow(wkernel_pWindow);
    wkernel_pWindow = NULL;
    stdWin95_SetWindow(NULL);
    SDL_Quit();
    return result == 1 ? 0 : result;
}

WKERNELPROC J3DAPI wkernel_SetProcessProc(WKERNELPROC pfProc)
{
    WKERNELPROC pfCurProc = wkernel_pfProcess;
    wkernel_pfProcess     = pfProc;
    return pfCurProc;
}

WKERNELSTARTUPPROC J3DAPI wkernel_SetStartupCallback(WKERNELSTARTUPPROC pfOnStartup)
{
    WKERNELSTARTUPPROC pfCurProc = wkernel_pfOnStartup;
    wkernel_pfOnStartup          = pfOnStartup;
    return pfCurProc;
}

WKERNELSHUTDOWNPROC J3DAPI wkernel_SetShutdownCallback(WKERNELSHUTDOWNPROC pfOnShutdown)
{
    WKERNELSHUTDOWNPROC pfCurProc = wkernel_pfOnShutdown;
    wkernel_pfOnShutdown          = pfOnShutdown;
    return pfCurProc;
}

// Returns 1 once the window was closed, else 0 (no blocking)
int wkernel_PeekProcessEvents(void)
{
    SDL_Event event;
    while ( !wkernel_bQuit && SDL_PollEvent(&event) )
    {
        wkernel_HandleEvent(&event);
    }
    indyTouch_Update((uint32_t)SDL_GetTicks()); // INDY: releases the touch controls' key pulses

    if ( !wkernel_bQuit )
    {
        wkernel_ProcessTimersAndMessages();
    }
    return wkernel_bQuit ? 1 : 0;
}

// Waits for at least one event (or a timer), then processes everything pending; returns 1 once the window was closed
int wkernel_ProcessEvents(void)
{
    if ( !wkernel_bQuit && wkernel_numMessages == 0 )
    {
        SDL_Event event;
        if ( SDL_WaitEventTimeout(&event, 50) )
        {
            wkernel_HandleEvent(&event);
        }
    }
    return wkernel_PeekProcessEvents();
}

void J3DAPI wkernel_SetWindowStyle(LONG dwNewLong)
{
    J3D_UNUSED(dwNewLong); // the display module decides between window and fullscreen
}

BOOL J3DAPI wkernel_SetWindowSize(int width, int height)
{
    return wkernel_pWindow && SDL_SetWindowSize(wkernel_pWindow, width, height);
}

void J3DAPI wkernel_SetWindowProc(WKERNELWNDPROC pfProc)
{
    wkernel_pfWndProc = pfProc;
}

// ---- window API used by the engine's window procedures (j3dcore/j3dwin32.h) ----------------------------------

BOOL J3D_GetWindowRect(HWND hwnd, LPRECT pRect)
{
    int x = 0, y = 0, w = 0, h = 0;
    if ( !hwnd || !pRect ) return FALSE;
    SDL_GetWindowPosition((SDL_Window*)hwnd, &x, &y);
    SDL_GetWindowSize((SDL_Window*)hwnd, &w, &h);
    pRect->left   = x;
    pRect->top    = y;
    pRect->right  = x + w;
    pRect->bottom = y + h;
    return TRUE;
}

BOOL J3D_GetClientRect(HWND hwnd, LPRECT pRect)
{
    int w = 0, h = 0;
    if ( !hwnd || !pRect ) return FALSE;
    SDL_GetWindowSize((SDL_Window*)hwnd, &w, &h);
    pRect->left   = 0;
    pRect->top    = 0;
    pRect->right  = w;
    pRect->bottom = h;
    return TRUE;
}

int J3D_GetSystemMetrics(int index)
{
    J3D_UNUSED(index); // window frame sizes: SDL windows are sized by their client area
    return 0;
}

HDC J3D_BeginPaint(HWND hwnd, LPPAINTSTRUCT pPaint)
{
    J3D_UNUSED(hwnd);
    if ( pPaint ) memset(pPaint, 0, sizeof(PAINTSTRUCT));
    return NULL;
}

BOOL J3D_EndPaint(HWND hwnd, const PAINTSTRUCT* pPaint)
{
    J3D_UNUSED(hwnd);
    J3D_UNUSED(pPaint);
    return TRUE;
}

BOOL J3D_ClientToScreen(HWND hwnd, LPPOINT pPoint)
{
    int x = 0, y = 0;
    if ( !hwnd || !pPoint ) return FALSE;
    SDL_GetWindowPosition((SDL_Window*)hwnd, &x, &y);
    pPoint->x += x;
    pPoint->y += y;
    return TRUE;
}

UINT_PTR J3D_SetTimer(HWND hwnd, UINT_PTR id, UINT elapse, TIMERPROC pfTimer)
{
    J3D_UNUSED(hwnd);
    J3D_UNUSED(pfTimer); // the engine only uses WM_TIMER
    WKernelTimer* pFree = NULL;
    for ( size_t i = 0; i < WKERNEL_MAXTIMERS; ++i )
    {
        if ( wkernel_aTimers[i].id == id ) { pFree = &wkernel_aTimers[i]; break; }
        if ( !wkernel_aTimers[i].id && !pFree ) pFree = &wkernel_aTimers[i];
    }
    if ( !pFree || !id ) return 0;
    pFree->id       = id;
    pFree->interval = elapse;
    pFree->nextTick = SDL_GetTicks() + elapse;
    return id;
}

BOOL J3D_KillTimer(HWND hwnd, UINT_PTR id)
{
    J3D_UNUSED(hwnd);
    for ( size_t i = 0; i < WKERNEL_MAXTIMERS; ++i )
    {
        if ( wkernel_aTimers[i].id == id )
        {
            wkernel_aTimers[i].id = 0;
            return TRUE;
        }
    }
    return FALSE;
}

LRESULT J3D_DefWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    J3D_UNUSED(hwnd);
    J3D_UNUSED(wParam);
    J3D_UNUSED(lParam);
    if ( msg == WM_CLOSE ) wkernel_bQuit = true;
    return 0;
}

BOOL J3D_PostMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    J3D_UNUSED(hwnd);
    if ( wkernel_numMessages >= WKERNEL_MAXMESSAGES ) return FALSE;
    wkernel_aMessages[wkernel_numMessages++] = (WKernelMessage){ msg, wParam, lParam };
    return TRUE;
}

void J3D_PostQuitMessage(int exitCode)
{
    J3D_UNUSED(exitCode);
    wkernel_bQuit = true;
}

BOOL J3D_ShowWindow(HWND hwnd, int cmd)
{
    SDL_Window* pWindow = (SDL_Window*)hwnd;
    if ( !pWindow ) return FALSE;
    if ( cmd == SW_HIDE ) SDL_HideWindow(pWindow);
    else if ( cmd == SW_MINIMIZE ) SDL_MinimizeWindow(pWindow);
    else SDL_ShowWindow(pWindow);
    return TRUE;
}

int J3D_ShowCursor(BOOL bShow)
{
    wkernel_numCursorShows += bShow ? 1 : -1;
    if ( wkernel_numCursorShows >= 0 ) SDL_ShowCursor();
    else SDL_HideCursor();
    return wkernel_numCursorShows;
}

int J3D_MessageBox(HWND hwnd, LPCSTR pText, LPCSTR pCaption, UINT type)
{
    J3D_UNUSED(type);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, pCaption ? pCaption : "", pText ? pText : "", (SDL_Window*)hwnd);
    return 1; // IDOK
}
