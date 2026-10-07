// Indycompiled: untextured 2D shapes on top of the frame. See indyDraw.h.
#include "indyDraw.h"

#include <std/Win95/std3D.h>

#include <math.h>
#include <string.h>

#define INDYDRAW_MAXVERTS   4096
#define INDYDRAW_MAXINDICES 12288
#define INDYDRAW_SEGMENTS   32

static D3DTLVERTEX indyDraw_aVerts[INDYDRAW_MAXVERTS];
static uint16_t indyDraw_aIndices[INDYDRAW_MAXINDICES];
static size_t indyDraw_numVerts, indyDraw_numIndices;

void indyDraw_Begin(void)
{
    indyDraw_numVerts = indyDraw_numIndices = 0;
}

void indyDraw_End(void)
{
    if ( indyDraw_numIndices )
    {
        std3D_DrawRenderList(NULL, (Std3DRenderState)(STD3D_RS_ZWRITE_DISABLED | STD3D_RS_TEXFILTER_BILINEAR), indyDraw_aVerts,
            indyDraw_numVerts, indyDraw_aIndices, indyDraw_numIndices);
    }
    indyDraw_numVerts = indyDraw_numIndices = 0;
}

static bool indyDraw_Reserve(size_t numVerts, size_t numIndices)
{
    return indyDraw_numVerts + numVerts <= INDYDRAW_MAXVERTS && indyDraw_numIndices + numIndices <= INDYDRAW_MAXINDICES;
}

static uint16_t indyDraw_AddVertex(float x, float y, D3DCOLOR color)
{
    D3DTLVERTEX* pVert = &indyDraw_aVerts[indyDraw_numVerts];
    memset(pVert, 0, sizeof(*pVert));
    pVert->sx    = x;
    pVert->sy    = y;
    pVert->sz    = 0.0f; // in front of everything
    pVert->rhw   = 1.0f;
    pVert->color = color;
    return (uint16_t)indyDraw_numVerts++;
}

static void indyDraw_AddIndices(uint16_t a, uint16_t b, uint16_t c)
{
    indyDraw_aIndices[indyDraw_numIndices++] = a;
    indyDraw_aIndices[indyDraw_numIndices++] = b;
    indyDraw_aIndices[indyDraw_numIndices++] = c;
}

void indyDraw_Triangle(float x0, float y0, float x1, float y1, float x2, float y2, D3DCOLOR color)
{
    if ( indyDraw_Reserve(3, 3) )
    {
        const uint16_t a = indyDraw_AddVertex(x0, y0, color), b = indyDraw_AddVertex(x1, y1, color);
        indyDraw_AddIndices(a, b, indyDraw_AddVertex(x2, y2, color));
    }
}

void indyDraw_Rect(float x0, float y0, float x1, float y1, D3DCOLOR color)
{
    if ( !indyDraw_Reserve(4, 6) )
    {
        return;
    }
    const uint16_t a = indyDraw_AddVertex(x0, y0, color), b = indyDraw_AddVertex(x1, y0, color);
    const uint16_t c = indyDraw_AddVertex(x1, y1, color), d = indyDraw_AddVertex(x0, y1, color);
    indyDraw_AddIndices(a, b, c);
    indyDraw_AddIndices(a, c, d);
}

void indyDraw_Frame(float x0, float y0, float x1, float y1, float thickness, D3DCOLOR color)
{
    indyDraw_Rect(x0, y0, x1, y0 + thickness, color);
    indyDraw_Rect(x0, y1 - thickness, x1, y1, color);
    indyDraw_Rect(x0, y0 + thickness, x0 + thickness, y1 - thickness, color);
    indyDraw_Rect(x1 - thickness, y0 + thickness, x1, y1 - thickness, color);
}

void indyDraw_Disc(float cx, float cy, float r, D3DCOLOR color)
{
    if ( !indyDraw_Reserve(INDYDRAW_SEGMENTS + 1, INDYDRAW_SEGMENTS * 3) )
    {
        return;
    }
    const uint16_t centre = indyDraw_AddVertex(cx, cy, color);
    for ( int i = 0; i < INDYDRAW_SEGMENTS; ++i )
    {
        const float a = (float)i * (2.0f * 3.14159265f / INDYDRAW_SEGMENTS);
        indyDraw_AddVertex(cx + cosf(a) * r, cy + sinf(a) * r, color);
    }
    for ( int i = 0; i < INDYDRAW_SEGMENTS; ++i )
    {
        indyDraw_AddIndices(centre, (uint16_t)(centre + 1 + i), (uint16_t)(centre + 1 + (i + 1) % INDYDRAW_SEGMENTS));
    }
}

void indyDraw_Ring(float cx, float cy, float r, float thickness, D3DCOLOR color)
{
    if ( !indyDraw_Reserve(INDYDRAW_SEGMENTS * 2, INDYDRAW_SEGMENTS * 6) )
    {
        return;
    }
    const uint16_t first = (uint16_t)indyDraw_numVerts;
    for ( int i = 0; i < INDYDRAW_SEGMENTS; ++i )
    {
        const float a = (float)i * (2.0f * 3.14159265f / INDYDRAW_SEGMENTS);
        indyDraw_AddVertex(cx + cosf(a) * r, cy + sinf(a) * r, color);
        indyDraw_AddVertex(cx + cosf(a) * (r - thickness), cy + sinf(a) * (r - thickness), color);
    }
    for ( int i = 0; i < INDYDRAW_SEGMENTS; ++i )
    {
        const uint16_t o0 = (uint16_t)(first + 2 * i), i0 = (uint16_t)(o0 + 1);
        const uint16_t o1 = (uint16_t)(first + 2 * ((i + 1) % INDYDRAW_SEGMENTS)), i1 = (uint16_t)(o1 + 1);
        indyDraw_AddIndices(o0, o1, i0);
        indyDraw_AddIndices(i0, o1, i1);
    }
}
