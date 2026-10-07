// Native builds (Linux, Android): the program entry point. Does what Indy3D.exe's WinMain does on Windows
// (dllmain.c): installs the crash handler, registers the main loop callbacks and runs the window kernel.
#include <Jones3D/Main/JonesMain.h>
#include <std/General/stdPlatform.h>
#include <std/General/stdUtil.h>
#include <wkernel/wkernel.h>

#include <SDL3/SDL_main.h>

#if defined(__i386__) && defined(__linux__)
#include <fpu_control.h>
#endif

#ifdef INDY_NATIVE_DEBUG
#include <indy/indyDebug.h>
#endif

#ifdef __ANDROID__
#include <std/SDL/stdAndroidSDL.h>
#endif

static const char* appName = "Open Jones 3D";

static int J3DAPI Startup(const char* aCmd)
{
    if ( JonesMain_Startup(aCmd) )
    {
        JonesMain_Shutdown();
        return -1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
#if defined(__i386__) && defined(__linux__)
    // x87 precision as on Windows (53 bits); Linux starts with 64 bits. The game's float results then match.
    fpu_control_t cw;
    _FPU_GETCW(cw);
    cw = (fpu_control_t)((cw & ~_FPU_EXTENDED) | _FPU_DOUBLE);
    _FPU_SETCW(cw);
#endif

#ifdef __ANDROID__
    if ( stdAndroid_PrepareDataDir() ) // game data: APK assets and internal storage; makes Resource/ the working dir
    {
        return 1;
    }
#endif

#ifdef INDY_NATIVE_DEBUG
    indyDebug_Startup(NULL); // Indycompiled debug aids, only active when their environment variable is set
#endif

    // The command line as one string, as WinMain receives it
    static char aCmdLine[1024];
    aCmdLine[0] = '\0';
    for ( int i = 1; i < argc; ++i )
    {
        if ( i > 1 )
        {
            stdUtil_StringCat(aCmdLine, sizeof(aCmdLine), " ");
        }
        stdUtil_StringCat(aCmdLine, sizeof(aCmdLine), argv[i]);
    }

    stdPlatform_InstallSignalHandler();

    wkernel_SetProcessProc(JonesMain_Process);
    wkernel_SetStartupCallback(Startup);
    wkernel_SetShutdownCallback(JonesMain_Shutdown);
    return wkernel_Run(NULL, NULL, aCmdLine, 1, appName);
}
