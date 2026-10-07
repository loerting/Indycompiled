// Native builds: the DirectInput 8 subset of cmake/compat/native/dinput.h on SDL3, for the control module
// (stdControlDX9.c). Devices: the system keyboard (256-byte DIK state) and mouse (relative motion, wheel, four buttons,
// buffered button events). No other DirectInput devices: gamepads are read through XInput (stdXInputSDL.c).
#include <j3dcore/j3d.h>
#include <dinput.h>

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

const DIDATAFORMAT c_dfDIKeyboard = { sizeof(DIDATAFORMAT) };
const DIDATAFORMAT c_dfDIMouse    = { sizeof(DIDATAFORMAT) };
const DIDATAFORMAT c_dfDIJoystick = { sizeof(DIDATAFORMAT) };
const GUID GUID_SysKeyboard = { 0x6F1D2B61, 0xD5A0, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
const GUID GUID_SysMouse    = { 0x6F1D2B60, 0xD5A0, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
const GUID IID_IDirectInput8 = { 0xBF798031, 0x483A, 0x4DA2, { 0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00 } };

#define DINPUTSDL_MOUSEBUFFER 64

typedef enum eDInputSDLDeviceType
{
    DINPUTSDL_KEYBOARD,
    DINPUTSDL_MOUSE,
} DInputSDLDeviceType;

struct IDirectInput8
{
    int refCount;
};

struct IDirectInputDevice8
{
    DInputSDLDeviceType type;
    bool bAcquired;
    bool bExclusive;
};

// Mouse wheel and button events, collected by an event watch so none are lost between two reads
static SDL_Mutex* DInputSDL_pMouseLock;
static int DInputSDL_wheelDelta;
static DIDEVICEOBJECTDATA DInputSDL_aMouseEvents[DINPUTSDL_MOUSEBUFFER];
static DWORD DInputSDL_numMouseEvents;
static DWORD DInputSDL_sequence;
static bool DInputSDL_bWatching;

static uint8_t DInputSDL_aScancodeToDik[SDL_SCANCODE_COUNT];
static bool DInputSDL_aVirtualKeys[256]; // DIK codes held by software (touch controls: Enter, Escape)

void DInputSDL_SetVirtualKey(uint8_t dik, bool bDown)
{
    DInputSDL_aVirtualKeys[dik] = bDown;
}

static void DInputSDL_InitKeyMap(void)
{
    static const struct { SDL_Scancode sc; uint8_t dik; } aMap[] = {
        { SDL_SCANCODE_ESCAPE, DIK_ESCAPE }, { SDL_SCANCODE_AC_BACK, DIK_ESCAPE } /* Android Back */, { SDL_SCANCODE_1, DIK_1 }, { SDL_SCANCODE_2, DIK_2 }, { SDL_SCANCODE_3, DIK_3 },
        { SDL_SCANCODE_4, DIK_4 }, { SDL_SCANCODE_5, DIK_5 }, { SDL_SCANCODE_6, DIK_6 }, { SDL_SCANCODE_7, DIK_7 },
        { SDL_SCANCODE_8, DIK_8 }, { SDL_SCANCODE_9, DIK_9 }, { SDL_SCANCODE_0, DIK_0 }, { SDL_SCANCODE_MINUS, DIK_MINUS },
        { SDL_SCANCODE_EQUALS, DIK_EQUALS }, { SDL_SCANCODE_BACKSPACE, DIK_BACK }, { SDL_SCANCODE_TAB, DIK_TAB },
        { SDL_SCANCODE_Q, DIK_Q }, { SDL_SCANCODE_W, DIK_W }, { SDL_SCANCODE_E, DIK_E }, { SDL_SCANCODE_R, DIK_R },
        { SDL_SCANCODE_T, DIK_T }, { SDL_SCANCODE_Y, DIK_Y }, { SDL_SCANCODE_U, DIK_U }, { SDL_SCANCODE_I, DIK_I },
        { SDL_SCANCODE_O, DIK_O }, { SDL_SCANCODE_P, DIK_P }, { SDL_SCANCODE_LEFTBRACKET, DIK_LBRACKET },
        { SDL_SCANCODE_RIGHTBRACKET, DIK_RBRACKET }, { SDL_SCANCODE_RETURN, DIK_RETURN }, { SDL_SCANCODE_LCTRL, DIK_LCONTROL },
        { SDL_SCANCODE_A, DIK_A }, { SDL_SCANCODE_S, DIK_S }, { SDL_SCANCODE_D, DIK_D }, { SDL_SCANCODE_F, DIK_F },
        { SDL_SCANCODE_G, DIK_G }, { SDL_SCANCODE_H, DIK_H }, { SDL_SCANCODE_J, DIK_J }, { SDL_SCANCODE_K, DIK_K },
        { SDL_SCANCODE_L, DIK_L }, { SDL_SCANCODE_SEMICOLON, DIK_SEMICOLON }, { SDL_SCANCODE_APOSTROPHE, DIK_APOSTROPHE },
        { SDL_SCANCODE_GRAVE, DIK_GRAVE }, { SDL_SCANCODE_LSHIFT, DIK_LSHIFT }, { SDL_SCANCODE_BACKSLASH, DIK_BACKSLASH },
        { SDL_SCANCODE_Z, DIK_Z }, { SDL_SCANCODE_X, DIK_X }, { SDL_SCANCODE_C, DIK_C }, { SDL_SCANCODE_V, DIK_V },
        { SDL_SCANCODE_B, DIK_B }, { SDL_SCANCODE_N, DIK_N }, { SDL_SCANCODE_M, DIK_M }, { SDL_SCANCODE_COMMA, DIK_COMMA },
        { SDL_SCANCODE_PERIOD, DIK_PERIOD }, { SDL_SCANCODE_SLASH, DIK_SLASH }, { SDL_SCANCODE_RSHIFT, DIK_RSHIFT },
        { SDL_SCANCODE_KP_MULTIPLY, DIK_MULTIPLY }, { SDL_SCANCODE_LALT, DIK_LMENU }, { SDL_SCANCODE_SPACE, DIK_SPACE },
        { SDL_SCANCODE_CAPSLOCK, DIK_CAPITAL }, { SDL_SCANCODE_F1, DIK_F1 }, { SDL_SCANCODE_F2, DIK_F2 },
        { SDL_SCANCODE_F3, DIK_F3 }, { SDL_SCANCODE_F4, DIK_F4 }, { SDL_SCANCODE_F5, DIK_F5 }, { SDL_SCANCODE_F6, DIK_F6 },
        { SDL_SCANCODE_F7, DIK_F7 }, { SDL_SCANCODE_F8, DIK_F8 }, { SDL_SCANCODE_F9, DIK_F9 }, { SDL_SCANCODE_F10, DIK_F10 },
        { SDL_SCANCODE_NUMLOCKCLEAR, DIK_NUMLOCK }, { SDL_SCANCODE_SCROLLLOCK, DIK_SCROLL }, { SDL_SCANCODE_KP_7, DIK_NUMPAD7 },
        { SDL_SCANCODE_KP_8, DIK_NUMPAD8 }, { SDL_SCANCODE_KP_9, DIK_NUMPAD9 }, { SDL_SCANCODE_KP_MINUS, DIK_SUBTRACT },
        { SDL_SCANCODE_KP_4, DIK_NUMPAD4 }, { SDL_SCANCODE_KP_5, DIK_NUMPAD5 }, { SDL_SCANCODE_KP_6, DIK_NUMPAD6 },
        { SDL_SCANCODE_KP_PLUS, DIK_ADD }, { SDL_SCANCODE_KP_1, DIK_NUMPAD1 }, { SDL_SCANCODE_KP_2, DIK_NUMPAD2 },
        { SDL_SCANCODE_KP_3, DIK_NUMPAD3 }, { SDL_SCANCODE_KP_0, DIK_NUMPAD0 }, { SDL_SCANCODE_KP_PERIOD, DIK_DECIMAL },
        { SDL_SCANCODE_NONUSBACKSLASH, DIK_OEM_102 }, { SDL_SCANCODE_F11, DIK_F11 }, { SDL_SCANCODE_F12, DIK_F12 },
        { SDL_SCANCODE_F13, DIK_F13 }, { SDL_SCANCODE_F14, DIK_F14 }, { SDL_SCANCODE_F15, DIK_F15 },
        { SDL_SCANCODE_KP_EQUALS, DIK_NUMPADEQUALS }, { SDL_SCANCODE_KP_ENTER, DIK_NUMPADENTER },
        { SDL_SCANCODE_RCTRL, DIK_RCONTROL }, { SDL_SCANCODE_MUTE, DIK_MUTE }, { SDL_SCANCODE_VOLUMEDOWN, DIK_VOLUMEDOWN },
        { SDL_SCANCODE_VOLUMEUP, DIK_VOLUMEUP }, { SDL_SCANCODE_KP_COMMA, DIK_NUMPADCOMMA },
        { SDL_SCANCODE_KP_DIVIDE, DIK_DIVIDE }, { SDL_SCANCODE_PRINTSCREEN, DIK_SYSRQ }, { SDL_SCANCODE_RALT, DIK_RMENU },
        { SDL_SCANCODE_PAUSE, DIK_PAUSE }, { SDL_SCANCODE_HOME, DIK_HOME }, { SDL_SCANCODE_UP, DIK_UP },
        { SDL_SCANCODE_PAGEUP, DIK_PRIOR }, { SDL_SCANCODE_LEFT, DIK_LEFT }, { SDL_SCANCODE_RIGHT, DIK_RIGHT },
        { SDL_SCANCODE_END, DIK_END }, { SDL_SCANCODE_DOWN, DIK_DOWN }, { SDL_SCANCODE_PAGEDOWN, DIK_NEXT },
        { SDL_SCANCODE_INSERT, DIK_INSERT }, { SDL_SCANCODE_DELETE, DIK_DELETE }, { SDL_SCANCODE_LGUI, DIK_LWIN },
        { SDL_SCANCODE_RGUI, DIK_RWIN }, { SDL_SCANCODE_APPLICATION, DIK_APPS },
    };
    memset(DInputSDL_aScancodeToDik, 0, sizeof(DInputSDL_aScancodeToDik));
    for ( size_t i = 0; i < SDL_arraysize(aMap); ++i )
    {
        DInputSDL_aScancodeToDik[aMap[i].sc] = aMap[i].dik;
    }
}

static void DInputSDL_AddMouseEvent(DWORD ofs, DWORD data, DWORD timestamp)
{
    if ( DInputSDL_numMouseEvents >= DINPUTSDL_MOUSEBUFFER ) return;
    DIDEVICEOBJECTDATA* pEvent = &DInputSDL_aMouseEvents[DInputSDL_numMouseEvents++];
    pEvent->dwOfs       = ofs;
    pEvent->dwData      = data;
    pEvent->dwTimeStamp = timestamp;
    pEvent->dwSequence  = ++DInputSDL_sequence;
    pEvent->uAppData    = 0;
}

static bool SDLCALL DInputSDL_EventWatch(void* pUserData, SDL_Event* pEvent)
{
    J3D_UNUSED(pUserData);
    if ( pEvent->type == SDL_EVENT_MOUSE_WHEEL )
    {
        SDL_LockMutex(DInputSDL_pMouseLock);
        DInputSDL_wheelDelta += (int)(pEvent->wheel.y * 120.0f); // WHEEL_DELTA per notch
        SDL_UnlockMutex(DInputSDL_pMouseLock);
    }
    else if ( pEvent->type == SDL_EVENT_MOUSE_BUTTON_DOWN || pEvent->type == SDL_EVENT_MOUSE_BUTTON_UP )
    {
        static const DWORD aButtonOfs[] = { 0, DIMOFS_BUTTON0, DIMOFS_BUTTON2, DIMOFS_BUTTON1, DIMOFS_BUTTON3 }; // SDL: left, middle, right, x1
        if ( pEvent->button.button >= 1 && pEvent->button.button <= 4 )
        {
            SDL_LockMutex(DInputSDL_pMouseLock);
            DInputSDL_AddMouseEvent(aButtonOfs[pEvent->button.button], pEvent->button.down ? 0x80 : 0, (DWORD)(pEvent->button.timestamp / 1000000u));
            SDL_UnlockMutex(DInputSDL_pMouseLock);
        }
    }
    return true;
}

HRESULT DirectInput8Create(HINSTANCE hinst, DWORD version, REFIID riid, void* ppOut, void* pUnkOuter)
{
    J3D_UNUSED(hinst);
    J3D_UNUSED(version);
    J3D_UNUSED(riid);
    J3D_UNUSED(pUnkOuter);
    IDirectInput8* pDI = calloc(1, sizeof(IDirectInput8));
    if ( !pDI ) return DIERR_OUTOFMEMORY;
    pDI->refCount = 1;

    DInputSDL_InitKeyMap();
    if ( !DInputSDL_bWatching )
    {
        DInputSDL_pMouseLock = SDL_CreateMutex();
        SDL_AddEventWatch(DInputSDL_EventWatch, NULL);
        DInputSDL_bWatching = true;
    }

    *(LPDIRECTINPUT8*)ppOut = pDI;
    return DI_OK;
}

HRESULT DInputSDL_Release(LPDIRECTINPUT8 pDI)
{
    if ( pDI && --pDI->refCount == 0 )
    {
        if ( DInputSDL_bWatching )
        {
            SDL_RemoveEventWatch(DInputSDL_EventWatch, NULL);
            SDL_DestroyMutex(DInputSDL_pMouseLock);
            DInputSDL_pMouseLock = NULL;
            DInputSDL_bWatching  = false;
        }
        free(pDI);
    }
    return DI_OK;
}

HRESULT DInputSDL_EnumDevices(LPDIRECTINPUT8 pDI, DWORD devType, LPDIENUMDEVICESCALLBACK pfCallback, LPVOID pRef, DWORD flags)
{
    J3D_UNUSED(pDI); J3D_UNUSED(devType); J3D_UNUSED(pfCallback); J3D_UNUSED(pRef); J3D_UNUSED(flags);
    return DI_OK; // no joysticks
}

HRESULT DInputSDL_CreateDevice(LPDIRECTINPUT8 pDI, REFGUID guid, LPDIRECTINPUTDEVICE8* ppDevice, void* pUnkOuter)
{
    J3D_UNUSED(pDI);
    J3D_UNUSED(pUnkOuter);
    *ppDevice = NULL;
    DInputSDLDeviceType type;
    if ( guid == &GUID_SysKeyboard ) type = DINPUTSDL_KEYBOARD;
    else if ( guid == &GUID_SysMouse ) type = DINPUTSDL_MOUSE;
    else return DIERR_DEVICENOTREG;

    IDirectInputDevice8* pDev = calloc(1, sizeof(IDirectInputDevice8));
    if ( !pDev ) return DIERR_OUTOFMEMORY;
    pDev->type = type;
    *ppDevice  = pDev;
    return DI_OK;
}

HRESULT DInputSDL_DeviceRelease(LPDIRECTINPUTDEVICE8 pDev)
{
    if ( pDev )
    {
        DInputSDL_DeviceUnacquire(pDev);
        free(pDev);
    }
    return DI_OK;
}

HRESULT DInputSDL_DeviceGetCapabilities(LPDIRECTINPUTDEVICE8 pDev, LPDIDEVCAPS pCaps)
{
    DWORD size = pCaps->dwSize;
    memset(pCaps, 0, sizeof(DIDEVCAPS));
    pCaps->dwSize = size;
    pCaps->dwFlags = 1; // DIDC_ATTACHED
    if ( pDev->type == DINPUTSDL_KEYBOARD )
    {
        pCaps->dwDevType = DI8DEVTYPE_KEYBOARD;
        pCaps->dwButtons = 128;
    }
    else
    {
        pCaps->dwDevType = DI8DEVTYPE_MOUSE;
        pCaps->dwAxes    = 3;
        pCaps->dwButtons = 4;
    }
    return DI_OK;
}

HRESULT DInputSDL_DeviceSetDataFormat(LPDIRECTINPUTDEVICE8 pDev, const DIDATAFORMAT* pFormat)
{
    J3D_UNUSED(pDev);
    J3D_UNUSED(pFormat);
    return DI_OK;
}

HRESULT DInputSDL_DeviceSetCooperativeLevel(LPDIRECTINPUTDEVICE8 pDev, HWND hwnd, DWORD flags)
{
    J3D_UNUSED(hwnd);
    pDev->bExclusive = (flags & DISCL_EXCLUSIVE) != 0;
    return DI_OK;
}

HRESULT DInputSDL_DeviceSetProperty(LPDIRECTINPUTDEVICE8 pDev, REFGUID prop, LPCDIPROPHEADER pHeader)
{
    J3D_UNUSED(pDev);
    J3D_UNUSED(pHeader);
    return prop == DIPROP_BUFFERSIZE ? DI_OK : DIERR_UNSUPPORTED;
}

HRESULT DInputSDL_DeviceGetProperty(LPDIRECTINPUTDEVICE8 pDev, REFGUID prop, LPDIPROPHEADER pHeader)
{
    J3D_UNUSED(pDev);
    J3D_UNUSED(prop);
    J3D_UNUSED(pHeader);
    return DIERR_UNSUPPORTED;
}

HRESULT DInputSDL_DeviceAcquire(LPDIRECTINPUTDEVICE8 pDev)
{
    if ( pDev->bAcquired ) return DI_OK;
    pDev->bAcquired = true;
    if ( pDev->type == DINPUTSDL_MOUSE )
    {
        float x, y;
        SDL_GetRelativeMouseState(&x, &y); // drop motion from before the acquisition
        SDL_LockMutex(DInputSDL_pMouseLock);
        DInputSDL_wheelDelta     = 0;
        DInputSDL_numMouseEvents = 0;
        SDL_UnlockMutex(DInputSDL_pMouseLock);

        SDL_Window* pWindow = SDL_GetKeyboardFocus();
        if ( pDev->bExclusive && pWindow )
        {
            SDL_SetWindowRelativeMouseMode(pWindow, true); // exclusive: captured, hidden cursor
        }
    }
    return DI_OK;
}

HRESULT DInputSDL_DeviceUnacquire(LPDIRECTINPUTDEVICE8 pDev)
{
    if ( !pDev->bAcquired ) return DI_OK;
    pDev->bAcquired = false;
    if ( pDev->type == DINPUTSDL_MOUSE && pDev->bExclusive )
    {
        SDL_Window* pWindow = SDL_GetKeyboardFocus();
        if ( pWindow ) SDL_SetWindowRelativeMouseMode(pWindow, false);
    }
    return DI_OK;
}

HRESULT DInputSDL_DevicePoll(LPDIRECTINPUTDEVICE8 pDev)
{
    J3D_UNUSED(pDev);
    return DI_NOTATTACHED;
}

HRESULT DInputSDL_DeviceGetDeviceState(LPDIRECTINPUTDEVICE8 pDev, DWORD size, LPVOID pData)
{
    if ( !pDev->bAcquired ) return DIERR_NOTACQUIRED;

    if ( pDev->type == DINPUTSDL_KEYBOARD )
    {
        if ( size < 256 ) return DIERR_INVALIDPARAM;
        uint8_t* pState = (uint8_t*)pData;
        memset(pState, 0, 256);
        for ( int dik = 0; dik < 256; ++dik )
        {
            if ( DInputSDL_aVirtualKeys[dik] ) pState[dik] = 0x80;
        }
        if ( !SDL_GetKeyboardFocus() ) return DI_OK; // foreground device
        int numKeys = 0;
        const bool* aKeys = SDL_GetKeyboardState(&numKeys);
        for ( int sc = 0; sc < numKeys && sc < SDL_SCANCODE_COUNT; ++sc )
        {
            if ( aKeys[sc] && DInputSDL_aScancodeToDik[sc] ) pState[DInputSDL_aScancodeToDik[sc]] = 0x80;
        }
        return DI_OK;
    }

    if ( size < sizeof(DIMOUSESTATE) ) return DIERR_INVALIDPARAM;
    DIMOUSESTATE* pState = (DIMOUSESTATE*)pData;
    float dx = 0.0f, dy = 0.0f;
    SDL_MouseButtonFlags buttons = SDL_GetRelativeMouseState(&dx, &dy);
    pState->lX = (LONG)dx;
    pState->lY = (LONG)dy;
    SDL_LockMutex(DInputSDL_pMouseLock);
    pState->lZ = DInputSDL_wheelDelta;
    DInputSDL_wheelDelta = 0;
    SDL_UnlockMutex(DInputSDL_pMouseLock);
    pState->rgbButtons[0] = (buttons & SDL_BUTTON_LMASK) ? 0x80 : 0;
    pState->rgbButtons[1] = (buttons & SDL_BUTTON_RMASK) ? 0x80 : 0;
    pState->rgbButtons[2] = (buttons & SDL_BUTTON_MMASK) ? 0x80 : 0;
    pState->rgbButtons[3] = (buttons & SDL_BUTTON_X1MASK) ? 0x80 : 0;
    return DI_OK;
}

HRESULT DInputSDL_DeviceGetDeviceData(LPDIRECTINPUTDEVICE8 pDev, DWORD objSize, LPDIDEVICEOBJECTDATA pData, LPDWORD pInOut, DWORD flags)
{
    J3D_UNUSED(flags);
    if ( !pDev->bAcquired ) return DIERR_NOTACQUIRED;
    if ( pDev->type != DINPUTSDL_MOUSE || objSize != sizeof(DIDEVICEOBJECTDATA) ) return DIERR_NOTBUFFERED;

    SDL_LockMutex(DInputSDL_pMouseLock);
    DWORD n = DInputSDL_numMouseEvents < *pInOut ? DInputSDL_numMouseEvents : *pInOut;
    if ( pData ) memcpy(pData, DInputSDL_aMouseEvents, n * sizeof(DIDEVICEOBJECTDATA));
    memmove(DInputSDL_aMouseEvents, DInputSDL_aMouseEvents + n, (DInputSDL_numMouseEvents - n) * sizeof(DIDEVICEOBJECTDATA));
    DInputSDL_numMouseEvents -= n;
    SDL_UnlockMutex(DInputSDL_pMouseLock);

    *pInOut = n;
    return DI_OK;
}
