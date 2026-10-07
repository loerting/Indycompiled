#include "std.h"
#include "stdFileUtil.h"
#include "stdFnames.h"
#include "stdMemory.h"
#include "stdUtil.h"

#include <j3dcore/j3dhook.h>
#include <std/RTI/symbols.h>

#ifndef _WIN32
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef __ANDROID__
#include <std/SDL/stdAndroidSDL.h>
#endif

void stdFileUtil_InstallHooks(void)
{
    J3D_HOOKFUNC(stdFileUtil_NewFind);
    J3D_HOOKFUNC(stdFileUtil_DisposeFind);
    J3D_HOOKFUNC(stdFileUtil_FindNext);
    J3D_HOOKFUNC(stdFileUtil_MkDir);
    J3D_HOOKFUNC(stdFileUtil_FileExists);
}

void stdFileUtil_ResetGlobals(void)
{}

#ifdef _WIN32
time_t FileTimeToUnixTime(const FILETIME* ft)
{
    // Windows FILETIME starts on January 1, 1601
    // Unix time starts on January 1, 1970
    // The difference between the two epochs is 11644473600 seconds
    static const ULONGLONG EPOCH_DIFF = 11644473600ULL;

    ULARGE_INTEGER ull;
    ull.LowPart  = ft->dwLowDateTime;
    ull.HighPart = ft->dwHighDateTime;

    // Convert from 100-nanosecond intervals to seconds and adjust the epoch
    ull.QuadPart /= 10000000ULL; // Convert to seconds
    ull.QuadPart -= EPOCH_DIFF;  // Subtract the epoch difference

    return (time_t)ull.QuadPart;
}
#endif

FindFileData* J3DAPI stdFileUtil_NewFind(const char* path, int mode, const char* pFilter)
{
    // Added: Release-build guard before building a search path.
    STD_GUARD(path, NULL);

    FindFileData* pData = (FindFileData*)STDMALLOC(sizeof(FindFileData));
    if ( !pData )
    {
        return NULL;
    }

    memset(pData, 0, sizeof(FindFileData));

    if ( mode < 0 )
    {
        return pData;
    }

    if ( mode <= 2 )
    {
        stdFnames_MakePath(pData->aSearchFilter, sizeof(pData->aSearchFilter), path, "*.*");
        return pData;
    }

    if ( mode != 3 )
    {
        return pData;
    }

    if ( !pFilter )
    {
        // Added: Treat a NULL mode-3 filter as the same wildcard used by the default modes.
        pFilter = "*";
    }

    if ( *pFilter == '.' )
    {
        pFilter++;
    }

    STD_FORMAT(std_g_genBuffer, "*.%s", pFilter);
    stdFnames_MakePath(pData->aSearchFilter, sizeof(pData->aSearchFilter), path, std_g_genBuffer);
    return pData;
}

#ifdef _WIN32
void J3DAPI stdFileUtil_DisposeFind(FindFileData* ffData)
{
    if ( ffData )
    {
        if ( ffData->nFoundFiles && ffData->handle != INVALID_HANDLE_VALUE )
        {
            FindClose(ffData->handle);
        }
        stdMemory_Free(ffData);
    }
}

int J3DAPI stdFileUtil_FindNext(FindFileData* ffData, tFoundFileInfo* pFileInfo)
{
    WIN32_FIND_DATA findData;

    if ( !ffData )
    {
        return 0;
    }

    int nFoundFiles = ffData->nFoundFiles;
    ffData->nFoundFiles = nFoundFiles + 1;
    if ( nFoundFiles )
    {
        if ( !FindNextFile(ffData->handle, &findData) )
        {
            return 0;
        }
    }
    else
    {
        ffData->handle = FindFirstFile(ffData->aSearchFilter, &findData);
        if ( ffData->handle == INVALID_HANDLE_VALUE )
        {
            return 0;
        }
    }

    STD_STRCPY(pFileInfo->aName, findData.cFileName);

    pFileInfo->lastChanged  = FileTimeToUnixTime(&findData.ftLastWriteTime); // TODO: After all code that uses find file functions is defined change the type of `lastChanged` to time_t
    pFileInfo->bIsDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return 1;
}

#else // native builds: dirent; aSearchFilter is "<dir>\\<pattern>", matched case-insensitively
void J3DAPI stdFileUtil_DisposeFind(FindFileData* ffData)
{
    if ( ffData )
    {
        if ( ffData->handle )
        {
            closedir((DIR*)ffData->handle);
        }
        stdMemory_Free(ffData);
    }
}

int J3DAPI stdFileUtil_FindNext(FindFileData* ffData, tFoundFileInfo* pFileInfo)
{
    if ( !ffData )
    {
        return 0;
    }

    char aDir[J3D_MAX_PATH];
    const char* pPattern = "*";
    STD_STRCPY(aDir, ffData->aSearchFilter);
    char* pSep = strrchr(aDir, '\\');
    if ( !pSep ) pSep = strrchr(aDir, '/');
    if ( pSep )
    {
        *pSep = 0;
        pPattern = ffData->aSearchFilter + (pSep - aDir) + 1;
    }
    else
    {
        pPattern = ffData->aSearchFilter;
        STD_STRCPY(aDir, ".");
    }

    char aResolved[J3D_MAX_PATH];
    J3D_ResolvePath(aDir, aResolved, sizeof(aResolved));
    if ( ffData->nFoundFiles++ == 0 )
    {
        ffData->handle = opendir(aResolved);
    }

    // "*.*" matches every name on Windows, also names without a dot
    bool bAll = streq(pPattern, "*.*") || streq(pPattern, "*");
    struct dirent* pEntry;
    while ( ffData->handle && (pEntry = readdir((DIR*)ffData->handle)) != NULL )
    {
        if ( bAll || fnmatch(pPattern, pEntry->d_name, FNM_CASEFOLD) == 0 )
        {
            char aPath[J3D_MAX_PATH];
            snprintf(aPath, sizeof(aPath), "%s/%s", aResolved, pEntry->d_name);
            struct stat st;
            bool bStat = stat(aPath, &st) == 0;

            STD_STRCPY(pFileInfo->aName, pEntry->d_name);
            pFileInfo->lastChanged  = bStat ? st.st_mtime : 0;
            pFileInfo->bIsDirectory = bStat && S_ISDIR(st.st_mode);
            return 1;
        }
    }

#ifdef __ANDROID__
    // Then the game data read in place from the APK (GOBs, movies)
    const char* pName;
    while ( (pName = stdAndroid_NextAsset(aResolved, &ffData->nextAsset)) != NULL )
    {
        if ( bAll || fnmatch(pPattern, pName, FNM_CASEFOLD) == 0 )
        {
            STD_STRCPY(pFileInfo->aName, pName);
            pFileInfo->lastChanged  = 0;
            pFileInfo->bIsDirectory = false;
            return 1;
        }
    }
#endif
    return 0;
}

#endif

int J3DAPI stdFileUtil_FindQuick(const char* pPath, int mode, const char* pFilter, tFoundFileInfo* pFileInfo)
{
    FindFileData* pFileData = stdFileUtil_NewFind(pPath, mode, pFilter);
    if ( !pFileData )
    {
        return 0;
    }

    int bFound = stdFileUtil_FindNext(pFileData, pFileInfo);
    stdFileUtil_DisposeFind(pFileData);
    return bFound;
}

int J3DAPI stdFileUtil_CountMatches(const char* pPath, int mode, const char* pFilter)
{
    tFoundFileInfo fileInfo;

    int count = 0;
    FindFileData* pFileData = stdFileUtil_NewFind(pPath, mode, pFilter);
    if ( !pFileData )
    {
        return 0;
    }

    while ( stdFileUtil_FindNext(pFileData, &fileInfo) )
    {
        ++count;
    }

    stdFileUtil_DisposeFind(pFileData);
    return count;
}

#ifdef _WIN32
int J3DAPI stdFileUtil_MkDir(const char* pPath)
{
    return CreateDirectoryA(pPath, NULL);
}

int J3DAPI stdFileUtil_FileExists(const char* pFilename)
{
    WIN32_FIND_DATAA findFileData;
    HANDLE hFind = FindFirstFileA(pFilename, &findFileData);
    if ( hFind == INVALID_HANDLE_VALUE )
    {
        return 0;
    }

    // Fixed: FindFirstFileA returns a search handle that must be closed.
    FindClose(hFind);
    return 1;
}

int J3DAPI stdFileUtil_RmDir(const char* pDir)
{
    return RemoveDirectoryA(pDir);
}

int J3DAPI stdFileUtil_DelFile(const char* pFilename)
{
    return DeleteFileA(pFilename);
}
#else
int J3DAPI stdFileUtil_MkDir(const char* pPath)
{
    char aPath[J3D_MAX_PATH];
    return mkdir(J3D_ResolvePath(pPath, aPath, sizeof(aPath)), 0755) == 0;
}

int J3DAPI stdFileUtil_FileExists(const char* pFilename)
{
    char aPath[J3D_MAX_PATH];
    struct stat st;
    if ( stat(J3D_ResolvePath(pFilename, aPath, sizeof(aPath)), &st) == 0 )
    {
        return 1;
    }
#ifdef __ANDROID__
    return stdAndroid_AssetExists(aPath);
#else
    return 0;
#endif
}

int J3DAPI stdFileUtil_RmDir(const char* pDir)
{
    char aPath[J3D_MAX_PATH];
    return rmdir(J3D_ResolvePath(pDir, aPath, sizeof(aPath))) == 0;
}

int J3DAPI stdFileUtil_DelFile(const char* pFilename)
{
    char aPath[J3D_MAX_PATH];
    return unlink(J3D_ResolvePath(pFilename, aPath, sizeof(aPath))) == 0;
}
#endif
