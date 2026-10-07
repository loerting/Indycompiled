#ifndef STD_STDWIN95_H
#define STD_STDWIN95_H
#include <j3dcore/j3d.h>
#include <std/types.h>
#include <std/RTI/addresses.h>

J3D_EXTERN_C_START

void J3DAPI stdWin95_SetWindow(HWND hwnd);
HWND J3DAPI stdWin95_GetWindow();
void J3DAPI stdWin95_SetInstance(HINSTANCE hinstance);
HINSTANCE stdWin95_GetInstance(void);
void J3DAPI stdWin95_SetGuid(const GUID* pGuid);
const GUID* J3DAPI stdWin95_GetGuid(void); // Added

// Helper hooking functions
void stdWin95_InstallHooks(void);
void stdWin95_ResetGlobals(void);

// INDY: Stage 4. The dialog templates and icons are Indy3D.exe's resources. The hook build finds them through the
// exe's instance (hInstance). Standalone, Jones3D.exe loads Indy3D.exe as a data file (WinMain in dllmain.c).
#ifdef J3D_STANDALONE
void stdWin95_SetResourceModule(HMODULE hModule);
HINSTANCE stdWin95_GetResourceModule(void);
#  define STDWIN95_RESOURCES(hInstance) stdWin95_GetResourceModule()
#else
#  define STDWIN95_RESOURCES(hInstance) (hInstance)
#endif

J3D_EXTERN_C_END
#endif // STD_STDWIN95_H
