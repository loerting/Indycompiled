// Native builds: the DirectSound 8 subset of cmake/compat/native/dsound.h as a software mixer on SDL3 audio, so the
// sound driver (DriverDX9.c) runs unchanged. Secondary buffers hold PCM (8-bit unsigned or 16-bit signed, mono or
// stereo, any rate); the mixer resamples them linearly to the output rate and applies DirectSound's volume and pan
// (hundredths of a decibel). No hardware 3D: the driver then computes volume and pan of 3D sounds itself.
#include <j3dcore/j3d.h>
#include <dsound.h>

#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define DSOUNDSDL_OUTRATE 44100

const GUID IID_IDirectSoundBuffer8      = { 0x6825a449, 0x7524, 0x4d82, { 0x92, 0x0f, 0x50, 0xe3, 0x6a, 0xb3, 0xab, 0x1e } };
const GUID IID_IDirectSound3DBuffer8    = { 0x279afa86, 0x4981, 0x11ce, { 0xa5, 0x21, 0x00, 0x20, 0xaf, 0x0b, 0xe5, 0x60 } };
const GUID IID_IDirectSound3DListener8  = { 0x279afa84, 0x4981, 0x11ce, { 0xa5, 0x21, 0x00, 0x20, 0xaf, 0x0b, 0xe5, 0x60 } };

typedef struct sDSoundSDLData // sample data, shared by duplicated buffers
{
    int refCount;
    uint8_t* pBytes;
    DWORD size;
} DSoundSDLData;

struct IDirectSoundBuffer
{
    IDirectSound* pDS;
    IDirectSoundBuffer* pNext; // in the device's list of secondary buffers
    int refCount;
    DWORD capsFlags;
    DSoundSDLData* pData;
    WAVEFORMATEX format;
    LONG volume;               // hundredths of dB, DSBVOLUME_MIN..DSBVOLUME_MAX
    LONG pan;                  // DSBPAN_LEFT..DSBPAN_RIGHT
    DWORD frequency;           // Hz
    bool bPlaying;
    bool bLooping;
    uint64_t position;         // in frames, 32.32 fixed point
    float gainLeft, gainRight; // from volume and pan
};

struct IDirectSound
{
    int refCount;
    SDL_AudioStream* pStream;
    SDL_Mutex* pLock;
    IDirectSoundBuffer* pBuffers;
};

static float DSoundSDL_DbToGain(LONG hundredthsDb)
{
    if ( hundredthsDb <= DSBVOLUME_MIN ) return 0.0f;
    return powf(10.0f, (float)hundredthsDb / 2000.0f);
}

static void DSoundSDL_UpdateGains(IDirectSoundBuffer* pBuf)
{
    float gain = DSoundSDL_DbToGain(pBuf->volume);
    pBuf->gainLeft  = gain * (pBuf->pan > 0 ? DSoundSDL_DbToGain(-pBuf->pan) : 1.0f);
    pBuf->gainRight = gain * (pBuf->pan < 0 ? DSoundSDL_DbToGain(pBuf->pan) : 1.0f);
}

static float DSoundSDL_Sample(const IDirectSoundBuffer* pBuf, DWORD frame, int channel)
{
    const WAVEFORMATEX* pFmt = &pBuf->format;
    int ch = pFmt->nChannels > 1 ? channel : 0;
    size_t offset = (size_t)frame * pFmt->nBlockAlign;
    if ( pFmt->wBitsPerSample == 16 )
    {
        int16_t s;
        memcpy(&s, pBuf->pData->pBytes + offset + (size_t)ch * 2, sizeof(s));
        return (float)s / 32768.0f;
    }
    return ((float)pBuf->pData->pBytes[offset + (size_t)ch] - 128.0f) / 128.0f;
}

// Mixes numFrames output frames of pBuf into pOut (stereo float)
static void DSoundSDL_MixBuffer(IDirectSoundBuffer* pBuf, float* pOut, int numFrames)
{
    if ( !pBuf->pData || !pBuf->format.nBlockAlign ) return;
    DWORD numSrcFrames = pBuf->pData->size / pBuf->format.nBlockAlign;
    if ( numSrcFrames == 0 )
    {
        pBuf->bPlaying = false;
        return;
    }

    const uint64_t step = ((uint64_t)pBuf->frequency << 32) / DSOUNDSDL_OUTRATE;
    for ( int i = 0; i < numFrames; ++i )
    {
        DWORD frame = (DWORD)(pBuf->position >> 32);
        if ( frame >= numSrcFrames )
        {
            if ( !pBuf->bLooping )
            {
                pBuf->bPlaying = false;
                pBuf->position = 0;
                return;
            }
            pBuf->position -= (uint64_t)numSrcFrames << 32;
            frame = (DWORD)(pBuf->position >> 32);
        }

        DWORD next = frame + 1 < numSrcFrames ? frame + 1 : (pBuf->bLooping ? 0 : frame);
        float t    = (float)(uint32_t)pBuf->position / 4294967296.0f;
        float l    = DSoundSDL_Sample(pBuf, frame, 0) * (1.0f - t) + DSoundSDL_Sample(pBuf, next, 0) * t;
        float r    = DSoundSDL_Sample(pBuf, frame, 1) * (1.0f - t) + DSoundSDL_Sample(pBuf, next, 1) * t;
        pOut[i * 2]     += l * pBuf->gainLeft;
        pOut[i * 2 + 1] += r * pBuf->gainRight;
        pBuf->position += step;
    }
}

static void SDLCALL DSoundSDL_AudioCallback(void* pUserData, SDL_AudioStream* pStream, int additionalAmount, int totalAmount)
{
    J3D_UNUSED(totalAmount);
    IDirectSound* pDS = (IDirectSound*)pUserData;
    float aMix[512 * 2];
    while ( additionalAmount > 0 )
    {
        int numFrames = additionalAmount / (int)(2 * sizeof(float));
        if ( numFrames <= 0 ) numFrames = 1;
        if ( numFrames > 512 ) numFrames = 512;
        memset(aMix, 0, sizeof(float) * 2 * (size_t)numFrames);

        SDL_LockMutex(pDS->pLock);
        for ( IDirectSoundBuffer* pBuf = pDS->pBuffers; pBuf; pBuf = pBuf->pNext )
        {
            if ( pBuf->bPlaying ) DSoundSDL_MixBuffer(pBuf, aMix, numFrames);
        }
        SDL_UnlockMutex(pDS->pLock);

        for ( int i = 0; i < numFrames * 2; ++i )
        {
            if ( aMix[i] > 1.0f ) aMix[i] = 1.0f;
            else if ( aMix[i] < -1.0f ) aMix[i] = -1.0f;
        }
        SDL_PutAudioStreamData(pStream, aMix, numFrames * (int)(2 * sizeof(float)));
        additionalAmount -= numFrames * (int)(2 * sizeof(float));
    }
}

HRESULT DirectSoundCreate8(const GUID* pDevice, LPDIRECTSOUND8* ppDS, void* pUnkOuter)
{
    J3D_UNUSED(pDevice);
    J3D_UNUSED(pUnkOuter);
    *ppDS = NULL;
    if ( !SDL_InitSubSystem(SDL_INIT_AUDIO) ) return DSERR_NODRIVER;

    IDirectSound* pDS = calloc(1, sizeof(IDirectSound));
    if ( !pDS )
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return DSERR_OUTOFMEMORY;
    }

    pDS->refCount = 1;
    pDS->pLock    = SDL_CreateMutex();
    const SDL_AudioSpec spec = { SDL_AUDIO_F32, 2, DSOUNDSDL_OUTRATE };
    pDS->pStream  = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, DSoundSDL_AudioCallback, pDS);
    if ( !pDS->pStream || !pDS->pLock )
    {
        if ( pDS->pLock ) SDL_DestroyMutex(pDS->pLock);
        free(pDS);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return DSERR_NODRIVER;
    }

    SDL_ResumeAudioStreamDevice(pDS->pStream);
    *ppDS = pDS;
    return DS_OK;
}

HRESULT DSoundSDL_SetCooperativeLevel(LPDIRECTSOUND8 pDS, HWND hwnd, DWORD level)
{
    J3D_UNUSED(pDS);
    J3D_UNUSED(hwnd);
    J3D_UNUSED(level);
    return DS_OK;
}

HRESULT DSoundSDL_GetCaps(LPDIRECTSOUND8 pDS, LPDSCAPS pCaps)
{
    J3D_UNUSED(pDS);
    DWORD size = pCaps->dwSize;
    memset(pCaps, 0, sizeof(DSCAPS));
    pCaps->dwSize                   = size;
    pCaps->dwMinSecondarySampleRate = 100;
    pCaps->dwMaxSecondarySampleRate = 200000;
    pCaps->dwPrimaryBuffers         = 1;
    return DS_OK; // no hardware mixing, no hardware 3D
}

static IDirectSoundBuffer* DSoundSDL_NewBuffer(IDirectSound* pDS)
{
    IDirectSoundBuffer* pBuf = calloc(1, sizeof(IDirectSoundBuffer));
    if ( !pBuf ) return NULL;
    pBuf->pDS       = pDS;
    pBuf->refCount  = 1;
    pBuf->volume    = DSBVOLUME_MAX;
    pBuf->frequency = DSOUNDSDL_OUTRATE;
    DSoundSDL_UpdateGains(pBuf);
    return pBuf;
}

static void DSoundSDL_LinkBuffer(IDirectSoundBuffer* pBuf)
{
    SDL_LockMutex(pBuf->pDS->pLock);
    pBuf->pNext           = pBuf->pDS->pBuffers;
    pBuf->pDS->pBuffers   = pBuf;
    SDL_UnlockMutex(pBuf->pDS->pLock);
}

HRESULT DSoundSDL_CreateSoundBuffer(LPDIRECTSOUND8 pDS, LPCDSBUFFERDESC pDesc, LPDIRECTSOUNDBUFFER* ppBuffer, void* pUnkOuter)
{
    J3D_UNUSED(pUnkOuter);
    *ppBuffer = NULL;
    IDirectSoundBuffer* pBuf = DSoundSDL_NewBuffer(pDS);
    if ( !pBuf ) return DSERR_OUTOFMEMORY;

    pBuf->capsFlags = (pDesc->dwFlags & ~(DSBCAPS_LOCHARDWARE | DSBCAPS_CTRL3D)) | DSBCAPS_LOCSOFTWARE;
    if ( (pDesc->dwFlags & DSBCAPS_PRIMARYBUFFER) == 0 )
    {
        if ( !pDesc->lpwfxFormat || !pDesc->dwBufferBytes ) { free(pBuf); return DSERR_INVALIDPARAM; }
        const WAVEFORMATEX* pFmt = pDesc->lpwfxFormat;
        if ( pFmt->wFormatTag != WAVE_FORMAT_PCM || (pFmt->wBitsPerSample != 8 && pFmt->wBitsPerSample != 16)
            || pFmt->nChannels < 1 || pFmt->nChannels > 2 )
        {
            free(pBuf);
            return DSERR_BADFORMAT;
        }

        pBuf->format    = *pFmt;
        pBuf->format.nBlockAlign = (WORD)(pFmt->nChannels * pFmt->wBitsPerSample / 8);
        pBuf->frequency = pFmt->nSamplesPerSec;
        pBuf->pData     = calloc(1, sizeof(DSoundSDLData));
        if ( pBuf->pData ) pBuf->pData->pBytes = calloc(1, pDesc->dwBufferBytes);
        if ( !pBuf->pData || !pBuf->pData->pBytes )
        {
            if ( pBuf->pData ) free(pBuf->pData);
            free(pBuf);
            return DSERR_OUTOFMEMORY;
        }
        pBuf->pData->refCount = 1;
        pBuf->pData->size     = pDesc->dwBufferBytes;
        DSoundSDL_LinkBuffer(pBuf);
    }

    pDS->refCount++;
    *ppBuffer = pBuf;
    return DS_OK;
}

HRESULT DSoundSDL_DuplicateSoundBuffer(LPDIRECTSOUND8 pDS, LPDIRECTSOUNDBUFFER pOriginal, LPDIRECTSOUNDBUFFER* ppDuplicate)
{
    *ppDuplicate = NULL;
    if ( !pOriginal || !pOriginal->pData ) return DSERR_INVALIDCALL;
    IDirectSoundBuffer* pBuf = DSoundSDL_NewBuffer(pDS);
    if ( !pBuf ) return DSERR_OUTOFMEMORY;

    // same data and parameters, stopped at the start
    pBuf->capsFlags = pOriginal->capsFlags;
    pBuf->format    = pOriginal->format;
    pBuf->volume    = pOriginal->volume;
    pBuf->pan       = pOriginal->pan;
    pBuf->frequency = pOriginal->frequency;
    pBuf->pData     = pOriginal->pData;
    pBuf->pData->refCount++;
    DSoundSDL_UpdateGains(pBuf);
    DSoundSDL_LinkBuffer(pBuf);

    pDS->refCount++;
    *ppDuplicate = pBuf;
    return DS_OK;
}

ULONG DSoundSDL_Release(LPDIRECTSOUND8 pDS)
{
    if ( !pDS ) return 0;
    if ( --pDS->refCount > 0 ) return (ULONG)pDS->refCount;
    SDL_DestroyAudioStream(pDS->pStream);
    SDL_DestroyMutex(pDS->pLock);
    free(pDS);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return 0;
}

HRESULT DSoundSDL_BufferQueryInterface(LPDIRECTSOUNDBUFFER pBuf, const GUID* pIID, void** ppObj)
{
    *ppObj = NULL;
    if ( pIID == &IID_IDirectSoundBuffer8 )
    {
        pBuf->refCount++;
        *ppObj = pBuf;
        return DS_OK;
    }
    return DSERR_NOINTERFACE; // no 3D buffer or listener
}

ULONG DSoundSDL_BufferRelease(LPDIRECTSOUNDBUFFER pBuf)
{
    if ( !pBuf ) return 0;
    if ( --pBuf->refCount > 0 ) return (ULONG)pBuf->refCount;

    IDirectSound* pDS = pBuf->pDS;
    SDL_LockMutex(pDS->pLock);
    for ( IDirectSoundBuffer** ppCur = &pDS->pBuffers; *ppCur; ppCur = &(*ppCur)->pNext )
    {
        if ( *ppCur == pBuf )
        {
            *ppCur = pBuf->pNext;
            break;
        }
    }
    SDL_UnlockMutex(pDS->pLock);

    if ( pBuf->pData && --pBuf->pData->refCount == 0 )
    {
        free(pBuf->pData->pBytes);
        free(pBuf->pData);
    }
    free(pBuf);
    DSoundSDL_Release(pDS);
    return 0;
}

HRESULT DSoundSDL_BufferGetCaps(LPDIRECTSOUNDBUFFER pBuf, LPDSBCAPS pCaps)
{
    pCaps->dwFlags              = pBuf->capsFlags;
    pCaps->dwBufferBytes        = pBuf->pData ? pBuf->pData->size : 0;
    pCaps->dwUnlockTransferRate = 0;
    pCaps->dwPlayCpuOverhead    = 0;
    return DS_OK;
}

HRESULT DSoundSDL_BufferSetFormat(LPDIRECTSOUNDBUFFER pBuf, const WAVEFORMATEX* pFormat)
{
    J3D_UNUSED(pBuf);
    J3D_UNUSED(pFormat);
    return DS_OK; // primary buffer: the mixer's output format is fixed
}

HRESULT DSoundSDL_BufferLock(LPDIRECTSOUNDBUFFER pBuf, DWORD offset, DWORD bytes, void** ppPtr1, DWORD* pBytes1, void** ppPtr2, DWORD* pBytes2, DWORD flags)
{
    if ( !pBuf->pData ) return DSERR_INVALIDCALL;
    if ( flags & DSBLOCK_ENTIREBUFFER )
    {
        offset = 0;
        bytes  = pBuf->pData->size;
    }
    if ( offset >= pBuf->pData->size ) return DSERR_INVALIDPARAM;
    if ( bytes > pBuf->pData->size ) bytes = pBuf->pData->size;

    DWORD first = bytes <= pBuf->pData->size - offset ? bytes : pBuf->pData->size - offset;
    *ppPtr1  = pBuf->pData->pBytes + offset;
    *pBytes1 = first;
    if ( ppPtr2 ) *ppPtr2 = first < bytes ? pBuf->pData->pBytes : NULL;
    if ( pBytes2 ) *pBytes2 = bytes - first;
    return DS_OK;
}

HRESULT DSoundSDL_BufferUnlock(LPDIRECTSOUNDBUFFER pBuf, void* pPtr1, DWORD bytes1, void* pPtr2, DWORD bytes2)
{
    J3D_UNUSED(pBuf); J3D_UNUSED(pPtr1); J3D_UNUSED(bytes1); J3D_UNUSED(pPtr2); J3D_UNUSED(bytes2);
    return DS_OK;
}

HRESULT DSoundSDL_BufferPlay(LPDIRECTSOUNDBUFFER pBuf, DWORD reserved, DWORD priority, DWORD flags)
{
    J3D_UNUSED(reserved);
    J3D_UNUSED(priority);
    if ( !pBuf->pData ) return DS_OK; // primary buffer
    SDL_LockMutex(pBuf->pDS->pLock);
    pBuf->bLooping = (flags & DSBPLAY_LOOPING) != 0;
    pBuf->bPlaying = true;
    SDL_UnlockMutex(pBuf->pDS->pLock);
    return DS_OK;
}

HRESULT DSoundSDL_BufferStop(LPDIRECTSOUNDBUFFER pBuf)
{
    SDL_LockMutex(pBuf->pDS->pLock);
    pBuf->bPlaying = false;
    SDL_UnlockMutex(pBuf->pDS->pLock);
    return DS_OK;
}

HRESULT DSoundSDL_BufferRestore(LPDIRECTSOUNDBUFFER pBuf)
{
    J3D_UNUSED(pBuf);
    return DS_OK; // memory buffers are never lost
}

HRESULT DSoundSDL_BufferSetVolume(LPDIRECTSOUNDBUFFER pBuf, LONG volume)
{
    if ( volume < DSBVOLUME_MIN || volume > DSBVOLUME_MAX ) return DSERR_INVALIDPARAM;
    SDL_LockMutex(pBuf->pDS->pLock);
    pBuf->volume = volume;
    DSoundSDL_UpdateGains(pBuf);
    SDL_UnlockMutex(pBuf->pDS->pLock);
    return DS_OK;
}

HRESULT DSoundSDL_BufferGetVolume(LPDIRECTSOUNDBUFFER pBuf, LONG* pVolume)
{
    *pVolume = pBuf->volume;
    return DS_OK;
}

HRESULT DSoundSDL_BufferSetPan(LPDIRECTSOUNDBUFFER pBuf, LONG pan)
{
    if ( pan < DSBPAN_LEFT || pan > DSBPAN_RIGHT ) return DSERR_INVALIDPARAM;
    SDL_LockMutex(pBuf->pDS->pLock);
    pBuf->pan = pan;
    DSoundSDL_UpdateGains(pBuf);
    SDL_UnlockMutex(pBuf->pDS->pLock);
    return DS_OK;
}

HRESULT DSoundSDL_BufferGetPan(LPDIRECTSOUNDBUFFER pBuf, LONG* pPan)
{
    *pPan = pBuf->pan;
    return DS_OK;
}

HRESULT DSoundSDL_BufferSetFrequency(LPDIRECTSOUNDBUFFER pBuf, DWORD freq)
{
    if ( freq != 0 && (freq < 100 || freq > 200000) ) return DSERR_INVALIDPARAM;
    SDL_LockMutex(pBuf->pDS->pLock);
    pBuf->frequency = freq ? freq : pBuf->format.nSamplesPerSec; // 0: the buffer's original rate
    SDL_UnlockMutex(pBuf->pDS->pLock);
    return DS_OK;
}

HRESULT DSoundSDL_BufferGetFrequency(LPDIRECTSOUNDBUFFER pBuf, DWORD* pFreq)
{
    *pFreq = pBuf->frequency;
    return DS_OK;
}

HRESULT DSoundSDL_BufferGetStatus(LPDIRECTSOUNDBUFFER pBuf, DWORD* pStatus)
{
    SDL_LockMutex(pBuf->pDS->pLock);
    *pStatus = pBuf->bPlaying ? (DSBSTATUS_PLAYING | (pBuf->bLooping ? DSBSTATUS_LOOPING : 0)) : 0;
    SDL_UnlockMutex(pBuf->pDS->pLock);
    return DS_OK;
}

HRESULT DSoundSDL_BufferGetCurrentPosition(LPDIRECTSOUNDBUFFER pBuf, DWORD* pPlay, DWORD* pWrite)
{
    SDL_LockMutex(pBuf->pDS->pLock);
    DWORD pos = (DWORD)(pBuf->position >> 32) * pBuf->format.nBlockAlign;
    SDL_UnlockMutex(pBuf->pDS->pLock);
    if ( pPlay ) *pPlay = pos;
    if ( pWrite ) *pWrite = pos;
    return DS_OK;
}

HRESULT DSoundSDL_BufferSetCurrentPosition(LPDIRECTSOUNDBUFFER pBuf, DWORD pos)
{
    if ( !pBuf->format.nBlockAlign ) return DS_OK;
    SDL_LockMutex(pBuf->pDS->pLock);
    pBuf->position = (uint64_t)(pos / pBuf->format.nBlockAlign) << 32;
    SDL_UnlockMutex(pBuf->pDS->pLock);
    return DS_OK;
}

HRESULT DSoundSDL_Unavailable(void)
{
    return DSERR_UNSUPPORTED;
}
