#include "SmushPlay.h"
#include "smushDecoder.h"
#include "smushPlatform.h"

#include <stdlib.h>
#include <string.h>

// Times are in ticks of the PC timer chip (1193180 Hz), as in the original player
#define SMUSHPLAY_TICKRATE         1193180
#define SMUSHPLAY_LAGSTEP          79545    // period correction per frame of lag: 1/15 s whatever the frame rate
#define SMUSHPLAY_MAXLAG           20
#define SMUSHPLAY_SKIPLAG          3        // start dropping pictures at this many frames behind the audio
#define SMUSHPLAY_MAXSKIPEPISODES  4        // after more catch-up episodes than this, dropping stays on
#define SMUSHPLAY_NUMSTARTFRAMES   10       // frames before the lag counting and dropping settle

#define SMUSHPLAY_FLAG_NOSKIP      0x02

#define SMUSHPLAY_SKIPPING         0x80     // SmushPlayTiming.skipState: dropping pictures

#define SMUSHPLAY_AUDIOBUFFERFRAMES 125000  // 500000 bytes, the original's movie channel buffer

typedef struct sSmushPlayAudio
{
    int bStarted;
    int16_t* pRing;          // stereo frames queued for the output
    size_t readPos;
    size_t numQueued;
    int volume;              // 0-127
    int bPlaying;            // the current movie has audio
    uint64_t numMixedFrames; // audio clock: frames handed to the output since startup
} SmushPlayAudio;

typedef struct sSmushPlayTiming
{
    int64_t basePeriod;     // ticks per frame at the nominal rate
    int64_t framePeriod;    // ticks from one frame to the next, corrected by the lag
    int64_t frameElapsed;
    int bFrameDue;
    int bLagStarted;
    int lag;                // frames the picture is behind the audio clock
    int64_t lagElapsed;
    int64_t sinceSync;      // real time since the audio clock last advanced
    uint64_t lastMixed;
    uint64_t mixRemainder;
    uint64_t startTime;     // usec
    int64_t lastTicks;
    int skipState;          // SMUSHPLAY_SKIPPING + number of catch-up episodes
} SmushPlayTiming;

static int SmushPlay_bStarted;
static int SmushPlay_globalVolume = 127;
static SmushPlayAudio SmushPlay_audio;

static void SmushPlay_MixAudio(int16_t* pOut, size_t numFrames)
{
    SmushPlayAudio* pAudio = &SmushPlay_audio;
    size_t numCopied = 0;
    if ( pAudio->bPlaying )
    {
        // Single channel at full channel volume: the original's mixer scales by volume + 1 and shifts by 7
        const int scale = pAudio->volume + 1;
        numCopied = numFrames < pAudio->numQueued ? numFrames : pAudio->numQueued;
        for ( size_t i = 0; i < numCopied; i++ )
        {
            const int16_t* pIn = &pAudio->pRing[pAudio->readPos * 2];
            pOut[2 * i]     = (int16_t)((pIn[0] * scale) >> 7);
            pOut[2 * i + 1] = (int16_t)((pIn[1] * scale) >> 7);
            if ( ++pAudio->readPos == SMUSHPLAY_AUDIOBUFFERFRAMES )
            {
                pAudio->readPos = 0;
            }
        }

        pAudio->numQueued -= numCopied;
    }

    memset(pOut + 2 * numCopied, 0, (numFrames - numCopied) * 2 * sizeof(int16_t));
    pAudio->numMixedFrames += numFrames;
}

static void SmushPlay_QueueAudio(const SmushFrame* pFrame)
{
    SmushPlayAudio* pAudio = &SmushPlay_audio;
    if ( !pAudio->bStarted || !pFrame->pAudio )
    {
        return;
    }

    SmushPlatform_LockAudio();

    for ( size_t i = 0; i < pFrame->numAudioSamples; i++ )
    {
        if ( pAudio->numQueued == SMUSHPLAY_AUDIOBUFFERFRAMES )
        {
            // Full: like the original, drop the oldest data rather than the new
            pAudio->readPos = (pAudio->readPos + 1) % SMUSHPLAY_AUDIOBUFFERFRAMES;
            pAudio->numQueued--;
        }

        const size_t writePos = (pAudio->readPos + pAudio->numQueued) % SMUSHPLAY_AUDIOBUFFERFRAMES;
        const int16_t* pIn = pFrame->pAudio + i * pFrame->numAudioChannels;
        pAudio->pRing[writePos * 2]     = pIn[0];
        pAudio->pRing[writePos * 2 + 1] = pFrame->numAudioChannels > 1 ? pIn[1] : pIn[0];
        pAudio->numQueued++;
    }

    pAudio->bPlaying = 1;

    SmushPlatform_UnlockAudio();
}

static void SmushPlay_StopMovieAudio(void)
{
    if ( !SmushPlay_audio.bStarted )
    {
        return;
    }

    SmushPlatform_LockAudio();
    SmushPlay_audio.bPlaying  = 0;
    SmushPlay_audio.numQueued = 0;
    SmushPlay_audio.readPos   = 0;
    SmushPlatform_UnlockAudio();
}

static void SmushPlay_InitTiming(SmushPlayTiming* pTiming, int fps)
{
    memset(pTiming, 0, sizeof(SmushPlayTiming));
    pTiming->basePeriod = SMUSHPLAY_TICKRATE / fps;
    pTiming->bFrameDue  = 1; // the first frame is due at once
    pTiming->startTime  = SmushPlatform_GetTimeUsec();

    if ( SmushPlay_audio.bStarted )
    {
        SmushPlatform_LockAudio();
        pTiming->lastMixed = SmushPlay_audio.numMixedFrames;
        SmushPlatform_UnlockAudio();
    }
}

// Advances the two clocks of the original player: the frame timer runs on real time; the lag counter runs on real
// time too, but each time the audio output takes data it is set to the audio clock, so the picture follows the sound.
static void SmushPlay_UpdateTiming(SmushPlayTiming* pTiming)
{
    const int64_t now = (int64_t)((SmushPlatform_GetTimeUsec() - pTiming->startTime) * SMUSHPLAY_TICKRATE / 1000000);
    const int64_t elapsed = now - pTiming->lastTicks;
    pTiming->lastTicks = now;

    if ( !pTiming->bFrameDue )
    {
        pTiming->frameElapsed += elapsed;
        if ( pTiming->frameElapsed >= pTiming->framePeriod )
        {
            pTiming->bFrameDue = 1;
        }
    }

    pTiming->lagElapsed += elapsed;
    pTiming->sinceSync  += elapsed;

    if ( SmushPlay_audio.bStarted )
    {
        SmushPlatform_LockAudio();
        const uint64_t mixed = SmushPlay_audio.numMixedFrames;
        SmushPlatform_UnlockAudio();

        if ( mixed != pTiming->lastMixed )
        {
            const uint64_t scaled = (mixed - pTiming->lastMixed) * SMUSHPLAY_TICKRATE + pTiming->mixRemainder;
            const int64_t audioTicks = (int64_t)(scaled / SMUSHPLATFORM_AUDIORATE);
            pTiming->mixRemainder = scaled % SMUSHPLATFORM_AUDIORATE;
            pTiming->lastMixed    = mixed;

            pTiming->lagElapsed += audioTicks - pTiming->sinceSync;
            pTiming->sinceSync   = 0;
        }
    }

    while ( pTiming->lagElapsed >= pTiming->basePeriod )
    {
        pTiming->lagElapsed -= pTiming->basePeriod;
        if ( pTiming->bLagStarted && pTiming->lag < SMUSHPLAY_MAXLAG )
        {
            pTiming->lag++;
        }
    }
}

// Called when a frame is due: sets the time to the next frame and decides whether pictures get dropped
static int SmushPlay_StartFrame(SmushPlayTiming* pTiming, int flags)
{
    pTiming->bFrameDue = 0;
    const int lag = --pTiming->lag;

    // Shorten the next frame when behind, lengthen it when ahead, within 3/4 to 4/3 of the nominal period
    int64_t period = pTiming->basePeriod - (int64_t)lag * SMUSHPLAY_LAGSTEP;
    const int64_t minPeriod = pTiming->basePeriod * 3 / 4;
    const int64_t maxPeriod = pTiming->basePeriod * 4 / 3;
    if ( period < minPeriod )
    {
        period = minPeriod;
    }

    if ( period > maxPeriod )
    {
        period = maxPeriod;
    }

    pTiming->framePeriod  = period;
    pTiming->frameElapsed = 0;

    if ( lag >= SMUSHPLAY_SKIPLAG )
    {
        if ( !(pTiming->skipState & SMUSHPLAY_SKIPPING) )
        {
            pTiming->skipState = (pTiming->skipState | SMUSHPLAY_SKIPPING) + 1;
        }
    }
    else if ( lag <= 0 && (pTiming->skipState & SMUSHPLAY_SKIPPING) && (pTiming->skipState & 0x7F) <= SMUSHPLAY_MAXSKIPEPISODES )
    {
        pTiming->skipState &= ~SMUSHPLAY_SKIPPING;
    }

    return (pTiming->skipState & SMUSHPLAY_SKIPPING) && !(flags & SMUSHPLAY_FLAG_NOSKIP);
}

static void SmushPlay_WaitForFrame(SmushPlayTiming* pTiming)
{
    for ( ;; )
    {
        SmushPlay_UpdateTiming(pTiming);
        if ( pTiming->bFrameDue )
        {
            return;
        }

        const int64_t remaining = pTiming->framePeriod - pTiming->frameElapsed;
        SmushPlatform_Wait((uint32_t)(remaining * 1000000 / SMUSHPLAY_TICKRATE));
    }
}

static void SmushPlay_InitBitmap(SmushBitmap* pBitmap, const SmushInfo* pInfo)
{
    memset(pBitmap, 0, sizeof(SmushBitmap));
    pBitmap->width     = (uint32_t)pInfo->width;
    pBitmap->height    = (uint32_t)pInfo->bufferHeight;
    pBitmap->pixelSize = 2;
    pBitmap->bpp       = 16;
    pBitmap->pitch     = (size_t)pInfo->width * 2;
    pBitmap->colorMode = STDCOLOR_RGB;

    pBitmap->format.redBPP             = 5;
    pBitmap->format.greenBPP           = 6;
    pBitmap->format.blueBPP            = 5;
    pBitmap->format.redPosShift        = 11;
    pBitmap->format.greenPosShift      = 5;
    pBitmap->format.bluePosShift       = 0;
    pBitmap->format.redPosShiftRight   = 3;
    pBitmap->format.greenPosShiftRight = 2;
    pBitmap->format.bluePosShiftRight  = 3;
}

// Plays the movie once; returns 0 if it can't be opened. *pbAbort is set when pfBlit asks to stop.
static int SmushPlay_PlayOnce(const char* pFilename, int fps, int flags, SmushBlitFunc pfBlit, int* pbAbort)
{
    void* pFile = SmushPlatform_OpenFile(pFilename);
    if ( !pFile )
    {
        return 0;
    }

    SmushDecoder* pDecoder = SmushDecoder_Open(SmushPlatform_ReadFile, pFile);
    if ( !pDecoder )
    {
        SmushPlatform_CloseFile(pFile);
        return 0;
    }

    const SmushInfo* pInfo = SmushDecoder_GetInfo(pDecoder);
    const int bAudio = pInfo->audioRate == SMUSHPLATFORM_AUDIORATE;

    SmushPlayTiming timing;
    SmushPlay_InitTiming(&timing, fps);

    for ( ;; )
    {
        SmushPlay_WaitForFrame(&timing);
        const int bSkip = SmushPlay_StartFrame(&timing, flags);

        SmushFrame frame;
        if ( SmushDecoder_DecodeFrame(pDecoder, bSkip, &frame) <= 0 )
        {
            break;
        }

        if ( bAudio )
        {
            SmushPlay_QueueAudio(&frame);
        }

        if ( frame.pPixels && pfBlit )
        {
            SmushBitmap bitmap;
            SmushPlay_InitBitmap(&bitmap, pInfo);
            bitmap.pPixels = (void*)frame.pPixels;
            *pbAbort = pfBlit(&bitmap, frame.frameNum);
        }

        if ( frame.frameNum < SMUSHPLAY_NUMSTARTFRAMES )
        {
            timing.bLagStarted = 1;
            timing.skipState   = 0;
        }

        // The movie ends right after its last frame is shown
        if ( *pbAbort || frame.frameNum >= pInfo->numFrames - 1 )
        {
            break;
        }
    }

    SmushPlay_StopMovieAudio();
    SmushDecoder_Close(pDecoder);
    SmushPlatform_CloseFile(pFile);
    return 1;
}

int J3DAPI SmushPlay_SysStartup(HWND hwnd, tDirectSound* pDSound)
{
    SmushPlayAudio* pAudio = &SmushPlay_audio;
    if ( SmushPlay_bStarted )
    {
        return 1;
    }

    memset(pAudio, 0, sizeof(SmushPlayAudio));
    pAudio->pRing = (int16_t*)malloc(SMUSHPLAY_AUDIOBUFFERFRAMES * 2 * sizeof(int16_t));
    if ( !pAudio->pRing )
    {
        return 0;
    }

    // Without audio output the movies still play, timed by the real-time clock alone
    pAudio->bStarted = 1;
    if ( !SmushPlatform_Startup(hwnd, pDSound, SmushPlay_MixAudio) )
    {
        pAudio->bStarted = 0;
        free(pAudio->pRing);
        pAudio->pRing = NULL;
    }

    SmushPlay_bStarted = 1;
    return 1;
}

void SmushPlay_SysShutdown(void)
{
    if ( !SmushPlay_bStarted )
    {
        return;
    }

    SmushPlatform_Shutdown();
    SmushPlay_audio.bStarted = 0;
    free(SmushPlay_audio.pRing);
    SmushPlay_audio.pRing = NULL;
    SmushPlay_bStarted = 0;
}

void J3DAPI SmushPlay_SetGlobalVolume(size_t volume)
{
    if ( volume < 128 )
    {
        SmushPlay_globalVolume = (int)volume;
    }
}

int J3DAPI SmushPlay_PlayMovie(const char* pFilename, int fps, int flags, int unused4, int unused5, int width, int height, int a8, int a9, SmushBlitFunc pfBlit, int numLoops, int streamParam1, int streamParam2)
{
    (void)unused4;
    (void)unused5;
    (void)width;
    (void)height;
    (void)a8;
    (void)a9;
    (void)streamParam1;
    (void)streamParam2;

    if ( !pFilename )
    {
        return 0;
    }

    if ( fps <= 0 )
    {
        fps = 15;
    }

    if ( SmushPlay_audio.bStarted )
    {
        SmushPlatform_LockAudio();
        SmushPlay_audio.volume = SmushPlay_globalVolume;
        SmushPlatform_UnlockAudio();
    }

    int bOpened = 1;
    int bAbort  = 0;
    for ( int loop = 0; loop < numLoops && !bAbort; loop++ )
    {
        bOpened = SmushPlay_PlayOnce(pFilename, fps, flags, pfBlit, &bAbort);
    }

    return bOpened;
}
