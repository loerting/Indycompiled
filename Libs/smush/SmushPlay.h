#ifndef SMUSHPLAY_H
#define SMUSHPLAY_H

#include <j3dcore/j3d.h>
#include <std/types.h>
#include <sound/Driver.h> // INDY: tDirectSound

J3D_EXTERN_C_START

#define SmushPlay_SysStartup_ADDR 0x004E30B0
#define SmushPlay_SysStartup_TYPE int (J3DAPI*)(HWND, tDirectSound*)

#define SmushPlay_SysShutdown_ADDR 0x004E33A0
#define SmushPlay_SysShutdown_TYPE int (*)(void)

#define SmushPlay_SetGlobalVolume_ADDR 0x004E33C0
#define SmushPlay_SetGlobalVolume_TYPE void (J3DAPI*)(size_t)

#define SmushPlay_PlayMovie_ADDR 0x004E3140
#define SmushPlay_PlayMovie_TYPE int (J3DAPI*)(const char*, int, int, int, int, int, int, int, int, int (__cdecl*)(const SmushBitmap*, int), int, int, int)

typedef struct sSmushColorFormat
{
    int redBPP;
    int greenBPP;
    int blueBPP;
    int redPosShift;
    int greenPosShift;
    int bluePosShift;
    int redPosShiftRight;
    int greenPosShiftRight;
    int bluePosShiftRight;
    int alphaBPP;
    int alphaPosShift;
    int alphaPosShiftRight;
} SmushColorFormat;

typedef struct sSmushBitmap
{
    void* pPixels;
    uint32_t width;
    uint32_t height;
    int pixelSize;
    int bpp;
    size_t pitch;
    int unknown6;
    int unknown7;
    tColorMode colorMode;
    SmushColorFormat format;
    int unknown21;
    int unknown22;
} SmushBitmap;

// INDY: the player is implemented in C (SmushPlay.c, smushDecoder.c, smushPlayWin32.c) instead of trampolines into
// the exe's statically linked SMUSH library

// Called for every picture of the movie; a nonzero return value stops the movie
typedef int (__cdecl* SmushBlitFunc)(const SmushBitmap* pBitmap, int frameNum);

// Starts the player's audio output on the game's DirectSound object (NULL: own object). Returns 1.
int J3DAPI SmushPlay_SysStartup(HWND hwnd, tDirectSound* pDSound);
void SmushPlay_SysShutdown(void);

// Volume 0-127 of the movie audio, used from the next SmushPlay_PlayMovie on
void J3DAPI SmushPlay_SetGlobalVolume(size_t volume);

// Plays a SMUSH movie numLoops times (0: not at all) at fps frames per second (0: 15); pfBlit gets each picture
// as RGB565. flags: 2 = never drop late pictures. width and height, a8 and a9 and the stream parameters only
// matter for the original's own rendering and file streaming, which the game doesn't use.
// Returns 1 if the movie could be opened (also when stopped by pfBlit), else 0.
int J3DAPI SmushPlay_PlayMovie(const char* pFilename, int fps, int flags, int unused4, int unused5, int width, int height, int a8, int a9, SmushBlitFunc pfBlit, int numLoops, int streamParam1, int streamParam2);

J3D_EXTERN_C_END

#endif // SMUSHPLAY_H
