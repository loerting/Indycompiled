// Native builds: the SMUSH player's system layer (smushPlatform.h) on SDL3. Audio is an SDL audio stream whose
// callback pulls stereo PCM16 at 22050 Hz from the player's mixer; the stream's lock is the audio lock.
#include "smushPlatform.h"

#include <SDL3/SDL.h>
#include <stdio.h>

static SDL_AudioStream* SmushSDL_pStream;
static SmushPlatformMixFunc SmushSDL_pfMix;
static bool SmushSDL_bAudioInit;

static void SDLCALL SmushSDL_AudioCallback(void* pUserData, SDL_AudioStream* pStream, int additionalAmount, int totalAmount)
{
    J3D_UNUSED(pUserData);
    J3D_UNUSED(totalAmount);

    int16_t aMix[1024 * 2];
    while ( additionalAmount > 0 )
    {
        size_t numFrames = (size_t)additionalAmount / (2 * sizeof(int16_t));
        if ( numFrames == 0 ) numFrames = 1;
        if ( numFrames > 1024 ) numFrames = 1024;
        SmushSDL_pfMix(aMix, numFrames);
        SDL_PutAudioStreamData(pStream, aMix, (int)(numFrames * 2 * sizeof(int16_t)));
        additionalAmount -= (int)(numFrames * 2 * sizeof(int16_t));
    }
}

int SmushPlatform_Startup(HWND hwnd, tDirectSound* pDSound, SmushPlatformMixFunc pfMix)
{
    J3D_UNUSED(hwnd);
    J3D_UNUSED(pDSound);

    SmushSDL_pfMix = pfMix;
    if ( !pfMix || !SDL_InitSubSystem(SDL_INIT_AUDIO) )
    {
        return 0;
    }
    SmushSDL_bAudioInit = true;

    const SDL_AudioSpec spec = { SDL_AUDIO_S16LE, 2, SMUSHPLATFORM_AUDIORATE };
    SmushSDL_pStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, SmushSDL_AudioCallback, NULL);
    if ( !SmushSDL_pStream )
    {
        fprintf(stderr, "SMUSH: no audio output: %s\n", SDL_GetError());
        return 0;
    }

    SDL_ResumeAudioStreamDevice(SmushSDL_pStream);
    return 1;
}

void SmushPlatform_Shutdown(void)
{
    if ( SmushSDL_pStream )
    {
        SDL_DestroyAudioStream(SmushSDL_pStream);
        SmushSDL_pStream = NULL;
    }

    if ( SmushSDL_bAudioInit )
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        SmushSDL_bAudioInit = false;
    }
    SmushSDL_pfMix = NULL;
}

void SmushPlatform_LockAudio(void)
{
    if ( SmushSDL_pStream ) SDL_LockAudioStream(SmushSDL_pStream);
}

void SmushPlatform_UnlockAudio(void)
{
    if ( SmushSDL_pStream ) SDL_UnlockAudioStream(SmushSDL_pStream);
}

uint64_t SmushPlatform_GetTimeUsec(void)
{
    return SDL_GetTicksNS() / 1000u;
}

void SmushPlatform_Wait(uint32_t usec)
{
    if ( usec < 1000 )
    {
        SDL_Delay(0);
        return;
    }
    SDL_DelayNS((uint64_t)usec * 1000u);
}

void* SmushPlatform_OpenFile(const char* pFilename)
{
    return fopen(pFilename, "rb");
}

size_t SmushPlatform_ReadFile(void* pFile, void* pBuffer, size_t size)
{
    return fread(pBuffer, 1, size, (FILE*)pFile);
}

void SmushPlatform_CloseFile(void* pFile)
{
    if ( pFile ) fclose((FILE*)pFile);
}
