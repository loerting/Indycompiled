// Native builds: the display module on SDL3 and OpenGL ES 3.0 (stdDisplayDX9.c is the Windows one). One display
// device, the video modes of the primary screen, a 32-bit back buffer (X8R8G8B8, as the DirectX 9 build) that is the
// window's GL framebuffer. Locking the back buffer (movies) reads it into memory; unlocking draws it back.
#include "stdGLES.h"

#include <std/General/std.h>
#include <std/General/stdBmp.h>
#include <std/General/stdColor.h>
#include <std/General/stdMemory.h>
#include <std/General/stdUtil.h>
#include <std/Win95/stdDisplay.h>
#include <std/Win95/stdWin95.h>

#include <SDL3/SDL.h>
#include <stdlib.h>

#define STDDISPLAY_MAXVIDEOMODES 64

struct sStdGLSurface // the window's framebuffer
{
    int unused;
};

struct sStdGLDevice
{
    SDL_GLContext context;
};

tVBuffer stdDisplay_g_frontBuffer;
tVBuffer stdDisplay_g_backBuffer;
uint32_t stdDisplay_g_glStateEpoch = 1; // changes when this module used GL state the renderer (std3DGLES.c) relies on

static bool stdDisplay_bStartup;
static bool stdDisplay_bOpen;
static bool stdDisplay_bModeSet;
static bool stdDisplay_bFullscreen;
static bool stdDisplay_bNoSync;

static size_t stdDisplay_numDevices;
static StdDisplayDevice stdDisplay_aDevices[1];
static size_t stdDisplay_numVideoModes;
static StdVideoMode stdDisplay_aVideoModes[STDDISPLAY_MAXVIDEOMODES];
static StdVideoMode stdDisplay_primaryVideoMode;
static StdVideoMode* stdDisplay_pCurVideoMode;

static struct sStdGLSurface stdDisplay_glSurface;
static struct sStdGLDevice stdDisplay_glDevice;
static GLint stdDisplay_maxTextureSize = 2048;

// Back buffer lock (movies): CPU copy in X8R8G8B8, top row first
static uint8_t* stdDisplay_pLockPixels;
static size_t stdDisplay_lockRef;
static GLuint stdDisplay_blitProgram;
static GLuint stdDisplay_blitTexture;
static GLuint stdDisplay_blitVao;
static GLint stdDisplay_blitTexLoc;

static tDisplayDevicePreResetCallback stdDisplay_pfPreReset;
static tDisplayDevicePostResetCallback stdDisplay_pfPostReset;
static tDisplayDeviceReleaseCallback stdDisplay_pfRelease;

void stdDisplay_InstallHooks(void) {}
void stdDisplay_ResetGlobals(void) {}

static SDL_Window* stdDisplay_GetSDLWindow(void)
{
    return (SDL_Window*)stdWin95_GetWindow();
}

static void stdDisplay_InitRasterInfo(tRasterInfo* pInfo, uint32_t width, uint32_t height)
{
    memset(pInfo, 0, sizeof(tRasterInfo));
    pInfo->width     = width;
    pInfo->height    = height;
    pInfo->colorInfo = stdColor_cfRGB8888;
    pInfo->rowSize   = width * 4;
    pInfo->rowWidth  = width;
    pInfo->size      = pInfo->rowSize * height;
}

static void stdDisplay_AddVideoMode(uint32_t width, uint32_t height, float refreshRate)
{
    for ( size_t i = 0; i < stdDisplay_numVideoModes; ++i )
    {
        if ( stdDisplay_aVideoModes[i].rasterInfo.width == width && stdDisplay_aVideoModes[i].rasterInfo.height == height )
        {
            return;
        }
    }
    if ( stdDisplay_numVideoModes >= STDDISPLAY_MAXVIDEOMODES ) return;

    StdVideoMode* pMode = &stdDisplay_aVideoModes[stdDisplay_numVideoModes++];
    memset(pMode, 0, sizeof(StdVideoMode));
    stdDisplay_InitRasterInfo(&pMode->rasterInfo, width, height);
    pMode->aspectRatio = 1.0f; // square pixels
    pMode->refreshRate = (uint32_t)(refreshRate + 0.5f);
}

int J3DAPI stdDisplay_VideoModeCompare(const StdVideoMode* pMode1, const StdVideoMode* pMode2)
{
    if ( pMode1->rasterInfo.colorInfo.bpp != pMode2->rasterInfo.colorInfo.bpp ) return (int)pMode1->rasterInfo.colorInfo.bpp - (int)pMode2->rasterInfo.colorInfo.bpp;
    if ( pMode1->rasterInfo.width != pMode2->rasterInfo.width ) return (int)pMode1->rasterInfo.width - (int)pMode2->rasterInfo.width;
    return (int)pMode1->rasterInfo.height - (int)pMode2->rasterInfo.height;
}

int stdDisplay_Startup(void)
{
    STDLOG_STATUS("Starting display system using SDL3 and OpenGL ES 3 ...\n");
    if ( stdDisplay_bStartup )
    {
        return 1;
    }

    memset(&stdDisplay_g_frontBuffer, 0, sizeof(stdDisplay_g_frontBuffer));
    memset(&stdDisplay_g_backBuffer, 0, sizeof(stdDisplay_g_backBuffer));

    StdDisplayDevice* pDevice = &stdDisplay_aDevices[0];
    memset(pDevice, 0, sizeof(StdDisplayDevice));
    STD_STRCPY(pDevice->aDeviceName, "OpenGL ES 3");
    STD_STRCPY(pDevice->aDriverName, "SDL3");
    pDevice->bHAL                      = 1;
    pDevice->bWindowRenderNotSupported = 0;
    pDevice->totalVideoMemory          = 512u * 1024u * 1024u;
    pDevice->freeVideoMemory           = pDevice->totalVideoMemory;
    pDevice->caps.maxTextureSize       = 2048;
    stdDisplay_numDevices = 1;

    stdDisplay_InitRasterInfo(&stdDisplay_primaryVideoMode.rasterInfo, 640, 480);
    stdDisplay_primaryVideoMode.aspectRatio = 1.0f;
    stdDisplay_bStartup = true;
    return 1;
}

void stdDisplay_Shutdown(void)
{
    if ( stdDisplay_bOpen )
    {
        stdDisplay_Close();
    }

    memset(&stdDisplay_g_frontBuffer, 0, sizeof(stdDisplay_g_frontBuffer));
    memset(&stdDisplay_g_backBuffer, 0, sizeof(stdDisplay_g_backBuffer));
    stdDisplay_pCurVideoMode = NULL;
    stdDisplay_numDevices    = 0;
    stdDisplay_numVideoModes = 0;
    stdDisplay_bStartup      = false;
}

int J3DAPI stdDisplay_Open(size_t deviceNum)
{
    STD_ASSERTREL(stdDisplay_bStartup == true);
    if ( stdDisplay_bOpen )
    {
        stdDisplay_Close();
    }
    if ( deviceNum >= stdDisplay_numDevices )
    {
        return 0;
    }

    // Video modes: the primary screen's fullscreen modes, plus the classic ones that fit on it
    stdDisplay_numVideoModes = 0;
    int numModes = 0;
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    SDL_DisplayMode** apModes = SDL_GetFullscreenDisplayModes(display, &numModes);
    uint32_t maxW = 640, maxH = 480;
    for ( int i = 0; apModes && i < numModes; ++i )
    {
        stdDisplay_AddVideoMode((uint32_t)apModes[i]->w, (uint32_t)apModes[i]->h, apModes[i]->refresh_rate);
        if ( (uint32_t)apModes[i]->w > maxW ) maxW = (uint32_t)apModes[i]->w;
        if ( (uint32_t)apModes[i]->h > maxH ) maxH = (uint32_t)apModes[i]->h;
    }
    SDL_free(apModes);

    const SDL_DisplayMode* pDesktop = SDL_GetDesktopDisplayMode(display);
    if ( pDesktop )
    {
        stdDisplay_AddVideoMode((uint32_t)pDesktop->w, (uint32_t)pDesktop->h, pDesktop->refresh_rate);
        if ( (uint32_t)pDesktop->w > maxW ) maxW = (uint32_t)pDesktop->w;
        if ( (uint32_t)pDesktop->h > maxH ) maxH = (uint32_t)pDesktop->h;
    }

    static const uint32_t aClassic[][2] = { { 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1280, 720 }, { 1280, 1024 }, { 1920, 1080 } };
    for ( size_t i = 0; i < STD_ARRAYLEN(aClassic); ++i )
    {
        if ( aClassic[i][0] <= maxW && aClassic[i][1] <= maxH ) stdDisplay_AddVideoMode(aClassic[i][0], aClassic[i][1], 60.0f);
    }

    qsort(stdDisplay_aVideoModes, stdDisplay_numVideoModes, sizeof(StdVideoMode), (int (*)(const void*, const void*))stdDisplay_VideoModeCompare);
    stdDisplay_bOpen = true;
    return 1;
}

bool stdDisplay_IsOpen(void)
{
    return stdDisplay_bOpen;
}

void stdDisplay_Close(void)
{
    if ( !stdDisplay_bOpen )
    {
        return;
    }
    if ( stdDisplay_bModeSet )
    {
        stdDisplay_ClearMode();
    }

    if ( stdDisplay_glDevice.context )
    {
        if ( stdDisplay_pfRelease ) stdDisplay_pfRelease(&stdDisplay_glDevice);
        SDL_GL_DestroyContext(stdDisplay_glDevice.context);
        stdDisplay_glDevice.context = NULL;
        stdDisplay_blitProgram = stdDisplay_blitTexture = stdDisplay_blitVao = 0;
    }

    stdDisplay_numVideoModes = 0;
    stdDisplay_pfPreReset    = NULL;
    stdDisplay_pfPostReset   = NULL;
    stdDisplay_pfRelease     = NULL;
    stdDisplay_bOpen         = false;
}

tSysDisplayDevice* stdDisplay_GetSystemDevice(void)
{
    return stdDisplay_glDevice.context ? &stdDisplay_glDevice : NULL;
}

int J3DAPI stdDisplay_CreateZBuffer(const tSysPixelFormat* pPixelFormat, int bSystemMemory)
{
    J3D_UNUSED(pPixelFormat);
    J3D_UNUSED(bSystemMemory);
    return 0; // the window has a depth buffer
}

static GLuint stdDisplay_CompileShader(GLenum type, const char* pSource)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &pSource, NULL);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if ( !ok )
    {
        char aLog[1024] = { 0 };
        glGetShaderInfoLog(shader, sizeof(aLog), NULL, aLog);
        STDLOG_ERROR("Shader compile error: %s\n", aLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// Shared with std3DGLES.c: compiles and links a program with the attribute locations given in pAttribs (NULL-ended)
GLuint stdDisplay_GLES_CreateProgram(const char* pVertex, const char* pFragment, const char* const* apAttribs)
{
    GLuint vs = stdDisplay_CompileShader(GL_VERTEX_SHADER, pVertex);
    GLuint fs = stdDisplay_CompileShader(GL_FRAGMENT_SHADER, pFragment);
    if ( !vs || !fs )
    {
        if ( vs ) glDeleteShader(vs);
        if ( fs ) glDeleteShader(fs);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    for ( GLuint i = 0; apAttribs && apAttribs[i]; ++i )
    {
        glBindAttribLocation(program, i, apAttribs[i]);
    }
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if ( !ok )
    {
        char aLog[1024] = { 0 };
        glGetProgramInfoLog(program, sizeof(aLog), NULL, aLog);
        STDLOG_ERROR("Shader link error: %s\n", aLog);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

static const char* stdDisplay_blitVS =
    "#version 300 es\n"
    "out vec2 vUV;\n"
    "void main() {\n"
    "    vec2 pos = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);\n"
    "    vUV = vec2((pos.x + 1.0) * 0.5, 1.0 - (pos.y + 1.0) * 0.5);\n" // top row of the CPU copy first
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "}\n";

static const char* stdDisplay_blitFS =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D uTex;\n"
    "in vec2 vUV;\n"
    "out vec4 oColor;\n"
    "void main() { oColor = vec4(texture(uTex, vUV).bgr, 1.0); }\n"; // X8R8G8B8 bytes are B, G, R, X

static bool stdDisplay_InitGL(void)
{
    SDL_Window* pWindow = stdDisplay_GetSDLWindow();
    if ( !stdDisplay_glDevice.context )
    {
        stdDisplay_glDevice.context = SDL_GL_CreateContext(pWindow);
        if ( !stdDisplay_glDevice.context )
        {
            STDLOG_ERROR("Error creating OpenGL ES 3 context: %s\n", SDL_GetError());
            return false;
        }
        if ( !stdGLES_LoadFunctions() )
        {
            STDLOG_ERROR("Error loading OpenGL ES 3 functions.\n");
            return false;
        }
        STDLOG_STATUS("OpenGL: %s, %s, %s\n", (const char*)glGetString(GL_VENDOR), (const char*)glGetString(GL_RENDERER), (const char*)glGetString(GL_VERSION));
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &stdDisplay_maxTextureSize);
        stdDisplay_aDevices[0].caps.maxTextureSize = stdDisplay_maxTextureSize;
    }

    SDL_GL_MakeCurrent(pWindow, stdDisplay_glDevice.context);
    SDL_GL_SetSwapInterval(stdDisplay_bNoSync ? 0 : 1);

    if ( !stdDisplay_blitProgram )
    {
        stdDisplay_blitProgram = stdDisplay_GLES_CreateProgram(stdDisplay_blitVS, stdDisplay_blitFS, NULL);
        stdDisplay_blitTexLoc  = glGetUniformLocation(stdDisplay_blitProgram, "uTex");
        glGenVertexArrays(1, &stdDisplay_blitVao);
        glGenTextures(1, &stdDisplay_blitTexture);
        glBindTexture(GL_TEXTURE_2D, stdDisplay_blitTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        ++stdDisplay_g_glStateEpoch;
    }
    return stdDisplay_blitProgram != 0;
}

static void stdDisplay_SetupBackBuffer(uint32_t width, uint32_t height)
{
    memset(&stdDisplay_g_backBuffer, 0, sizeof(stdDisplay_g_backBuffer));
    stdDisplay_g_backBuffer.type                = VBUFFER_HARDWARE;
    stdDisplay_g_backBuffer.bVideoMemory        = 1;
    stdDisplay_g_backBuffer.surface.pSysSurface = &stdDisplay_glSurface;
    stdDisplay_g_backBuffer.surface.desc.width  = width;
    stdDisplay_g_backBuffer.surface.desc.height = height;
    stdDisplay_InitRasterInfo(&stdDisplay_g_backBuffer.rasterInfo, width, height);
    stdDisplay_g_frontBuffer = stdDisplay_g_backBuffer;
}

int J3DAPI stdDisplay_SetMode(size_t modeNum, int bFullscreen, size_t numBackBuffers)
{
    J3D_UNUSED(numBackBuffers);
    if ( bFullscreen && modeNum >= stdDisplay_numVideoModes )
    {
        return 1;
    }
    if ( stdDisplay_bModeSet )
    {
        stdDisplay_ClearMode();
    }

    SDL_Window* pWindow = stdDisplay_GetSDLWindow();
    if ( !pWindow )
    {
        return 1;
    }

    stdDisplay_pCurVideoMode = bFullscreen ? &stdDisplay_aVideoModes[modeNum] : &stdDisplay_primaryVideoMode;
    int width  = (int)stdDisplay_pCurVideoMode->rasterInfo.width;
    int height = (int)stdDisplay_pCurVideoMode->rasterInfo.height;

    if ( bFullscreen )
    {
        SDL_DisplayMode mode;
        if ( SDL_GetClosestFullscreenDisplayMode(SDL_GetDisplayForWindow(pWindow), width, height, 0.0f, true, &mode) )
        {
            SDL_SetWindowFullscreenMode(pWindow, &mode);
        }
        else
        {
            SDL_SetWindowFullscreenMode(pWindow, NULL); // desktop resolution, scaled
        }
        SDL_SetWindowFullscreen(pWindow, true);
    }
    else
    {
        SDL_SetWindowFullscreen(pWindow, false);
        SDL_SetWindowSize(pWindow, width, height);
        SDL_SetWindowPosition(pWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    SDL_ShowWindow(pWindow);
    SDL_SyncWindow(pWindow);

    if ( !stdDisplay_InitGL() )
    {
        return 1;
    }

    // The back buffer is the window's framebuffer
    int pixelW = width, pixelH = height;
    SDL_GetWindowSizeInPixels(pWindow, &pixelW, &pixelH);
    stdDisplay_SetupBackBuffer((uint32_t)pixelW, (uint32_t)pixelH);
    stdDisplay_pCurVideoMode->rasterInfo.width  = (uint32_t)pixelW;
    stdDisplay_pCurVideoMode->rasterInfo.height = (uint32_t)pixelH;
    stdDisplay_InitRasterInfo(&stdDisplay_pCurVideoMode->rasterInfo, (uint32_t)pixelW, (uint32_t)pixelH);

    stdDisplay_bModeSet    = true;
    stdDisplay_bFullscreen = bFullscreen;

    glViewport(0, 0, pixelW, pixelH);
    stdDisplay_BackBufferFill(0, NULL);
    stdDisplay_Update();
    stdDisplay_BackBufferFill(0, NULL);
    return 0;
}

void stdDisplay_ClearMode(void)
{
    if ( stdDisplay_pLockPixels )
    {
        stdMemory_Free(stdDisplay_pLockPixels);
        stdDisplay_pLockPixels = NULL;
    }
    stdDisplay_lockRef  = 0;
    stdDisplay_bModeSet = false;
}

size_t stdDisplay_GetNumDevices(void)
{
    return stdDisplay_numDevices;
}

int J3DAPI stdDisplay_GetDevice(size_t deviceNum, StdDisplayDevice* pDest)
{
    if ( deviceNum >= stdDisplay_numDevices ) return 1;
    *pDest = stdDisplay_aDevices[deviceNum];
    return 0;
}

int J3DAPI stdDisplay_GetCurrentDevice(StdDisplayDevice* pDevice)
{
    if ( !stdDisplay_bOpen ) return 1;
    *pDevice = stdDisplay_aDevices[0];
    return 0;
}

const StdDisplayDevice* stdDisplay_GetAllDevices(void)
{
    return stdDisplay_aDevices;
}

void J3DAPI stdDisplay_Refresh(int bReload)
{
    if ( bReload && stdDisplay_bModeSet && stdDisplay_pCurVideoMode )
    {
        size_t modeNum = stdDisplay_bFullscreen ? (size_t)(stdDisplay_pCurVideoMode - stdDisplay_aVideoModes) : 0;
        stdDisplay_SetMode(modeNum, stdDisplay_bFullscreen, 2);
    }
}

void stdDisplay_RegisterDevicePreResetCallback(tDisplayDevicePreResetCallback pCallback) { stdDisplay_pfPreReset = pCallback; }
void stdDisplay_RegisterDevicePostResetCallback(tDisplayDevicePostResetCallback pCallback) { stdDisplay_pfPostReset = pCallback; }
void stdDisplay_RegisterDeviceReleaseCallback(tDisplayDeviceReleaseCallback pCallback) { stdDisplay_pfRelease = pCallback; }

size_t stdDisplay_GetNumVideoModes(void)
{
    return stdDisplay_numVideoModes;
}

int J3DAPI stdDisplay_GetVideoMode(size_t modeNum, StdVideoMode* pDestMode)
{
    if ( modeNum >= stdDisplay_numVideoModes ) return 1;
    *pDestMode = stdDisplay_aVideoModes[modeNum];
    return 0;
}

int J3DAPI stdDisplay_GetCurrentVideoMode(StdVideoMode* pDisplayMode)
{
    if ( !stdDisplay_pCurVideoMode ) return 1;
    *pDisplayMode = *stdDisplay_pCurVideoMode;
    return 0;
}

// ---- VBuffers: memory surfaces, as in the DirectX 9 build; the back buffer is the only hardware one -----------

static void J3DAPI stdDisplay_SetPixels16(uint16_t* pPixels16, uint16_t pixel, size_t size)
{
    for ( size_t i = 0; i < size; ++i ) pPixels16[i] = pixel;
}

static void J3DAPI stdDisplay_SetPixels32(uint32_t* pPixels32, uint32_t pixel, size_t size)
{
    for ( size_t i = 0; i < size; ++i ) pPixels32[i] = pixel;
}

tVBuffer* J3DAPI stdDisplay_VBufferNew(const tRasterInfo* pRasterInfo, int bUseVSurface, int bUseVideoMemory)
{
    J3D_UNUSED(bUseVSurface);
    J3D_UNUSED(bUseVideoMemory);
    STD_ASSERTREL((pRasterInfo->colorInfo.bpp % 8) == 0);

    tVBuffer* pVBuffer = (tVBuffer*)STDMALLOC(sizeof(tVBuffer));
    if ( !pVBuffer )
    {
        STDLOG_ERROR("Error allocating vbuffer.\n");
        return NULL;
    }

    memset(pVBuffer, 0, sizeof(tVBuffer));
    pVBuffer->rasterInfo          = *pRasterInfo;
    uint32_t bpp                  = pVBuffer->rasterInfo.colorInfo.bpp / 8;
    pVBuffer->rasterInfo.rowSize  = pVBuffer->rasterInfo.width * bpp;
    pVBuffer->rasterInfo.rowWidth = pVBuffer->rasterInfo.width;
    pVBuffer->rasterInfo.size     = pVBuffer->rasterInfo.rowSize * pVBuffer->rasterInfo.height;
    pVBuffer->type                = VBUFFER_SOFTWARE;
    pVBuffer->pPixels             = (uint8_t*)STDMALLOC(pVBuffer->rasterInfo.size);
    if ( !pVBuffer->pPixels )
    {
        stdMemory_Free(pVBuffer);
        return NULL;
    }
    pVBuffer->lockRefCount = 1; // software buffers are always locked
    return pVBuffer;
}

void J3DAPI stdDisplay_VBufferFree(tVBuffer* pVBuffer)
{
    STD_ASSERTREL(pVBuffer != NULL);
    if ( pVBuffer->type == VBUFFER_SOFTWARE && pVBuffer->pPixels )
    {
        stdMemory_Free(pVBuffer->pPixels);
        pVBuffer->pPixels = NULL;
    }
    if ( pVBuffer != &stdDisplay_g_backBuffer && pVBuffer != &stdDisplay_g_frontBuffer )
    {
        stdMemory_Free(pVBuffer);
    }
}

int J3DAPI stdDisplay_VBufferLock(tVBuffer* pVBuffer)
{
    STD_ASSERTREL(pVBuffer != NULL);
    if ( pVBuffer->type == VBUFFER_SOFTWARE )
    {
        ++pVBuffer->lockRefCount;
        return 1;
    }

    uint32_t width, height;
    int32_t pitch;
    if ( stdDisplay_LockBackBuffer((void**)&pVBuffer->pPixels, &width, &height, &pitch) || !pVBuffer->pPixels )
    {
        return 0;
    }
    ++pVBuffer->lockRefCount;
    return 1;
}

int J3DAPI stdDisplay_VBufferUnlock(tVBuffer* pVBuffer)
{
    STD_ASSERTREL(pVBuffer != NULL);
    if ( pVBuffer->type == VBUFFER_SOFTWARE )
    {
        if ( pVBuffer->lockRefCount ) --pVBuffer->lockRefCount;
        return 1;
    }
    if ( pVBuffer->lockRefCount == 0 )
    {
        return 0;
    }
    stdDisplay_UnlockBackBuffer();
    --pVBuffer->lockRefCount;
    if ( pVBuffer->lockRefCount == 0 ) pVBuffer->pPixels = NULL;
    return 0;
}

int J3DAPI stdDisplay_VBufferFill(tVBuffer* pVBuffer, uint32_t color, const StdRect* pRect)
{
    STD_ASSERTREL(pVBuffer != NULL);
    if ( pVBuffer->type == VBUFFER_HARDWARE )
    {
        return stdDisplay_BackBufferFill(color, pRect) == 0;
    }

    const size_t bpp = pVBuffer->rasterInfo.colorInfo.bpp / 8;
    for ( int32_t row = pRect ? pRect->top : 0; row < (pRect ? pRect->top + pRect->bottom : (int32_t)pVBuffer->rasterInfo.height); ++row )
    {
        uint8_t* pRow   = &pVBuffer->pPixels[pVBuffer->rasterInfo.rowSize * (size_t)row + bpp * (size_t)(pRect ? pRect->left : 0)];
        size_t numPixels = pRect ? (size_t)pRect->right : pVBuffer->rasterInfo.width;
        switch ( bpp )
        {
            case 1: memset(pRow, (uint8_t)color, numPixels); break;
            case 2: stdDisplay_SetPixels16((uint16_t*)pRow, (uint16_t)color, numPixels); break;
            case 4: stdDisplay_SetPixels32((uint32_t*)pRow, color, numPixels); break;
            default: STDLOG_FATAL("24-bit fill not implemented"); break;
        }
    }
    return 1;
}

tVBuffer* J3DAPI stdDisplay_VBufferConvertColorFormat(const ColorInfo* pDesiredColorFormat, tVBuffer* pSrc, int bColorKey, LPDDCOLORKEY pColorKey)
{
    STD_ASSERTREL(pSrc != NULL);
    if ( memcmp(pDesiredColorFormat, &pSrc->rasterInfo.colorInfo, sizeof(ColorInfo)) == 0 )
    {
        return pSrc;
    }

    if ( pSrc->rasterInfo.colorInfo.colorMode == STDCOLOR_PAL )
    {
        if ( pDesiredColorFormat->colorMode == STDCOLOR_PAL )
        {
            return pSrc;
        }
        STD_ASSERTREL(pSrc->rasterInfo.colorInfo.colorMode != STDCOLOR_PAL);
    }

    tVBuffer* pDest = pSrc;
    if ( pSrc->rasterInfo.colorInfo.bpp != pDesiredColorFormat->bpp )
    {
        tRasterInfo rasterInfo = pSrc->rasterInfo;
        rasterInfo.colorInfo   = *pDesiredColorFormat;
        pDest = stdDisplay_VBufferNew(&rasterInfo, 0, 0);
        if ( !pDest )
        {
            STDLOG_ERROR("Unable to allocate memory for new tVBuffer");
            return NULL;
        }
    }

    stdDisplay_VBufferLock(pSrc);
    stdDisplay_VBufferLock(pDest);
    for ( size_t row = 0; row < pDest->rasterInfo.height; ++row )
    {
        stdColor_ColorConvertOneRow(
            &pDest->pPixels[pDest->rasterInfo.rowSize * row],
            pDesiredColorFormat,
            &pSrc->pPixels[pSrc->rasterInfo.rowSize * row],
            &pSrc->rasterInfo.colorInfo,
            pDest->rasterInfo.width,
            bColorKey,
            pColorKey
        );
    }
    stdDisplay_VBufferUnlock(pSrc);
    stdDisplay_VBufferUnlock(pDest);

    pDest->rasterInfo.colorInfo = *pDesiredColorFormat;
    if ( pDest != pSrc )
    {
        stdDisplay_VBufferFree(pSrc);
    }
    return pDest;
}

int J3DAPI stdDisplay_GetTextureMemory(size_t* pTotal, size_t* pFree)
{
    *pTotal = stdDisplay_aDevices[0].totalVideoMemory;
    *pFree  = stdDisplay_aDevices[0].freeVideoMemory;
    return 0;
}

int J3DAPI stdDisplay_GetTotalMemory(size_t* pTotal, size_t* pFree)
{
    return stdDisplay_GetTextureMemory(pTotal, pFree);
}

// ---- back buffer -------------------------------------------------------------------------------------------

void stdDisplay_DisableVSync(bool bDisable)
{
    stdDisplay_bNoSync = bDisable;
    if ( stdDisplay_glDevice.context )
    {
        SDL_GL_SetSwapInterval(bDisable ? 0 : 1);
    }
}

int stdDisplay_Update(void)
{
    if ( !stdDisplay_glDevice.context )
    {
        return 1;
    }
    SDL_GL_SwapWindow(stdDisplay_GetSDLWindow());
    return 0;
}

// color is in the back buffer's format (X8R8G8B8); pRect: left, top, width, height
int J3DAPI stdDisplay_BackBufferFill(uint32_t color, const StdRect* pRect)
{
    if ( !stdDisplay_glDevice.context || !stdDisplay_bModeSet )
    {
        return 1;
    }

    if ( pRect )
    {
        if ( pRect->right <= 0 || pRect->bottom <= 0 ) return 1;
        glEnable(GL_SCISSOR_TEST);
        glScissor(pRect->left, (GLint)stdDisplay_g_backBuffer.rasterInfo.height - (pRect->top + pRect->bottom), pRect->right, pRect->bottom);
    }
    glClearColor((float)((color >> 16) & 0xFF) / 255.0f, (float)((color >> 8) & 0xFF) / 255.0f, (float)(color & 0xFF) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if ( pRect )
    {
        glDisable(GL_SCISSOR_TEST);
    }
    ++stdDisplay_g_glStateEpoch;
    return 0;
}

static void stdDisplay_ReadBackBuffer(uint8_t* pDest)
{
    const uint32_t width  = stdDisplay_g_backBuffer.rasterInfo.width;
    const uint32_t height = stdDisplay_g_backBuffer.rasterInfo.height;
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, (GLsizei)width, (GLsizei)height, GL_RGBA, GL_UNSIGNED_BYTE, pDest);

    // GL: bottom row first, RGBA -> top row first, X8R8G8B8 (bytes B, G, R, X)
    const size_t rowSize = (size_t)width * 4;
    uint8_t* pTmp = (uint8_t*)SDL_malloc(rowSize);
    for ( uint32_t y = 0; y < height / 2; ++y )
    {
        uint8_t* pTop    = pDest + rowSize * y;
        uint8_t* pBottom = pDest + rowSize * (height - 1 - y);
        memcpy(pTmp, pTop, rowSize);
        memcpy(pTop, pBottom, rowSize);
        memcpy(pBottom, pTmp, rowSize);
    }
    SDL_free(pTmp);
    for ( size_t i = 0; i < rowSize * height; i += 4 )
    {
        uint8_t r = pDest[i];
        pDest[i]     = pDest[i + 2];
        pDest[i + 2] = r;
        pDest[i + 3] = 0xFF;
    }
}

int J3DAPI stdDisplay_LockBackBuffer(void** ppSurface, uint32_t* pWidth, uint32_t* pHeight, int32_t* pPitch)
{
    if ( !stdDisplay_bOpen || !stdDisplay_bModeSet || !stdDisplay_glDevice.context )
    {
        return 1;
    }

    const uint32_t width  = stdDisplay_g_backBuffer.rasterInfo.width;
    const uint32_t height = stdDisplay_g_backBuffer.rasterInfo.height;
    if ( stdDisplay_lockRef == 0 )
    {
        if ( !stdDisplay_pLockPixels )
        {
            stdDisplay_pLockPixels = (uint8_t*)STDMALLOC((size_t)width * height * 4);
            if ( !stdDisplay_pLockPixels ) return 1;
        }
        stdDisplay_ReadBackBuffer(stdDisplay_pLockPixels);
    }

    ++stdDisplay_lockRef;
    *ppSurface = stdDisplay_pLockPixels;
    *pWidth    = width;
    *pHeight   = height;
    *pPitch    = (int32_t)(width * 4);
    return 0;
}

void stdDisplay_UnlockBackBuffer(void)
{
    if ( stdDisplay_lockRef == 0 )
    {
        return;
    }
    if ( --stdDisplay_lockRef > 0 || !stdDisplay_pLockPixels || !stdDisplay_bModeSet )
    {
        return;
    }

    // Draw the CPU copy over the whole framebuffer
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glUseProgram(stdDisplay_blitProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindSampler(0, 0);
    glBindTexture(GL_TEXTURE_2D, stdDisplay_blitTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)stdDisplay_g_backBuffer.rasterInfo.width, (GLsizei)stdDisplay_g_backBuffer.rasterInfo.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, stdDisplay_pLockPixels);
    glUniform1i(stdDisplay_blitTexLoc, 0);
    glBindVertexArray(stdDisplay_blitVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    ++stdDisplay_g_glStateEpoch;
}

int J3DAPI stdDisplay_SaveScreen(const char* pFilename)
{
    if ( !stdDisplay_bModeSet ) return 1;
    tVBuffer* pShot = stdDisplay_VBufferNew(&stdDisplay_g_backBuffer.rasterInfo, 0, 0);
    if ( !pShot ) return 1;
    stdDisplay_ReadBackBuffer(pShot->pPixels);
    int result = stdBmp_WriteVBuffer(pFilename, pShot);
    stdDisplay_VBufferFree(pShot);
    return result;
}

void J3DAPI stdDisplay_SetDefaultResolution(uint32_t width, uint32_t height)
{
    stdDisplay_InitRasterInfo(&stdDisplay_primaryVideoMode.rasterInfo, width, height);
}

void J3DAPI stdDisplay_GetBackBufferSize(uint32_t* pWidth, uint32_t* pHeight)
{
    *pWidth  = stdDisplay_g_backBuffer.rasterInfo.width;
    *pHeight = stdDisplay_g_backBuffer.rasterInfo.height;
}

HDC stdDisplay_GetFrontBufferDC(void) { return NULL; }
void J3DAPI stdDisplay_ReleaseFrontBufferDC(HDC hdc) { J3D_UNUSED(hdc); }
HDC stdDisplay_GetBackBufferDC(void) { return NULL; }
void J3DAPI stdDisplay_ReleaseBackBufferDC(HDC hdc) { J3D_UNUSED(hdc); }
int stdDisplay_FlipToGDISurface(void) { return 0; }
int J3DAPI stdDisplay_SetBufferClipper(int bFrontBuffer) { J3D_UNUSED(bFrontBuffer); return 0; }
HRESULT J3DAPI stdDisplay_RemoveBufferClipper(int bFrontBuffer) { J3D_UNUSED(bFrontBuffer); return 0; }
int stdDisplay_CanRenderWindowed(void) { return 1; }
int stdDisplay_IsFullscreen(void) { return stdDisplay_bFullscreen; }

uint32_t J3DAPI stdDisplay_EncodeFromRGB565(uint16_t pixel)
{
    const ColorInfo* pCi = &stdDisplay_g_backBuffer.rasterInfo.colorInfo;
    uint8_t red = (uint8_t)(8 * (pixel >> 11));
    if ( (red & 8) != 0 ) red |= 7;
    uint8_t green = (uint8_t)(4 * (pixel >> 5));
    if ( (green & 4) != 0 ) green |= 3;
    uint8_t blue = (uint8_t)(8 * pixel);
    if ( (blue & 8) != 0 ) blue |= 7;
    return ((uint32_t)(red >> pCi->redPosShiftRight) << pCi->redPosShift)
        | ((uint32_t)(green >> pCi->greenPosShiftRight) << pCi->greenPosShift)
        | ((uint32_t)(blue >> pCi->bluePosShiftRight) << pCi->bluePosShift);
}
