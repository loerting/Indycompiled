// Android builds: game data in the APK and in internal storage.
// The APK's assets mirror the game's install layout (Resource/, KeySets/, Install/) and list their files in
// indy_assets.txt (written by the packing recipe, Scripts/android/pack_data.sh). The big read-only files (GOBs,
// movies) are read in place through SDL's asset streams; the folders the game lists or writes are copied to
// internal storage on first start, where Resource/ is the working dir.
#ifdef __ANDROID__
#include "stdAndroidSDL.h"

#include <j3dcore/j3d.h>
#include <SDL3/SDL.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#undef fopen // the C library's (j3dwin32.h wraps it)

#define STDANDROID_ASSETINDEX "indy_assets.txt"

static char stdAndroid_aBaseDir[PATH_MAX]; // internal storage, canonical
static char* stdAndroid_pIndex;            // indy_assets.txt, one asset path per line ('\n' replaced by 0)
static const char** stdAndroid_apAssets;
static size_t stdAndroid_numAssets;

static int stdAndroid_MakeDirs(const char* pPath)
{
    char aPath[PATH_MAX];
    SDL_strlcpy(aPath, pPath, sizeof(aPath));
    for ( char* p = aPath + 1; *p; ++p )
    {
        if ( *p != '/' ) continue;
        *p = 0;
        if ( mkdir(aPath, 0700) != 0 && errno != EEXIST ) return 1;
        *p = '/';
    }
    return mkdir(aPath, 0700) != 0 && errno != EEXIST;
}

static int stdAndroid_LoadIndex(void)
{
    size_t size = 0;
    stdAndroid_pIndex = (char*)SDL_LoadFile(STDANDROID_ASSETINDEX, &size);
    if ( !stdAndroid_pIndex )
    {
        SDL_Log("stdAndroid: no %s in the APK (built without game data?): %s", STDANDROID_ASSETINDEX, SDL_GetError());
        return 1;
    }

    size_t numLines = 1;
    for ( size_t i = 0; i < size; ++i ) numLines += stdAndroid_pIndex[i] == '\n';
    stdAndroid_apAssets = (const char**)SDL_calloc(numLines, sizeof(const char*));
    if ( !stdAndroid_apAssets ) return 1;

    char* pLine = stdAndroid_pIndex;
    while ( pLine && *pLine )
    {
        char* pEnd = SDL_strchr(pLine, '\n');
        if ( pEnd ) *pEnd = 0;
        size_t len = SDL_strlen(pLine);
        if ( len && pLine[len - 1] == '\r' ) pLine[--len] = 0;
        if ( len ) stdAndroid_apAssets[stdAndroid_numAssets++] = pLine;
        pLine = pEnd ? pEnd + 1 : NULL;
    }
    return 0;
}

static const char* stdAndroid_FindAsset(const char* pName)
{
    for ( size_t i = 0; i < stdAndroid_numAssets; ++i )
    {
        if ( strcasecmp(stdAndroid_apAssets[i], pName) == 0 ) return stdAndroid_apAssets[i];
    }
    return NULL;
}

// Copies an asset to internal storage unless a file of the same size is there
static int stdAndroid_ExtractAsset(const char* pAsset, const char* pDest)
{
    SDL_IOStream* pIn = SDL_IOFromFile(pAsset, "rb");
    if ( !pIn ) return 1;

    struct stat st;
    const Sint64 size = SDL_GetIOSize(pIn);
    if ( stat(pDest, &st) == 0 && st.st_size == size )
    {
        SDL_CloseIO(pIn);
        return 0;
    }

    char aDir[PATH_MAX];
    SDL_strlcpy(aDir, pDest, sizeof(aDir));
    char* pSlash = SDL_strrchr(aDir, '/');
    if ( pSlash ) *pSlash = 0;
    stdAndroid_MakeDirs(aDir);

    int result = 1;
    FILE* pOut = (fopen)(pDest, "wb");
    if ( pOut )
    {
        char aBuf[64 * 1024];
        size_t n;
        result = 0;
        while ( (n = SDL_ReadIO(pIn, aBuf, sizeof(aBuf))) > 0 )
        {
            if ( fwrite(aBuf, 1, n, pOut) != n )
            {
                result = 1;
                break;
            }
        }
        result |= fclose(pOut) != 0;
    }
    SDL_CloseIO(pIn);
    return result;
}

int stdAndroid_PrepareDataDir(void)
{
    const char* pStorage = SDL_GetAndroidInternalStoragePath();
    if ( !pStorage || !realpath(pStorage, stdAndroid_aBaseDir) )
    {
        SDL_Log("stdAndroid: no internal storage: %s", SDL_GetError());
        return 1;
    }

    char aPath[PATH_MAX];
    SDL_snprintf(aPath, sizeof(aPath), "%s/Resource", stdAndroid_aBaseDir);
    if ( stdAndroid_MakeDirs(aPath) || chdir(aPath) != 0 )
    {
        SDL_Log("stdAndroid: can't use %s", aPath);
        return 1;
    }
    SDL_snprintf(aPath, sizeof(aPath), "%s/SaveGames", stdAndroid_aBaseDir);
    stdAndroid_MakeDirs(aPath);

    // The engine logs to stderr: keep it next to JonesLog.txt (adb shell run-as <package> cat files/Resource/...)
    freopen("stderr.txt", "w", stderr);
    setvbuf(stderr, NULL, _IOLBF, 0);

    // Apps get no environment: the debug variables (INDY_*) can be put into Resource/indy_env.txt, NAME=VALUE per line
    FILE* pEnv = (fopen)("indy_env.txt", "r");
    if ( pEnv )
    {
        char aLine[512];
        while ( fgets(aLine, sizeof(aLine), pEnv) )
        {
            aLine[strcspn(aLine, "\r\n")] = 0;
            char* pEq = SDL_strchr(aLine, '=');
            if ( !pEq || aLine[0] == '#' ) continue;
            *pEq = 0;
            setenv(aLine, pEq + 1, 1);
            fprintf(stderr, "indy_env.txt: %s=%s\n", aLine, pEq + 1);
        }
        fclose(pEnv);
    }

    if ( stdAndroid_LoadIndex() )
    {
        return 0; // the game reports the missing data itself
    }

    // Resource/ stays in the APK (Jones.cfg is created by the game); the other folders are small, and the game lists
    // or writes them
    for ( size_t i = 0; i < stdAndroid_numAssets; ++i )
    {
        const char* pAsset = stdAndroid_apAssets[i];
        if ( SDL_strncasecmp(pAsset, "Resource/", 9) == 0 ) continue;

        SDL_snprintf(aPath, sizeof(aPath), "%s/%s", stdAndroid_aBaseDir, pAsset);
        if ( stdAndroid_ExtractAsset(pAsset, aPath) )
        {
            SDL_Log("stdAndroid: couldn't extract %s", pAsset);
        }
    }
    return 0;
}

// Asset name of a path below the base dir: made absolute against the working dir, '.' and '..' resolved
static bool stdAndroid_GetAssetName(const char* pPath, char* pOut, size_t outSize)
{
    char aAbs[PATH_MAX];
    if ( pPath[0] == '/' )
    {
        SDL_strlcpy(aAbs, pPath, sizeof(aAbs));
    }
    else
    {
        char aCwd[PATH_MAX];
        if ( !getcwd(aCwd, sizeof(aCwd)) ) return false;
        SDL_snprintf(aAbs, sizeof(aAbs), "%s/%s", aCwd, pPath);
    }

    // Normalize: components are copied to aNorm, ".." drops the last one
    char aNorm[PATH_MAX];
    size_t o = 0;
    for ( char* p = aAbs; *p; )
    {
        while ( *p == '/' ) ++p;
        if ( !*p ) break;
        char* pEnd = SDL_strchr(p, '/');
        size_t len = pEnd ? (size_t)(pEnd - p) : SDL_strlen(p);
        if ( len == 2 && p[0] == '.' && p[1] == '.' )
        {
            while ( o > 0 && aNorm[o - 1] != '/' ) --o;
            if ( o > 0 ) --o;
        }
        else if ( !(len == 1 && p[0] == '.') )
        {
            if ( o + len + 2 >= sizeof(aNorm) ) return false;
            aNorm[o++] = '/';
            SDL_memcpy(aNorm + o, p, len);
            o += len;
        }
        p += len;
    }
    aNorm[o] = 0;

    // Below the base dir, or the base dir itself (asset name "")
    const size_t baseLen = SDL_strlen(stdAndroid_aBaseDir);
    if ( SDL_strncmp(aNorm, stdAndroid_aBaseDir, baseLen) != 0 || (aNorm[baseLen] != '/' && aNorm[baseLen] != 0) ) return false;
    SDL_strlcpy(pOut, aNorm[baseLen] ? aNorm + baseLen + 1 : "", outSize);
    return true;
}

bool stdAndroid_AssetExists(const char* pPath)
{
    char aName[PATH_MAX];
    return pPath && stdAndroid_numAssets && stdAndroid_GetAssetName(pPath, aName, sizeof(aName)) && stdAndroid_FindAsset(aName);
}

const char* stdAndroid_NextAsset(const char* pDir, size_t* pNext)
{
    char aDir[PATH_MAX];
    if ( !pDir || !stdAndroid_numAssets || !stdAndroid_GetAssetName(pDir, aDir, sizeof(aDir)) ) return NULL;

    const size_t dirLen = SDL_strlen(aDir);
    while ( *pNext < stdAndroid_numAssets )
    {
        const char* pAsset = stdAndroid_apAssets[(*pNext)++];
        if ( dirLen && (SDL_strncasecmp(pAsset, aDir, dirLen) != 0 || pAsset[dirLen] != '/') ) continue;

        const char* pName = dirLen ? pAsset + dirLen + 1 : pAsset;
        if ( !*pName || SDL_strchr(pName, '/') ) continue; // not directly in pDir

        char aPath[PATH_MAX];
        SDL_snprintf(aPath, sizeof(aPath), "%s/%s", stdAndroid_aBaseDir, pAsset);
        if ( access(aPath, F_OK) == 0 ) continue; // listed with the folder already
        return pName;
    }
    return NULL;
}

static int stdAndroid_AssetRead(void* pCookie, char* pBuf, int size)
{
    SDL_IOStream* pIO = (SDL_IOStream*)pCookie;
    size_t n = SDL_ReadIO(pIO, pBuf, (size_t)size);
    if ( n == 0 && SDL_GetIOStatus(pIO) == SDL_IO_STATUS_ERROR ) return -1;
    return (int)n;
}

static fpos64_t stdAndroid_AssetSeek(void* pCookie, fpos64_t offset, int whence)
{
    const SDL_IOWhence sdlWhence = whence == SEEK_SET ? SDL_IO_SEEK_SET : (whence == SEEK_CUR ? SDL_IO_SEEK_CUR : SDL_IO_SEEK_END);
    return (fpos64_t)SDL_SeekIO((SDL_IOStream*)pCookie, (Sint64)offset, sdlWhence);
}

static int stdAndroid_AssetClose(void* pCookie)
{
    return SDL_CloseIO((SDL_IOStream*)pCookie) ? 0 : -1;
}

FILE* stdAndroid_OpenAsset(const char* pPath, const char* mode)
{
    if ( !pPath || SDL_strpbrk(mode, "wa+") || !stdAndroid_numAssets ) return NULL;

    char aName[PATH_MAX];
    if ( !stdAndroid_GetAssetName(pPath, aName, sizeof(aName)) ) return NULL;
    const char* pAsset = stdAndroid_FindAsset(aName);
    if ( !pAsset ) return NULL;

    SDL_IOStream* pIO = SDL_IOFromFile(pAsset, "rb");
    if ( !pIO ) return NULL;
    FILE* pFile = funopen64(pIO, stdAndroid_AssetRead, NULL, stdAndroid_AssetSeek, stdAndroid_AssetClose);
    if ( !pFile ) SDL_CloseIO(pIO);
    return pFile;
}

#endif // __ANDROID__
