#ifndef INDY_INDYDISPLAYDX9_H
#define INDY_INDYDISPLAYDX9_H
// Indycompiled helpers for upstream's DirectX 9 display code.
#include <j3dcore/j3d.h>

#ifdef J3D_DIRECTX9
#include <d3d9.h>
#include <stdbool.h>

J3D_EXTERN_C_START

// Copies a non-multisampled render-target surface onto a multisampled render target (e.g. the MSAA back buffer)
// by drawing it as a full-screen textured quad. Fallback for IDirect3DDevice9_StretchRect, which may not write into
// multisampled surfaces: with upstream's MSAA lock/copy-back path, video frames and other CPU-drawn content
// otherwise stay black under Wine. Device state is saved and restored. Returns true on success.
bool J3DAPI indyDisplay_DrawSurfaceToTarget(LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DSURFACE9 pSrc, LPDIRECT3DSURFACE9 pDest);

// True if the surface is multisampled (MSAA).
bool J3DAPI indyDisplay_IsMultisampled(LPDIRECT3DSURFACE9 pSurface);

J3D_EXTERN_C_END
#endif // J3D_DIRECTX9
#endif // INDY_INDYDISPLAYDX9_H
