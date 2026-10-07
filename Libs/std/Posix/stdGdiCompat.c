// Native builds: the few GDI calls the engine makes outside dialogs (savegame thumbnails): memory DCs, DIB sections,
// StretchBlt with area averaging (as STRETCH_HALFTONE) and GetObject. Declared in j3dcore/j3dwin32.h.
#include <j3dcore/j3d.h>
#include <stdlib.h>
#include <string.h>

typedef struct sGdiDC
{
    HBITMAP hSelected;
    int stretchMode;
} GdiDC;

typedef struct sGdiBitmap
{
    BITMAP bm;
    bool bTopDown;
} GdiBitmap;

HDC J3D_CreateCompatibleDC(HDC hdc)
{
    (void)hdc;
    return (HDC)calloc(1, sizeof(GdiDC));
}

BOOL J3D_DeleteDC(HDC hdc)
{
    free(hdc);
    return TRUE;
}

HBITMAP J3D_CreateDIBSection(HDC hdc, const BITMAPINFO* pbmi, UINT usage, void** ppvBits, HANDLE hSection, DWORD offset)
{
    (void)hdc; (void)usage; (void)hSection; (void)offset;
    const BITMAPINFOHEADER* pHdr = &pbmi->bmiHeader;
    if ( pHdr->biWidth <= 0 || pHdr->biHeight == 0 || (pHdr->biBitCount != 16 && pHdr->biBitCount != 24 && pHdr->biBitCount != 32) )
    {
        return NULL;
    }

    LONG height   = pHdr->biHeight < 0 ? -pHdr->biHeight : pHdr->biHeight;
    LONG rowBytes = ((pHdr->biWidth * pHdr->biBitCount + 31) / 32) * 4; // DWORD-aligned rows
    GdiBitmap* pBmp = calloc(1, sizeof(GdiBitmap) + (size_t)rowBytes * (size_t)height);
    if ( !pBmp )
    {
        return NULL;
    }

    pBmp->bm.bmWidth      = pHdr->biWidth;
    pBmp->bm.bmHeight     = height;
    pBmp->bm.bmWidthBytes = rowBytes;
    pBmp->bm.bmPlanes     = 1;
    pBmp->bm.bmBitsPixel  = pHdr->biBitCount;
    pBmp->bm.bmBits       = pBmp + 1;
    pBmp->bTopDown        = pHdr->biHeight < 0;
    if ( ppvBits )
    {
        *ppvBits = pBmp->bm.bmBits;
    }
    return (HBITMAP)pBmp;
}

HGDIOBJ J3D_SelectObject(HDC hdc, HGDIOBJ h)
{
    GdiDC* pDC = (GdiDC*)hdc;
    if ( !pDC ) return NULL;
    HGDIOBJ hPrev = pDC->hSelected;
    pDC->hSelected = h;
    return hPrev;
}

BOOL J3D_DeleteObject(HGDIOBJ h)
{
    free(h);
    return TRUE;
}

int J3D_SetStretchBltMode(HDC hdc, int mode)
{
    GdiDC* pDC = (GdiDC*)hdc;
    if ( !pDC ) return 0;
    int prev = pDC->stretchMode;
    pDC->stretchMode = mode;
    return prev;
}

// Same pixel format on both sides (the engine stretches RGB888 to RGB888); each destination pixel averages the source
// pixels it covers.
BOOL J3D_StretchBlt(HDC hdcDest, int xDest, int yDest, int wDest, int hDest, HDC hdcSrc, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop)
{
    (void)rop;
    GdiDC* pDst = (GdiDC*)hdcDest;
    GdiDC* pSrc = (GdiDC*)hdcSrc;
    if ( !pDst || !pSrc || !pDst->hSelected || !pSrc->hSelected || wDest <= 0 || hDest <= 0 || wSrc <= 0 || hSrc <= 0 )
    {
        return FALSE;
    }

    const GdiBitmap* pS = (const GdiBitmap*)pSrc->hSelected;
    GdiBitmap* pD       = (GdiBitmap*)pDst->hSelected;
    if ( pS->bm.bmBitsPixel != pD->bm.bmBitsPixel )
    {
        return FALSE;
    }

    const int bpp = pS->bm.bmBitsPixel / 8;
    for ( int y = 0; y < hDest; ++y )
    {
        int sy0 = ySrc + (y * hSrc) / hDest;
        int sy1 = ySrc + ((y + 1) * hSrc) / hDest;
        if ( sy1 <= sy0 ) sy1 = sy0 + 1;
        int dy = yDest + y;
        if ( dy < 0 || dy >= pD->bm.bmHeight ) continue;
        uint8_t* pRow = (uint8_t*)pD->bm.bmBits + (size_t)dy * pD->bm.bmWidthBytes;

        for ( int x = 0; x < wDest; ++x )
        {
            int sx0 = xSrc + (x * wSrc) / wDest;
            int sx1 = xSrc + ((x + 1) * wSrc) / wDest;
            if ( sx1 <= sx0 ) sx1 = sx0 + 1;
            int dx = xDest + x;
            if ( dx < 0 || dx >= pD->bm.bmWidth ) continue;

            uint32_t aSum[4] = { 0 };
            uint32_t n = 0;
            for ( int sy = sy0; sy < sy1 && sy < pS->bm.bmHeight; ++sy )
            {
                const uint8_t* pSrcRow = (const uint8_t*)pS->bm.bmBits + (size_t)sy * pS->bm.bmWidthBytes;
                for ( int sx = sx0; sx < sx1 && sx < pS->bm.bmWidth; ++sx )
                {
                    for ( int c = 0; c < bpp; ++c ) aSum[c] += pSrcRow[sx * bpp + c];
                    ++n;
                }
            }
            if ( !n ) continue;
            if ( bpp == 2 ) // 16-bit: no averaging across packed channels, take the first pixel
            {
                const uint8_t* pSrcRow = (const uint8_t*)pS->bm.bmBits + (size_t)sy0 * pS->bm.bmWidthBytes;
                memcpy(&pRow[dx * 2], &pSrcRow[sx0 * 2], 2);
                continue;
            }
            for ( int c = 0; c < bpp; ++c ) pRow[dx * bpp + c] = (uint8_t)((aSum[c] + n / 2) / n);
        }
    }
    return TRUE;
}

int J3D_GetObject(HGDIOBJ h, int size, void* pv)
{
    if ( !h || !pv || size < (int)sizeof(BITMAP) ) return 0;
    memcpy(pv, &((const GdiBitmap*)h)->bm, sizeof(BITMAP));
    return (int)sizeof(BITMAP);
}
