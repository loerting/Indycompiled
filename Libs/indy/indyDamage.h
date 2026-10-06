#ifndef INDY_INDYDAMAGE_H
#define INDY_INDYDAMAGE_H
// Indycompiled: continuous (per-frame) damage independent of the frame rate (FIX-0002, fpsIndependentDamage).
#include <j3dcore/j3d.h>
#include <sith/types.h>

J3D_EXTERN_C_START

// Applies damage that is dealt a little every frame (drowning, raft leak, IMP blast).
// sithThing_DamageThing hands damage to COG as an integer, so per-frame amounts lose their fraction and amounts below
// 1 vanish: the higher the frame rate, the less damage (drowning stops completely at about 250 FPS). With FIX-0002
// the fraction is carried to the next frame per thing and damage type; without it, this is the original call.
void J3DAPI indyDamage_ApplyContinuous(SithThing* pThing, float damage, SithDamageType type);

// Test aid (INDY_TEST_DPS="<damage per second>[@<start>+<duration>]", game seconds): continuous damage to the local player through
// indyDamage_ApplyContinuous, so the frame-rate dependence can be measured from the health in INDY_INPUT_TRACE.
// Call once per frame from sithActor_Update.
void J3DAPI indyDamage_TestTick(SithThing* pThing, unsigned int msecDeltaTime);

J3D_EXTERN_C_END
#endif // INDY_INDYDAMAGE_H
