// Native builds: the Win32 dialog host (JonesDialog.c) has nothing to restore; dialogs are drawn by the engine.
#include "JonesDialog.h"

int J3DAPI JonesDialog_RestoreBackground(HDC hdc, HWND hwnd, LPPOINT a3, LPRECT a4)
{
    J3D_UNUSED(hdc);
    J3D_UNUSED(hwnd);
    J3D_UNUSED(a3);
    J3D_UNUSED(a4);
    return 0;
}
