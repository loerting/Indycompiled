#ifndef INDY_INDYDEBUG_H
#define INDY_INDYDEBUG_H
// Indycompiled debug aids (only active when their environment variable is set).
#include <j3dcore/j3d.h>

J3D_EXTERN_C_START

// Starts the thread sampler when INDY_SAMPLE_THREADS=<seconds> is set (see indyDebug.c). Call from DllMain.
void indyDebug_Startup(HMODULE hDll);

// Frame cap for tests (INDY_FPS_CAP=<frames per second>): waits until the frame's time slice is used up. Call once
// per frame. No effect without the variable.
void indyDebug_FrameCap(void);

// World snapshot for A/B tests (INDY_DUMP_WORLD=<file>): writes the current world's thing templates and things, one
// line each, with heap pointers replaced by what they point to (runs can be compared even when heap addresses differ).
// Called at the end of sithOpen (level loaded, nothing simulated yet).
void indyDebug_DumpWorld(void);

J3D_EXTERN_C_END
#endif // INDY_INDYDEBUG_H
