// Native builds (Linux, Android): the subset of DirectSound 8 that the sound driver (sound/DriverDX9.c) uses,
// implemented as a software mixer on SDL3 audio (sound/DirectSoundSDL.c). Constants and structs as in the Windows SDK.
// There is no 3D hardware: GetCaps reports none and the 3D interfaces aren't available, so the driver mixes 3D sounds
// itself (volume and pan), as on Windows without hardware 3D.
#pragma once
#include <j3dcore/j3dwin32.h>
#include <Mmreg.h>

typedef struct IDirectSound IDirectSound, IDirectSound8, *LPDIRECTSOUND, *LPDIRECTSOUND8;
typedef struct IDirectSoundBuffer IDirectSoundBuffer, IDirectSoundBuffer8, *LPDIRECTSOUNDBUFFER, *LPDIRECTSOUNDBUFFER8;
typedef struct IDirectSound3DBuffer IDirectSound3DBuffer, IDirectSound3DBuffer8, *LPDIRECTSOUND3DBUFFER, *LPDIRECTSOUND3DBUFFER8;
typedef struct IDirectSound3DListener IDirectSound3DListener, IDirectSound3DListener8, *LPDIRECTSOUND3DLISTENER, *LPDIRECTSOUND3DLISTENER8;
typedef float D3DVALUE;

#define DS_OK                    0
#define DSHRESULT(code)          ((HRESULT)(0x88780000u | (code)))
#define DSERR_ALLOCATED          DSHRESULT(10)
#define DSERR_CONTROLUNAVAIL     DSHRESULT(30)
#define DSERR_INVALIDPARAM       ((HRESULT)0x80070057)
#define DSERR_INVALIDCALL        DSHRESULT(50)
#define DSERR_GENERIC            E_FAIL
#define DSERR_PRIOLEVELNEEDED    DSHRESULT(70)
#define DSERR_OUTOFMEMORY        ((HRESULT)0x8007000E)
#define DSERR_BADFORMAT          DSHRESULT(100)
#define DSERR_UNSUPPORTED        ((HRESULT)0x80004001)
#define DSERR_NODRIVER           DSHRESULT(120)
#define DSERR_ALREADYINITIALIZED DSHRESULT(130)
#define DSERR_NOAGGREGATION      ((HRESULT)0x80040110)
#define DSERR_BUFFERLOST         DSHRESULT(150)
#define DSERR_OTHERAPPHASPRIO    DSHRESULT(160)
#define DSERR_UNINITIALIZED      DSHRESULT(170)
#define DSERR_NOINTERFACE        ((HRESULT)0x80004002)

#define DSSCL_NORMAL    1
#define DSSCL_PRIORITY  2
#define DSSCL_EXCLUSIVE 3

#define DSBCAPS_PRIMARYBUFFER       0x00000001
#define DSBCAPS_LOCHARDWARE         0x00000004
#define DSBCAPS_LOCSOFTWARE         0x00000008
#define DSBCAPS_CTRL3D              0x00000010
#define DSBCAPS_CTRLFREQUENCY       0x00000020
#define DSBCAPS_CTRLPAN             0x00000040
#define DSBCAPS_CTRLVOLUME          0x00000080
#define DSBCAPS_GLOBALFOCUS         0x00008000
#define DSBCAPS_GETCURRENTPOSITION2 0x00010000

#define DSBPLAY_LOOPING         0x00000001
#define DSBSTATUS_PLAYING       0x00000001
#define DSBSTATUS_LOOPING       0x00000004
#define DSBLOCK_FROMWRITECURSOR 0x00000001
#define DSBLOCK_ENTIREBUFFER    0x00000002

#define DSBVOLUME_MIN -10000
#define DSBVOLUME_MAX 0
#define DSBPAN_LEFT   -10000
#define DSBPAN_RIGHT  10000

#define DS3D_IMMEDIATE                0x00000000
#define DS3D_DEFERRED                 0x00000001
#define DS3DMODE_NORMAL               0x00000000
#define DS3DMODE_DISABLE              0x00000002
#define DS3D_DEFAULTCONEOUTSIDEVOLUME DSBVOLUME_MAX

typedef struct _DSCAPS
{
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwMinSecondarySampleRate;
    DWORD dwMaxSecondarySampleRate;
    DWORD dwPrimaryBuffers;
    DWORD dwMaxHwMixingAllBuffers;
    DWORD dwMaxHwMixingStaticBuffers;
    DWORD dwMaxHwMixingStreamingBuffers;
    DWORD dwFreeHwMixingAllBuffers;
    DWORD dwFreeHwMixingStaticBuffers;
    DWORD dwFreeHwMixingStreamingBuffers;
    DWORD dwMaxHw3DAllBuffers;
    DWORD dwMaxHw3DStaticBuffers;
    DWORD dwMaxHw3DStreamingBuffers;
    DWORD dwFreeHw3DAllBuffers;
    DWORD dwFreeHw3DStaticBuffers;
    DWORD dwFreeHw3DStreamingBuffers;
    DWORD dwTotalHwMemBytes;
    DWORD dwFreeHwMemBytes;
    DWORD dwMaxContigFreeHwMemBytes;
    DWORD dwUnlockTransferRateHwBuffers;
    DWORD dwPlayCpuOverheadSwBuffers;
    DWORD dwReserved1;
    DWORD dwReserved2;
} DSCAPS, *LPDSCAPS;

typedef struct _DSBCAPS
{
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwBufferBytes;
    DWORD dwUnlockTransferRate;
    DWORD dwPlayCpuOverhead;
} DSBCAPS, *LPDSBCAPS;

typedef struct _DSBUFFERDESC
{
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwBufferBytes;
    DWORD dwReserved;
    LPWAVEFORMATEX lpwfxFormat;
    GUID guid3DAlgorithm;
} DSBUFFERDESC, *LPDSBUFFERDESC;
typedef const DSBUFFERDESC* LPCDSBUFFERDESC;

// Interface IDs: only their addresses are compared
extern const GUID IID_IDirectSoundBuffer8;
extern const GUID IID_IDirectSound3DBuffer8;
extern const GUID IID_IDirectSound3DListener8;

HRESULT DirectSoundCreate8(const GUID* pDevice, LPDIRECTSOUND8* ppDS, void* pUnkOuter);
HRESULT DSoundSDL_SetCooperativeLevel(LPDIRECTSOUND8 pDS, HWND hwnd, DWORD level);
HRESULT DSoundSDL_GetCaps(LPDIRECTSOUND8 pDS, LPDSCAPS pCaps);
HRESULT DSoundSDL_CreateSoundBuffer(LPDIRECTSOUND8 pDS, LPCDSBUFFERDESC pDesc, LPDIRECTSOUNDBUFFER* ppBuffer, void* pUnkOuter);
HRESULT DSoundSDL_DuplicateSoundBuffer(LPDIRECTSOUND8 pDS, LPDIRECTSOUNDBUFFER pOriginal, LPDIRECTSOUNDBUFFER* ppDuplicate);
ULONG DSoundSDL_Release(LPDIRECTSOUND8 pDS);

HRESULT DSoundSDL_BufferQueryInterface(LPDIRECTSOUNDBUFFER pBuf, const GUID* pIID, void** ppObj);
ULONG DSoundSDL_BufferRelease(LPDIRECTSOUNDBUFFER pBuf);
HRESULT DSoundSDL_BufferGetCaps(LPDIRECTSOUNDBUFFER pBuf, LPDSBCAPS pCaps);
HRESULT DSoundSDL_BufferSetFormat(LPDIRECTSOUNDBUFFER pBuf, const WAVEFORMATEX* pFormat);
HRESULT DSoundSDL_BufferLock(LPDIRECTSOUNDBUFFER pBuf, DWORD offset, DWORD bytes, void** ppPtr1, DWORD* pBytes1, void** ppPtr2, DWORD* pBytes2, DWORD flags);
HRESULT DSoundSDL_BufferUnlock(LPDIRECTSOUNDBUFFER pBuf, void* pPtr1, DWORD bytes1, void* pPtr2, DWORD bytes2);
HRESULT DSoundSDL_BufferPlay(LPDIRECTSOUNDBUFFER pBuf, DWORD reserved, DWORD priority, DWORD flags);
HRESULT DSoundSDL_BufferStop(LPDIRECTSOUNDBUFFER pBuf);
HRESULT DSoundSDL_BufferRestore(LPDIRECTSOUNDBUFFER pBuf);
HRESULT DSoundSDL_BufferSetVolume(LPDIRECTSOUNDBUFFER pBuf, LONG volume);
HRESULT DSoundSDL_BufferGetVolume(LPDIRECTSOUNDBUFFER pBuf, LONG* pVolume);
HRESULT DSoundSDL_BufferSetPan(LPDIRECTSOUNDBUFFER pBuf, LONG pan);
HRESULT DSoundSDL_BufferGetPan(LPDIRECTSOUNDBUFFER pBuf, LONG* pPan);
HRESULT DSoundSDL_BufferSetFrequency(LPDIRECTSOUNDBUFFER pBuf, DWORD freq);
HRESULT DSoundSDL_BufferGetFrequency(LPDIRECTSOUNDBUFFER pBuf, DWORD* pFreq);
HRESULT DSoundSDL_BufferGetStatus(LPDIRECTSOUNDBUFFER pBuf, DWORD* pStatus);
HRESULT DSoundSDL_BufferGetCurrentPosition(LPDIRECTSOUNDBUFFER pBuf, DWORD* pPlay, DWORD* pWrite);
HRESULT DSoundSDL_BufferSetCurrentPosition(LPDIRECTSOUNDBUFFER pBuf, DWORD pos);
HRESULT DSoundSDL_Unavailable(void); // 3D interfaces: never handed out

#define IDirectSound8_SetCooperativeLevel(p, h, l)          DSoundSDL_SetCooperativeLevel(p, h, l)
#define IDirectSound8_GetCaps(p, c)                         DSoundSDL_GetCaps(p, c)
#define IDirectSound8_CreateSoundBuffer(p, d, b, u)         DSoundSDL_CreateSoundBuffer(p, d, b, u)
#define IDirectSound8_DuplicateSoundBuffer(p, o, d)         DSoundSDL_DuplicateSoundBuffer(p, o, d)
#define IDirectSound8_Release(p)                            DSoundSDL_Release(p)
#define IDirectSound_Release(p)                             DSoundSDL_Release(p)

#define IDirectSoundBuffer8_QueryInterface(p, i, o)         DSoundSDL_BufferQueryInterface(p, i, (void**)(o))
#define IDirectSoundBuffer_QueryInterface(p, i, o)          DSoundSDL_BufferQueryInterface(p, i, (void**)(o))
#define IDirectSoundBuffer8_Release(p)                      DSoundSDL_BufferRelease(p)
#define IDirectSoundBuffer_Release(p)                       DSoundSDL_BufferRelease(p)
#define IDirectSoundBuffer8_GetCaps(p, c)                   DSoundSDL_BufferGetCaps(p, c)
#define IDirectSoundBuffer8_SetFormat(p, f)                 DSoundSDL_BufferSetFormat(p, f)
#define IDirectSoundBuffer8_Lock(p, o, b, p1, b1, p2, b2, f) DSoundSDL_BufferLock(p, o, b, p1, b1, p2, b2, f)
#define IDirectSoundBuffer8_Unlock(p, p1, b1, p2, b2)       DSoundSDL_BufferUnlock(p, p1, b1, p2, b2)
#define IDirectSoundBuffer8_Play(p, r, pr, f)               DSoundSDL_BufferPlay(p, r, pr, f)
#define IDirectSoundBuffer8_Stop(p)                         DSoundSDL_BufferStop(p)
#define IDirectSoundBuffer8_Restore(p)                      DSoundSDL_BufferRestore(p)
#define IDirectSoundBuffer8_SetVolume(p, v)                 DSoundSDL_BufferSetVolume(p, v)
#define IDirectSoundBuffer8_GetVolume(p, v)                 DSoundSDL_BufferGetVolume(p, v)
#define IDirectSoundBuffer8_SetPan(p, v)                    DSoundSDL_BufferSetPan(p, v)
#define IDirectSoundBuffer8_GetPan(p, v)                    DSoundSDL_BufferGetPan(p, v)
#define IDirectSoundBuffer8_SetFrequency(p, v)              DSoundSDL_BufferSetFrequency(p, v)
#define IDirectSoundBuffer8_GetFrequency(p, v)              DSoundSDL_BufferGetFrequency(p, v)
#define IDirectSoundBuffer8_GetStatus(p, s)                 DSoundSDL_BufferGetStatus(p, s)
#define IDirectSoundBuffer8_GetCurrentPosition(p, pl, w)    DSoundSDL_BufferGetCurrentPosition(p, pl, w)
#define IDirectSoundBuffer8_SetCurrentPosition(p, pos)      DSoundSDL_BufferSetCurrentPosition(p, pos)

#define IDirectSound3DBuffer_SetMode(p, ...)                    ((void)(p), DSoundSDL_Unavailable())
#define IDirectSound3DBuffer_SetPosition(p, ...)                ((void)(p), DSoundSDL_Unavailable())
#define IDirectSound3DBuffer_SetVelocity(p, ...)                ((void)(p), DSoundSDL_Unavailable())
#define IDirectSound3DBuffer_SetMinDistance(p, ...)             ((void)(p), DSoundSDL_Unavailable())
#define IDirectSound3DBuffer_SetMaxDistance(p, ...)             ((void)(p), DSoundSDL_Unavailable())
#define IDirectSound3DBuffer_Release(p)                         ((void)(p), 0)
#define IDirectSound3DListener_SetPosition(p, ...)              ((void)(p), DSoundSDL_Unavailable())
#define IDirectSound3DListener_SetVelocity(p, ...)              ((void)(p), DSoundSDL_Unavailable())
#define IDirectSound3DListener_SetOrientation(p, ...)           ((void)(p), DSoundSDL_Unavailable())
#define IDirectSound3DListener_SetDistanceFactor(p, ...)        ((void)(p), DSoundSDL_Unavailable())
#define IDirectSound3DListener_SetRolloffFactor(p, ...)         ((void)(p), DSoundSDL_Unavailable())
#define IDirectSound3DListener_SetDopplerFactor(p, ...)         ((void)(p), DSoundSDL_Unavailable())
#define IDirectSound3DListener_CommitDeferredSettings(p)        ((void)(p), DSoundSDL_Unavailable())
#define IDirectSound3DListener_Release(p)                       ((void)(p), 0)
