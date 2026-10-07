#ifndef SMUSH_SMUSHPLATFORM_H
#define SMUSH_SMUSHPLATFORM_H

// System layer of the SMUSH player (SmushPlay.c): audio output, time and file access.
// smushPlayWin32.c implements it with DirectSound; another backend (e.g. SDL3) can replace that file.

#include <j3dcore/j3d.h>
#include <smush/SmushPlay.h>
#include <stddef.h>
#include <stdint.h>

J3D_EXTERN_C_START

#define SMUSHPLATFORM_AUDIORATE 22050 // stereo PCM16, the format of the movies' audio

// Fills pOut with numFrames stereo frames. Called on the audio thread with the audio lock held.
typedef void (*SmushPlatformMixFunc)(int16_t* pOut, size_t numFrames);

// Starts the system layer and the audio output, which then calls pfMix whenever it needs data until shutdown.
// pDSound is the game's DirectSound object (NULL: the backend creates its own). Returns 0 if there is no audio;
// time and file access work either way.
int SmushPlatform_Startup(HWND hwnd, tDirectSound* pDSound, SmushPlatformMixFunc pfMix);
void SmushPlatform_Shutdown(void);

void SmushPlatform_LockAudio(void);
void SmushPlatform_UnlockAudio(void);

uint64_t SmushPlatform_GetTimeUsec(void);  // monotonic
void SmushPlatform_Wait(uint32_t usec);    // sleeps about usec, or just yields for very short waits

void* SmushPlatform_OpenFile(const char* pFilename);
size_t SmushPlatform_ReadFile(void* pFile, void* pBuffer, size_t size);
void SmushPlatform_CloseFile(void* pFile);

J3D_EXTERN_C_END

#endif // SMUSH_SMUSHPLATFORM_H
