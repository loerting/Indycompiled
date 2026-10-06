#ifndef INDY_INDYFRAME_H
#define INDY_INDYFRAME_H
// Indycompiled: "every N-th frame" events on game time instead of frame count (FIX-0003, fpsIndependentCycles).
#include <j3dcore/j3d.h>
#include <stddef.h>

J3D_EXTERN_C_START

// The game does some things on every N-th frame (SITH_ISFRAMECYCLE): random idle animations, breathing sounds,
// sprite flicker, AI awareness pings. At 120 FPS they happened 4x as often as at 30 FPS. With FIX-0003 the sites
// that are about time use SITH_ISTIMECYCLE, which counts "intended" frames at INDY_FRAME_INTENDED_FPS of game time
// instead. Sites that are per-frame upkeep (matrix normalisation, texture wrap, target search) keep SITH_ISFRAMECYCLE.
#define INDY_FRAME_INTENDED_FPS 30.0f

// Call from sithOpen (resets the counter) and once per game frame from sithUpdate with the frame time in seconds.
void indyFrame_Reset(void);
void indyFrame_Advance(float secFrameTime);

// True on the frames where the cycle (every n-th intended frame, n a power of two, shifted by offset) comes round.
// Without FIX-0003: the original frame-count check.
int J3DAPI indyFrame_IsTimeCycle(size_t offset, size_t n);

// Test aid: how often a cycle of 16 (offset 0) came round since the level started (logged by INDY_INPUT_TRACE).
unsigned int indyFrame_GetTestCount(void);

J3D_EXTERN_C_END
#endif // INDY_INDYFRAME_H
