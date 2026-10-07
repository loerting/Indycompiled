#ifndef INDY_INDYDIFF_H
#define INDY_INDYDIFF_H
// Indycompiled differential tests (Stage 3): original v1.2 code vs our C reimplementation, same inputs.
#include <j3dcore/j3d.h>

J3D_EXTERN_C_START

// With INDY_DIFFTEST=<suite> set: runs the suite once, writes indy_difftest.txt (and JonesLog.txt), then exits the
// game (exit code 0 if everything matched). Without it: nothing. Called from the game loop.
#ifdef J3D_STANDALONE // Stage 4: no exe to compare with (indyDiff.c isn't built)
static inline void indyDiff_RunOnce(void) {}
#else
void indyDiff_RunOnce(void);
#endif

J3D_EXTERN_C_END
#endif // INDY_INDYDIFF_H
