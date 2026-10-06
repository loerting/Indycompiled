#include "indyDisplayDX9.h"

#include <std/General/std.h>

#ifdef J3D_DIRECTX9

typedef struct sIndyQuadVertex
{
    float x, y, z, rhw;
    float u, v;
} IndyQuadVertex;

#define INDY_QUAD_FVF (D3DFVF_XYZRHW | D3DFVF_TEX1)

bool J3DAPI indyDisplay_IsMultisampled(LPDIRECT3DSURFACE9 pSurface)
{
    D3DSURFACE_DESC desc;
    return pSurface && SUCCEEDED(IDirect3DSurface9_GetDesc(pSurface, &desc)) && desc.MultiSampleType != D3DMULTISAMPLE_NONE;
}

bool J3DAPI indyDisplay_DrawSurfaceToTarget(LPDIRECT3DDEVICE9 pDevice, LPDIRECT3DSURFACE9 pSrc, LPDIRECT3DSURFACE9 pDest)
{
    if ( !pDevice || !pSrc || !pDest )
    {
        return false;
    }

    D3DSURFACE_DESC srcDesc, destDesc;
    if ( FAILED(IDirect3DSurface9_GetDesc(pSrc, &srcDesc)) || FAILED(IDirect3DSurface9_GetDesc(pDest, &destDesc)) )
    {
        return false;
    }

    // A render-target texture the source can be StretchRect'ed into (non-multisampled to non-multisampled is fine).
    // Created per call: this path only runs for CPU-drawn frames (videos, dialogs), and nothing has to survive a
    // device reset.
    LPDIRECT3DTEXTURE9 pTex = NULL;
    if ( FAILED(IDirect3DDevice9_CreateTexture(pDevice, srcDesc.Width, srcDesc.Height, 1, D3DUSAGE_RENDERTARGET,
        srcDesc.Format, D3DPOOL_DEFAULT, &pTex, NULL)) )
    {
        return false;
    }

    bool bOk = false;
    LPDIRECT3DSURFACE9 pTexSurf     = NULL;
    LPDIRECT3DSURFACE9 pOldTarget   = NULL;
    LPDIRECT3DSURFACE9 pOldDepth    = NULL;
    LPDIRECT3DSTATEBLOCK9 pState    = NULL;

    if ( FAILED(IDirect3DTexture9_GetSurfaceLevel(pTex, 0, &pTexSurf))
        || FAILED(IDirect3DDevice9_StretchRect(pDevice, pSrc, NULL, pTexSurf, NULL, D3DTEXF_NONE))
        || FAILED(IDirect3DDevice9_CreateStateBlock(pDevice, D3DSBT_ALL, &pState)) )
    {
        goto done;
    }

    IDirect3DDevice9_GetRenderTarget(pDevice, 0, &pOldTarget);
    IDirect3DDevice9_GetDepthStencilSurface(pDevice, &pOldDepth);

    IDirect3DDevice9_SetRenderTarget(pDevice, 0, pDest);
    IDirect3DDevice9_SetDepthStencilSurface(pDevice, NULL);

    IDirect3DDevice9_SetVertexShader(pDevice, NULL);
    IDirect3DDevice9_SetPixelShader(pDevice, NULL);
    IDirect3DDevice9_SetFVF(pDevice, INDY_QUAD_FVF);
    IDirect3DDevice9_SetRenderState(pDevice, D3DRS_ZENABLE, D3DZB_FALSE);
    IDirect3DDevice9_SetRenderState(pDevice, D3DRS_ZWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(pDevice, D3DRS_ALPHABLENDENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(pDevice, D3DRS_ALPHATESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(pDevice, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(pDevice, D3DRS_FOGENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(pDevice, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(pDevice, D3DRS_STENCILENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(pDevice, D3DRS_SCISSORTESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(pDevice, D3DRS_COLORWRITEENABLE, 0x0F);
    IDirect3DDevice9_SetTexture(pDevice, 0, (LPDIRECT3DBASETEXTURE9)pTex);
    IDirect3DDevice9_SetTextureStageState(pDevice, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(pDevice, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(pDevice, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(pDevice, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(pDevice, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetSamplerState(pDevice, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(pDevice, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(pDevice, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    IDirect3DDevice9_SetSamplerState(pDevice, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(pDevice, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    D3DVIEWPORT9 viewport = { 0, 0, destDesc.Width, destDesc.Height, 0.0f, 1.0f };
    IDirect3DDevice9_SetViewport(pDevice, &viewport);

    // -0.5: map texel centres onto pixel centres (Direct3D 9 rasterisation rules)
    const float w = (float)destDesc.Width - 0.5f, h = (float)destDesc.Height - 0.5f;
    const IndyQuadVertex aQuad[4] = {
        { -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
        { w,     -0.5f, 0.0f, 1.0f, 1.0f, 0.0f },
        { -0.5f, h,     0.0f, 1.0f, 0.0f, 1.0f },
        { w,     h,     0.0f, 1.0f, 1.0f, 1.0f },
    };

    // The copy-back may run inside or outside a scene.
    bool bBegan = SUCCEEDED(IDirect3DDevice9_BeginScene(pDevice));
    bOk = SUCCEEDED(IDirect3DDevice9_DrawPrimitiveUP(pDevice, D3DPT_TRIANGLESTRIP, 2, aQuad, sizeof(IndyQuadVertex)));
    if ( bBegan )
    {
        IDirect3DDevice9_EndScene(pDevice);
    }

    IDirect3DDevice9_SetTexture(pDevice, 0, NULL);
    IDirect3DStateBlock9_Apply(pState);
    IDirect3DDevice9_SetRenderTarget(pDevice, 0, pOldTarget);
    IDirect3DDevice9_SetDepthStencilSurface(pDevice, pOldDepth);

done:
    if ( pOldTarget ) IDirect3DSurface9_Release(pOldTarget);
    if ( pOldDepth ) IDirect3DSurface9_Release(pOldDepth);
    if ( pState ) IDirect3DStateBlock9_Release(pState);
    if ( pTexSurf ) IDirect3DSurface9_Release(pTexSurf);
    IDirect3DTexture9_Release(pTex);

    static bool bLogged;
    if ( !bLogged )
    {
        STDLOG_STATUS("indyDisplay: MSAA copy-back via quad %s\n", bOk ? "works" : "FAILED");
        bLogged = true;
    }
    return bOk;
}

#endif // J3D_DIRECTX9
