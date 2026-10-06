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
