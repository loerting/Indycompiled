#ifndef SMUSH_SMUSHDECODER_H
#define SMUSH_SMUSHDECODER_H

// Platform-neutral decoder for LucasArts SMUSH movies in the 16-bit SANM variant used by the intro videos
// (Bl16 video frames in RGB565, Wave chunks with VIMA-compressed 16-bit audio).
// Plain C, no engine or OS dependencies: the file is read through a callback.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Reads up to size bytes into pBuffer; returns the number of bytes read (0 at the end of the file or on error).
typedef size_t (*SmushReadFunc)(void* pUser, void* pBuffer, size_t size);

typedef struct sSmushInfo
{
    int width;
    int height;
    int bufferHeight;   // rows of the decoded picture buffer: height rounded up to the 8x8 block grid
    int numFrames;      // frame count from the file header
    int frameDuration;  // microseconds per frame from the file header (0 if not set)
    int audioRate;      // sample rate of the audio track (0 if the file has none)
} SmushInfo;

typedef struct sSmushFrame
{
    int frameNum;               // 0-based index of the frame in the file

    // Picture shown by this frame, RGB565 with pitch = width pixels and SmushInfo.bufferHeight rows.
    // NULL if the frame has no picture or it was skipped. Valid until the next SmushDecoder_DecodeFrame call.
    const uint16_t* pPixels;

    // PCM16 samples of the frame's audio chunks, interleaved by channel. Valid until the next call.
    const int16_t* pAudio;
    size_t numAudioSamples;     // samples per channel
    int numAudioChannels;
} SmushFrame;

typedef struct sSmushDecoder SmushDecoder;

// Reads the file header; returns NULL if the file is not a SANM movie or memory runs out.
SmushDecoder* SmushDecoder_Open(SmushReadFunc pfRead, void* pUser);
void SmushDecoder_Close(SmushDecoder* pDecoder);

const SmushInfo* SmushDecoder_GetInfo(const SmushDecoder* pDecoder);

// Reads and decodes the next frame.
// bSkipDisposable: drop pictures that no later frame depends on (used by the player to catch up when late).
// Returns 1 if a frame was decoded, 0 at the end of the movie, -1 on a read or format error.
int SmushDecoder_DecodeFrame(SmushDecoder* pDecoder, int bSkipDisposable, SmushFrame* pFrame);

#ifdef __cplusplus
}
#endif

#endif // SMUSH_SMUSHDECODER_H
