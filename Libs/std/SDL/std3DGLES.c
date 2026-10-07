// Native builds: the 3D device on OpenGL ES 3.0, the counterpart of std3DX9.c with the same look: pre-transformed
// vertices (D3DTLVERTEX), texture * vertex color, alpha blending, alpha test (> 0, or > 160 with STD3D_RS_ALPHAREF_SET),
// depth test <=, no culling, linear fog over the vertex depth, 32-bit textures. The shaders are the DirectX 9 build's
// (Shaders/default_vs.hlsl, default_ps.hlsl) in GLSL ES.
#include "stdGLES.h"

#include <std/General/std.h>
#include <std/General/stdColor.h>
#include <std/General/stdConfig.h>
#include <std/General/stdMath.h>
#include <std/General/stdMemory.h>
#include <std/General/stdUtil.h>
#include <std/Win95/std3D.h>
#include <std/Win95/stdDisplay.h>

#include <SDL3/SDL.h>
#include <math.h>

#define STD3D_MAXSTREAMVERTICES 65535

// Ring buffers for the streamed vertices and indices: each draw call appends behind the previous one, and the storage
// is orphaned only when full. (Orphaning per draw call made the Adreno driver allocate GPU memory, an ioctl, for every
// call: 2 fps on a phone.)
#define STD3D_RINGVERTICES (STD3D_MAXSTREAMVERTICES * 4)
#define STD3D_RINGINDICES  (STD3D_MAXSTREAMVERTICES * 3 * 4)

struct sStdGLTexture
{
    GLuint id;
};

typedef enum eStd3DTextureFormatIndex
{
    STD3D_TEXFORMAT_RGB  = 0, // bytes R, G, B, X
    STD3D_TEXFORMAT_RGBA = 1, // bytes R, G, B, A
} Std3DTextureFormatIndex;

extern uint32_t stdDisplay_g_glStateEpoch;
GLuint stdDisplay_GLES_CreateProgram(const char* pVertex, const char* pFragment, const char* const* apAttribs);

float std3D_g_fogDensity   = 1.0f;
size_t std3D_g_maxVertices = STD3D_MAXSTREAMVERTICES;

static bool std3D_bStartup;
static bool std3D_bOpen;
static bool std3D_bFindAllDevices;
static size_t std3D_numDevices;
static Device3D std3D_aDevices[1];
static Device3D* std3D_pCurDevice;

static size_t std3D_frameCount = 1;
static float std3D_zDepth;
static Std3DRenderState std3D_renderState;
static uint32_t std3D_glStateEpoch;
static tSysTexture* std3D_pCurTexture;
static bool std3D_bAnisotropicFilter;
static bool std3D_bAutoGenMipmap;
static float std3D_maxAnisotropy = 1.0f;
static Std3DMipmapFilterType std3D_mipmapFilter = -1;
static bool std3D_bLinearFilter;

static bool std3D_bRenderFog = true;
static float std3D_fogStartDepth;
static float std3D_fogEndDepth;
static float std3D_fogDepthFactor;
static float std3D_aFogColor[4];
static bool std3D_bFogActive;

static float std3D_aActiveRect[4]; // x1, y1, x2, y2

static size_t std3D_numTextureFormats;
static StdTextureFormat std3D_aTextureFormats[2];
static size_t std3D_numCachedTextures;
static tSystemTexture* std3D_pFirstTexCache;
static tSystemTexture* std3D_pLastTexCache;

static GLuint std3D_program;
static GLint std3D_locViewport, std3D_locTex, std3D_locFogParams, std3D_locFogColor, std3D_locAlphaRef;
static GLuint std3D_vao, std3D_vbo, std3D_ibo, std3D_sampler;
static size_t std3D_vboPos, std3D_iboPos; // next free entry in the ring buffers
static bool std3D_bScissor;               // std3D_SetScissor
static GLint std3D_aScissor[4];           // x, y (bottom-left origin), width, height

// Draw queue: rdCache sends a draw per face list, about 2400 a frame and most of them a face or two. std3D_DrawRenderList
// queues them with their vertices and indices; std3D_FlushDraws uploads all with one mapping each and replays them in
// order, merging consecutive draws with the same texture and state. The result is the same; the GL calls per draw drop
// from about 12 to 1, which mobile drivers need. Whatever changes GL state or ends the frame flushes first.
#define STD3D_QUEUEVERTICES 65535 // 16-bit indices
#define STD3D_QUEUEINDICES  (STD3D_QUEUEVERTICES * 3)
#define STD3D_QUEUEDRAWS    4096

typedef struct sStd3DQueuedDraw
{
    tSysTexture* pTex;
    Std3DRenderState rdflags;
    uint32_t firstIndex;
    uint32_t numIndices;
} Std3DQueuedDraw;

static D3DTLVERTEX std3D_aQueueVerts[STD3D_QUEUEVERTICES];
static uint16_t std3D_aQueueIndices[STD3D_QUEUEINDICES];
static Std3DQueuedDraw std3D_aQueueDraws[STD3D_QUEUEDRAWS];
static size_t std3D_numQueueVerts, std3D_numQueueIndices, std3D_numQueueDraws;
size_t std3D_g_numDrawCalls, std3D_g_numDrawVertices, std3D_g_numGLDraws; // statistics for INDY_FPS_LOG (stdDisplaySDL.c)
static struct sStdGLTexture std3D_whiteTexture;

static void J3DAPI std3D_AddTextureToCacheList(tSystemTexture* pTexture);
static void J3DAPI std3D_RemoveTextureFromCacheList(tSystemTexture* pCacheTexture);
static int J3DAPI std3D_PurgeTextureCache(size_t size);

void std3D_InstallHooks(void) {}
void std3D_ResetGlobals(void) {}

static const char* std3D_vertexShader =
    "#version 300 es\n"
    "uniform vec4 uViewport;\n" // x1, y1, x2, y2
    "in vec4 aPos;\n"            // sx, sy, sz, rhw
    "in vec4 aColor;\n"          // D3DCOLOR bytes: B, G, R, A
    "in vec2 aUV;\n"
    "out vec4 vColor;\n"
    "out vec2 vUV;\n"
    "out float vDepth;\n"
    "void main() {\n"
    "    float width  = uViewport.z - uViewport.x;\n"
    "    float height = uViewport.w - uViewport.y;\n"
    "    float ndcX   = ((aPos.x - uViewport.x) / width) * 2.0 - 1.0;\n"
    "    float ndcY   = (1.0 - (aPos.y - uViewport.y) / height) * 2.0 - 1.0;\n"
    "    float w      = 1.0 / aPos.w;\n"
    "    gl_Position  = vec4(ndcX * w, ndcY * w, (aPos.z * 2.0 - 1.0) * w, w);\n" // D3D depth 0..1 -> GL -1..1
    "    vColor = aColor.bgra;\n"
    "    vUV    = aUV;\n"
    "    vDepth = w;\n"
    "}\n";

static const char* std3D_fragmentShader =
    "#version 300 es\n"
    "precision highp float;\n"
    "uniform sampler2D uTex;\n"
    "uniform vec4 uFogParams;\n" // start, end, 1 / (end - start), enabled
    "uniform vec4 uFogColor;\n"
    "uniform float uAlphaRef;\n"
    "in vec4 vColor;\n"
    "in vec2 vUV;\n"
    "in float vDepth;\n"
    "out vec4 oColor;\n"
    "void main() {\n"
    "    vec4 color = texture(uTex, vUV) * vColor;\n"
    "    if ( color.a * 255.0 <= uAlphaRef ) discard;\n" // alpha test: greater than the reference
    "    if ( uFogParams.w > 0.0 ) {\n"
    "        float fogFactor = clamp((vDepth - uFogParams.x) * uFogParams.z, 0.0, 1.0);\n"
    "        color.rgb = mix(color.rgb, uFogColor.rgb, fogFactor);\n"
    "    }\n"
    "    oColor = color;\n"
    "}\n";

int std3D_Startup(void)
{
    STD_ASSERTREL(std3D_bStartup == false);
    memset(std3D_aDevices, 0, sizeof(std3D_aDevices));
    std3D_numDevices = 0;

    StdDisplayDevice displayDevice = { 0 };
    if ( stdDisplay_GetNumDevices() == 0 || stdDisplay_GetDevice(0, &displayDevice) )
    {
        return 0;
    }

    Device3D* pDevice = &std3D_aDevices[0];
    STD_STRCPY(pDevice->deviceDescription, displayDevice.aDeviceName);
    STD_STRCPY(pDevice->deviceName, displayDevice.aDriverName);
    pDevice->bHAL                         = 1;
    pDevice->bTexturePerspectiveSupported = 1;
    pDevice->hasZBuffer                   = 1;
    pDevice->bColorkeyTextureSupported    = 1;
    pDevice->bAlphaTextureSupported       = 1;
    pDevice->bAlphaBlendSupported         = 1;
    pDevice->bSqareOnlyTexture            = 0;
    pDevice->minTexWidth                  = 1;
    pDevice->minTexHeight                 = 1;
    pDevice->maxTexWidth                  = (size_t)displayDevice.caps.maxTextureSize;
    pDevice->maxTexHeight                 = (size_t)displayDevice.caps.maxTextureSize;
    pDevice->maxVertexCount               = STD3D_MAXSTREAMVERTICES;
    pDevice->totalMemory                  = displayDevice.totalVideoMemory;
    pDevice->availableMemory              = displayDevice.freeVideoMemory;
    pDevice->d3dDesc                      = displayDevice.caps;
    std3D_numDevices = 1;

    std3D_bStartup = true;
    return 1;
}

void std3D_Shutdown(void)
{
    if ( !std3D_bStartup )
    {
        return;
    }
    if ( std3D_bOpen )
    {
        std3D_Close();
    }
    std3D_numDevices = 0;
    std3D_bStartup   = false;
}

const Device3D* std3D_GetCurrentDevice(void) { return std3D_pCurDevice; }
size_t std3D_GetNumDevices(void) { return std3D_numDevices; }
const Device3D* std3D_GetAllDevices(void) { return std3D_aDevices; }

void J3DAPI std3D_SetFindAllDevices(int bFindAll)
{
    std3D_bFindAllDevices = bFindAll;
}

static void std3D_ApplySamplerFilter(void)
{
    GLint mag = std3D_bLinearFilter ? GL_LINEAR : GL_NEAREST;
    GLint min = mag;
    if ( std3D_mipmapFilter == STD3D_MIPMAPFILTER_BILINEAR )
    {
        min = std3D_bLinearFilter ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST;
    }
    else if ( std3D_mipmapFilter == STD3D_MIPMAPFILTER_TRILINEAR )
    {
        min = std3D_bLinearFilter ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;
    }
    glSamplerParameteri(std3D_sampler, GL_TEXTURE_MAG_FILTER, mag);
    glSamplerParameteri(std3D_sampler, GL_TEXTURE_MIN_FILTER, min);
    if ( std3D_maxAnisotropy > 1.0f )
    {
        bool bAniso = (std3D_renderState & STD3D_RS_TEXFILTER_ANISOTROPIC) != 0 && std3D_bAnisotropicFilter;
        glSamplerParameterf(std3D_sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, bAniso ? std3D_maxAnisotropy : 1.0f);
    }
}

static void std3D_ApplyFog(void)
{
    if ( std3D_bFogActive )
    {
        glUniform4f(std3D_locFogParams, std3D_fogStartDepth, std3D_fogEndDepth, std3D_fogDepthFactor, 1.0f);
        glUniform4f(std3D_locFogColor, std3D_aFogColor[0], std3D_aFogColor[1], std3D_aFogColor[2], std3D_aFogColor[3]);
    }
    else
    {
        glUniform4f(std3D_locFogParams, 0.0f, 0.0f, 0.0f, 0.0f);
    }
}

// The fixed state, as std3DX9.c's std3D_InitRenderState; also restored after the display module used GL
static void std3D_ApplyScissor(void)
{
    if ( std3D_bScissor )
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(std3D_aScissor[0], std3D_aScissor[1], std3D_aScissor[2], std3D_aScissor[3]);
    }
    else
    {
        glDisable(GL_SCISSOR_TEST);
    }
}

static void std3D_SetupGLState(void)
{
    glViewport(0, 0, (GLsizei)stdDisplay_g_backBuffer.rasterInfo.width, (GLsizei)stdDisplay_g_backBuffer.rasterInfo.height);
    std3D_ApplyScissor();
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask((std3D_renderState & STD3D_RS_ZWRITE_DISABLED) ? GL_FALSE : GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(std3D_program);
    glBindVertexArray(std3D_vao);
    glActiveTexture(GL_TEXTURE0);
    glBindSampler(0, std3D_sampler);
    glUniform1i(std3D_locTex, 0);
    glUniform4f(std3D_locViewport, std3D_aActiveRect[0], std3D_aActiveRect[1], std3D_aActiveRect[2], std3D_aActiveRect[3]);
    glUniform1f(std3D_locAlphaRef, (std3D_renderState & STD3D_RS_ALPHAREF_SET) ? 160.0f : 0.0f);
    std3D_ApplyFog();
    glBindTexture(GL_TEXTURE_2D, std3D_pCurTexture ? std3D_pCurTexture->id : std3D_whiteTexture.id);
    std3D_glStateEpoch = stdDisplay_g_glStateEpoch;
}

// 32-bit RGBA with the bytes R, G, B, A in memory: OpenGL's GL_RGBA/GL_UNSIGNED_BYTE (stdColor_cfRGBA8888 is A, B, G, R)
static const ColorInfo std3D_cfGLRGBA8888 =
{
    .colorMode     = STDCOLOR_RGBA,
    .bpp           = 32,
    .redBPP        = 8,
    .greenBPP      = 8,
    .blueBPP       = 8,
    .redPosShift   = 0,
    .greenPosShift = 8,
    .bluePosShift  = 16,
    .alphaBPP      = 8,
    .alphaPosShift = 24,
};

static void std3D_InitTextureFormats(void)
{
    // Byte order R, G, B, A: the VBuffers are uploaded as they are (RGB textures read alpha as 1)
    memset(std3D_aTextureFormats, 0, sizeof(std3D_aTextureFormats));
    std3D_aTextureFormats[STD3D_TEXFORMAT_RGB].ci          = stdColor_cfBGR8888;
    std3D_aTextureFormats[STD3D_TEXFORMAT_RGB].ddPixelFmt  = STD3D_TEXFORMAT_RGB;
    std3D_aTextureFormats[STD3D_TEXFORMAT_RGBA].ci         = std3D_cfGLRGBA8888;
    std3D_aTextureFormats[STD3D_TEXFORMAT_RGBA].ddPixelFmt = STD3D_TEXFORMAT_RGBA;
    std3D_numTextureFormats = 2;
}

int J3DAPI std3D_Open(size_t deviceNum)
{
    STD_ASSERTREL(std3D_bStartup == true);
    if ( std3D_bOpen || deviceNum >= std3D_numDevices || !stdDisplay_IsOpen() || !stdDisplay_GetSystemDevice() )
    {
        STDLOG_ERROR("std3D_Open: display not ready or device %d invalid.\n", (int)deviceNum);
        return 0;
    }

    std3D_pCurDevice = &std3D_aDevices[deviceNum];

    static const char* const aAttribs[] = { "aPos", "aColor", "aUV", NULL };
    std3D_program = stdDisplay_GLES_CreateProgram(std3D_vertexShader, std3D_fragmentShader, aAttribs);
    if ( !std3D_program )
    {
        return 0;
    }
    std3D_locViewport  = glGetUniformLocation(std3D_program, "uViewport");
    std3D_locTex       = glGetUniformLocation(std3D_program, "uTex");
    std3D_locFogParams = glGetUniformLocation(std3D_program, "uFogParams");
    std3D_locFogColor  = glGetUniformLocation(std3D_program, "uFogColor");
    std3D_locAlphaRef  = glGetUniformLocation(std3D_program, "uAlphaRef");

    // Streaming vertex and index buffers, D3DTLVERTEX layout
    glGenVertexArrays(1, &std3D_vao);
    glBindVertexArray(std3D_vao);
    glGenBuffers(1, &std3D_vbo);
    glGenBuffers(1, &std3D_ibo);
    glBindBuffer(GL_ARRAY_BUFFER, std3D_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(STD3D_RINGVERTICES * sizeof(D3DTLVERTEX)), NULL, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, std3D_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(STD3D_RINGINDICES * sizeof(uint16_t)), NULL, GL_STREAM_DRAW);
    std3D_vboPos = std3D_iboPos = 0;
    std3D_numQueueVerts = std3D_numQueueIndices = std3D_numQueueDraws = 0;
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(D3DTLVERTEX), (const void*)offsetof(D3DTLVERTEX, sx));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(D3DTLVERTEX), (const void*)offsetof(D3DTLVERTEX, color));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(D3DTLVERTEX), (const void*)offsetof(D3DTLVERTEX, tu));
    glBindVertexArray(0);

    // White texture: drawing without a texture
    const uint8_t aWhite[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    glGenTextures(1, &std3D_whiteTexture.id);
    glBindTexture(GL_TEXTURE_2D, std3D_whiteTexture.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, aWhite);

    // Sampler state (D3D's sampler stage 0)
    glGenSamplers(1, &std3D_sampler);
    glSamplerParameteri(std3D_sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(std3D_sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);

    std3D_bAutoGenMipmap     = stdConfig_GetBool(STD3D_CFG_MIPMAPAUTOGEN, true);
    std3D_bAnisotropicFilter = stdConfig_GetBool(STD3D_CFG_ANISOTROPICFILTER, true);
    std3D_maxAnisotropy      = 1.0f;
    if ( stdGLES_HasExtension("GL_EXT_texture_filter_anisotropic") )
    {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &std3D_maxAnisotropy);
    }
    std3D_pCurDevice->bAnisotropicFilteringSupported = std3D_maxAnisotropy > 1.0f;
    std3D_pCurDevice->bMipmapAutoGenSupported        = true;

    std3D_InitTextureFormats();

    std3D_aActiveRect[0] = 0.0f;
    std3D_aActiveRect[1] = 0.0f;
    std3D_aActiveRect[2] = (float)stdDisplay_g_backBuffer.rasterInfo.width;
    std3D_aActiveRect[3] = (float)stdDisplay_g_backBuffer.rasterInfo.height;

    std3D_g_maxVertices     = STD3D_MAXSTREAMVERTICES;
    std3D_frameCount        = 1;
    std3D_numCachedTextures = 0;
    std3D_pFirstTexCache    = NULL;
    std3D_pLastTexCache     = NULL;
    std3D_pCurTexture       = NULL;
    std3D_renderState       = 0;
    std3D_bLinearFilter     = false;
    std3D_mipmapFilter      = -1;
    std3D_bFogActive        = false;
    std3D_SetMipmapFilter(STD3D_MIPMAPFILTER_NONE);
    std3D_SetupGLState();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    STDLOG_STATUS("Texture Ram  Total: %u bytes  Free: %u bytes.\n", (unsigned)std3D_pCurDevice->totalMemory, (unsigned)std3D_pCurDevice->availableMemory);
    std3D_bOpen = true;
    return 1;
}

void std3D_Close(void)
{
    if ( !std3D_bOpen )
    {
        return;
    }

    std3D_numQueueVerts = std3D_numQueueIndices = std3D_numQueueDraws = 0; // nothing more will be shown

    std3D_ResetTextureCache();
    glDeleteTextures(1, &std3D_whiteTexture.id);
    glDeleteSamplers(1, &std3D_sampler);
    glDeleteBuffers(1, &std3D_vbo);
    glDeleteBuffers(1, &std3D_ibo);
    glDeleteVertexArrays(1, &std3D_vao);
    glDeleteProgram(std3D_program);
    std3D_program = std3D_vao = std3D_vbo = std3D_ibo = std3D_sampler = 0;
    std3D_whiteTexture.id = 0;

    std3D_mipmapFilter      = -1;
    std3D_numTextureFormats = 0;
    std3D_pCurDevice        = NULL;
    std3D_bOpen             = false;
}

void J3DAPI std3D_GetTextureFormat(StdColorFormatType type, ColorInfo* pDest, int* pbColorKeySet, LPDDCOLORKEY* ppColorKey)
{
    const StdTextureFormat* pFormat = &std3D_aTextureFormats[type == STDCOLOR_FORMAT_RGB ? STD3D_TEXFORMAT_RGB : STD3D_TEXFORMAT_RGBA];
    *pbColorKeySet = type == STDCOLOR_FORMAT_RGB ? 0 : pFormat->bColorKey;
    if ( type != STDCOLOR_FORMAT_RGB ) *ppColorKey = pFormat->pColorKey;
    *pDest = pFormat->ci;
}

StdColorFormatType J3DAPI std3D_GetColorFormat(const ColorInfo* pCi)
{
    if ( pCi->alphaBPP == 0 ) return STDCOLOR_FORMAT_RGB;
    if ( pCi->alphaBPP == 1 ) return STDCOLOR_FORMAT_RGBA_1BITALPHA;
    return STDCOLOR_FORMAT_RGBA;
}

size_t std3D_GetNumTextureFormats(void)
{
    return std3D_numTextureFormats;
}

int std3D_StartScene(void)
{
    ++std3D_frameCount;
    std3D_SetupGLState();
    return 0;
}

void std3D_EndScene(void)
{
    std3D_FlushDraws();
}

static void std3D_BindTexture(tSysTexture* pTex)
{
    std3D_pCurTexture = pTex;
    glBindTexture(GL_TEXTURE_2D, pTex ? pTex->id : std3D_whiteTexture.id);
}

static void std3D_ApplyRenderState(Std3DRenderState rdflags)
{
    if ( std3D_renderState == rdflags )
    {
        return;
    }

    const Std3DRenderState changed = std3D_renderState ^ rdflags;
    std3D_renderState = rdflags;

    if ( changed & STD3D_RS_ZWRITE_DISABLED )
    {
        glDepthMask((rdflags & STD3D_RS_ZWRITE_DISABLED) ? GL_FALSE : GL_TRUE);
    }
    if ( changed & STD3D_RS_TEX_CPAMP_U )
    {
        glSamplerParameteri(std3D_sampler, GL_TEXTURE_WRAP_S, (rdflags & STD3D_RS_TEX_CPAMP_U) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    }
    if ( changed & STD3D_RS_TEX_CPAMP_V )
    {
        glSamplerParameteri(std3D_sampler, GL_TEXTURE_WRAP_T, (rdflags & STD3D_RS_TEX_CPAMP_V) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    }
    if ( changed & STD3D_RS_FOG_ENABLED )
    {
        std3D_bFogActive = (rdflags & STD3D_RS_FOG_ENABLED) != 0 && std3D_bRenderFog;
        std3D_ApplyFog();
    }
    if ( changed & (STD3D_RS_TEXFILTER_ANISOTROPIC | STD3D_RS_TEXFILTER_BILINEAR) )
    {
        // As std3DX9.c: anisotropic implies linear; without it, bilinear or point
        std3D_bLinearFilter = (rdflags & (STD3D_RS_TEXFILTER_BILINEAR | STD3D_RS_TEXFILTER_ANISOTROPIC)) != 0;
        std3D_ApplySamplerFilter();
    }
    if ( changed & STD3D_RS_ALPHAREF_SET )
    {
        glUniform1f(std3D_locAlphaRef, (rdflags & STD3D_RS_ALPHAREF_SET) ? 160.0f : 0.0f);
    }
}

// Appends count elements to the ring buffer bound to target and returns the index of the first one. Unsynchronized
// mapping is safe: a range is written again only after the storage was orphaned.
static size_t std3D_StreamData(GLenum target, size_t* pPos, size_t capacity, size_t elemSize, const void* pData, size_t count)
{
    if ( *pPos + count > capacity )
    {
        glBufferData(target, (GLsizeiptr)(capacity * elemSize), NULL, GL_STREAM_DRAW); // orphan
        *pPos = 0;
    }

    const size_t first = *pPos;
    // No GL_MAP_INVALIDATE_RANGE_BIT and no glBufferSubData: on Adreno both allocate per call (2.6 fps against 50, measured)
    void* pDest = glMapBufferRange(target, (GLintptr)(first * elemSize), (GLsizeiptr)(count * elemSize), GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
    if ( pDest )
    {
        memcpy(pDest, pData, count * elemSize);
        glUnmapBuffer(target);
    }
    else
    {
        glBufferSubData(target, (GLintptr)(first * elemSize), (GLsizeiptr)(count * elemSize), pData);
    }

    *pPos = first + count;
    return first;
}

// Uploads the vertices and points the vertex attributes at them
static void std3D_StreamVertices(LPD3DTLVERTEX aVerts, size_t numVerts)
{
    glBindBuffer(GL_ARRAY_BUFFER, std3D_vbo);
    const size_t base = std3D_StreamData(GL_ARRAY_BUFFER, &std3D_vboPos, STD3D_RINGVERTICES, sizeof(D3DTLVERTEX), aVerts, numVerts) * sizeof(D3DTLVERTEX);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(D3DTLVERTEX), (const void*)(base + offsetof(D3DTLVERTEX, sx)));
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(D3DTLVERTEX), (const void*)(base + offsetof(D3DTLVERTEX, color)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(D3DTLVERTEX), (const void*)(base + offsetof(D3DTLVERTEX, tu)));
}

static void std3D_PrepareDraw(void)
{
    if ( std3D_glStateEpoch != stdDisplay_g_glStateEpoch )
    {
        std3D_SetupGLState();
    }
}

void std3D_FlushDraws(void)
{
    if ( !std3D_numQueueDraws )
    {
        return;
    }

    std3D_PrepareDraw();
    std3D_StreamVertices(std3D_aQueueVerts, std3D_numQueueVerts);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, std3D_ibo);
    const size_t firstIndex = std3D_StreamData(GL_ELEMENT_ARRAY_BUFFER, &std3D_iboPos, STD3D_RINGINDICES, sizeof(uint16_t), std3D_aQueueIndices, std3D_numQueueIndices);

    for ( size_t i = 0; i < std3D_numQueueDraws; ++i )
    {
        const Std3DQueuedDraw* pDraw = &std3D_aQueueDraws[i];
        std3D_ApplyRenderState(pDraw->rdflags);
        if ( pDraw->pTex != std3D_pCurTexture )
        {
            std3D_BindTexture(pDraw->pTex);
        }
        glDrawElements(GL_TRIANGLES, (GLsizei)pDraw->numIndices, GL_UNSIGNED_SHORT, (const void*)((firstIndex + pDraw->firstIndex) * sizeof(uint16_t)));
    }
    std3D_g_numGLDraws += std3D_numQueueDraws;

    std3D_numQueueVerts = std3D_numQueueIndices = std3D_numQueueDraws = 0;
}

void J3DAPI std3D_SetRenderState(Std3DRenderState rdflags)
{
    std3D_FlushDraws();
    std3D_ApplyRenderState(rdflags);
}

void J3DAPI std3D_DrawRenderList(tSysTexture* pTex, Std3DRenderState rdflags, LPD3DTLVERTEX aVerts, size_t numVerts, LPWORD aIndices, size_t numIndices)
{
    if ( numVerts > STD3D_QUEUEVERTICES || numIndices > STD3D_QUEUEINDICES )
    {
        STDLOG_ERROR("Error %d > %d maxVertices.\n", (int)numVerts, (int)std3D_g_maxVertices);
        return;
    }

    if ( std3D_numQueueVerts + numVerts > STD3D_QUEUEVERTICES || std3D_numQueueIndices + numIndices > STD3D_QUEUEINDICES
        || std3D_numQueueDraws == STD3D_QUEUEDRAWS )
    {
        std3D_FlushDraws();
    }

    ++std3D_g_numDrawCalls;
    std3D_g_numDrawVertices += numVerts;

    // Indices relative to the queued vertices
    const uint16_t base = (uint16_t)std3D_numQueueVerts;
    memcpy(&std3D_aQueueVerts[std3D_numQueueVerts], aVerts, numVerts * sizeof(D3DTLVERTEX));
    uint16_t* pIndices = &std3D_aQueueIndices[std3D_numQueueIndices];
    for ( size_t i = 0; i < numIndices; ++i )
    {
        pIndices[i] = (uint16_t)(aIndices[i] + base);
    }

    Std3DQueuedDraw* pLast = std3D_numQueueDraws ? &std3D_aQueueDraws[std3D_numQueueDraws - 1] : NULL;
    if ( pLast && pLast->pTex == pTex && pLast->rdflags == rdflags )
    {
        pLast->numIndices += (uint32_t)numIndices; // contiguous: one draw
    }
    else
    {
        Std3DQueuedDraw* pDraw = &std3D_aQueueDraws[std3D_numQueueDraws++];
        pDraw->pTex       = pTex;
        pDraw->rdflags    = rdflags;
        pDraw->firstIndex = (uint32_t)std3D_numQueueIndices;
        pDraw->numIndices = (uint32_t)numIndices;
    }

    std3D_numQueueVerts += numVerts;
    std3D_numQueueIndices += numIndices;
}

static void std3D_DrawArrays(GLenum mode, LPD3DTLVERTEX aVerts, size_t numVerts)
{
    if ( numVerts > std3D_g_maxVertices )
    {
        STDLOG_ERROR("Error %d > %d maxVertices.\n", (int)numVerts, (int)std3D_g_maxVertices);
        return;
    }

    std3D_FlushDraws();
    std3D_PrepareDraw();
    std3D_StreamVertices(aVerts, numVerts);
    glDrawArrays(mode, 0, (GLsizei)numVerts);
}

void std3D_SetWireframeRenderState(void)
{
    std3D_SetRenderState(std3D_renderState & ~(STD3D_RS_FOG_ENABLED | STD3D_RS_UNKNOWN_400 | STD3D_RS_UNKNOWN_200));
    std3D_BindTexture(NULL);
}

void J3DAPI std3D_DrawLineStrip(LPD3DTLVERTEX aVerts, size_t numVerts)
{
    std3D_DrawArrays(GL_LINE_STRIP, aVerts, numVerts);
}

void J3DAPI std3D_DrawPointList(LPD3DTLVERTEX aVerts, size_t numVerts)
{
    std3D_DrawArrays(GL_POINTS, aVerts, numVerts);
}

// ---- textures: as std3DX9.c, memory copies of the mip levels, uploaded when first used ------------------------

void J3DAPI std3D_GetValidDimensions(uint32_t width, uint32_t height, uint32_t* pOutWidth, uint32_t* pOutHeight)
{
    *pOutWidth  = STDMATH_CLAMP(width, std3D_pCurDevice->minTexWidth, std3D_pCurDevice->maxTexWidth);
    *pOutHeight = STDMATH_CLAMP(height, std3D_pCurDevice->minTexHeight, std3D_pCurDevice->maxTexHeight);
}

void J3DAPI std3D_AllocSystemTexture(tSystemTexture* pTexture, tVBuffer** apVBuffers, size_t numMipLevels, StdColorFormatType formatType)
{
    STD_ZEROMEM(pTexture, sizeof(tSystemTexture));
    if ( !std3D_numTextureFormats )
    {
        return;
    }

    tVBuffer* pVBuffer = *apVBuffers;
    uint32_t texWidth = 0, texHeight = 0;
    std3D_GetValidDimensions(pVBuffer->rasterInfo.width, pVBuffer->rasterInfo.height, &texWidth, &texHeight);
    while ( numMipLevels > 1 && (pVBuffer->rasterInfo.width > texWidth || pVBuffer->rasterInfo.height > texHeight) )
    {
        --numMipLevels;
        pVBuffer = *++apVBuffers;
        std3D_GetValidDimensions(pVBuffer->rasterInfo.width, pVBuffer->rasterInfo.height, &texWidth, &texHeight);
    }

    if ( std3D_mipmapFilter == STD3D_MIPMAPFILTER_NONE )
    {
        numMipLevels = 1;
    }

    pTexture->apMipmaps = (tVBuffer**)STDMALLOC(numMipLevels * sizeof(tVBuffer*));
    if ( !pTexture->apMipmaps )
    {
        STDLOG_ERROR("Failed to allocate memory for VBuffer array.\n");
        goto error;
    }
    memset(pTexture->apMipmaps, 0, numMipLevels * sizeof(tVBuffer*));

    pTexture->numMipLevels = numMipLevels;
    pTexture->format       = formatType == STDCOLOR_FORMAT_RGB ? STD3D_TEXFORMAT_RGB : STD3D_TEXFORMAT_RGBA;
    pTexture->textureSize  = (pVBuffer->rasterInfo.colorInfo.bpp * texHeight * texWidth) / 8;

    for ( size_t mmNum = 0; mmNum < numMipLevels; ++mmNum )
    {
        if ( apVBuffers[mmNum]->rasterInfo.colorInfo.colorMode == STDCOLOR_PAL )
        {
            STDLOG_ERROR("Can't use paletized textures.\n");
            goto error;
        }

        pTexture->apMipmaps[mmNum] = stdDisplay_VBufferNew(&apVBuffers[mmNum]->rasterInfo, 0, 0);
        if ( !pTexture->apMipmaps[mmNum] )
        {
            STDLOG_ERROR("Failed to create VBuffer for mip level %d.\n", (int)mmNum);
            goto error;
        }
        memcpy(pTexture->apMipmaps[mmNum]->pPixels, apVBuffers[mmNum]->pPixels, apVBuffers[mmNum]->rasterInfo.height * apVBuffers[mmNum]->rasterInfo.rowSize);
    }
    return;

error:
    if ( pTexture->apMipmaps )
    {
        for ( size_t i = 0; i < numMipLevels; ++i )
        {
            if ( pTexture->apMipmaps[i] ) stdDisplay_VBufferFree(pTexture->apMipmaps[i]);
        }
        stdMemory_Free(pTexture->apMipmaps);
    }
    STD_ZEROMEM(pTexture, sizeof(tSystemTexture));
}

void J3DAPI std3D_ClearSystemTexture(tSystemTexture* pTex)
{
    std3D_FlushDraws(); // queued draws may use the texture
    while ( pTex->numMipLevels > 0 )
    {
        stdDisplay_VBufferFree(pTex->apMipmaps[--pTex->numMipLevels]);
    }
    if ( pTex->apMipmaps )
    {
        stdMemory_Free(pTex->apMipmaps);
        pTex->apMipmaps = NULL;
    }
    if ( pTex->pCachedTexture )
    {
        std3D_RemoveTextureFromCacheList(pTex);
        if ( std3D_pCurTexture == pTex->pCachedTexture ) std3D_BindTexture(NULL);
        glDeleteTextures(1, &pTex->pCachedTexture->id);
        stdMemory_Free(pTex->pCachedTexture);
    }
    STD_ZEROMEM(pTex, sizeof(tSystemTexture));
}

void J3DAPI std3D_AddToTextureCache(tSystemTexture* pCacheTexture, StdColorFormatType format)
{
    J3D_UNUSED(format);
    std3D_FlushDraws(); // the cache may evict textures that queued draws use
    STD_ASSERTREL(pCacheTexture);
    if ( pCacheTexture->numMipLevels == 0 || !pCacheTexture->apMipmaps )
    {
        STDLOG_ERROR("No Source texture.\n");
        return;
    }

    if ( pCacheTexture->textureSize > std3D_pCurDevice->availableMemory )
    {
        std3D_PurgeTextureCache(pCacheTexture->textureSize);
    }

    tSysTexture* pTex = (tSysTexture*)STDMALLOC(sizeof(tSysTexture));
    if ( !pTex )
    {
        return;
    }

    const size_t numLevels = std3D_bAutoGenMipmap ? 1 : pCacheTexture->numMipLevels;
    glGenTextures(1, &pTex->id);
    glBindTexture(GL_TEXTURE_2D, pTex->id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    for ( size_t level = 0; level < numLevels; ++level )
    {
        const tVBuffer* pLevel = pCacheTexture->apMipmaps[level];
        glTexImage2D(GL_TEXTURE_2D, (GLint)level, GL_RGBA8, (GLsizei)pLevel->rasterInfo.width, (GLsizei)pLevel->rasterInfo.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pLevel->pPixels);
    }

    if ( pCacheTexture->format == STD3D_TEXFORMAT_RGB )
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE); // X8R8G8B8: no alpha
    }

    if ( std3D_bAutoGenMipmap && pCacheTexture->numMipLevels > 1 )
    {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (GLint)numLevels - 1);
    }

    glBindTexture(GL_TEXTURE_2D, std3D_pCurTexture ? std3D_pCurTexture->id : std3D_whiteTexture.id);

    pCacheTexture->pCachedTexture = pTex;
    pCacheTexture->frameNum       = std3D_frameCount;
    std3D_AddTextureToCacheList(pCacheTexture);
}

size_t J3DAPI std3D_GetMipMapCount(const tSystemTexture* pTexture)
{
    return pTexture ? pTexture->numMipLevels : 0;
}

void std3D_ResetTextureCache(void)
{
    std3D_FlushDraws();
    if ( std3D_bOpen )
    {
        std3D_BindTexture(NULL);
    }

    tSystemTexture* pCurTex = std3D_pFirstTexCache;
    while ( pCurTex )
    {
        if ( pCurTex->pCachedTexture )
        {
            glDeleteTextures(1, &pCurTex->pCachedTexture->id);
            stdMemory_Free(pCurTex->pCachedTexture);
            pCurTex->pCachedTexture = NULL;
        }
        tSystemTexture* pNextTex    = pCurTex->pNextCachedTexture;
        pCurTex->frameNum           = 0;
        pCurTex->pNextCachedTexture = NULL;
        pCurTex->pPrevCachedTexture = NULL;
        pCurTex = pNextTex;
    }

    std3D_pFirstTexCache    = NULL;
    std3D_pLastTexCache     = NULL;
    std3D_numCachedTextures = 0;
    if ( std3D_pCurDevice )
    {
        std3D_pCurDevice->availableMemory = std3D_pCurDevice->totalMemory;
    }
    std3D_frameCount = 1;
}

void J3DAPI std3D_UpdateFrameCount(tSystemTexture* pTexture)
{
    pTexture->frameNum = std3D_frameCount;
    std3D_RemoveTextureFromCacheList(pTexture);
    std3D_AddTextureToCacheList(pTexture);
}

size_t J3DAPI std3D_FindClosestFormat(const ColorInfo* pMatch)
{
    return pMatch->alphaBPP ? STD3D_TEXFORMAT_RGBA : STD3D_TEXFORMAT_RGB;
}

static void J3DAPI std3D_AddTextureToCacheList(tSystemTexture* pTexture)
{
    if ( std3D_pFirstTexCache )
    {
        std3D_pLastTexCache->pNextCachedTexture = pTexture;
        pTexture->pPrevCachedTexture            = std3D_pLastTexCache;
        pTexture->pNextCachedTexture            = NULL;
        std3D_pLastTexCache                     = pTexture;
    }
    else
    {
        std3D_pLastTexCache          = pTexture;
        std3D_pFirstTexCache         = pTexture;
        pTexture->pPrevCachedTexture = NULL;
        pTexture->pNextCachedTexture = NULL;
    }
    ++std3D_numCachedTextures;
    std3D_pCurDevice->availableMemory -= pTexture->textureSize;
}

static void J3DAPI std3D_RemoveTextureFromCacheList(tSystemTexture* pCacheTexture)
{
    if ( pCacheTexture == std3D_pFirstTexCache )
    {
        std3D_pFirstTexCache = pCacheTexture->pNextCachedTexture;
        if ( std3D_pFirstTexCache )
        {
            std3D_pFirstTexCache->pPrevCachedTexture = NULL;
            if ( !std3D_pFirstTexCache->pNextCachedTexture )
            {
                std3D_pLastTexCache = std3D_pFirstTexCache;
            }
        }
        else
        {
            std3D_pLastTexCache = NULL;
        }
    }
    else if ( pCacheTexture == std3D_pLastTexCache )
    {
        std3D_pLastTexCache = pCacheTexture->pPrevCachedTexture;
        pCacheTexture->pPrevCachedTexture->pNextCachedTexture = NULL;
    }
    else
    {
        if ( pCacheTexture->pPrevCachedTexture ) pCacheTexture->pPrevCachedTexture->pNextCachedTexture = pCacheTexture->pNextCachedTexture;
        if ( pCacheTexture->pNextCachedTexture ) pCacheTexture->pNextCachedTexture->pPrevCachedTexture = pCacheTexture->pPrevCachedTexture;
    }

    pCacheTexture->pNextCachedTexture = NULL;
    pCacheTexture->pPrevCachedTexture = NULL;
    --std3D_numCachedTextures;
    std3D_pCurDevice->availableMemory += pCacheTexture->textureSize;
}

// Frees the least recently used textures that weren't drawn this frame until size bytes are free
static int J3DAPI std3D_PurgeTextureCache(size_t size)
{
    tSystemTexture* pCurTex = std3D_pFirstTexCache;
    while ( pCurTex && std3D_pCurDevice->availableMemory < size )
    {
        tSystemTexture* pNext = pCurTex->pNextCachedTexture;
        if ( pCurTex->frameNum != std3D_frameCount )
        {
            std3D_RemoveTextureFromCacheList(pCurTex);
            if ( std3D_pCurTexture == pCurTex->pCachedTexture ) std3D_BindTexture(NULL);
            glDeleteTextures(1, &pCurTex->pCachedTexture->id);
            stdMemory_Free(pCurTex->pCachedTexture);
            pCurTex->pCachedTexture = NULL;
            pCurTex->frameNum       = 0;
        }
        pCurTex = pNext;
    }
    return std3D_pCurDevice->availableMemory >= size;
}

// ---- states -------------------------------------------------------------------------------------------------

int J3DAPI std3D_SetMipmapFilter(Std3DMipmapFilterType filter)
{
    std3D_FlushDraws();
    std3D_mipmapFilter = filter;
    if ( std3D_sampler )
    {
        std3D_ApplySamplerFilter();
    }
    return 0;
}

int J3DAPI std3D_SetProjection(float fov, float nearPlane, float farPlane)
{
    J3D_UNUSED(fov);
    float distance = farPlane - nearPlane;
    if ( fabsf(distance) < 0.01f )
    {
        return E_FAIL;
    }
    std3D_zDepth = distance * 10.0f; // vertices come pre-transformed: the projection only matters for this depth scale
    return 0;
}

void J3DAPI std3D_EnableFog(int bEnabled, float density)
{
    std3D_FlushDraws();
    std3D_bRenderFog = bEnabled;
    if ( !std3D_pCurDevice )
    {
        return;
    }
    std3D_g_fogDensity = density;
}

void J3DAPI std3D_SetFog(float red, float green, float blue, float startDepth, float endDepth)
{
    std3D_FlushDraws();
    std3D_EnableFog(std3D_bRenderFog, std3D_g_fogDensity);
    std3D_fogStartDepth  = startDepth;
    std3D_fogEndDepth    = (2.0f - std3D_g_fogDensity) * endDepth;
    std3D_fogDepthFactor = 1.0f / (std3D_fogEndDepth - std3D_fogStartDepth);
    std3D_aFogColor[0]   = red;
    std3D_aFogColor[1]   = green;
    std3D_aFogColor[2]   = blue;
    std3D_aFogColor[3]   = 1.0f;
    if ( std3D_bOpen )
    {
        std3D_bFogActive = (std3D_renderState & STD3D_RS_FOG_ENABLED) != 0 && std3D_bRenderFog;
        std3D_ApplyFog();
    }
}

void J3DAPI std3D_SetScissor(int bEnable, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    std3D_FlushDraws();
    std3D_bScissor    = bEnable != 0;
    std3D_aScissor[0] = (GLint)x;
    std3D_aScissor[1] = (GLint)stdDisplay_g_backBuffer.rasterInfo.height - (GLint)(y + height);
    std3D_aScissor[2] = (GLint)width;
    std3D_aScissor[3] = (GLint)height;
    if ( std3D_bOpen )
    {
        std3D_PrepareDraw();
        std3D_ApplyScissor();
    }
}

void std3D_ClearZBuffer(void)
{
    if ( !std3D_bOpen )
    {
        return;
    }
    std3D_FlushDraws();
    std3D_PrepareDraw();
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glDepthMask((std3D_renderState & STD3D_RS_ZWRITE_DISABLED) ? GL_FALSE : GL_TRUE);
}

bool std3D_IsShaderSystemActive(void) { return true; }
bool std3D_IsAnisotropicFilteringSupported(void) { return true; }
bool std3D_IsMipmapAutoGenSupported(void) { return true; }
bool std3D_IsMSAASupported(void) { return false; }

// ---- display environment, as std3DX9.c ------------------------------------------------------------------------

StdDisplayEnvironment* J3DAPI std3D_BuildDisplayEnvironment()
{
    StdDisplayEnvironment* pEnv = (StdDisplayEnvironment*)STDMALLOC(sizeof(StdDisplayEnvironment));
    STD_ZEROMEM(pEnv, sizeof(StdDisplayEnvironment));
    if ( !stdDisplay_Startup() )
    {
        STDLOG_ERROR("Error starting stdDisplay system.\n");
        std3D_FreeDisplayEnvironment(pEnv);
        return NULL;
    }

    pEnv->numInfos = stdDisplay_GetNumDevices();
    if ( pEnv->numInfos )
    {
        pEnv->aDisplayInfos = (StdDisplayInfo*)STDMALLOC(sizeof(StdDisplayInfo) * pEnv->numInfos);
        for ( size_t deviceNum = 0; deviceNum < pEnv->numInfos; ++deviceNum )
        {
            StdDisplayInfo* pInfo = &pEnv->aDisplayInfos[deviceNum];
            STD_ZEROMEM(pInfo, sizeof(StdDisplayInfo));
            if ( stdDisplay_GetDevice(deviceNum, &pInfo->displayDevice) || !stdDisplay_Open(deviceNum) )
            {
                STDLOG_ERROR("Error opening stdDisplay device.\n");
                std3D_FreeDisplayEnvironment(pEnv);
                return NULL;
            }

            pInfo->numModes = stdDisplay_GetNumVideoModes();
            if ( pInfo->numModes )
            {
                pInfo->aModes = (StdVideoMode*)STDMALLOC(sizeof(StdVideoMode) * pInfo->numModes);
                StdVideoMode* pCurMode = pInfo->aModes;
                for ( size_t modeNum = 0; modeNum < pInfo->numModes; ++modeNum )
                {
                    if ( !stdDisplay_GetVideoMode(modeNum, pCurMode) ) ++pCurMode;
                }

                if ( pInfo->displayDevice.bHAL && std3D_Startup() )
                {
                    pInfo->numDevices = std3D_GetNumDevices();
                    if ( pInfo->numDevices )
                    {
                        size_t listSize  = sizeof(Device3D) * pInfo->numDevices;
                        pInfo->aDevices = (Device3D*)STDMALLOC(listSize);
                        memcpy(pInfo->aDevices, std3D_GetAllDevices(), listSize);
                    }
                    std3D_Shutdown();
                }
            }
            stdDisplay_Close();
        }
    }

    stdDisplay_Shutdown();
    return pEnv;
}

void J3DAPI std3D_FreeDisplayEnvironment(StdDisplayEnvironment* pEnv)
{
    if ( pEnv->aDisplayInfos )
    {
        for ( size_t i = 0; i < pEnv->numInfos; ++i )
        {
            if ( pEnv->aDisplayInfos[i].aDevices ) stdMemory_Free(pEnv->aDisplayInfos[i].aDevices);
            if ( pEnv->aDisplayInfos[i].aModes ) stdMemory_Free(pEnv->aDisplayInfos[i].aModes);
        }
        stdMemory_Free(pEnv->aDisplayInfos);
    }
    stdMemory_Free(pEnv);
}
