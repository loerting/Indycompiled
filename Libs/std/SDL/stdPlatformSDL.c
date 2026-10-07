// Native builds: host services, timer and assert handler on SDL3 and the C library (stdPlatform.c is the Win32 one).
#include "../General/std.h"
#include "../General/stdMemory.h"
#include "../General/stdPlatform.h"
#include "../General/stdUtil.h"

#include <SDL3/SDL.h>
#ifndef __ANDROID__ // Android: debuggerd writes the backtrace of a crash to logcat
#include <execinfo.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static bool stdPlatform_bAssert = false;

void stdPlatform_InstallHooks(void) {}
void stdPlatform_ResetGlobals(void) {}

int J3DAPI stdPlatform_InitServices(tHostServices* pHS)
{
    pHS->unknown1 = 1000.0f; // sec to msec converter constant

    pHS->pMessagePrint = stdPlatform_Printf;
    pHS->pStatusPrint  = stdPlatform_Printf;
    pHS->pWarningPrint = stdPlatform_Printf;
    pHS->pErrorPrint   = stdPlatform_Printf;
    pHS->pDebugPrint   = stdPlatform_Printf;

    pHS->pAssert = stdPlatform_Assert;
    pHS->pAtExit = NULL;

    pHS->pMalloc  = stdMemory_BlockMalloc;
    pHS->pFree    = stdMemory_BlockFree;
    pHS->pRealloc = stdMemory_BlockRealloc;

    pHS->pGetTimeMsec = stdPlatform_GetTimeMsec;

    pHS->pFileOpen   = stdFileOpen;
    pHS->pFileClose  = stdFileClose;
    pHS->pFileRead   = stdFileRead;
    pHS->pFileGets   = stdFileGets;
    pHS->pFileWrite  = stdFileWrite;
    pHS->pFileEOF    = stdFileEof;
    pHS->pFileTell   = stdFileTell;
    pHS->pFileSeek   = stdFileSeek;
    pHS->pFileSize   = stdFileSize;
    pHS->pFilePrintf = stdFilePrintf;
    pHS->pFileGetws  = stdFileGetws;

    pHS->pAllocHandle   = stdPlatform_AllocHandle;
    pHS->pFreeHandle    = stdPlatform_FreeHandle;
    pHS->pReallocHandle = stdPlatform_ReallocHandle;

    pHS->pLockHandle   = stdPlatform_LockHandle;
    pHS->pUnlockHandle = stdPlatform_UnlockHandle;

    return 0; // as CoInitialize's S_OK
}

void J3DAPI stdPlatform_ClearServices(tHostServices* pHS)
{
    memset(pHS, 0, sizeof(tHostServices));
}

tStdTime stdPlatform_GetTimeMsec(void)
{
    // milliseconds since start, wrapping like timeGetTime()
    return (tStdTime)SDL_GetTicks();
}

J3DNORETURN void J3DAPI stdPlatform_Assert(const char* pErrorStr, const char* pFilename, int linenum)
{
    if ( stdPlatform_bAssert )
    {
        abort();
    }
    stdPlatform_bAssert = true;

    const char* pName = pFilename;
    for ( const char* p = pFilename; *p; ++p )
    {
        if ( *p == '\\' || *p == '/' ) pName = p + 1;
    }

    char aText[512];
    STD_FORMAT(aText, "%s(%d):  %s\n", pName, linenum, pErrorStr);
    std_g_pHS->pErrorPrint("ASSERT: %s", aText);
    stdPlatform_PrintStackTrace((tStackTracePrintFunc)std_g_pHS->pErrorPrint, 32);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Assert Handler", aText, NULL);
    abort();
}

int stdPlatform_Printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(std_g_genBuffer, sizeof(std_g_genBuffer), format, args);
    va_end(args);

    fputs(std_g_genBuffer, stderr);
    return 1;
}

void* J3DAPI stdPlatform_AllocHandle(size_t size)
{
    return malloc(size);
}

void J3DAPI stdPlatform_FreeHandle(void* pData)
{
    free(pData);
}

void* J3DAPI stdPlatform_ReallocHandle(void* pMemory, size_t newSize)
{
    return realloc(pMemory, newSize);
}

int J3DAPI stdPlatform_LockHandle(int a1)
{
    return a1;
}

void J3DAPI stdPlatform_UnlockHandle()
{}

bool J3DAPI stdPlatform_DirExists(const char* pPath)
{
    char aPath[J3D_MAX_PATH];
    struct stat st;
    return stat(J3D_ResolvePath(pPath, aPath, sizeof(aPath)), &st) == 0 && S_ISDIR(st.st_mode);
}

void stdPlatform_PrintStackTrace(tStackTracePrintFunc pfPrintFunc, size_t numFrames)
{
#ifdef __ANDROID__
    J3D_UNUSED(pfPrintFunc);
    J3D_UNUSED(numFrames);
#else
    void* aFrames[64];
    if ( numFrames > STD_ARRAYLEN(aFrames) ) numFrames = STD_ARRAYLEN(aFrames);
    int n = backtrace(aFrames, (int)numFrames);
    char** aSymbols = backtrace_symbols(aFrames, n);
    for ( int i = 0; i < n; ++i )
    {
        pfPrintFunc("  #%d %s\n", i, aSymbols ? aSymbols[i] : "?");
    }
    free(aSymbols);
#endif
}

#ifndef __ANDROID__
static void stdPlatform_CrashHandler(int sig)
{
    fprintf(stderr, "Fatal signal %d\n", sig);
    void* aFrames[64];
    int n = backtrace(aFrames, 64);
    backtrace_symbols_fd(aFrames, n, STDERR_FILENO);
    signal(sig, SIG_DFL);
    raise(sig);
}

#endif

void J3DAPI stdPlatform_InstallSignalHandler(void)
{
#ifndef __ANDROID__
    signal(SIGSEGV, stdPlatform_CrashHandler);
    signal(SIGABRT, stdPlatform_CrashHandler);
    signal(SIGFPE, stdPlatform_CrashHandler);
    signal(SIGILL, stdPlatform_CrashHandler);
#endif
}
