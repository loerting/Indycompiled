// Native test for Libs/smush/smushDecoder.c: decodes a SMUSH movie and compares every frame and the audio with
// FFmpeg's decoders, which serve as an independent reference. Built and run by test_smush.sh.
//
// Usage: smush_test <file.snm> [--dump-frame N out.rgb565]
// Built normally, the exit code is 1 only on decoding errors: FFmpeg's glyph tables differ from the original's.
// Built with -DSMUSH_TEST_FFMPEG (the decoder then uses FFmpeg's glyph rule), the exit code is 1 if a frame differs
// before the first motion copy that reads outside the picture (undefined in the original), or if the audio differs.

#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smushDecoder.h"

#ifdef SMUSH_TEST_FFMPEG
extern int SmushDecoder_numOutsideCopies;
#endif

static size_t ReadFile(void* pUser, void* pBuffer, size_t size)
{
    return fread(pBuffer, 1, size, (FILE*)pUser);
}

static FILE* OpenFfmpeg(const char* pPath, const char* pArgs)
{
    char aCommand[4096];
    snprintf(aCommand, sizeof(aCommand), "ffmpeg -nostdin -v error -i '%s' %s -", pPath, pArgs);
    return popen(aCommand, "r");
}

static int16_t* ReadAllPcm(const char* pPath, size_t* pNumValues)
{
    FILE* pPipe = OpenFfmpeg(pPath, "-f s16le -ac 2 -ar 22050");
    if ( !pPipe )
    {
        return NULL;
    }

    size_t capacity = 1 << 20, count = 0;
    int16_t* pData = malloc(capacity * sizeof(int16_t));
    for ( ;; )
    {
        if ( count == capacity )
        {
            capacity *= 2;
            pData = realloc(pData, capacity * sizeof(int16_t));
        }

        size_t got = fread(pData + count, sizeof(int16_t), capacity - count, pPipe);
        if ( !got )
        {
            break;
        }

        count += got;
    }

    pclose(pPipe);
    *pNumValues = count;
    return pData;
}

int main(int argc, char** argv)
{
    if ( argc < 2 )
    {
        fprintf(stderr, "usage: %s file.snm [--dump-frame N out.rgb565]\n", argv[0]);
        return 2;
    }

    const char* pPath = argv[1];
    int dumpFrame = -1;
    const char* pDumpPath = NULL;
    if ( argc >= 5 && !strcmp(argv[2], "--dump-frame") )
    {
        dumpFrame = atoi(argv[3]);
        pDumpPath = argv[4];
    }

    FILE* pFile = fopen(pPath, "rb");
    if ( !pFile )
    {
        fprintf(stderr, "%s: cannot open\n", pPath);
        return 2;
    }

    SmushDecoder* pDecoder = SmushDecoder_Open(ReadFile, pFile);
    if ( !pDecoder )
    {
        fprintf(stderr, "%s: not a SANM movie\n", pPath);
        return 2;
    }

    const SmushInfo* pInfo = SmushDecoder_GetInfo(pDecoder);
    const int width = pInfo->width, height = pInfo->height;
    printf("%s: %dx%d, %d frames, %d us/frame, audio %d Hz\n", pPath, width, height, pInfo->numFrames, pInfo->frameDuration, pInfo->audioRate);

    FILE* pVideo = OpenFfmpeg(pPath, "-f rawvideo -pix_fmt rgb565le -fps_mode passthrough");
    if ( !pVideo )
    {
        fprintf(stderr, "cannot run ffmpeg\n");
        return 2;
    }

    const size_t frameBytes = (size_t)width * height * 2;
    uint8_t* pRef = malloc(frameBytes);
    uint8_t* pOurs = malloc(frameBytes);

    size_t pcmCapacity = 1 << 20, pcmCount = 0;
    int16_t* pPcm = malloc(pcmCapacity * sizeof(int16_t));

    int numFrames = 0, numPictures = 0, numIdentical = 0, numDiffering = 0, firstDiff = -1, firstOutside = -1;
    size_t maxDiffPixels = 0;
    SmushFrame frame;
    int result;
    while ( (result = SmushDecoder_DecodeFrame(pDecoder, 0, &frame)) > 0 )
    {
        numFrames++;
#ifdef SMUSH_TEST_FFMPEG
        if ( SmushDecoder_numOutsideCopies && firstOutside < 0 )
        {
            firstOutside = frame.frameNum;
        }
#endif

        if ( frame.pAudio )
        {
            const size_t numValues = frame.numAudioSamples * 2;
            while ( pcmCount + numValues > pcmCapacity )
            {
                pcmCapacity *= 2;
                pPcm = realloc(pPcm, pcmCapacity * sizeof(int16_t));
            }

            for ( size_t i = 0; i < frame.numAudioSamples; i++ )
            {
                // Mono would be duplicated, as FFmpeg does with -ac 2
                const int16_t* pIn = frame.pAudio + i * frame.numAudioChannels;
                pPcm[pcmCount++] = pIn[0];
                pPcm[pcmCount++] = frame.numAudioChannels > 1 ? pIn[1] : pIn[0];
            }
        }

        if ( !frame.pPixels )
        {
            continue;
        }

        numPictures++;
        for ( int y = 0; y < height; y++ )
        {
            for ( int x = 0; x < width; x++ )
            {
                const uint16_t pixel = frame.pPixels[(size_t)y * width + x];
                pOurs[((size_t)y * width + x) * 2]     = (uint8_t)pixel;
                pOurs[((size_t)y * width + x) * 2 + 1] = (uint8_t)(pixel >> 8);
            }
        }

        if ( frame.frameNum == dumpFrame && pDumpPath )
        {
            FILE* pDump = fopen(pDumpPath, "wb");
            fwrite(pOurs, 1, frameBytes, pDump);
            fclose(pDump);
        }

        if ( fread(pRef, 1, frameBytes, pVideo) != frameBytes )
        {
            printf("  frame %d: FFmpeg delivered no more frames\n", frame.frameNum);
            numDiffering++;
            continue;
        }

        if ( !memcmp(pRef, pOurs, frameBytes) )
        {
            numIdentical++;
            continue;
        }

        size_t numDiffPixels = 0;
        int firstX = -1, firstY = -1;
        for ( size_t i = 0; i < frameBytes / 2; i++ )
        {
            if ( pRef[2 * i] != pOurs[2 * i] || pRef[2 * i + 1] != pOurs[2 * i + 1] )
            {
                if ( !numDiffPixels )
                {
                    firstX = (int)(i % width);
                    firstY = (int)(i / width);
                }

                numDiffPixels++;
            }
        }

        if ( numDiffering < 10 )
        {
            printf("  frame %d differs: %zu pixels, first at (%d,%d)\n", frame.frameNum, numDiffPixels, firstX, firstY);
        }

        if ( firstDiff < 0 )
        {
            firstDiff = frame.frameNum;
        }

        if ( numDiffPixels > maxDiffPixels )
        {
            maxDiffPixels = numDiffPixels;
        }

        numDiffering++;
    }

    size_t extra = 0;
    while ( fread(pRef, 1, frameBytes, pVideo) == frameBytes )
    {
        extra++;
    }

    pclose(pVideo);
    SmushDecoder_Close(pDecoder);
    fclose(pFile);

    if ( result < 0 )
    {
        printf("  decode error after frame %d\n", numFrames);
    }

    printf("  video: %d frames, %d pictures: %d identical, %d differing", numFrames, numPictures, numIdentical, numDiffering);
    if ( numDiffering )
    {
        printf(" (first %d, max %zu pixels)", firstDiff, maxDiffPixels);
    }

    printf("; FFmpeg frames left over: %zu\n", extra);
    if ( firstOutside >= 0 )
    {
        printf("  first motion copy from outside the picture: frame %d\n", firstOutside);
    }

    size_t refCount = 0;
    int16_t* pRefPcm = ReadAllPcm(pPath, &refCount);
    if ( !pRefPcm )
    {
        printf("  audio: cannot run ffmpeg\n");
        return 2;
    }

    const size_t common = pcmCount < refCount ? pcmCount : refCount;
    int maxDiff = 0;
    size_t numDiffSamples = 0, firstDiffSample = (size_t)-1;
    for ( size_t i = 0; i < common; i++ )
    {
        int diff = abs((int)pPcm[i] - (int)pRefPcm[i]);
        if ( diff )
        {
            if ( firstDiffSample == (size_t)-1 )
            {
                firstDiffSample = i / 2;
            }

            numDiffSamples++;
            if ( diff > maxDiff )
            {
                maxDiff = diff;
            }
        }
    }

    printf("  audio: ours %zu, FFmpeg %zu samples per channel; %zu values differ, max difference %d", pcmCount / 2, refCount / 2, numDiffSamples, maxDiff);
    if ( numDiffSamples )
    {
        printf(" (first at sample %zu)", firstDiffSample);
    }

    printf("\n");

    free(pRefPcm);
    free(pPcm);
    free(pRef);
    free(pOurs);
#ifdef SMUSH_TEST_FFMPEG
    const int bExplained = firstDiff < 0 || (firstOutside >= 0 && firstDiff >= firstOutside);
    return (result < 0 || extra || !bExplained || numDiffSamples || pcmCount != refCount) ? 1 : 0;
#else
    return result < 0 ? 1 : 0;
#endif
}
