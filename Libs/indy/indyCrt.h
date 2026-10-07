#ifndef INDY_INDYCRT_H
#define INDY_INDYCRT_H
// Indycompiled: C runtime functions whose exact results the game depends on.
#include <stddef.h>
#include <j3dcore/j3d.h>

J3D_EXTERN_C_START

// Sorts like the qsort of the C runtime Indy3D.exe links statically (0x004F7CD0 in v1.2). qsort isn't stable: elements
// that compare equal end up in an order that depends on the algorithm, and the game relies on that order (AI waypoint
// choice), so this reproduces the original's algorithm instead of using the host C runtime's qsort.
void indyCrt_Qsort(void* pBase, size_t num, size_t width, int (__cdecl* pfCompare)(const void*, const void*));

J3D_EXTERN_C_END
#endif // INDY_INDYCRT_H
