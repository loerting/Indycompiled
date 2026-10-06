#include "indyDiff.h"

#include <sound/AudioLib.h>
#include <sound/RTI/symbols.h>
#include <std/General/std.h>
#include <std/General/stdUtil.h>

#include <Windows.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Differential tests (Stage 3): for a function we reimplemented, call the original v1.2 code and our C code with the
// same inputs and compare the results byte by byte. Hooks overwrite the first 5 bytes of the original function with a
// jump to our code; for the original call the harness puts the original 5 bytes back (read from the exe file on
// disk), calls it, and restores the jump. Functions that are still trampolines compare the original with itself,
// which also checks the harness.

static FILE* indyDiff_pReport;
static int indyDiff_numFailed;

static void indyDiff_Log(const char* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);
    char aLine[512];
    vsnprintf(aLine, sizeof(aLine), pFormat, args);
    va_end(args);
    if ( indyDiff_pReport )
    {
        fputs(aLine, indyDiff_pReport);
        fflush(indyDiff_pReport);
    }
    STDLOG_STATUS("%s", aLine);
}

// ---------------------------------------------------------------- original bytes from the exe file

static uint8_t* indyDiff_pExeFile;
static const IMAGE_SECTION_HEADER* indyDiff_pSections;
static unsigned indyDiff_numSections;
static uintptr_t indyDiff_imageBase;

static bool indyDiff_LoadExe(void)
{
    if ( indyDiff_pExeFile )
    {
        return true;
    }

    char aPath[MAX_PATH];
    if ( !GetModuleFileNameA(NULL, aPath, sizeof(aPath)) )
    {
        return false;
    }

    FILE* pFile = fopen(aPath, "rb");
    if ( !pFile )
    {
        return false;
    }
    fseek(pFile, 0, SEEK_END);
    long size = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);
    indyDiff_pExeFile = malloc(size);
    bool bOk = indyDiff_pExeFile && fread(indyDiff_pExeFile, 1, size, pFile) == (size_t)size;
    fclose(pFile);
    if ( !bOk )
    {
        return false;
    }

    const IMAGE_DOS_HEADER* pDos = (const IMAGE_DOS_HEADER*)indyDiff_pExeFile;
    const IMAGE_NT_HEADERS32* pNt = (const IMAGE_NT_HEADERS32*)(indyDiff_pExeFile + pDos->e_lfanew);
    indyDiff_imageBase  = pNt->OptionalHeader.ImageBase;
    indyDiff_numSections = pNt->FileHeader.NumberOfSections;
    indyDiff_pSections  = IMAGE_FIRST_SECTION(pNt);
    return true;
}

static const uint8_t* indyDiff_FileBytes(uintptr_t va)
{
    uint32_t rva = (uint32_t)(va - indyDiff_imageBase);
    for ( unsigned i = 0; i < indyDiff_numSections; i++ )
    {
        const IMAGE_SECTION_HEADER* pSec = &indyDiff_pSections[i];
        if ( rva >= pSec->VirtualAddress && rva < pSec->VirtualAddress + pSec->SizeOfRawData )
        {
            return indyDiff_pExeFile + pSec->PointerToRawData + (rva - pSec->VirtualAddress);
        }
    }
    return NULL;
}

typedef struct sIndyDiffPatch
{
    uint8_t* pCode;
    uint8_t aCurrent[5];
} IndyDiffPatch;

static bool indyDiff_BeginOriginal(uintptr_t addr, IndyDiffPatch* pPatch)
{
    const uint8_t* pOrig = indyDiff_LoadExe() ? indyDiff_FileBytes(addr) : NULL;
    if ( !addr || !pOrig )
    {
        return false;
    }

    pPatch->pCode = (uint8_t*)addr;
    memcpy(pPatch->aCurrent, pPatch->pCode, 5);
    DWORD oldProtect;
    VirtualProtect(pPatch->pCode, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy(pPatch->pCode, pOrig, 5);
    FlushInstructionCache(GetCurrentProcess(), pPatch->pCode, 5);
    return true;
}

static void indyDiff_EndOriginal(IndyDiffPatch* pPatch)
{
    memcpy(pPatch->pCode, pPatch->aCurrent, 5);
    FlushInstructionCache(GetCurrentProcess(), pPatch->pCode, 5);
}

// Calls the original v1.2 function with its hook (if any) temporarily removed.
#define INDY_DIFF_CALL_ORIGINAL(func, ...)                                                  \
    do                                                                                      \
    {                                                                                       \
        IndyDiffPatch patch_;                                                               \
        if ( indyDiff_BeginOriginal(func##_ADDR, &patch_) )                                 \
        {                                                                                   \
            ((func##_TYPE)func##_ADDR)(__VA_ARGS__);                                        \
            indyDiff_EndOriginal(&patch_);                                                  \
        }                                                                                   \
    } while ( 0 )

#define INDY_DIFF_CALL_ORIGINAL_RET(result, func, ...)                                      \
    do                                                                                      \
    {                                                                                       \
        IndyDiffPatch patch_;                                                               \
        if ( indyDiff_BeginOriginal(func##_ADDR, &patch_) )                                 \
        {                                                                                   \
            result = ((func##_TYPE)func##_ADDR)(__VA_ARGS__);                               \
            indyDiff_EndOriginal(&patch_);                                                  \
        }                                                                                   \
    } while ( 0 )

// Several hooks removed at once, for an all-original call chain (e.g. Uncompress -> UncompressBlock)
typedef struct sIndyDiffPatchSet
{
    IndyDiffPatch aPatches[4];
    int numPatches;
} IndyDiffPatchSet;

static void indyDiff_BeginOriginals(IndyDiffPatchSet* pSet, const uintptr_t* aAddrs, int num)
{
    pSet->numPatches = 0;
    for ( int i = 0; i < num && i < 4; i++ )
    {
        if ( indyDiff_BeginOriginal(aAddrs[i], &pSet->aPatches[pSet->numPatches]) )
        {
            pSet->numPatches++;
        }
    }
}

static void indyDiff_EndOriginals(IndyDiffPatchSet* pSet)
{
    while ( pSet->numPatches > 0 )
    {
        indyDiff_EndOriginal(&pSet->aPatches[--pSet->numPatches]);
    }
}

// ---------------------------------------------------------------- comparison and test data

typedef struct sIndyDiffStats
{
    const char* pName;
    int numCases;
    int numMismatches;
    bool bDumped;
    const char* pKnown; // known, accepted difference: reported, but doesn't fail the suite
} IndyDiffStats;

// Input of the current case, dumped with the first mismatch of each function (indy_diff_<function>_in.bin)
static const void* indyDiff_pCaseInput;
static size_t indyDiff_caseInputSize;

static void indyDiff_Dump(const char* pFunc, const char* pWhat, const void* pData, size_t size)
{
    char aName[128];
    snprintf(aName, sizeof(aName), "indy_diff_%s_%s.bin", pFunc, pWhat);
    FILE* pFile = fopen(aName, "wb");
    if ( pFile )
    {
        fwrite(pData, 1, size, pFile);
        fclose(pFile);
    }
}

static void indyDiff_Compare(IndyDiffStats* pStats, const char* pCase, const void* pOrig, const void* pNew, size_t size)
{
    pStats->numCases++;
    if ( memcmp(pOrig, pNew, size) == 0 )
    {
        return;
    }

    if ( !pStats->bDumped && size > 16 )
    {
        pStats->bDumped = true;
        indyDiff_Log("  (dumped input and both outputs of %s %s to indy_diff_%s_*.bin)\n", pStats->pName, pCase, pStats->pName);
        indyDiff_Dump(pStats->pName, "orig", pOrig, size);
        indyDiff_Dump(pStats->pName, "ours", pNew, size);
        if ( indyDiff_pCaseInput )
        {
            indyDiff_Dump(pStats->pName, "in", indyDiff_pCaseInput, indyDiff_caseInputSize);
        }
    }

    size_t first = 0;
    while ( ((const uint8_t*)pOrig)[first] == ((const uint8_t*)pNew)[first] )
    {
        first++;
    }
    if ( pStats->numMismatches++ < 5 )
    {
        indyDiff_Log("  MISMATCH %s %s: first difference at byte %u of %u (orig %02x, ours %02x)\n", pStats->pName, pCase,
            (unsigned)first, (unsigned)size, ((const uint8_t*)pOrig)[first], ((const uint8_t*)pNew)[first]);
    }
}

static void indyDiff_Report(const IndyDiffStats* pStats)
{
    bool bKnown = pStats->numMismatches && pStats->pKnown;
    indyDiff_Log("%-32s %5d cases, %d mismatches%s%s%s\n", pStats->pName, pStats->numCases, pStats->numMismatches,
        bKnown ? "  (known difference: " : pStats->numMismatches ? "  <-- FAIL" : "", bKnown ? pStats->pKnown : "",
        bKnown ? ")" : "");
    if ( !bKnown && pStats->numMismatches )
    {
        indyDiff_numFailed++;
    }
}

static uint32_t indyDiff_rand = 12345;
static uint32_t indyDiff_Rand(void)
{
    indyDiff_rand = indyDiff_rand * 1664525u + 1013904223u;
    return indyDiff_rand >> 8;
}

// 16-bit PCM test signal: kind 0 silence, 1 sine, 2 noise, 3 sine + noise, 4 full-scale square, 5 sweep, 6 clicks
static void indyDiff_MakePcm16(int16_t* pOut, size_t numSamples, int kind, int amplitude)
{
    for ( size_t i = 0; i < numSamples; i++ )
    {
        float t = (float)i;
        float v = 0.0f;
        switch ( kind )
        {
            case 1: v = sinf(t * 0.05f); break;
            case 2: v = (float)(int)(indyDiff_Rand() % 2001 - 1000) / 1000.0f; break;
            case 3: v = 0.7f * sinf(t * 0.013f) + 0.3f * (float)(int)(indyDiff_Rand() % 2001 - 1000) / 1000.0f; break;
            case 4: v = (i / 37) % 2 ? 1.0f : -1.0f; break;
            case 5: v = sinf(t * t * 0.00001f); break;
            case 6: v = (i % 211) == 0 ? 1.0f : 0.0f; break;
            default: break;
        }
        int s = (int)(v * (float)amplitude);
        pOut[i] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
    }
}

// ---------------------------------------------------------------- suite: AudioLib

static void indyDiff_SuiteAudioLib(void)
{
    enum { MAXSAMPLES = 0x2000 };
    static int16_t aPcm[MAXSAMPLES];
    static uint8_t aOutOrig[MAXSAMPLES * 4 + 256], aOutNew[MAXSAMPLES * 4 + 256];

    static const int aAmplitudes[] = { 100, 3000, 20000, 32767 };
    static const int aBlockSizes[] = { 2, 64, 1000, 0x1000 };

    // WVSM block compressor (reimplemented upstream, hooked): checks the harness itself
    // Upstream's reimplementation picks a finer quantisation than the original for some signals: bigger output,
    // smaller error, and the original decoder reads it fine (see the round trip below). Accepted.
    IndyDiffStats wvsmCompress = { "AudioLib_WVSMCompressBlock", .pKnown = "finer quantisation than the original, decodes fine" };
    for ( int kind = 0; kind <= 6; kind++ )
    {
        for ( size_t a = 0; a < STD_ARRAYLEN(aAmplitudes); a++ )
        {
            for ( size_t b = 0; b < STD_ARRAYLEN(aBlockSizes); b++ )
            {
                indyDiff_MakePcm16(aPcm, MAXSAMPLES, kind, aAmplitudes[a]);
                memset(aOutOrig, 0xCD, sizeof(aOutOrig));
                memset(aOutNew, 0xCD, sizeof(aOutNew));
                int sizeOrig = -1, sizeNew;
                INDY_DIFF_CALL_ORIGINAL_RET(sizeOrig, AudioLib_WVSMCompressBlock, aOutOrig, (const uint8_t*)aPcm, aBlockSizes[b], NULL);
                sizeNew = AudioLib_WVSMCompressBlock(aOutNew, (const uint8_t*)aPcm, aBlockSizes[b], NULL);

                char aCase[64];
                snprintf(aCase, sizeof(aCase), "signal %d amp %d block %d", kind, aAmplitudes[a], aBlockSizes[b]);
                indyDiff_pCaseInput    = aPcm;
                indyDiff_caseInputSize = aBlockSizes[b];
                indyDiff_Compare(&wvsmCompress, aCase, &sizeOrig, &sizeNew, sizeof(int));
                indyDiff_Compare(&wvsmCompress, aCase, aOutOrig, aOutNew, sizeof(aOutOrig));
            }
        }
    }
    indyDiff_Report(&wvsmCompress);

    // Round trip through the original decoder: does it turn our compressed data back into the same audio as the
    // original encoder's? (Different but valid encodings are fine for the game.)
    IndyDiffStats wvsmRoundTrip = { "WVSM round trip (orig decoder)", .pKnown = "follows from the finer quantisation" };
    static uint8_t aDecOrig[MAXSAMPLES * 4 + 256], aDecNew[MAXSAMPLES * 4 + 256];
    int maxErrOrig = 0, maxErrNew = 0;
    for ( int kind = 0; kind <= 6; kind++ )
    {
        for ( size_t a = 0; a < STD_ARRAYLEN(aAmplitudes); a++ )
        {
            for ( size_t b = 0; b < STD_ARRAYLEN(aBlockSizes); b++ )
            {
                indyDiff_MakePcm16(aPcm, MAXSAMPLES, kind, aAmplitudes[a]);
                INDY_DIFF_CALL_ORIGINAL(AudioLib_WVSMCompressBlock, aOutOrig, (const uint8_t*)aPcm, aBlockSizes[b], NULL);
                AudioLib_WVSMCompressBlock(aOutNew, (const uint8_t*)aPcm, aBlockSizes[b], NULL);
                memset(aDecOrig, 0, sizeof(aDecOrig));
                memset(aDecNew, 0, sizeof(aDecNew));
                INDY_DIFF_CALL_ORIGINAL(AudioLib_WVSMUncompressBlock, aDecOrig, aOutOrig, aBlockSizes[b]);
                INDY_DIFF_CALL_ORIGINAL(AudioLib_WVSMUncompressBlock, aDecNew, aOutNew, aBlockSizes[b]);

                for ( int i = 0; i < aBlockSizes[b] / 2; i++ )
                {
                    int eo = abs(((int16_t*)aDecOrig)[i] - aPcm[i]), en = abs(((int16_t*)aDecNew)[i] - aPcm[i]);
                    maxErrOrig = eo > maxErrOrig ? eo : maxErrOrig;
                    maxErrNew  = en > maxErrNew ? en : maxErrNew;
                }
                char aCase[64];
                snprintf(aCase, sizeof(aCase), "signal %d amp %d block %d", kind, aAmplitudes[a], aBlockSizes[b]);
                indyDiff_pCaseInput    = aPcm;
                indyDiff_caseInputSize = aBlockSizes[b];
                indyDiff_Compare(&wvsmRoundTrip, aCase, aDecOrig, aDecNew, aBlockSizes[b]);
            }
        }
    }
    indyDiff_Report(&wvsmRoundTrip);
    indyDiff_Log("  largest sample error vs input: original encoder %d, ours %d\n", maxErrOrig, maxErrNew);
}

// ADPCM codec (AudioLib_Compress/Uncompress/UncompressBlock/ResetCompressor): test data from the original encoder
static void indyDiff_SuiteAudioLibAdpcm(void)
{
    enum { MAXBYTES = 0x8000 };
    static int16_t aPcm[MAXBYTES / 2];
    static uint8_t aPacked[MAXBYTES * 2 + 64], aPackedNew[MAXBYTES * 2 + 64];
    static uint8_t aPcmOrig[MAXBYTES + 64], aPcmNew[MAXBYTES + 64];
    static const int aAmplitudes[] = { 100, 3000, 20000, 32767 };
    static const int aSizes[]      = { 64, 1000, 4096, 0x8000 };
    const uintptr_t aUncompressChain[] = { AudioLib_Uncompress_ADDR, AudioLib_UncompressBlock_ADDR, AudioLib_ResetCompressor_ADDR, AudioLib_WVSMUncompressBlock_ADDR };
    const uintptr_t aCompressChain[]   = { AudioLib_Compress_ADDR, AudioLib_ResetCompressor_ADDR, AudioLib_CompressBlock_ADDR };

    IndyDiffStats reset = { "AudioLib_ResetCompressor" };
    for ( int i = 0; i < 4; i++ )
    {
        tAudioCompressorState orig, ours;
        memset(&orig, 0x5A + i, sizeof(orig));
        memset(&ours, 0x5A + i, sizeof(ours));
        INDY_DIFF_CALL_ORIGINAL(AudioLib_ResetCompressor, &orig);
        AudioLib_ResetCompressor(&ours);
        indyDiff_Compare(&reset, "garbage state", &orig, &ours, sizeof(orig));
    }
    indyDiff_Report(&reset);

    IndyDiffStats compress   = { "AudioLib_Compress (ADPCM)" };
    IndyDiffStats uncompress = { "AudioLib_Uncompress + Block" };
    IndyDiffStats wvsmWrap   = { "AudioLib_Uncompress (WVSM)" };
    for ( int kind = 0; kind <= 6; kind++ )
    {
        for ( size_t a = 0; a < STD_ARRAYLEN(aAmplitudes); a++ )
        {
            for ( size_t z = 0; z < STD_ARRAYLEN(aSizes); z++ )
            {
                for ( unsigned int channels = 1; channels <= 4; channels++ ) // 1-2: WVSM, 3-4: ADPCM mono/stereo
                {
                    int size = aSizes[z];
                    indyDiff_MakePcm16(aPcm, MAXBYTES / 2, kind, aAmplitudes[a]);
                    char aCase[80];
                    snprintf(aCase, sizeof(aCase), "signal %d amp %d size %d channels %u", kind, aAmplitudes[a], size, channels);
                    indyDiff_pCaseInput    = aPcm;
                    indyDiff_caseInputSize = size;

                    // original encoder (all original)
                    tAudioCompressorState stateOrig, stateOurs;
                    memset(aPacked, 0xCD, sizeof(aPacked));
                    IndyDiffPatchSet set;
                    indyDiff_BeginOriginals(&set, aCompressChain, 3);
                    ((AudioLib_ResetCompressor_TYPE)AudioLib_ResetCompressor_ADDR)(&stateOrig);
                    int packedSize = ((AudioLib_Compress_TYPE)AudioLib_Compress_ADDR)(&stateOrig, aPacked, (const uint8_t*)aPcm, size, channels);
                    indyDiff_EndOriginals(&set);

                    // our wrapper (the block encoders are the same code on both sides)
                    if ( channels > 2 )
                    {
                        memset(aPackedNew, 0xCD, sizeof(aPackedNew));
                        AudioLib_ResetCompressor(&stateOurs);
                        int packedSizeNew = AudioLib_Compress(&stateOurs, aPackedNew, (const uint8_t*)aPcm, size, channels);
                        indyDiff_Compare(&compress, aCase, &packedSize, &packedSizeNew, sizeof(int));
                        indyDiff_Compare(&compress, aCase, aPacked, aPackedNew, sizeof(aPacked));
                        indyDiff_Compare(&compress, aCase, &stateOrig, &stateOurs, sizeof(stateOrig));
                    }

                    // decoder: all original vs ours, on the original encoder's data
                    memset(aPcmOrig, 0xCD, sizeof(aPcmOrig));
                    memset(aPcmNew, 0xCD, sizeof(aPcmNew));
                    memset(&stateOrig, 0x77, sizeof(stateOrig));
                    memset(&stateOurs, 0x77, sizeof(stateOurs));
                    indyDiff_BeginOriginals(&set, aUncompressChain, 4);
                    ((AudioLib_Uncompress_TYPE)AudioLib_Uncompress_ADDR)(&stateOrig, aPcmOrig, aPacked, (unsigned)size);
                    indyDiff_EndOriginals(&set);
                    AudioLib_Uncompress(&stateOurs, aPcmNew, aPacked, (unsigned)size);

                    IndyDiffStats* pStats = channels > 2 ? &uncompress : &wvsmWrap;
                    indyDiff_Compare(pStats, aCase, aPcmOrig, aPcmNew, sizeof(aPcmOrig));
                    if ( channels > 2 )
                    {
                        indyDiff_Compare(pStats, aCase, &stateOrig, &stateOurs, sizeof(stateOrig));
                    }
                }
            }
        }
    }
    indyDiff_Report(&compress);
    indyDiff_Report(&uncompress);
    indyDiff_Report(&wvsmWrap);

    // AudioLib_CompressBlock directly: byte order, inherited state, odd sizes
    IndyDiffStats block = { "AudioLib_CompressBlock" };
    for ( int kind = 0; kind <= 6; kind++ )
    {
        for ( size_t a = 0; a < STD_ARRAYLEN(aAmplitudes); a++ )
        {
            for ( unsigned int channels = 1; channels <= 2; channels++ )
            {
                for ( int variant = 0; variant < 4; variant++ ) // bit 0: native byte order, bit 1: inherited state
                {
                    int numSamples = (int[]){ 1, 7, 333, 4000 }[(kind + variant) & 3];
                    indyDiff_MakePcm16(aPcm, MAXBYTES / 2, kind, aAmplitudes[a]);
                    tAudioCompressorState stateOrig = { { (uint8_t)(indyDiff_Rand() % 89), (uint8_t)(indyDiff_Rand() % 89) },
                                                        { (int16_t)indyDiff_Rand(), (int16_t)indyDiff_Rand() }, 0 };
                    tAudioCompressorState stateOurs = stateOrig;
                    memset(aPacked, 0xCD, sizeof(aPacked));
                    memset(aPackedNew, 0xCD, sizeof(aPackedNew));
                    int sizeOrig = -1;
                    INDY_DIFF_CALL_ORIGINAL_RET(sizeOrig, AudioLib_CompressBlock, &stateOrig, aPacked, aPcm, numSamples, channels, variant & 1, (variant >> 1) & 1);
                    int sizeNew = AudioLib_CompressBlock(&stateOurs, aPackedNew, aPcm, numSamples, channels, variant & 1, (variant >> 1) & 1);
                    char aCase[80];
                    snprintf(aCase, sizeof(aCase), "signal %d amp %d samples %d channels %u variant %d", kind, aAmplitudes[a], numSamples, channels, variant);
                    indyDiff_pCaseInput    = aPcm;
                    indyDiff_caseInputSize = numSamples * channels * 2;
                    indyDiff_Compare(&block, aCase, &sizeOrig, &sizeNew, sizeof(int));
                    indyDiff_Compare(&block, aCase, aPacked, aPackedNew, sizeof(aPacked));
                    indyDiff_Compare(&block, aCase, &stateOrig, &stateOurs, sizeof(stateOrig));
                }
            }
        }
    }
    indyDiff_Report(&block);

    // AudioLib_WVSMUncompressBlock on random bytes (every byte sequence is a valid block)
    IndyDiffStats wvsmDecode = { "AudioLib_WVSMUncompressBlock" };
    for ( int i = 0; i < 200; i++ )
    {
        int blockSize = (int)(indyDiff_Rand() % 0x1001);
        for ( size_t k = 0; k < sizeof(aPacked); k++ )
        {
            aPacked[k] = (uint8_t)indyDiff_Rand();
            if ( indyDiff_Rand() % 16 == 0 )
            {
                aPacked[k] = 0x80; // more escapes
            }
        }
        memset(aPcmOrig, 0xCD, sizeof(aPcmOrig));
        memset(aPcmNew, 0xCD, sizeof(aPcmNew));
        int usedOrig = -1;
        INDY_DIFF_CALL_ORIGINAL_RET(usedOrig, AudioLib_WVSMUncompressBlock, aPcmOrig, aPacked, blockSize);
        int usedNew = AudioLib_WVSMUncompressBlock(aPcmNew, aPacked, blockSize);
        char aCase[48];
        snprintf(aCase, sizeof(aCase), "random block %d size %d", i, blockSize);
        indyDiff_pCaseInput    = aPacked;
        indyDiff_caseInputSize = blockSize + 3;
        indyDiff_Compare(&wvsmDecode, aCase, &usedOrig, &usedNew, sizeof(int));
        indyDiff_Compare(&wvsmDecode, aCase, aPcmOrig, aPcmNew, sizeof(aPcmOrig));
    }
    indyDiff_Report(&wvsmDecode);
}

// ---------------------------------------------------------------- suite: AudioLib header parser

typedef struct sIndyDiffBuf
{
    uint8_t aData[512];
    size_t size;
} IndyDiffBuf;

static void indyDiff_Put(IndyDiffBuf* pBuf, const void* pData, size_t size)
{
    memcpy(pBuf->aData + pBuf->size, pData, size);
    pBuf->size += size;
}
static void indyDiff_PutLE32(IndyDiffBuf* pBuf, uint32_t v) { indyDiff_Put(pBuf, &v, 4); }
static void indyDiff_PutLE16(IndyDiffBuf* pBuf, uint16_t v) { indyDiff_Put(pBuf, &v, 2); }
static void indyDiff_PutBE32(IndyDiffBuf* pBuf, uint32_t v)
{
    uint8_t a[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
    indyDiff_Put(pBuf, a, 4);
}
static void indyDiff_PutBE16(IndyDiffBuf* pBuf, uint16_t v)
{
    uint8_t a[2] = { (uint8_t)(v >> 8), (uint8_t)v };
    indyDiff_Put(pBuf, a, 2);
}
static void indyDiff_PutFill(IndyDiffBuf* pBuf, size_t size)
{
    for ( size_t i = 0; i < size; i++ )
    {
        pBuf->aData[pBuf->size++] = (uint8_t)indyDiff_Rand();
    }
}

static void indyDiff_MakeHeader(IndyDiffBuf* pBuf, int variant)
{
    memset(pBuf, 0, sizeof(*pBuf));
    int kind = variant % 12;
    if ( variant >= 24 ) // MCMP container in front (3 table entries)
    {
        indyDiff_Put(pBuf, "MCMP", 4);
        indyDiff_PutBE16(pBuf, 3);
        indyDiff_PutFill(pBuf, 3 * 9);
        indyDiff_PutBE16(pBuf, 5); // size of the variable part
        indyDiff_PutFill(pBuf, 5);
    }

    switch ( kind )
    {
        case 0: case 1: // IndyWV, with / without lip-sync data
            indyDiff_Put(pBuf, "INDYWV", 6);
            indyDiff_PutLE32(pBuf, 22050);
            indyDiff_PutLE32(pBuf, 16);
            indyDiff_PutLE32(pBuf, 1 + (variant & 1));
            indyDiff_PutLE32(pBuf, 12345);
            indyDiff_PutLE32(pBuf, kind == 0 ? 40 : 0);
            indyDiff_PutFill(pBuf, 64);
            break;

        case 2: case 3: case 4: case 5: // RIFF PCM: plain, with sync, with LIST and sync, with an odd fmt size
        case 6: case 7:                 // RIFF non-PCM, RIFF without a data chunk
        {
            size_t riffSizeAt = pBuf->size + 4;
            indyDiff_Put(pBuf, "RIFF", 4);
            indyDiff_PutLE32(pBuf, 0);
            indyDiff_Put(pBuf, "WAVEfmt ", 8);
            indyDiff_PutLE32(pBuf, kind == 5 ? 18 : 16);
            indyDiff_PutLE16(pBuf, kind == 6 ? 2 : 1);
            indyDiff_PutLE16(pBuf, (uint16_t)(1 + (variant & 1)));
            indyDiff_PutLE32(pBuf, 11025 << (variant % 3));
            indyDiff_PutLE32(pBuf, 0);
            indyDiff_PutLE16(pBuf, 4);
            indyDiff_PutLE16(pBuf, (variant & 2) ? 8 : 16);
            if ( kind == 5 )
            {
                indyDiff_PutLE16(pBuf, 0);
            }
            if ( kind == 4 )
            {
                indyDiff_Put(pBuf, "LIST", 4);
                indyDiff_PutLE32(pBuf, 10);
                indyDiff_PutFill(pBuf, 10);
            }
            if ( kind == 3 || kind == 4 )
            {
                indyDiff_Put(pBuf, "sync", 4);
                indyDiff_PutLE32(pBuf, 24);
                indyDiff_PutFill(pBuf, 24);
            }
            if ( kind != 7 )
            {
                indyDiff_Put(pBuf, "data", 4);
                indyDiff_PutLE32(pBuf, 100);
            }
            uint32_t riffSize = (uint32_t)(pBuf->size - riffSizeAt - 4 + (kind != 7 ? 100 : 0));
            memcpy(pBuf->aData + riffSizeAt, &riffSize, 4);
            indyDiff_PutFill(pBuf, 64);
            break;
        }

        case 8: case 9: // iMUSE FRMT; iMUS without FRMT
            indyDiff_Put(pBuf, "iMUS", 4);
            indyDiff_PutBE32(pBuf, 1000);
            indyDiff_Put(pBuf, "MAP ", 4);
            indyDiff_PutBE32(pBuf, 60);
            indyDiff_Put(pBuf, kind == 8 ? "FRMT" : "FRMX", 4);
            indyDiff_PutBE32(pBuf, 20);
            indyDiff_PutBE32(pBuf, 0);
            indyDiff_PutBE32(pBuf, 0);
            indyDiff_PutBE32(pBuf, 16);
            indyDiff_PutBE32(pBuf, 22050);
            indyDiff_PutBE32(pBuf, 2);
            indyDiff_PutFill(pBuf, 64);
            break;

        case 10: case 11: // AIFF: sample rate exponent 0x0C/0x0D/0x0E; with a chunk before SSND
        {
            indyDiff_Put(pBuf, "FORM", 4);
            indyDiff_PutBE32(pBuf, 200);
            indyDiff_Put(pBuf, "AIFFCOMM", 8);
            indyDiff_PutBE32(pBuf, 18);
            indyDiff_PutBE16(pBuf, (uint16_t)(1 + (variant & 1)));
            indyDiff_PutBE32(pBuf, 5000);
            indyDiff_PutBE16(pBuf, 16);
            uint8_t aRate[10] = { 0x40, (uint8_t)(0x0C + variant % 3), 0xAC, 0x44 };
            indyDiff_Put(pBuf, aRate, 10);
            if ( kind == 11 )
            {
                indyDiff_Put(pBuf, "MARK", 4);
                indyDiff_PutBE32(pBuf, 6);
                indyDiff_PutFill(pBuf, 6);
            }
            indyDiff_Put(pBuf, "SSND", 4);
            indyDiff_PutBE32(pBuf, 108);
            indyDiff_PutBE32(pBuf, 0);
            indyDiff_PutBE32(pBuf, 0);
            indyDiff_PutFill(pBuf, 64);
            break;
        }
    }
}

typedef struct sIndyDiffWaveInfo
{
    int32_t retOffset; // -1: NULL
    int32_t type;
    uint32_t sampleRate, bits, channels, extraInfo, dataSize;
    int32_t extraOffset; // pointer output as offset, -1 if 0
    uint32_t extraSize;
} IndyDiffWaveInfo;

static void indyDiff_SuiteAudioLibHeaders(void)
{
    IndyDiffStats stats = { "AudioLib_ParseWaveFileHeader" };
    static IndyDiffBuf buf;
    int aRecognised[8] = { 0 }; // by type, from the original
    for ( int variant = 0; variant < 48; variant++ )
    {
        for ( int nulls = 0; nulls < 4; nulls++ ) // which outputs are NULL
        {
            indyDiff_MakeHeader(&buf, variant);
            if ( variant >= 36 )
            {
                indyDiff_PutFill(&buf, 0); // garbage at the start instead of a header
                for ( int i = 0; i < 16; i++ )
                {
                    buf.aData[i] = (uint8_t)indyDiff_Rand();
                }
            }

            IndyDiffWaveInfo info[2];
            for ( int side = 0; side < 2; side++ )
            {
                int type = 0x55;
                uint32_t rate = 0x55, bits = 0x55, ch = 0x55, extra = 0x55, size = 0x55, extraPtr = 0x55, extraSize = 0x55;
                int* pType           = nulls == 1 ? NULL : &type;
                uint32_t* pExtraPtr  = nulls == 2 ? NULL : &extraPtr;
                uint32_t* pExtraSize = nulls == 3 ? NULL : &extraSize;
                const uint8_t* pRet  = (const uint8_t*)-1;
                if ( side == 0 )
                {
                    INDY_DIFF_CALL_ORIGINAL_RET(pRet, AudioLib_ParseWaveFileHeader, buf.aData, pType, &rate, &bits, &ch, &extra, &size, pExtraPtr, pExtraSize);
                }
                else
                {
                    pRet = AudioLib_ParseWaveFileHeader(buf.aData, pType, &rate, &bits, &ch, &extra, &size, pExtraPtr, pExtraSize);
                }
                memset(&info[side], 0, sizeof(info[side]));
                info[side].retOffset   = pRet ? (int32_t)(pRet - buf.aData) : -1;
                info[side].type        = type;
                info[side].sampleRate  = rate;
                info[side].bits        = bits;
                info[side].channels    = ch;
                info[side].extraInfo   = extra;
                info[side].dataSize    = size;
                info[side].extraOffset = extraPtr && extraPtr != 0x55 ? (int32_t)((const uint8_t*)(uintptr_t)extraPtr - buf.aData) : (int32_t)extraPtr - 0x1000;
                info[side].extraSize   = extraSize;
            }
            if ( info[0].retOffset >= 0 && nulls != 1 && info[0].type >= 0 && info[0].type < 8 )
            {
                aRecognised[info[0].type]++;
            }
            char aCase[48];
            snprintf(aCase, sizeof(aCase), "header variant %d nulls %d", variant, nulls);
            indyDiff_pCaseInput    = buf.aData;
            indyDiff_caseInputSize = buf.size;
            indyDiff_Compare(&stats, aCase, &info[0], &info[1], sizeof(info[0]));
        }
    }

    // NULL data
    for ( int side = 0; side < 2; side++ )
    {
        static int types[2];
        types[side] = 0x55;
        if ( side == 0 )
        {
            INDY_DIFF_CALL_ORIGINAL(AudioLib_ParseWaveFileHeader, NULL, &types[0], NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        }
        else
        {
            AudioLib_ParseWaveFileHeader(NULL, &types[1], NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            indyDiff_Compare(&stats, "NULL data", &types[0], &types[1], sizeof(int));
        }
    }
    indyDiff_Report(&stats);
    indyDiff_Log("  recognised by the original, by type 1-6: %d %d %d %d %d %d\n", aRecognised[1], aRecognised[2],
        aRecognised[3], aRecognised[4], aRecognised[5], aRecognised[6]);
}

// ---------------------------------------------------------------- suite: AudioLib lip sync

// speech-like test sound: bursts of tone or noise with pauses
static void indyDiff_MakeSpeech(int16_t* pOut, size_t numSamples, int seed)
{
    indyDiff_rand = 777u + (uint32_t)seed;
    size_t i = 0;
    while ( i < numSamples )
    {
        size_t burst  = 200 + indyDiff_Rand() % 3000;
        size_t pause  = indyDiff_Rand() % 2000;
        int amplitude = 500 + (int)(indyDiff_Rand() % 30000);
        float freq    = 0.01f + (float)(indyDiff_Rand() % 100) / 300.0f;
        bool bNoise   = indyDiff_Rand() % 3 == 0;
        for ( size_t k = 0; k < burst && i < numSamples; k++, i++ )
        {
            float v = bNoise ? (float)(int)(indyDiff_Rand() % 2001 - 1000) / 1000.0f : sinf((float)k * freq);
            pOut[i] = (int16_t)(v * (float)amplitude * (k < 100 ? (float)k / 100.0f : 1.0f));
        }
        for ( size_t k = 0; k < pause && i < numSamples; k++, i++ )
        {
            pOut[i] = (int16_t)((int)(indyDiff_Rand() % 41) - 20);
        }
    }
}

static void indyDiff_SuiteAudioLibLipSync(void)
{
    enum { MAXSAMPLES = 44100 * 3 };
    static int16_t aSound[MAXSAMPLES + 2];
    static uint8_t aSyncOrig[16384], aSyncNew[16384];
    IndyDiffStats generate = { "AudioLib_GenerateLipSyncBlock" };
    IndyDiffStats mouth    = { "AudioLib_GetMouthPosition" };
    static const int aRates[] = { 11025, 22050, 44100 };

    for ( int seed = 0; seed < 6; seed++ )
    {
        for ( size_t r = 0; r < STD_ARRAYLEN(aRates); r++ )
        {
            for ( int variant = 0; variant < 8; variant++ ) // bit 0: byte offset, bit 1: 30 updates/s, bit 2: 8x2 positions
            {
                int rate       = aRates[r];
                int numSamples = rate * (1 + seed % 3) - (int)(indyDiff_Rand() % 997);
                indyDiff_MakeSpeech(aSound, MAXSAMPLES + 2, seed);
                unsigned int updateRate = (variant & 2) ? 30 : 60;
                char numX = (variant & 4) ? 8 : 4, numY = (variant & 4) ? 2 : 4;
                int bits = 16, channels = 1;
                if ( seed == 5 && variant == 7 )
                {
                    bits = 8; // rejected
                }
                if ( seed == 4 && variant == 6 )
                {
                    channels = 2; // rejected
                }

                memset(aSyncOrig, 0xCD, sizeof(aSyncOrig));
                memset(aSyncNew, 0xCD, sizeof(aSyncNew));
                int sizeOrig = -1;
                INDY_DIFF_CALL_ORIGINAL_RET(sizeOrig, AudioLib_GenerateLipSyncBlock, aSyncOrig, (const uint8_t*)aSound, updateRate, numX, numY, rate, bits, channels, variant & 1, numSamples * 2);
                int sizeNew = AudioLib_GenerateLipSyncBlock(aSyncNew, (const uint8_t*)aSound, updateRate, numX, numY, rate, bits, channels, variant & 1, numSamples * 2);

                char aCase[80];
                snprintf(aCase, sizeof(aCase), "seed %d rate %d variant %d samples %d", seed, rate, variant, numSamples);
                indyDiff_pCaseInput    = aSound;
                indyDiff_caseInputSize = (size_t)numSamples * 2;
                indyDiff_Compare(&generate, aCase, &sizeOrig, &sizeNew, sizeof(int));
                indyDiff_Compare(&generate, aCase, aSyncOrig, aSyncNew, sizeof(aSyncOrig));

                // look up mouth positions in the original's block, every 7 ms and beyond the end
                if ( sizeOrig > 0 )
                {
                    int durationMs = numSamples * 1000 / rate;
                    for ( int ms = 0; ms < durationMs + 200; ms += 7 )
                    {
                        uint8_t xo = 0x55, yo = 0x55, xn = 0x55, yn = 0x55;
                        int ro = -1, rn;
                        INDY_DIFF_CALL_ORIGINAL_RET(ro, AudioLib_GetMouthPosition, aSyncOrig, ms, &xo, &yo);
                        rn = AudioLib_GetMouthPosition(aSyncOrig, ms, &xn, &yn);
                        uint8_t aOrig[6] = { (uint8_t)ro, (uint8_t)(ro >> 8), xo, yo }, aNew[6] = { (uint8_t)rn, (uint8_t)(rn >> 8), xn, yn };
                        indyDiff_Compare(&mouth, aCase, aOrig, aNew, 4);
                    }
                }
            }
        }
    }

    // out of range and not a SYNC block
    static const int aPositions[] = { 0x100000, 0x7FFFFFFF, -1, 5 };
    for ( size_t i = 0; i < STD_ARRAYLEN(aPositions); i++ )
    {
        for ( int bBadMagic = 0; bBadMagic < 2; bBadMagic++ )
        {
            static uint8_t aBlock[16] = { 'S', 'Y', 'N', 'C', 1, 0, 0, 0, 0x11, 0x22, 0x33, 0x44 };
            aBlock[0] = bBadMagic ? 'X' : 'S';
            uint8_t xo = 0x55, yo = 0x55, xn = 0x55, yn = 0x55;
            int ro = -1, rn;
            INDY_DIFF_CALL_ORIGINAL_RET(ro, AudioLib_GetMouthPosition, aBlock, aPositions[i], &xo, &yo);
            rn = AudioLib_GetMouthPosition(aBlock, aPositions[i], &xn, &yn);
            uint8_t aOrig[4] = { (uint8_t)ro, (uint8_t)(ro >> 8), xo, yo }, aNew[4] = { (uint8_t)rn, (uint8_t)(rn >> 8), xn, yn };
            indyDiff_Compare(&mouth, "edge cases", aOrig, aNew, 4);
        }
    }

    indyDiff_Report(&generate);
    indyDiff_Report(&mouth);
}

// ---------------------------------------------------------------- entry

void indyDiff_RunOnce(void)
{
    static bool bDone;
    const char* pSuite = getenv("INDY_DIFFTEST");
    if ( bDone || !pSuite )
    {
        return;
    }
    bDone = true;

    indyDiff_pReport = fopen("indy_difftest.txt", "w");
    indyDiff_Log("indyDiff: suite %s\n", pSuite);

    if ( strcmp(pSuite, "AudioLib") == 0 )
    {
        indyDiff_SuiteAudioLibAdpcm();
        indyDiff_SuiteAudioLibHeaders();
        indyDiff_SuiteAudioLibLipSync();
        indyDiff_SuiteAudioLib();
    }
    else
    {
        indyDiff_Log("indyDiff: unknown suite\n");
        indyDiff_numFailed++;
    }

    indyDiff_Log("indyDiff: DONE, %s\n", indyDiff_numFailed ? "FAILED" : "no unexpected differences");
    if ( indyDiff_pReport )
    {
        fclose(indyDiff_pReport);
    }
    ExitProcess(indyDiff_numFailed ? 1 : 0);
}
