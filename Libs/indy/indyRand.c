// One random number generator for the original code and ours.
//
// Indy3D.exe links the C runtime statically: its rand() (0x004F6F30 in v1.2) is MSVC's linear congruential generator
// with the seed in a global (0x0054E1C0), starting at 1 (the game never calls srand). Our DLL would otherwise get
// rand() from its own C runtime, a second generator with its own seed. That made random sequences depend on which
// functions are already reimplemented, and A/B tests of functions that use rand() (AI) impossible.
// These definitions take precedence over the C runtime's, so every rand() call in the DLL uses the exe's generator.
// Without the exe (native builds) the same generator runs on its own seed, so random sequences stay the same.
#include <stdint.h>
#include <stdlib.h>

#ifdef INDY_HOST_EXE_SHA256
#  define INDY_RAND_SEED (*(volatile uint32_t*)0x0054E1C0) // the exe's seed (v1.2)
#else
static uint32_t indyRand_seed = 1;
#  define INDY_RAND_SEED indyRand_seed
#endif

int __cdecl rand(void)
{
    uint32_t seed  = INDY_RAND_SEED * 214013u + 2531011u;
    INDY_RAND_SEED = seed;
    return (int)((seed >> 16) & 0x7FFF);
}

void __cdecl srand(unsigned int seed)
{
    INDY_RAND_SEED = seed;
}
