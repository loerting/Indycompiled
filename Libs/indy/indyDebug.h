#ifndef INDY_INDYDEBUG_H
#define INDY_INDYDEBUG_H
// Indycompiled debug aids (only active when their environment variable is set).
#include <j3dcore/j3d.h>
#include <stdint.h>

J3D_EXTERN_C_START

// Starts the thread sampler when INDY_SAMPLE_THREADS=<seconds> is set (see indyDebug.c). Call from DllMain.
void indyDebug_Startup(HMODULE hDll);

// Frame cap for tests (INDY_FPS_CAP=<frames per second>): waits until the frame's time slice is used up. Call once
// per frame. No effect without the variable.
void indyDebug_FrameCap(void);

// Deterministic simulation for A/B tests: with INDY_FIXED_FRAME_MS=<ms>, game time advances by exactly that much per
// frame instead of following the wall clock. Returns 0 without the variable.
uint32_t indyDebug_GetFixedFrameMs(void);

// Called after each simulated frame (sithUpdate). With INDY_DUMP_WORLD_FRAME=<n>, writes the world snapshot
// (INDY_DUMP_WORLD) after n frames of the level and exits the game. With INDY_SAVE_FRAME=<n> and
// INDY_SAVE_FILE=<file>, saves the game at frame n; with INDY_RESTORE_FRAME=<n> and INDY_RESTORE_FILE=<file>, loads that
// savegame once at frame n (savegame A/B tests).
void indyDebug_SimFrame(void);

// World snapshot for A/B tests (INDY_DUMP_WORLD=<file>): writes the current world's thing templates and things, one
// line each, with heap pointers replaced by what they point to (runs can be compared even when heap addresses differ).
// Called at the end of sithOpen (level loaded, nothing simulated yet); with INDY_DUMP_WORLD_FRAME set, the snapshot is
// written later (indyDebug_SimFrame) instead.
void indyDebug_DumpWorld(void);

J3D_EXTERN_C_END
#endif // INDY_INDYDEBUG_H
