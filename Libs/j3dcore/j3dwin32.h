// Native builds (Linux, Android): the Win32 types, constants and CRT names that the engine uses, for native (non-Windows)
// builds. Only what the portable code needs; the Win32-only modules have SDL/POSIX counterparts (*SDL.c, std/Posix).
#ifndef J3DCORE_J3DWIN32_H
#define J3DCORE_J3DWIN32_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <wchar.h>
#include <ctype.h>

#define __cdecl
#define __stdcall
#define __fastcall
#define WINAPI
#define CALLBACK
#define APIENTRY
#define __forceinline inline __attribute__((always_inline))
#define __declspec(x)

typedef int BOOL;
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef int16_t SHORT;
typedef uint16_t USHORT;
typedef unsigned int UINT;
typedef int INT;
typedef char CHAR;
typedef unsigned char UCHAR;
typedef float FLOAT;
typedef void VOID;
typedef wchar_t WCHAR;
typedef int64_t LONGLONG;
typedef uint64_t ULONGLONG;
typedef int32_t HRESULT;
typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef ULONG_PTR DWORD_PTR;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
typedef BYTE* LPBYTE;
typedef WORD* LPWORD;
typedef DWORD* LPDWORD;
typedef LONG* LPLONG;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef wchar_t* LPWSTR;
typedef const wchar_t* LPCWSTR;
typedef BOOL* LPBOOL;

typedef void* HANDLE;
typedef HANDLE HWND;
typedef HANDLE HINSTANCE;
typedef HANDLE HMODULE;
typedef HANDLE HDC;
typedef HANDLE HBITMAP;
typedef HANDLE HFONT;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef HANDLE HMENU;
typedef HANDLE HKEY;
typedef HANDLE HGDIOBJ;
typedef HANDLE HBRUSH;

typedef BYTE* PBYTE;

typedef struct _FILETIME
{
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *LPFILETIME;

typedef union _ULARGE_INTEGER
{
    struct
    {
        DWORD LowPart;
        DWORD HighPart;
    };
    ULONGLONG QuadPart;
} ULARGE_INTEGER;

typedef struct _SYSTEMTIME
{
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;

#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER
{
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER;
#pragma pack(pop)

typedef struct tagBITMAPINFOHEADER
{
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagRGBQUAD
{
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
} RGBQUAD;

typedef struct tagBITMAPINFO
{
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[1];
} BITMAPINFO;
#define BI_RGB 0

typedef struct _ICONINFO
{
    BOOL fIcon;
    DWORD xHotspot;
    DWORD yHotspot;
    HBITMAP hbmMask;
    HBITMAP hbmColor;
} ICONINFO;

typedef HANDLE HIMAGELIST;
typedef LRESULT (CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef INT_PTR (CALLBACK* DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef UINT_PTR (CALLBACK* LPOFNHOOKPROC)(HWND, UINT, WPARAM, LPARAM);
typedef struct tagOFNA* LPOPENFILENAMEA;
typedef LPOPENFILENAMEA LPOPENFILENAME;
typedef struct _ABC
{
    int abcA;
    UINT abcB;
    int abcC;
} ABC;
typedef struct tagTEXTMETRICA
{
    LONG tmHeight, tmAscent, tmDescent, tmInternalLeading, tmExternalLeading, tmAveCharWidth, tmMaxCharWidth, tmWeight,
        tmOverhang, tmDigitizedAspectX, tmDigitizedAspectY;
    BYTE tmFirstChar, tmLastChar, tmDefaultChar, tmBreakChar, tmItalic, tmUnderlined, tmStruckOut, tmPitchAndFamily, tmCharSet;
} TEXTMETRICA;

#define TRUE  1
#define FALSE 0
#define MAX_PATH 260
#define FOREGROUND_BLUE      0x0001
#define FOREGROUND_GREEN     0x0002
#define FOREGROUND_RED       0x0004
#define FOREGROUND_INTENSITY 0x0008
#define S_OK    ((HRESULT)0)
#define E_FAIL  ((HRESULT)0x80004005L)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr)    (((HRESULT)(hr)) < 0)
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w) ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w) ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define MAKELONG(a, b) ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))

typedef struct tagRECT
{
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *LPRECT;
typedef const RECT* LPCRECT;

typedef struct tagPOINT
{
    LONG x;
    LONG y;
} POINT, *LPPOINT, *PPOINT;

typedef struct tagSIZE
{
    LONG cx;
    LONG cy;
} SIZE, *LPSIZE;

typedef struct _GUID
{
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
} GUID, *LPGUID;
typedef const GUID* LPCGUID;
typedef GUID IID;
typedef GUID CLSID;

typedef struct tagPALETTEENTRY
{
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
} PALETTEENTRY, *LPPALETTEENTRY;

typedef union _LARGE_INTEGER
{
    struct
    {
        DWORD LowPart;
        LONG HighPart;
    };
    LONGLONG QuadPart;
} LARGE_INTEGER;

#define HKEY_CLASSES_ROOT                ((HKEY)(uintptr_t)0x80000000)
#define HKEY_CURRENT_USER                ((HKEY)(uintptr_t)0x80000001)
#define HKEY_LOCAL_MACHINE               ((HKEY)(uintptr_t)0x80000002)
#define HKEY_USERS                       ((HKEY)(uintptr_t)0x80000003)
#define HKEY_PERFORMANCE_DATA            ((HKEY)(uintptr_t)0x80000004)
#define HKEY_CURRENT_CONFIG              ((HKEY)(uintptr_t)0x80000005)
#define HKEY_CURRENT_USER_LOCAL_SETTINGS ((HKEY)(uintptr_t)0x80000007)

// CRT names (MSVC)
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _strcmpi strcasecmp
#define strcmpi strcasecmp
#define strncmpi strncasecmp
#define _wcsicmp J3D_wcsicmp
#define _wcsnicmp J3D_wcsnicmp
#define wstrcmpi J3D_wcsicmp
#define wstrncmpi J3D_wcsnicmp
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#define _snwprintf J3D_snwprintf
#define _vsnwprintf J3D_vsnwprintf
#define _strdup strdup
#define _wcsdup J3D_wcsdup
#define _fileno fileno
#define _unlink unlink
#define _access access
#define _getcwd getcwd
#define _chdir chdir
#define _isnan isnan
#define _finite isfinite
#define _hypot hypot
#define __debugbreak() __builtin_trap()

static inline char* J3D_strlwr(char* s) { for ( char* p = s; *p; ++p ) *p = (char)tolower((unsigned char)*p); return s; }
static inline char* J3D_strupr(char* s) { for ( char* p = s; *p; ++p ) *p = (char)toupper((unsigned char)*p); return s; }
#define _strlwr J3D_strlwr
#define _strupr J3D_strupr
#define strlwr J3D_strlwr
#define strupr J3D_strupr

// Secure CRT variants (the engine passes sizes that always fit; truncate like MSVC's _TRUNCATE would not, but never overflow)
static inline int J3D_strcpy_s(char* d, size_t n, const char* s) { if ( !d || !n ) return 22; snprintf(d, n, "%s", s ? s : ""); return 0; }
static inline int J3D_strncpy_s(char* d, size_t n, const char* s, size_t c)
{
    if ( !d || !n ) return 22;
    size_t len = s ? strnlen(s, c) : 0;
    if ( len >= n ) len = n - 1;
    memcpy(d, s, len);
    d[len] = 0;
    return 0;
}
static inline int J3D_strcat_s(char* d, size_t n, const char* s)
{
    size_t l = strnlen(d, n);
    if ( l < n ) snprintf(d + l, n - l, "%s", s ? s : "");
    return 0;
}
#define strcpy_s J3D_strcpy_s
#define strncpy_s J3D_strncpy_s
#define strcat_s J3D_strcat_s
#define sprintf_s snprintf
#define vsprintf_s vsnprintf
#define _vsnprintf_s(b, n, c, f, a) vsnprintf(b, n, f, a)
#define _snprintf_s(b, n, c, ...) snprintf(b, n, __VA_ARGS__)
#define strtok_s strtok_r
#define _TRUNCATE ((size_t)-1)
#define _countof(a) (sizeof(a) / sizeof((a)[0]))

// Window messages, styles and virtual keys: the native window layer (wkernel) sends the engine Win32-style messages
#define GWL_STYLE              (-16)
#define GWL_USERDATA           (-21)
#define MB_ICONSTOP            0x00000010L
#define MB_ICONERROR           0x00000010L
#define MB_OK                  (0x00000000)
#define MB_TASKMODAL           (0x00002000)
#define SC_KEYMENU             0xF100
#define SC_SCREENSAVE          0xF140
#define SM_CXFRAME             32
#define SM_CXVSCROLL           2
#define SM_CYCAPTION           4
#define SM_CYDLGFRAME          8
#define SM_CYMENU              15
#define SW_HIDE                0
#define SW_MINIMIZE            6
#define SW_SHOW                5
#define SWP_NOACTIVATE         0x0010
#define SWP_NOMOVE             0x0002
#define SWP_NOREDRAW           0x0008
#define SWP_NOSIZE             0x0001
#define SWP_NOZORDER           0x0004
#define SWP_SHOWWINDOW         0x0040
#define VK_ESCAPE              0x1B
#define VK_F12                 0x7B
#define VK_F4                  0x73
#define VK_F5                  0x74
#define VK_F8                  0x77
#define VK_RETURN              0x0D
#define VK_SPACE               0x20
#define VK_BACK                0x08
#define VK_TAB                 0x09
#define VK_F1                  0x70
#define VK_F2                  0x71
#define VK_F3                  0x72
#define VK_F6                  0x75
#define VK_F7                  0x76
#define VK_F9                  0x78
#define VK_F10                 0x79
#define VK_F11                 0x7A
#define VK_UP                  0x26
#define VK_DOWN                0x28
#define VK_LEFT                0x25
#define VK_RIGHT               0x27
#define VK_SHIFT               0x10
#define VK_CONTROL             0x11
#define VK_MENU                0x12
#define VK_PAUSE               0x13
#define WA_INACTIVE            0
#define WA_ACTIVE              1
#define WM_ACTIVATE            0x0006
#define WM_ACTIVATEAPP         0x001C
#define WM_CHAR                0x0102
#define WM_CLOSE               0x0010
#define WM_COMMAND             0x0111
#define WM_DESTROY             0x0002
#define WM_GETMINMAXINFO       0x0024
#define WM_KEYDOWN             0x0100
#define WM_KEYUP               0x0101
#define WM_SYSKEYDOWN          0x0104
#define WM_SYSKEYUP            0x0105
#define WM_LBUTTONDOWN         0x0201
#define WM_LBUTTONUP           0x0202
#define WM_MBUTTONUP           0x0208
#define WM_RBUTTONDOWN         0x0204
#define WM_RBUTTONUP           0x0205
#define WM_MOUSEMOVE           0x0200
#define WM_MOUSEFIRST          0x0200
#define WM_PAINT               0x000F
#define WM_POWERBROADCAST      0x0218
#define WM_SYSCOMMAND          0x0112
#define WM_TIMER               0x0113
#define WM_QUIT                0x0012
#define WM_SIZE                0x0005
#define WM_SETFOCUS            0x0007
#define WM_KILLFOCUS           0x0008
#define WM_SHOWWINDOW          0x0018
#define WS_CHILD               (0x40000000)
#define WS_GROUP               (0x00020000)
#define WS_OVERLAPPEDWINDOW    0x00CF0000L
#define WS_POPUP               (0x80000000)
#define WS_TABSTOP             (0x00010000)
#define WS_VISIBLE             (0x10000000)
#define WS_BORDER              (0x00800000)
#define WS_CAPTION             (0x00C00000)
#define PBT_APMQUERYSUSPEND    0x0000
#define PBT_APMSUSPEND         0x0004
#define PBT_APMRESUMECRITICAL  0x0006
#define PBT_APMRESUMESUSPEND   0x0007

// The window functions JonesMain's message handlers call (implemented by the native window layer, wkernelSDL.c)
#define KF_REPEAT 0x4000
#define KF_UP     0x8000
typedef struct tagMINMAXINFO
{
    POINT ptReserved;
    POINT ptMaxSize;
    POINT ptMaxPosition;
    POINT ptMinTrackSize;
    POINT ptMaxTrackSize;
} MINMAXINFO, *LPMINMAXINFO;
typedef struct tagPAINTSTRUCT
{
    HDC hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
} PAINTSTRUCT, *LPPAINTSTRUCT;
typedef void (CALLBACK* TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);
BOOL J3D_GetWindowRect(HWND hwnd, LPRECT pRect);
BOOL J3D_GetClientRect(HWND hwnd, LPRECT pRect);
int J3D_GetSystemMetrics(int index);
HDC J3D_BeginPaint(HWND hwnd, LPPAINTSTRUCT pPaint);
BOOL J3D_EndPaint(HWND hwnd, const PAINTSTRUCT* pPaint);
BOOL J3D_ClientToScreen(HWND hwnd, LPPOINT pPoint);
UINT_PTR J3D_SetTimer(HWND hwnd, UINT_PTR id, UINT elapse, TIMERPROC pfTimer);
BOOL J3D_KillTimer(HWND hwnd, UINT_PTR id);
LRESULT J3D_DefWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL J3D_PostMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void J3D_PostQuitMessage(int exitCode);
BOOL J3D_ShowWindow(HWND hwnd, int cmd);
int J3D_ShowCursor(BOOL bShow);
int J3D_MessageBox(HWND hwnd, LPCSTR pText, LPCSTR pCaption, UINT type);
#define GetWindowRect J3D_GetWindowRect
#define GetClientRect J3D_GetClientRect
#define GetSystemMetrics J3D_GetSystemMetrics
#define BeginPaint J3D_BeginPaint
#define EndPaint J3D_EndPaint
#define ClientToScreen J3D_ClientToScreen
#define SetTimer J3D_SetTimer
#define KillTimer J3D_KillTimer
#define DefWindowProc J3D_DefWindowProc
#define PostMessage J3D_PostMessage
#define PostQuitMessage J3D_PostQuitMessage
#define ShowWindow J3D_ShowWindow
#define ShowCursor J3D_ShowCursor
#define MessageBox J3D_MessageBox
#define MessageBoxA J3D_MessageBox
#define OutputDebugString(s) ((void)(s))
#define OutputDebugStringA(s) ((void)(s))
#define DebugBreak() __builtin_trap()
BOOL J3D_CloseHandle(HANDLE h);
BOOL J3D_GetComputerName(LPSTR pBuffer, LPDWORD pSize);
#define CloseHandle J3D_CloseHandle
#define GetComputerName J3D_GetComputerName
#define GetComputerNameA J3D_GetComputerName
DWORD J3D_SearchPath(LPCSTR pPath, LPCSTR pFileName, LPCSTR pExtension, DWORD bufferLength, LPSTR pBuffer, LPSTR* ppFilePart);
#define SearchPath J3D_SearchPath
#define SearchPathA J3D_SearchPath

// Time
DWORD J3D_GetTickCount(void);
void J3D_GetLocalTime(SYSTEMTIME* pTime);
void J3D_Sleep(DWORD msec);
BOOL J3D_QueryPerformanceCounter(LARGE_INTEGER* pCount); // nanoseconds
BOOL J3D_QueryPerformanceFrequency(LARGE_INTEGER* pFreq);
#define QueryPerformanceCounter J3D_QueryPerformanceCounter
#define QueryPerformanceFrequency J3D_QueryPerformanceFrequency
#define GetTickCount J3D_GetTickCount
#define ExitProcess(code) exit((int)(code))
#define GetLocalTime J3D_GetLocalTime
#define Sleep J3D_Sleep

// GDI: memory DCs and DIB sections only (savegame thumbnails; std/Posix/stdGdiCompat.c)
typedef struct tagBITMAP
{
    LONG bmType;
    LONG bmWidth;
    LONG bmHeight;
    LONG bmWidthBytes;
    WORD bmPlanes;
    WORD bmBitsPixel;
    LPVOID bmBits;
} BITMAP;
#define DIB_RGB_COLORS   0
#define STRETCH_HALFTONE 4
#define SRCCOPY          0x00CC0020
HDC J3D_CreateCompatibleDC(HDC hdc);
BOOL J3D_DeleteDC(HDC hdc);
HBITMAP J3D_CreateDIBSection(HDC hdc, const BITMAPINFO* pbmi, UINT usage, void** ppvBits, HANDLE hSection, DWORD offset);
HGDIOBJ J3D_SelectObject(HDC hdc, HGDIOBJ h);
BOOL J3D_DeleteObject(HGDIOBJ h);
int J3D_SetStretchBltMode(HDC hdc, int mode);
BOOL J3D_StretchBlt(HDC hdcDest, int xDest, int yDest, int wDest, int hDest, HDC hdcSrc, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop);
int J3D_GetObject(HGDIOBJ h, int size, void* pv);
#define CreateCompatibleDC J3D_CreateCompatibleDC
#define DeleteDC J3D_DeleteDC
#define CreateDIBSection J3D_CreateDIBSection
#define SelectObject J3D_SelectObject
#define DeleteObject J3D_DeleteObject
#define SetStretchBltMode J3D_SetStretchBltMode
#define StretchBlt J3D_StretchBlt
#define GetObject J3D_GetObject

// Secure CRT and 16-bit wide strings (std/Posix/stdCrtCompat.c). The engine is built with -fshort-wchar: its
// structs and savegames hold 16-bit wchar_t, as on Windows, so the C library's wide functions can't be used.
typedef size_t rsize_t;
size_t J3D_strnlen_s(const char* s, size_t n);
int J3D_strncat_s(char* d, size_t n, const char* s, size_t c);
int J3D_vsscanf_s(const char* str, const char* fmt, va_list args);
int J3D_sscanf_s(const char* str, const char* fmt, ...);
int J3D_fopen_s(FILE** pf, const char* name, const char* mode);
FILE* J3D_fopen(const char* name, const char* mode);
#define J3D_MAX_PATH 1024
char* J3D_ResolvePath(const char* pPath, char* pOut, size_t outSize); // '\\' -> '/', case-insensitive match
size_t J3D_wcslen(const wchar_t* s);
size_t J3D_wcsnlen(const wchar_t* s, size_t n);
wchar_t* J3D_wcsncpy(wchar_t* d, const wchar_t* s, size_t n);
wchar_t* J3D_wcscpy(wchar_t* d, const wchar_t* s);
int J3D_wcsncpy_s(wchar_t* d, size_t n, const wchar_t* s, size_t c);
int J3D_wcscmp(const wchar_t* a, const wchar_t* b);
int J3D_wcsncmp(const wchar_t* a, const wchar_t* b, size_t n);
int J3D_wcsicmp(const wchar_t* a, const wchar_t* b);
int J3D_wcsnicmp(const wchar_t* a, const wchar_t* b, size_t n);
wchar_t* J3D_wcsdup(const wchar_t* s);
wchar_t* J3D_wmemchr(const wchar_t* s, wchar_t c, size_t n);
wchar_t* J3D_wcschr(const wchar_t* s, wchar_t c);
wchar_t* J3D_wcsstr(const wchar_t* s, const wchar_t* sub);
int J3D_vsnwprintf(wchar_t* buf, size_t n, const wchar_t* fmt, va_list args); // MSVC semantics: %s is wide
int J3D_snwprintf(wchar_t* buf, size_t n, const wchar_t* fmt, ...);

#define strnlen_s J3D_strnlen_s
#define strncat_s J3D_strncat_s
#define vsscanf_s J3D_vsscanf_s
#define sscanf_s J3D_sscanf_s
#define fopen_s J3D_fopen_s
#define fopen J3D_fopen
#define vsnprintf_s(b, n, c, f, a) vsnprintf(b, n, f, a)
#undef wcslen
#undef wcsnlen
#undef wcsncpy
#undef wcscpy
#undef wcscmp
#undef wcsncmp
#undef wcsdup
#undef wmemchr
#undef wcschr
#undef wcsstr
#undef swprintf
#undef vswprintf
#define wcslen J3D_wcslen
#define wcsnlen J3D_wcsnlen
#define wcsncpy J3D_wcsncpy
#define wcscpy J3D_wcscpy
#define wcsncpy_s J3D_wcsncpy_s
#define wcscmp J3D_wcscmp
#define wcsncmp J3D_wcsncmp
#define wcscasecmp J3D_wcsicmp
#define wcsncasecmp J3D_wcsnicmp
#define wcsdup J3D_wcsdup
#define wmemchr J3D_wmemchr
#define wcschr J3D_wcschr
#define wcsstr J3D_wcsstr
#define swprintf J3D_snwprintf
#define vswprintf J3D_vsnwprintf
#define _vsnwprintf_s(b, n, c, f, a) J3D_vsnwprintf(b, n, f, a)
#define vsnwprintf_s(b, n, c, f, a) J3D_vsnwprintf(b, n, f, a)

#endif // J3DCORE_J3DWIN32_H
