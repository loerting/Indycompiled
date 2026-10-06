#ifndef INDY_INDYDIFF_H
#define INDY_INDYDIFF_H
// Indycompiled differential tests (Stage 3): original v1.2 code vs our C reimplementation, same inputs.
#include <j3dcore/j3d.h>

J3D_EXTERN_C_START

// With INDY_DIFFTEST=<suite> set: runs the suite once, writes indy_difftest.txt (and JonesLog.txt), then exits the
// game (exit code 0 if everything matched). Without it: nothing. Called from the game loop.
void indyDiff_RunOnce(void);

J3D_EXTERN_C_END
#endif // INDY_INDYDIFF_H
