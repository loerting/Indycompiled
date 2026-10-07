#ifndef INDY_INDYDRAW_H
#define INDY_INDYDRAW_H
// Indycompiled: untextured 2D shapes on top of the frame (touch overlay, native menus), batched into one draw.
// Coordinates are back buffer pixels; colours are D3DCOLOR (D3DCOLOR_ARGB(a, r, g, b)).
#include <j3dcore/j3d.h>
#include <std/types.h>
#include <stdbool.h>

J3D_EXTERN_C_START

void indyDraw_Begin(void);
void indyDraw_End(void); // queues the shapes with std3D_DrawRenderList

void indyDraw_Rect(float x0, float y0, float x1, float y1, D3DCOLOR color);
void indyDraw_Frame(float x0, float y0, float x1, float y1, float thickness, D3DCOLOR color); // outline
void indyDraw_Disc(float cx, float cy, float r, D3DCOLOR color);
void indyDraw_Ring(float cx, float cy, float r, float thickness, D3DCOLOR color);
void indyDraw_Triangle(float x0, float y0, float x1, float y1, float x2, float y2, D3DCOLOR color);

J3D_EXTERN_C_END
#endif // INDY_INDYDRAW_H
