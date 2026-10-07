// Native builds (Linux, Android): the wave format structs of <Mmreg.h> that the sound module uses.
#pragma once
#include <j3dcore/j3dwin32.h>

#define WAVE_FORMAT_PCM 1
#define WAVE_FORMAT_IMA_ADPCM 0x0011

#pragma pack(push, 1)
typedef struct tWAVEFORMATEX
{
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize;
} WAVEFORMATEX, *LPWAVEFORMATEX;
typedef const WAVEFORMATEX* LPCWAVEFORMATEX;
#pragma pack(pop)
