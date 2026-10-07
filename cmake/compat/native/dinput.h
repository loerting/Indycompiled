// Native builds (Linux, Android): the subset of DirectInput 8 that the control module (stdControlDX9.c) uses,
// implemented on SDL3 (std/SDL/stdDInputSDL.c): the system keyboard and mouse, no other devices (gamepads go through
// XInput, see Xinput.h). DIK_* key codes are the engine's keyboard key IDs. Values as in the Windows SDK.
#pragma once
#include <j3dcore/j3dwin32.h>

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

typedef struct IDirectInput8 IDirectInput8, *LPDIRECTINPUT8;
typedef struct IDirectInputDevice8 IDirectInputDevice8, *LPDIRECTINPUTDEVICE8;
typedef const GUID* REFGUID;
typedef const GUID* REFIID;

#define DI_OK            ((HRESULT)0)
#define DI_NOTATTACHED   ((HRESULT)1)
#define DI_BUFFEROVERFLOW ((HRESULT)1)
#define DI_PROPNOEFFECT  ((HRESULT)1)
#define DI_POLLEDDEVICE  ((HRESULT)2)
#define DIERR_OLDDIRECTINPUTVERSION  ((HRESULT)0x8007047E)
#define DIERR_BETADIRECTINPUTVERSION ((HRESULT)0x80070481)
#define DIERR_BADDRIVERVER           ((HRESULT)0x80070077)
#define DIERR_DEVICENOTREG           ((HRESULT)0x80040154)
#define DIERR_NOTFOUND               ((HRESULT)0x80070002)
#define DIERR_OBJECTNOTFOUND         ((HRESULT)0x80070002)
#define DIERR_INVALIDPARAM           ((HRESULT)0x80070057)
#define DIERR_NOINTERFACE            ((HRESULT)0x80004002)
#define DIERR_GENERIC                ((HRESULT)0x80004005)
#define DIERR_OUTOFMEMORY            ((HRESULT)0x8007000E)
#define DIERR_UNSUPPORTED            ((HRESULT)0x80004001)
#define DIERR_NOTINITIALIZED         ((HRESULT)0x80070015)
#define DIERR_ALREADYINITIALIZED     ((HRESULT)0x800704DF)
#define DIERR_NOAGGREGATION          ((HRESULT)0x80040110)
#define DIERR_OTHERAPPHASPRIO        ((HRESULT)0x80070005)
#define DIERR_INPUTLOST              ((HRESULT)0x8007001E)
#define DIERR_ACQUIRED               ((HRESULT)0x800700AA)
#define DIERR_NOTACQUIRED            ((HRESULT)0x8007000C)
#define DIERR_READONLY               ((HRESULT)0x80070005)
#define DIERR_HANDLEEXISTS           ((HRESULT)0x80070005)
#define DIERR_INSUFFICIENTPRIVS      ((HRESULT)0x80040200)
#define DIERR_DEVICEFULL             ((HRESULT)0x80040201)
#define DIERR_MOREDATA               ((HRESULT)0x80040202)
#define DIERR_NOTDOWNLOADED          ((HRESULT)0x80040203)
#define DIERR_HASEFFECTS             ((HRESULT)0x80040204)
#define DIERR_NOTEXCLUSIVEACQUIRED   ((HRESULT)0x80040205)
#define DIERR_INCOMPLETEEFFECT       ((HRESULT)0x80040206)
#define DIERR_NOTBUFFERED            ((HRESULT)0x80040207)
#define DIERR_EFFECTPLAYING          ((HRESULT)0x80040208)

#define DISCL_EXCLUSIVE    0x00000001
#define DISCL_NONEXCLUSIVE 0x00000002
#define DISCL_FOREGROUND   0x00000004
#define DISCL_BACKGROUND   0x00000008
#define DIPH_DEVICE   0
#define DIPH_BYOFFSET 1
#define DI8DEVCLASS_ALL     0
#define DIEDFL_ATTACHEDONLY 0x00000001
#define DIENUM_STOP     0
#define DIENUM_CONTINUE 1
#define DI_DEGREES 100
#define DI8DEVTYPE_DEVICE        0x11
#define DI8DEVTYPE_MOUSE         0x12
#define DI8DEVTYPE_KEYBOARD      0x13
#define DI8DEVTYPE_JOYSTICK      0x14
#define DI8DEVTYPE_GAMEPAD       0x15
#define DI8DEVTYPE_DRIVING       0x16
#define DI8DEVTYPE_FLIGHT        0x17
#define DI8DEVTYPE_DEVICECTRL    0x19
#define DI8DEVTYPE_SCREENPOINTER 0x1A
#define DI8DEVTYPE_REMOTE        0x1B
#define DI8DEVTYPE_SUPPLEMENTAL  0x1C
#define GET_DIDEVICE_TYPE(dwDevType) LOBYTE(dwDevType)
#define DI8DEVTYPE_LIMITEDGAMESUBTYPE 1
#define GET_DIDEVICE_SUBTYPE(dwDevType) HIBYTE(dwDevType)
#define DI8DEVTYPEMOUSE_UNKNOWN 1
#define DI8DEVTYPEMOUSE_TRADITIONAL 2
#define DI8DEVTYPEMOUSE_FINGERSTICK 3
#define DI8DEVTYPEMOUSE_TOUCHPAD 4
#define DI8DEVTYPEMOUSE_TRACKBALL 5
#define DI8DEVTYPEMOUSE_ABSOLUTE 6
#define DI8DEVTYPEKEYBOARD_UNKNOWN 0
#define DI8DEVTYPEKEYBOARD_PCXT 1
#define DI8DEVTYPEKEYBOARD_OLIVETTI 2
#define DI8DEVTYPEKEYBOARD_PCAT 3
#define DI8DEVTYPEKEYBOARD_PCENH 4
#define DI8DEVTYPEKEYBOARD_NOKIA1050 5
#define DI8DEVTYPEKEYBOARD_NOKIA9140 6
#define DI8DEVTYPEKEYBOARD_NEC98 7
#define DI8DEVTYPEKEYBOARD_NEC98LAPTOP 8
#define DI8DEVTYPEKEYBOARD_NEC98106 9
#define DI8DEVTYPEKEYBOARD_JAPAN106 10
#define DI8DEVTYPEKEYBOARD_JAPANAX 11
#define DI8DEVTYPEKEYBOARD_J3100 12
#define DI8DEVTYPEJOYSTICK_LIMITED DI8DEVTYPE_LIMITEDGAMESUBTYPE
#define DI8DEVTYPEJOYSTICK_STANDARD 2
#define DI8DEVTYPEGAMEPAD_LIMITED DI8DEVTYPE_LIMITEDGAMESUBTYPE
#define DI8DEVTYPEGAMEPAD_STANDARD 2
#define DI8DEVTYPEGAMEPAD_TILT 3
#define DI8DEVTYPEDRIVING_LIMITED DI8DEVTYPE_LIMITEDGAMESUBTYPE
#define DI8DEVTYPEDRIVING_COMBINEDPEDALS 2
#define DI8DEVTYPEDRIVING_DUALPEDALS 3
#define DI8DEVTYPEDRIVING_THREEPEDALS 4
#define DI8DEVTYPEDRIVING_HANDHELD 5
#define DI8DEVTYPEFLIGHT_LIMITED DI8DEVTYPE_LIMITEDGAMESUBTYPE
#define DI8DEVTYPEFLIGHT_STICK 2
#define DI8DEVTYPEFLIGHT_YOKE 3
#define DI8DEVTYPEFLIGHT_RC 4
#define DI8DEVTYPESCREENPTR_UNKNOWN 2
#define DI8DEVTYPESCREENPTR_LIGHTGUN 3
#define DI8DEVTYPESCREENPTR_LIGHTPEN 4
#define DI8DEVTYPESCREENPTR_TOUCH 5
#define DI8DEVTYPEREMOTE_UNKNOWN 2
#define DI8DEVTYPEDEVICECTRL_UNKNOWN 2
#define DI8DEVTYPEDEVICECTRL_COMMSSELECTION 3
#define DI8DEVTYPESUPPLEMENTAL_UNKNOWN 2
#define DI8DEVTYPESUPPLEMENTAL_2NDHANDCONTROLLER 3
#define DI8DEVTYPESUPPLEMENTAL_HEADTRACKER 4
#define DI8DEVTYPESUPPLEMENTAL_HANDTRACKER 5
#define DI8DEVTYPESUPPLEMENTAL_SHIFTSTICKGATE 6
#define DI8DEVTYPESUPPLEMENTAL_SHIFTER 7
#define DI8DEVTYPESUPPLEMENTAL_THROTTLE 8
#define DI8DEVTYPESUPPLEMENTAL_SPLITTHROTTLE 9
#define DI8DEVTYPESUPPLEMENTAL_COMBINEDPEDALS 10
#define DI8DEVTYPESUPPLEMENTAL_DUALPEDALS 11
#define DI8DEVTYPESUPPLEMENTAL_THREEPEDALS 12
#define DI8DEVTYPESUPPLEMENTAL_RUDDERPEDALS 13
#define DI8DEVTYPE_1STPERSON 0x18
#define DI8DEVTYPE1STPERSON_LIMITED DI8DEVTYPE_LIMITEDGAMESUBTYPE
#define DI8DEVTYPE1STPERSON_UNKNOWN 2
#define DI8DEVTYPE1STPERSON_SIXDOF 3
#define DI8DEVTYPE1STPERSON_SHOOTER 4
#define DIDC_ATTACHED 0x00000001
#define DIDC_POLLEDDEVICE 0x00000002
#define DIDC_EMULATED 0x00000004
#define DIDC_POLLEDDATAFORMAT 0x00000008
#define DIDC_FORCEFEEDBACK 0x00000100
#define DIDC_FFATTACK 0x00000200
#define DIDC_FFFADE 0x00000400
#define DIDC_SATURATION 0x00000800
#define DIDC_POSNEGCOEFFICIENTS 0x00001000
#define DIDC_POSNEGSATURATION 0x00002000
#define DIDC_DEADBAND 0x00004000
#define DIDC_STARTDELAY 0x00008000
#define DIDC_ALIAS 0x00010000
#define DIDC_PHANTOM 0x00020000
#define DIDC_HIDDEN 0x00040000
#define DIJOFS_X  0
#define DIJOFS_Y  4
#define DIJOFS_Z  8
#define DIJOFS_RX 12
#define DIJOFS_RY 16
#define DIJOFS_RZ 20
#define DIJOFS_SLIDER(n)  (24 + (n) * 4)
#define DIJOFS_POV(n)     (32 + (n) * 4)
#define DIJOFS_BUTTON(n)  (48 + (n))
#define MAKEDIPROP(prop) ((REFGUID)(uintptr_t)(prop))
#define DIPROP_BUFFERSIZE MAKEDIPROP(1)
#define DIPROP_RANGE      MAKEDIPROP(4)

typedef struct DIDEVCAPS
{
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwDevType;
    DWORD dwAxes;
    DWORD dwButtons;
    DWORD dwPOVs;
    DWORD dwFFSamplePeriod;
    DWORD dwFFMinTimeResolution;
    DWORD dwFirmwareRevision;
    DWORD dwHardwareRevision;
    DWORD dwFFDriverVersion;
} DIDEVCAPS, *LPDIDEVCAPS;

typedef struct DIDEVICEINSTANCEA
{
    DWORD dwSize;
    GUID guidInstance;
    GUID guidProduct;
    DWORD dwDevType;
    CHAR tszInstanceName[MAX_PATH];
    CHAR tszProductName[MAX_PATH];
    GUID guidFFDriver;
    WORD wUsagePage;
    WORD wUsage;
} DIDEVICEINSTANCE, *LPDIDEVICEINSTANCE;
typedef const DIDEVICEINSTANCE* LPCDIDEVICEINSTANCE;
typedef BOOL (CALLBACK* LPDIENUMDEVICESCALLBACK)(LPCDIDEVICEINSTANCE, LPVOID);

typedef struct DIPROPHEADER
{
    DWORD dwSize;
    DWORD dwHeaderSize;
    DWORD dwObj;
    DWORD dwHow;
} DIPROPHEADER, *LPDIPROPHEADER;
typedef const DIPROPHEADER* LPCDIPROPHEADER;

typedef struct DIPROPDWORD
{
    DIPROPHEADER diph;
    DWORD dwData;
} DIPROPDWORD;

typedef struct DIPROPRANGE
{
    DIPROPHEADER diph;
    LONG lMin;
    LONG lMax;
} DIPROPRANGE;

typedef struct DIDEVICEOBJECTDATA
{
    DWORD dwOfs;
    DWORD dwData;
    DWORD dwTimeStamp;
    DWORD dwSequence;
    UINT_PTR uAppData;
} DIDEVICEOBJECTDATA, *LPDIDEVICEOBJECTDATA;

typedef struct DIMOUSESTATE
{
    LONG lX;
    LONG lY;
    LONG lZ;
    BYTE rgbButtons[4];
} DIMOUSESTATE;

typedef struct DIJOYSTATE
{
    LONG lX, lY, lZ, lRx, lRy, lRz;
    LONG rglSlider[2];
    DWORD rgdwPOV[4];
    BYTE rgbButtons[32];
} DIJOYSTATE;

#define DIMOFS_X       0
#define DIMOFS_Y       4
#define DIMOFS_Z       8
#define DIMOFS_BUTTON0 12
#define DIMOFS_BUTTON1 13
#define DIMOFS_BUTTON2 14
#define DIMOFS_BUTTON3 15

typedef struct DIDATAFORMAT { DWORD dwSize; } DIDATAFORMAT; // only its identity matters
extern const DIDATAFORMAT c_dfDIKeyboard;
extern const DIDATAFORMAT c_dfDIMouse;
extern const DIDATAFORMAT c_dfDIJoystick;
extern const GUID GUID_SysKeyboard;
extern const GUID GUID_SysMouse;
extern const GUID IID_IDirectInput8;

HRESULT DirectInput8Create(HINSTANCE hinst, DWORD version, REFIID riid, void* ppOut, void* pUnkOuter);
HRESULT DInputSDL_Release(LPDIRECTINPUT8 pDI);
HRESULT DInputSDL_EnumDevices(LPDIRECTINPUT8 pDI, DWORD devType, LPDIENUMDEVICESCALLBACK pfCallback, LPVOID pRef, DWORD flags);
HRESULT DInputSDL_CreateDevice(LPDIRECTINPUT8 pDI, REFGUID guid, LPDIRECTINPUTDEVICE8* ppDevice, void* pUnkOuter);
HRESULT DInputSDL_DeviceRelease(LPDIRECTINPUTDEVICE8 pDev);
HRESULT DInputSDL_DeviceGetCapabilities(LPDIRECTINPUTDEVICE8 pDev, LPDIDEVCAPS pCaps);
HRESULT DInputSDL_DeviceSetDataFormat(LPDIRECTINPUTDEVICE8 pDev, const DIDATAFORMAT* pFormat);
HRESULT DInputSDL_DeviceSetCooperativeLevel(LPDIRECTINPUTDEVICE8 pDev, HWND hwnd, DWORD flags);
HRESULT DInputSDL_DeviceSetProperty(LPDIRECTINPUTDEVICE8 pDev, REFGUID prop, LPCDIPROPHEADER pHeader);
HRESULT DInputSDL_DeviceGetProperty(LPDIRECTINPUTDEVICE8 pDev, REFGUID prop, LPDIPROPHEADER pHeader);
HRESULT DInputSDL_DeviceAcquire(LPDIRECTINPUTDEVICE8 pDev);
HRESULT DInputSDL_DeviceUnacquire(LPDIRECTINPUTDEVICE8 pDev);
HRESULT DInputSDL_DevicePoll(LPDIRECTINPUTDEVICE8 pDev);
HRESULT DInputSDL_DeviceGetDeviceState(LPDIRECTINPUTDEVICE8 pDev, DWORD size, LPVOID pData);
HRESULT DInputSDL_DeviceGetDeviceData(LPDIRECTINPUTDEVICE8 pDev, DWORD objSize, LPDIDEVICEOBJECTDATA pData, LPDWORD pInOut, DWORD flags);

#define IDirectInput8_Release(p)                            DInputSDL_Release(p)
#define IDirectInput8_EnumDevices(p, t, cb, r, f)           DInputSDL_EnumDevices(p, t, cb, r, f)
#define IDirectInput8_CreateDevice(p, g, d, u)              DInputSDL_CreateDevice(p, g, d, u)
#define IDirectInputDevice8_Release(p)                      DInputSDL_DeviceRelease(p)
#define IDirectInputDevice8_GetCapabilities(p, c)           DInputSDL_DeviceGetCapabilities(p, c)
#define IDirectInputDevice8_SetDataFormat(p, f)             DInputSDL_DeviceSetDataFormat(p, f)
#define IDirectInputDevice8_SetCooperativeLevel(p, h, f)    DInputSDL_DeviceSetCooperativeLevel(p, h, f)
#define IDirectInputDevice8_SetProperty(p, g, h)            DInputSDL_DeviceSetProperty(p, g, h)
#define IDirectInputDevice8_GetProperty(p, g, h)            DInputSDL_DeviceGetProperty(p, g, h)
#define IDirectInputDevice8_Acquire(p)                      DInputSDL_DeviceAcquire(p)
#define IDirectInputDevice8_Unacquire(p)                    DInputSDL_DeviceUnacquire(p)
#define IDirectInputDevice8_Poll(p)                         DInputSDL_DevicePoll(p)
#define IDirectInputDevice8_GetDeviceState(p, s, d)         DInputSDL_DeviceGetDeviceState(p, s, d)
#define IDirectInputDevice8_GetDeviceData(p, s, d, n, f)    DInputSDL_DeviceGetDeviceData(p, s, d, n, f)

// DIK_* key codes
#define DIK_ESCAPE           0x01
#define DIK_1                0x02
#define DIK_2                0x03
#define DIK_3                0x04
#define DIK_4                0x05
#define DIK_5                0x06
#define DIK_6                0x07
#define DIK_7                0x08
#define DIK_8                0x09
#define DIK_9                0x0A
#define DIK_0                0x0B
#define DIK_MINUS            0x0C
#define DIK_EQUALS           0x0D
#define DIK_BACK             0x0E
#define DIK_TAB              0x0F
#define DIK_Q                0x10
#define DIK_W                0x11
#define DIK_E                0x12
#define DIK_R                0x13
#define DIK_T                0x14
#define DIK_Y                0x15
#define DIK_U                0x16
#define DIK_I                0x17
#define DIK_O                0x18
#define DIK_P                0x19
#define DIK_LBRACKET         0x1A
#define DIK_RBRACKET         0x1B
#define DIK_RETURN           0x1C
#define DIK_LCONTROL         0x1D
#define DIK_A                0x1E
#define DIK_S                0x1F
#define DIK_D                0x20
#define DIK_F                0x21
#define DIK_G                0x22
#define DIK_H                0x23
#define DIK_J                0x24
#define DIK_K                0x25
#define DIK_L                0x26
#define DIK_SEMICOLON        0x27
#define DIK_APOSTROPHE       0x28
#define DIK_GRAVE            0x29
#define DIK_LSHIFT           0x2A
#define DIK_BACKSLASH        0x2B
#define DIK_Z                0x2C
#define DIK_X                0x2D
#define DIK_C                0x2E
#define DIK_V                0x2F
#define DIK_B                0x30
#define DIK_N                0x31
#define DIK_M                0x32
#define DIK_COMMA            0x33
#define DIK_PERIOD           0x34
#define DIK_SLASH            0x35
#define DIK_RSHIFT           0x36
#define DIK_MULTIPLY         0x37
#define DIK_LMENU            0x38
#define DIK_SPACE            0x39
#define DIK_CAPITAL          0x3A
#define DIK_F1               0x3B
#define DIK_F2               0x3C
#define DIK_F3               0x3D
#define DIK_F4               0x3E
#define DIK_F5               0x3F
#define DIK_F6               0x40
#define DIK_F7               0x41
#define DIK_F8               0x42
#define DIK_F9               0x43
#define DIK_F10              0x44
#define DIK_NUMLOCK          0x45
#define DIK_SCROLL           0x46
#define DIK_NUMPAD7          0x47
#define DIK_NUMPAD8          0x48
#define DIK_NUMPAD9          0x49
#define DIK_SUBTRACT         0x4A
#define DIK_NUMPAD4          0x4B
#define DIK_NUMPAD5          0x4C
#define DIK_NUMPAD6          0x4D
#define DIK_ADD              0x4E
#define DIK_NUMPAD1          0x4F
#define DIK_NUMPAD2          0x50
#define DIK_NUMPAD3          0x51
#define DIK_NUMPAD0          0x52
#define DIK_DECIMAL          0x53
#define DIK_OEM_102          0x56
#define DIK_F11              0x57
#define DIK_F12              0x58
#define DIK_F13              0x64
#define DIK_F14              0x65
#define DIK_F15              0x66
#define DIK_KANA             0x70
#define DIK_ABNT_C1          0x73
#define DIK_CONVERT          0x79
#define DIK_NOCONVERT        0x7B
#define DIK_YEN              0x7D
#define DIK_ABNT_C2          0x7E
#define DIK_NUMPADEQUALS     0x8D
#define DIK_PREVTRACK        0x90
#define DIK_CIRCUMFLEX       0x90
#define DIK_AT               0x91
#define DIK_COLON            0x92
#define DIK_UNDERLINE        0x93
#define DIK_KANJI            0x94
#define DIK_STOP             0x95
#define DIK_AX               0x96
#define DIK_UNLABELED        0x97
#define DIK_NEXTTRACK        0x99
#define DIK_NUMPADENTER      0x9C
#define DIK_RCONTROL         0x9D
#define DIK_MUTE             0xA0
#define DIK_CALCULATOR       0xA1
#define DIK_PLAYPAUSE        0xA2
#define DIK_MEDIASTOP        0xA4
#define DIK_VOLUMEDOWN       0xAE
#define DIK_VOLUMEUP         0xB0
#define DIK_WEBHOME          0xB2
#define DIK_NUMPADCOMMA      0xB3
#define DIK_DIVIDE           0xB5
#define DIK_SYSRQ            0xB7
#define DIK_RMENU            0xB8
#define DIK_PAUSE            0xC5
#define DIK_HOME             0xC7
#define DIK_UP               0xC8
#define DIK_PRIOR            0xC9
#define DIK_LEFT             0xCB
#define DIK_RIGHT            0xCD
#define DIK_END              0xCF
#define DIK_DOWN             0xD0
#define DIK_NEXT             0xD1
#define DIK_INSERT           0xD2
#define DIK_DELETE           0xD3
#define DIK_LWIN             0xDB
#define DIK_RWIN             0xDC
#define DIK_APPS             0xDD
#define DIK_POWER            0xDE
#define DIK_SLEEP            0xDF
#define DIK_WAKE             0xE3
#define DIK_WEBSEARCH        0xE5
#define DIK_WEBFAVORITES     0xE6
#define DIK_WEBREFRESH       0xE7
#define DIK_WEBSTOP          0xE8
#define DIK_WEBFORWARD       0xE9
#define DIK_WEBBACK          0xEA
#define DIK_MYCOMPUTER       0xEB
#define DIK_MAIL             0xEC
#define DIK_MEDIASELECT      0xED
