#include "indyDebug.h"

#include <Windows.h>
#include <tlhelp32.h>

#include <stdio.h>
#include <stdlib.h>

#include <sith/World/sithWorld.h>
#include <std/General/stdUtil.h>
#include <std/types.h>

#include <stddef.h>
#include <string.h>

// Thread sampler (debug aid): with INDY_SAMPLE_THREADS=<seconds> set, a background thread periodically suspends every
// other thread of the game, logs its instruction pointer and the return-address candidates found on its stack (exe and
// Jones3D.dll ranges), and appends that to indy_samples.txt in the working directory. Meant for hangs and endless loops,
// where an external debugger can't attach (Wine WoW64). Resolve exe addresses with Scripts/indy/rti_v12.csv and DLL
// addresses with addr2line (image base 0x10000000).

static uintptr_t indyDebug_dllBase, indyDebug_dllEnd;

static void indyDebug_ModuleRange(HMODULE hModule, uintptr_t* pBase, uintptr_t* pEnd)
{
    const IMAGE_DOS_HEADER* pDos = (const IMAGE_DOS_HEADER*)hModule;
    const IMAGE_NT_HEADERS* pNt  = (const IMAGE_NT_HEADERS*)((const uint8_t*)hModule + pDos->e_lfanew);
    *pBase = (uintptr_t)hModule;
    *pEnd  = *pBase + pNt->OptionalHeader.SizeOfImage;
}

static const char* indyDebug_Where(uintptr_t addr, char* pBuf, size_t size)
{
    if ( addr >= 0x00401000 && addr < 0x00505000 )
    {
        snprintf(pBuf, size, "exe:%08lx", (unsigned long)addr);
    }
    else if ( addr >= indyDebug_dllBase && addr < indyDebug_dllEnd )
    {
        // print as the address at the DLL's preferred base, ready for addr2line
        snprintf(pBuf, size, "dll:%08lx", (unsigned long)(addr - indyDebug_dllBase + 0x10000000));
    }
    else
    {
        return NULL;
    }
    return pBuf;
}

static DWORD WINAPI indyDebug_SamplerThread(LPVOID pParam)
{
    DWORD intervalMs = (DWORD)(uintptr_t)pParam;
    DWORD pid        = GetCurrentProcessId();
    DWORD self       = GetCurrentThreadId();

    for ( unsigned sample = 1;; sample++ )
    {
        Sleep(intervalMs);
        FILE* pFile = fopen("indy_samples.txt", "a");
        if ( !pFile )
        {
            continue;
        }

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        THREADENTRY32 te = { .dwSize = sizeof(te) };
        for ( BOOL ok = Thread32First(hSnap, &te); ok; ok = Thread32Next(hSnap, &te) )
        {
            if ( te.th32OwnerProcessID != pid || te.th32ThreadID == self )
            {
                continue;
            }

            HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, te.th32ThreadID);
            if ( !hThread )
            {
                continue;
            }

            CONTEXT ctx = { .ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER };
            uint32_t aStack[256];
            SIZE_T read = 0;
            if ( SuspendThread(hThread) != (DWORD)-1 )
            {
                if ( GetThreadContext(hThread, &ctx) )
                {
                    ReadProcessMemory(GetCurrentProcess(), (LPCVOID)ctx.Esp, aStack, sizeof(aStack), &read);
                }
                ResumeThread(hThread);
            }
            CloseHandle(hThread);

            char aBuf[32];
            const char* pEip = indyDebug_Where(ctx.Eip, aBuf, sizeof(aBuf));
            fprintf(pFile, "sample %u thread %lu eip %08lx%s%s stack:", sample, te.th32ThreadID, ctx.Eip,
                pEip ? " " : "", pEip ? pEip : "");
            for ( size_t i = 0, shown = 0; i < read / 4 && shown < 24; i++ )
            {
                if ( indyDebug_Where(aStack[i], aBuf, sizeof(aBuf)) )
                {
                    fprintf(pFile, " %s", aBuf);
                    shown++;
                }
            }
            fprintf(pFile, "\n");
        }
        CloseHandle(hSnap);
        fclose(pFile);
    }
    return 0;
}

void indyDebug_Startup(HMODULE hDll)
{
    const char* pInterval = getenv("INDY_SAMPLE_THREADS");
    if ( !pInterval || atoi(pInterval) <= 0 )
    {
        return;
    }

    indyDebug_ModuleRange(hDll, &indyDebug_dllBase, &indyDebug_dllEnd);
    HANDLE hThread = CreateThread(NULL, 0, indyDebug_SamplerThread, (LPVOID)(uintptr_t)(atoi(pInterval) * 1000), 0, NULL);
    if ( hThread )
    {
        CloseHandle(hThread);
    }
}

void indyDebug_FrameCap(void)
{
    static int cap = -1;
    static LARGE_INTEGER freq, next;
    if ( cap == -1 )
    {
        const char* pCap = getenv("INDY_FPS_CAP");
        cap = pCap ? atoi(pCap) : 0;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&next);
    }

    if ( cap <= 0 )
    {
        return;
    }

    LONGLONG period = freq.QuadPart / cap;
    LARGE_INTEGER now;
    for ( ;; )
    {
        QueryPerformanceCounter(&now);
        LONGLONG left = next.QuadPart - now.QuadPart;
        if ( left <= 0 )
        {
            break;
        }
        if ( left > freq.QuadPart / 500 ) // more than 2 ms: sleep, then spin for the rest
        {
            Sleep(1);
        }
    }

    // a slow frame (e.g. loading) restarts the schedule instead of letting later frames catch up
    next.QuadPart = now.QuadPart - next.QuadPart > period ? now.QuadPart + period : next.QuadPart + period;
}

// World snapshots compare runs, but heap addresses differ between runs (levels whose loading overlaps a cutscene or
// streaming don't allocate deterministically). So every word that points into the heap is written as what it points
// to: a world array element (T/O/S/... index + offset), a stdMemory block (size and, a few levels deep, its
// canonical contents), a name (string), or just "P". Other words are written as hex.
#define INDY_DUMP_HEADERMAGIC 0x12345678 // stdMemory block header magic

typedef struct sIndyDumpArray
{
    char aTag[4];
    uintptr_t base;
    size_t elemSize;
    size_t count;
} IndyDumpArray;

static IndyDumpArray indyDebug_aDumpArrays[40];
static size_t indyDebug_numDumpArrays;

static void indyDebug_AddDumpArray(const char* pTag, const void* pBase, size_t elemSize, size_t count)
{
    if ( !pBase || !count || indyDebug_numDumpArrays >= STD_ARRAYLEN(indyDebug_aDumpArrays) )
    {
        return;
    }
    IndyDumpArray* pArray = &indyDebug_aDumpArrays[indyDebug_numDumpArrays++];
    snprintf(pArray->aTag, sizeof(pArray->aTag), "%s", pTag);
    pArray->base     = (uintptr_t)pBase;
    pArray->elemSize = elemSize;
    pArray->count    = count;
}

static void indyDebug_AddWorldArrays(const SithWorld* pWorld, const char* pPrefix)
{
    if ( !pWorld )
    {
        return;
    }
    char aTag[4];
    #define INDY_ADD(tag, arr, num) snprintf(aTag, sizeof(aTag), "%s%s", pPrefix, tag); indyDebug_AddDumpArray(aTag, pWorld->arr, sizeof(*pWorld->arr), pWorld->num)
    INDY_ADD("T", aThingTemplates, sizeThingTemplates);
    INDY_ADD("O", aThings, numThings);
    INDY_ADD("S", aSectors, numSectors);
    INDY_ADD("F", aSurfaces, numSurfaces);
    INDY_ADD("A", aAdjoins, numAdjoins);
    INDY_ADD("V", aVertices, numVertices);
    INDY_ADD("M", aMaterials, sizeMaterials);
    INDY_ADD("D", aModels, sizeModels);
    INDY_ADD("R", aSprites, sizeSprites);
    INDY_ADD("K", aKeyframes, sizeKeyframes);
    INDY_ADD("U", aPuppetClasses, sizePuppetClasses);
    INDY_ADD("N", aSoundClasses, sizeSoundClasses);
    INDY_ADD("X", aCogScripts, sizeCogScripts);
    INDY_ADD("C", aCogs, sizeCogs);
    INDY_ADD("I", aAIClasses, sizeAIClasses);
    INDY_ADD("Q", aParticles, sizeParticles);
    #undef INDY_ADD
}

// true if [p, p+size) is committed, readable process memory outside loaded images (heap or mapped)
static bool indyDebug_IsHeapMemory(uintptr_t p, size_t size)
{
    MEMORY_BASIC_INFORMATION mbi;
    if ( p < 0x10000 || !VirtualQuery((const void*)p, &mbi, sizeof(mbi)) )
    {
        return false;
    }
    if ( mbi.State != MEM_COMMIT || mbi.Type == MEM_IMAGE || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) )
    {
        return false;
    }
    return p + size <= (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
}

static void indyDebug_DumpCanonical(FILE* pFile, const uint8_t* pData, size_t size, int depth);

static void indyDebug_DumpWord(FILE* pFile, uint32_t value, int depth)
{
    if ( !indyDebug_IsHeapMemory(value, 4) )
    {
        fprintf(pFile, "%08x ", value);
        return;
    }

    for ( size_t i = 0; i < indyDebug_numDumpArrays; i++ )
    {
        const IndyDumpArray* pArray = &indyDebug_aDumpArrays[i];
        if ( value >= pArray->base && value < pArray->base + pArray->elemSize * pArray->count )
        {
            size_t offset = value - pArray->base;
            fprintf(pFile, "%s%u+%x ", pArray->aTag, (unsigned)(offset / pArray->elemSize), (unsigned)(offset % pArray->elemSize));
            return;
        }
    }

    // stdMemory block (the pointer is the block's data, right after its header)
    const tMemoryHeap* pHeap = (const tMemoryHeap*)(value - offsetof(tMemoryHeap, pMemory));
    if ( indyDebug_IsHeapMemory((uintptr_t)pHeap, sizeof(tMemoryHeap)) && pHeap->header.id == (uint32_t)(uintptr_t)pHeap
        && pHeap->header.magic == INDY_DUMP_HEADERMAGIC && indyDebug_IsHeapMemory(value, pHeap->header.size) )
    {
        if ( depth > 0 && pHeap->header.size <= 0x10000 )
        {
            fprintf(pFile, "{B%u: ", (unsigned)pHeap->header.size);
            indyDebug_DumpCanonical(pFile, (const uint8_t*)value, pHeap->header.size, depth - 1);
            fputs("} ", pFile);
        }
        else
        {
            fprintf(pFile, "B%u ", (unsigned)pHeap->header.size);
        }
        return;
    }

    // name (resources start with their name)
    const char* pName = (const char*)value;
    size_t len = 0;
    while ( len < 64 && indyDebug_IsHeapMemory(value + len, 1) && pName[len] >= 0x21 && pName[len] < 0x7f )
    {
        len++;
    }
    if ( len >= 2 && len < 64 && pName[len] == '\0' )
    {
        fprintf(pFile, "\"%.*s\" ", (int)len, pName);
        return;
    }

    fputs("P ", pFile);
}

static void indyDebug_DumpCanonical(FILE* pFile, const uint8_t* pData, size_t size, int depth)
{
    size_t i = 0;
    for ( ; i + 4 <= size; i += 4 )
    {
        uint32_t value;
        memcpy(&value, pData + i, 4);
        indyDebug_DumpWord(pFile, value, depth);
    }
    for ( ; i < size; i++ )
    {
        fprintf(pFile, "%02x", pData[i]);
    }
}

void indyDebug_DumpWorld(void)
{
    const char* pPath = getenv("INDY_DUMP_WORLD");
    const SithWorld* pWorld = sithWorld_g_pCurrentWorld;
    if ( !pPath || !pWorld )
    {
        return;
    }

    FILE* pFile = fopen(pPath, "w");
    if ( !pFile )
    {
        return;
    }

    indyDebug_numDumpArrays = 0;
    indyDebug_AddWorldArrays(pWorld, "");
    if ( sithWorld_g_pStaticWorld != pWorld )
    {
        indyDebug_AddWorldArrays(sithWorld_g_pStaticWorld, "s");
    }

    fprintf(pFile, "world %s templates %u things %d\n", pWorld->aName, (unsigned)pWorld->numThingTemplates, pWorld->lastThingIdx + 1);
    for ( size_t i = 0; i < pWorld->numThingTemplates; i++ )
    {
        fprintf(pFile, "T%u ", (unsigned)i);
        indyDebug_DumpCanonical(pFile, (const uint8_t*)&pWorld->aThingTemplates[i], sizeof(SithThing), 3);
        fputc('\n', pFile);
    }
    for ( int i = 0; i <= pWorld->lastThingIdx; i++ )
    {
        fprintf(pFile, "O%d ", i);
        indyDebug_DumpCanonical(pFile, (const uint8_t*)&pWorld->aThings[i], sizeof(SithThing), 3);
        fputc('\n', pFile);
    }
    fclose(pFile);
}
