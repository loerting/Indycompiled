#include "indyDebug.h"

#include <Windows.h>
#include <tlhelp32.h>

#include <stdio.h>
#include <stdlib.h>

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
