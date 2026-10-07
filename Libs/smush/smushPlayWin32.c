#include "smushPlatform.h"

#include <mmsystem.h>
#include <stdio.h>
#include <string.h>

// The original streams the mixed audio through a looping secondary buffer of three blocks and refills a block as
// soon as the play cursor has left it
#define SMUSHWIN32_NUMBLOCKS    3
#define SMUSHWIN32_BLOCKFRAMES  1536
#define SMUSHWIN32_BLOCKSIZE    (SMUSHWIN32_BLOCKFRAMES * 2 * (int)sizeof(int16_t))
#define SMUSHWIN32_POLLMSEC     10

static CRITICAL_SECTION SmushWin32_audioLock;
static int SmushWin32_bStarted;
static HANDLE SmushWin32_hThread;
static HANDLE SmushWin32_hStopEvent;
static SmushPlatformMixFunc SmushWin32_pfMix;

static IDirectSound* SmushWin32_pDSound;
static int SmushWin32_bOwnDSound;
static IDirectSoundBuffer* SmushWin32_pPrimary;
static LONG SmushWin32_primaryVolume;
static IDirectSoundBuffer* SmushWin32_pBuffer;
static int SmushWin32_lastBlock;
static int16_t SmushWin32_aMixBuffer[SMUSHWIN32_BLOCKFRAMES * 2];

static LARGE_INTEGER SmushWin32_timerFrequency;

static void SmushWin32_InitFormat(WAVEFORMATEX* pFormat)
{
    memset(pFormat, 0, sizeof(WAVEFORMATEX));
    pFormat->wFormatTag      = WAVE_FORMAT_PCM;
    pFormat->nChannels       = 2;
    pFormat->nSamplesPerSec  = SMUSHPLATFORM_AUDIORATE;
    pFormat->wBitsPerSample  = 16;
    pFormat->nBlockAlign     = (WORD)(pFormat->nChannels * pFormat->wBitsPerSample / 8);
    pFormat->nAvgBytesPerSec = pFormat->nSamplesPerSec * pFormat->nBlockAlign;
}

// Own DirectSound object when the game has none, set up as the original does
static int SmushWin32_CreateDirectSound(HWND hwnd)
{
    IDirectSound* pDSound = NULL;
    if ( FAILED(DirectSoundCreate(NULL, &pDSound, NULL)) )
    {
        return 0;
    }

    if ( FAILED(IDirectSound_SetCooperativeLevel(pDSound, hwnd, DSSCL_EXCLUSIVE)) )
    {
        IDirectSound_Release(pDSound);
        return 0;
    }

    DSBUFFERDESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize  = sizeof(desc);
    desc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;

    IDirectSoundBuffer* pPrimary = NULL;
    if ( FAILED(IDirectSound_CreateSoundBuffer(pDSound, &desc, &pPrimary, NULL)) )
    {
        IDirectSound_Release(pDSound);
        return 0;
    }

    WAVEFORMATEX format;
    SmushWin32_InitFormat(&format);
    if ( FAILED(IDirectSoundBuffer_SetFormat(pPrimary, &format))
        || FAILED(IDirectSoundBuffer_GetVolume(pPrimary, &SmushWin32_primaryVolume))
        || FAILED(IDirectSoundBuffer_SetVolume(pPrimary, DSBVOLUME_MAX)) )
    {
        IDirectSoundBuffer_Release(pPrimary);
        IDirectSound_Release(pDSound);
        return 0;
    }

    SmushWin32_pDSound     = pDSound;
    SmushWin32_pPrimary    = pPrimary;
    SmushWin32_bOwnDSound  = 1;
    return 1;
}

static void SmushWin32_ReleaseDirectSound(void)
{
    if ( SmushWin32_bOwnDSound )
    {
        if ( SmushWin32_pPrimary )
        {
            IDirectSoundBuffer_SetVolume(SmushWin32_pPrimary, SmushWin32_primaryVolume);
            IDirectSoundBuffer_Release(SmushWin32_pPrimary);
        }

        IDirectSound_Release(SmushWin32_pDSound);
    }

    SmushWin32_pPrimary   = NULL;
    SmushWin32_pDSound    = NULL;
    SmushWin32_bOwnDSound = 0;
}

static int SmushWin32_CreateBuffer(void)
{
    WAVEFORMATEX format;
    SmushWin32_InitFormat(&format);

    DSBUFFERDESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize        = sizeof(desc);
    desc.dwFlags       = DSBCAPS_CTRLVOLUME | DSBCAPS_GETCURRENTPOSITION2;
    desc.dwBufferBytes = SMUSHWIN32_NUMBLOCKS * SMUSHWIN32_BLOCKSIZE;
    desc.lpwfxFormat   = &format;

    if ( FAILED(IDirectSound_CreateSoundBuffer(SmushWin32_pDSound, &desc, &SmushWin32_pBuffer, NULL)) )
    {
        SmushWin32_pBuffer = NULL;
        return 0;
    }

    void* pData;
    DWORD dataSize;
    if ( SUCCEEDED(IDirectSoundBuffer_Lock(SmushWin32_pBuffer, 0, 0, &pData, &dataSize, NULL, NULL, DSBLOCK_ENTIREBUFFER)) )
    {
        memset(pData, 0, dataSize);
        IDirectSoundBuffer_Unlock(SmushWin32_pBuffer, pData, dataSize, NULL, 0);
    }

    if ( FAILED(IDirectSoundBuffer_Play(SmushWin32_pBuffer, 0, 0, DSBPLAY_LOOPING)) )
    {
        IDirectSoundBuffer_Release(SmushWin32_pBuffer);
        SmushWin32_pBuffer = NULL;
        return 0;
    }

    SmushWin32_lastBlock = -1;
    return 1;
}

static void SmushWin32_WriteBlock(int block)
{
    void* pData1;
    void* pData2;
    DWORD size1, size2;
    HRESULT res = IDirectSoundBuffer_Lock(SmushWin32_pBuffer, (DWORD)(block * SMUSHWIN32_BLOCKSIZE), SMUSHWIN32_BLOCKSIZE, &pData1, &size1, &pData2, &size2, 0);
    if ( res == DSERR_BUFFERLOST )
    {
        IDirectSoundBuffer_Restore(SmushWin32_pBuffer);
        res = IDirectSoundBuffer_Lock(SmushWin32_pBuffer, (DWORD)(block * SMUSHWIN32_BLOCKSIZE), SMUSHWIN32_BLOCKSIZE, &pData1, &size1, &pData2, &size2, 0);
    }

    if ( FAILED(res) )
    {
        return;
    }

    memcpy(pData1, SmushWin32_aMixBuffer, size1);
    if ( pData2 )
    {
        memcpy(pData2, (uint8_t*)SmushWin32_aMixBuffer + size1, size2);
    }

    IDirectSoundBuffer_Unlock(SmushWin32_pBuffer, pData1, size1, pData2, size2);
}

static void SmushWin32_FillBuffer(void)
{
    DWORD playPos, writePos;
    if ( FAILED(IDirectSoundBuffer_GetCurrentPosition(SmushWin32_pBuffer, &playPos, &writePos)) )
    {
        return;
    }

    const int playBlock = (int)(playPos / SMUSHWIN32_BLOCKSIZE) % SMUSHWIN32_NUMBLOCKS;
    if ( SmushWin32_lastBlock < 0 )
    {
        SmushWin32_lastBlock = playBlock;
    }

    // Keep every block except the one playing filled
    for ( int block = (SmushWin32_lastBlock + 1) % SMUSHWIN32_NUMBLOCKS; block != playBlock; block = (block + 1) % SMUSHWIN32_NUMBLOCKS )
    {
        EnterCriticalSection(&SmushWin32_audioLock);
        SmushWin32_pfMix(SmushWin32_aMixBuffer, SMUSHWIN32_BLOCKFRAMES);
        LeaveCriticalSection(&SmushWin32_audioLock);

        SmushWin32_WriteBlock(block);
        SmushWin32_lastBlock = block;
    }
}

static DWORD WINAPI SmushWin32_AudioThread(LPVOID pParam)
{
    (void)pParam;
    while ( WaitForSingleObject(SmushWin32_hStopEvent, SMUSHWIN32_POLLMSEC) == WAIT_TIMEOUT )
    {
        SmushWin32_FillBuffer();
    }

    return 0;
}

static int SmushWin32_StartAudio(HWND hwnd, tDirectSound* pDSound, SmushPlatformMixFunc pfMix)
{
    if ( pDSound )
    {
        // IDirectSound8 starts with the IDirectSound interface
        SmushWin32_pDSound = (IDirectSound*)pDSound;
    }
    else if ( !SmushWin32_CreateDirectSound(hwnd) )
    {
        return 0;
    }

    if ( !SmushWin32_CreateBuffer() )
    {
        SmushWin32_ReleaseDirectSound();
        return 0;
    }

    SmushWin32_pfMix      = pfMix;
    SmushWin32_hStopEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    SmushWin32_hThread    = SmushWin32_hStopEvent ? CreateThread(NULL, 0, SmushWin32_AudioThread, NULL, 0, NULL) : NULL;
    if ( !SmushWin32_hThread )
    {
        if ( SmushWin32_hStopEvent )
        {
            CloseHandle(SmushWin32_hStopEvent);
            SmushWin32_hStopEvent = NULL;
        }

        IDirectSoundBuffer_Stop(SmushWin32_pBuffer);
        IDirectSoundBuffer_Release(SmushWin32_pBuffer);
        SmushWin32_pBuffer = NULL;
        SmushWin32_ReleaseDirectSound();
        return 0;
    }

    SetThreadPriority(SmushWin32_hThread, THREAD_PRIORITY_TIME_CRITICAL);
    return 1;
}

static void SmushWin32_StopAudio(void)
{
    if ( !SmushWin32_hThread )
    {
        return;
    }

    SetEvent(SmushWin32_hStopEvent);
    WaitForSingleObject(SmushWin32_hThread, INFINITE);
    CloseHandle(SmushWin32_hThread);
    CloseHandle(SmushWin32_hStopEvent);
    SmushWin32_hThread    = NULL;
    SmushWin32_hStopEvent = NULL;

    IDirectSoundBuffer_Stop(SmushWin32_pBuffer);
    IDirectSoundBuffer_Release(SmushWin32_pBuffer);
    SmushWin32_pBuffer = NULL;
    SmushWin32_ReleaseDirectSound();
}

int SmushPlatform_Startup(HWND hwnd, tDirectSound* pDSound, SmushPlatformMixFunc pfMix)
{
    if ( !SmushWin32_bStarted )
    {
        InitializeCriticalSection(&SmushWin32_audioLock);
        QueryPerformanceFrequency(&SmushWin32_timerFrequency);
        timeBeginPeriod(1); // 1 ms sleeps for the frame timing
        SmushWin32_bStarted = 1;
    }

    return SmushWin32_StartAudio(hwnd, pDSound, pfMix);
}

void SmushPlatform_Shutdown(void)
{
    if ( !SmushWin32_bStarted )
    {
        return;
    }

    SmushWin32_StopAudio();
    timeEndPeriod(1);
    DeleteCriticalSection(&SmushWin32_audioLock);
    SmushWin32_bStarted = 0;
}

void SmushPlatform_LockAudio(void)
{
    EnterCriticalSection(&SmushWin32_audioLock);
}

void SmushPlatform_UnlockAudio(void)
{
    LeaveCriticalSection(&SmushWin32_audioLock);
}

uint64_t SmushPlatform_GetTimeUsec(void)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    if ( !SmushWin32_timerFrequency.QuadPart )
    {
        QueryPerformanceFrequency(&SmushWin32_timerFrequency);
    }

    const uint64_t frequency = (uint64_t)SmushWin32_timerFrequency.QuadPart;
    const uint64_t ticks     = (uint64_t)counter.QuadPart;
    return ticks / frequency * 1000000 + ticks % frequency * 1000000 / frequency;
}

void SmushPlatform_Wait(uint32_t usec)
{
    if ( usec >= 2000 )
    {
        Sleep(usec / 1000 - 1);
    }
    else
    {
        Sleep(0);
    }
}

void* SmushPlatform_OpenFile(const char* pFilename)
{
    FILE* pFile = NULL;
    fopen_s(&pFile, pFilename, "rb");
    return pFile;
}

size_t SmushPlatform_ReadFile(void* pFile, void* pBuffer, size_t size)
{
    return fread(pBuffer, 1, size, (FILE*)pFile);
}

void SmushPlatform_CloseFile(void* pFile)
{
    fclose((FILE*)pFile);
}
