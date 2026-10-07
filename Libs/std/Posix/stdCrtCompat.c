// Native builds (Linux, Android): the MSVC CRT functions the engine uses (secure variants, sscanf_s) and the
// wide string functions for 16-bit wchar_t (-fshort-wchar, the Windows layout of the engine's structs and savegames).
// Declared in j3dcore/j3dwin32.h.
#include <j3dcore/j3d.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>

#include <time.h>
#include <unistd.h>

#undef fopen // the C library's, wrapped by J3D_fopen

size_t J3D_strnlen_s(const char* s, size_t n)
{
    return s ? strnlen(s, n) : 0;
}

int J3D_strncat_s(char* d, size_t n, const char* s, size_t c)
{
    if ( !d || !n || !s ) return 22; // EINVAL
    size_t l = strnlen(d, n);
    if ( l >= n ) return 22;
    size_t add = strnlen(s, c == (size_t)-1 ? n : c);
    if ( add > n - l - 1 ) add = n - l - 1;
    memcpy(d + l, s, add);
    d[l + add] = 0;
    return 0;
}

// Paths: the engine uses '\\' separators and Windows' case-insensitive names. J3D_ResolvePath turns '\\' into '/'
// and matches each existing path component case-insensitively; components that don't exist are kept as written.
char* J3D_ResolvePath(const char* pPath, char* pOut, size_t outSize)
{
    if ( !pOut || !outSize ) return NULL;
    pOut[0] = 0;
    if ( !pPath ) return pOut;

    char path[J3D_MAX_PATH];
    snprintf(path, sizeof(path), "%s", pPath);
    for ( char* p = path; *p; ++p )
    {
        if ( *p == '\\' ) *p = '/';
    }

    size_t o = 0;
    const char* p = path;
    if ( *p == '/' )
    {
        pOut[o++] = '/';
        while ( *p == '/' ) ++p;
    }
    pOut[o] = 0;

    bool bDirValid = true;
    while ( *p )
    {
        const char* pEnd = strchr(p, '/');
        size_t len = pEnd ? (size_t)(pEnd - p) : strlen(p);
        char comp[256];
        if ( len >= sizeof(comp) ) len = sizeof(comp) - 1;
        memcpy(comp, p, len);
        comp[len] = 0;

        if ( bDirValid && strcmp(comp, ".") != 0 && strcmp(comp, "..") != 0 )
        {
            const char* pDir = o ? pOut : ".";
            char test[J3D_MAX_PATH];
            snprintf(test, sizeof(test), "%s%s", pOut, comp);
            struct stat st;
            if ( stat(test, &st) != 0 )
            {
                DIR* pDirHandle = opendir(pDir);
                bool bFound = false;
                if ( pDirHandle )
                {
                    struct dirent* pEntry;
                    while ( (pEntry = readdir(pDirHandle)) != NULL )
                    {
                        if ( strcasecmp(pEntry->d_name, comp) == 0 )
                        {
                            snprintf(comp, sizeof(comp), "%s", pEntry->d_name);
                            bFound = true;
                            break;
                        }
                    }
                    closedir(pDirHandle);
                }
                bDirValid = bFound;
            }
        }

        int n = snprintf(pOut + o, outSize - o, "%s%s", comp, pEnd ? "/" : "");
        if ( n < 0 || (size_t)n >= outSize - o ) break;
        o += (size_t)n;

        if ( !pEnd ) break;
        p = pEnd;
        while ( *p == '/' ) ++p;
    }
    return pOut;
}

FILE* J3D_fopen(const char* name, const char* mode)
{
    char path[J3D_MAX_PATH];
    return (fopen)(J3D_ResolvePath(name, path, sizeof(path)), mode);
}

int J3D_fopen_s(FILE** pf, const char* name, const char* mode)
{
    if ( !pf ) return 22;
    *pf = J3D_fopen(name, mode);
    return *pf ? 0 : 2; // ENOENT
}

// sscanf_s: %s, %c and %[ take a buffer size after the pointer. Collect the pointers, turn the sizes into field
// widths, then run the C library's sscanf once.
#define J3D_SCANF_MAXARGS 32

int J3D_vsscanf_s(const char* str, const char* fmt, va_list args)
{
    char newFmt[1024];
    void* aPtrs[J3D_SCANF_MAXARGS] = { 0 };
    size_t numPtrs = 0;
    size_t o = 0;
    const char* p = fmt;
    while ( *p && o < sizeof(newFmt) - 32 )
    {
        if ( *p != '%' )
        {
            newFmt[o++] = *p++;
            continue;
        }

        newFmt[o++] = *p++;
        if ( *p == '%' )
        {
            newFmt[o++] = *p++;
            continue;
        }

        bool bSuppress = false;
        if ( *p == '*' )
        {
            bSuppress = true;
            newFmt[o++] = *p++;
        }

        bool bHasWidth = false;
        while ( *p >= '0' && *p <= '9' )
        {
            bHasWidth = true;
            newFmt[o++] = *p++;
        }

        size_t lenStart = o;
        while ( *p == 'h' || *p == 'l' || *p == 'L' || *p == 'z' || *p == 'j' || *p == 't' || *p == 'q' )
        {
            newFmt[o++] = *p++;
        }

        char conv = *p;
        if ( !conv ) break;

        if ( !bSuppress )
        {
            if ( numPtrs >= J3D_SCANF_MAXARGS ) return -1;
            aPtrs[numPtrs++] = va_arg(args, void*);
            if ( conv == 's' || conv == 'c' || conv == '[' )
            {
                size_t size = va_arg(args, size_t); // rsize_t
                if ( !bHasWidth && size > 0 )
                {
                    // insert the width before the length modifiers
                    char aWidth[16];
                    int w = snprintf(aWidth, sizeof(aWidth), "%u", (unsigned)(conv == 'c' ? 1 : size - 1));
                    memmove(newFmt + lenStart + w, newFmt + lenStart, o - lenStart);
                    memcpy(newFmt + lenStart, aWidth, (size_t)w);
                    o += (size_t)w;
                }
            }
        }

        newFmt[o++] = *p++;
        if ( conv == '[' )
        {
            if ( *p == '^' ) newFmt[o++] = *p++;
            if ( *p == ']' ) newFmt[o++] = *p++;
            while ( *p && *p != ']' && o < sizeof(newFmt) - 2 ) newFmt[o++] = *p++;
            if ( *p == ']' ) newFmt[o++] = *p++;
        }
    }
    newFmt[o] = 0;

    return sscanf(str, newFmt,
        aPtrs[0], aPtrs[1], aPtrs[2], aPtrs[3], aPtrs[4], aPtrs[5], aPtrs[6], aPtrs[7],
        aPtrs[8], aPtrs[9], aPtrs[10], aPtrs[11], aPtrs[12], aPtrs[13], aPtrs[14], aPtrs[15],
        aPtrs[16], aPtrs[17], aPtrs[18], aPtrs[19], aPtrs[20], aPtrs[21], aPtrs[22], aPtrs[23],
        aPtrs[24], aPtrs[25], aPtrs[26], aPtrs[27], aPtrs[28], aPtrs[29], aPtrs[30], aPtrs[31]);
}

int J3D_sscanf_s(const char* str, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int r = J3D_vsscanf_s(str, fmt, args);
    va_end(args);
    return r;
}

// ---- 16-bit wide strings -------------------------------------------------------------------------------------

static inline wchar_t J3D_towlower(wchar_t c)
{
    return (c >= L'A' && c <= L'Z') ? (wchar_t)(c + 32) : c;
}

size_t J3D_wcslen(const wchar_t* s)
{
    size_t n = 0;
    while ( s[n] ) ++n;
    return n;
}

size_t J3D_wcsnlen(const wchar_t* s, size_t max)
{
    size_t n = 0;
    while ( n < max && s[n] ) ++n;
    return n;
}

wchar_t* J3D_wcsncpy(wchar_t* d, const wchar_t* s, size_t n)
{
    size_t i = 0;
    for ( ; i < n && s[i]; ++i ) d[i] = s[i];
    for ( ; i < n; ++i ) d[i] = 0;
    return d;
}

wchar_t* J3D_wcscpy(wchar_t* d, const wchar_t* s)
{
    size_t i = 0;
    do d[i] = s[i]; while ( s[i++] );
    return d;
}

int J3D_wcsncpy_s(wchar_t* d, size_t n, const wchar_t* s, size_t c)
{
    if ( !d || !n ) return 22;
    if ( !s )
    {
        d[0] = 0;
        return 22;
    }
    size_t len = J3D_wcsnlen(s, c == (size_t)-1 ? n : c);
    if ( len >= n ) len = n - 1;
    memcpy(d, s, len * sizeof(wchar_t));
    d[len] = 0;
    return 0;
}

int J3D_wcscmp(const wchar_t* a, const wchar_t* b)
{
    while ( *a && *a == *b ) ++a, ++b;
    return (int)(uint16_t)*a - (int)(uint16_t)*b;
}

int J3D_wcsncmp(const wchar_t* a, const wchar_t* b, size_t n)
{
    for ( ; n; --n, ++a, ++b )
    {
        if ( *a != *b || !*a ) return (int)(uint16_t)*a - (int)(uint16_t)*b;
    }
    return 0;
}

int J3D_wcsicmp(const wchar_t* a, const wchar_t* b)
{
    while ( *a && J3D_towlower(*a) == J3D_towlower(*b) ) ++a, ++b;
    return (int)J3D_towlower(*a) - (int)J3D_towlower(*b);
}

int J3D_wcsnicmp(const wchar_t* a, const wchar_t* b, size_t n)
{
    for ( ; n; --n, ++a, ++b )
    {
        if ( J3D_towlower(*a) != J3D_towlower(*b) || !*a ) return (int)J3D_towlower(*a) - (int)J3D_towlower(*b);
    }
    return 0;
}

wchar_t* J3D_wcsdup(const wchar_t* s)
{
    size_t n = (J3D_wcslen(s) + 1) * sizeof(wchar_t);
    wchar_t* d = malloc(n);
    if ( d ) memcpy(d, s, n);
    return d;
}

wchar_t* J3D_wmemchr(const wchar_t* s, wchar_t c, size_t n)
{
    for ( ; n; --n, ++s )
    {
        if ( *s == c ) return (wchar_t*)s;
    }
    return NULL;
}

wchar_t* J3D_wcschr(const wchar_t* s, wchar_t c)
{
    for ( ;; ++s )
    {
        if ( *s == c ) return (wchar_t*)s;
        if ( !*s ) return NULL;
    }
}

wchar_t* J3D_wcsstr(const wchar_t* s, const wchar_t* sub)
{
    size_t n = J3D_wcslen(sub);
    if ( !n ) return (wchar_t*)s;
    for ( ; *s; ++s )
    {
        if ( J3D_wcsncmp(s, sub, n) == 0 ) return (wchar_t*)s;
    }
    return NULL;
}

// Wide printf with MSVC semantics: %s and %c take wide arguments, %S and %C narrow ones. Each conversion is formatted
// by the narrow vsnprintf; text is widened as Latin-1 (the game's strings are Latin-1).
int J3D_vsnwprintf(wchar_t* buf, size_t n, const wchar_t* fmt, va_list args)
{
    size_t o = 0;
    int total = 0;
#define J3D_PUTW(ch) do { if ( buf && o + 1 < n ) buf[o++] = (wchar_t)(ch); ++total; } while ( 0 )

    for ( const wchar_t* p = fmt; *p; )
    {
        if ( *p != L'%' )
        {
            J3D_PUTW(*p++);
            continue;
        }

        // copy the conversion spec as narrow text
        char spec[32];
        size_t si = 0;
        spec[si++] = (char)*p++;
        while ( *p && si < sizeof(spec) - 2 && strchr("-+ #0123456789.*hlLzjtwI", (int)*p) )
        {
            spec[si++] = (char)*p++;
        }
        wchar_t conv = *p ? *p++ : 0;
        if ( !conv ) break;

        if ( conv == L'%' )
        {
            J3D_PUTW(L'%');
            continue;
        }

        // '*' width/precision
        int aStars[2];
        int numStars = 0;
        for ( size_t i = 0; i < si; ++i )
        {
            if ( spec[i] == '*' && numStars < 2 ) aStars[numStars++] = va_arg(args, int);
        }

        bool bLong = memchr(spec, 'l', si) != NULL;
        bool bWide = (conv == L's' || conv == L'c') ? !(memchr(spec, 'h', si) != NULL) : bLong;
        if ( conv == L'S' || conv == L'C' ) bWide = bLong;

        // strip length modifiers for strings/chars, keep flags/width/precision
        char narrowSpec[40];
        size_t ni = 0;
        for ( size_t i = 0; i < si; ++i )
        {
            if ( (conv == L's' || conv == L'S' || conv == L'c' || conv == L'C') && strchr("hlLwI", spec[i]) ) continue;
            narrowSpec[ni++] = spec[i];
        }

        char tmp[1024];
        int len = 0;
        if ( conv == L's' || conv == L'S' )
        {
            narrowSpec[ni++] = 's';
            narrowSpec[ni] = 0;
            char text[1024];
            if ( bWide )
            {
                const wchar_t* ws = va_arg(args, const wchar_t*);
                size_t k = 0;
                if ( !ws ) ws = L"(null)";
                for ( ; ws[k] && k < sizeof(text) - 1; ++k ) text[k] = (char)(ws[k] < 256 ? ws[k] : '?');
                text[k] = 0;
            }
            else
            {
                const char* s = va_arg(args, const char*);
                snprintf(text, sizeof(text), "%s", s ? s : "(null)");
            }
            len = numStars == 2 ? snprintf(tmp, sizeof(tmp), narrowSpec, aStars[0], aStars[1], text)
                : numStars == 1 ? snprintf(tmp, sizeof(tmp), narrowSpec, aStars[0], text)
                : snprintf(tmp, sizeof(tmp), narrowSpec, text);
        }
        else if ( conv == L'c' || conv == L'C' )
        {
            narrowSpec[ni++] = 'c';
            narrowSpec[ni] = 0;
            int c = va_arg(args, int);
            len = numStars == 1 ? snprintf(tmp, sizeof(tmp), narrowSpec, aStars[0], (char)c) : snprintf(tmp, sizeof(tmp), narrowSpec, (char)c);
            if ( bWide && c >= 256 ) tmp[0] = '?';
        }
        else
        {
            narrowSpec[ni++] = (char)conv;
            narrowSpec[ni] = 0;
            bool bFloat = strchr("eEfFgGaA", (int)conv) != NULL;
            bool bLongLong = strstr(narrowSpec, "ll") != NULL || strstr(narrowSpec, "I64") != NULL;
            if ( bFloat )
            {
                double v = va_arg(args, double);
                len = numStars == 2 ? snprintf(tmp, sizeof(tmp), narrowSpec, aStars[0], aStars[1], v)
                    : numStars == 1 ? snprintf(tmp, sizeof(tmp), narrowSpec, aStars[0], v)
                    : snprintf(tmp, sizeof(tmp), narrowSpec, v);
            }
            else if ( conv == L'p' )
            {
                len = snprintf(tmp, sizeof(tmp), narrowSpec, va_arg(args, void*));
            }
            else if ( bLongLong )
            {
                long long v = va_arg(args, long long);
                len = numStars == 1 ? snprintf(tmp, sizeof(tmp), narrowSpec, aStars[0], v) : snprintf(tmp, sizeof(tmp), narrowSpec, v);
            }
            else
            {
                int v = va_arg(args, int);
                len = numStars == 2 ? snprintf(tmp, sizeof(tmp), narrowSpec, aStars[0], aStars[1], v)
                    : numStars == 1 ? snprintf(tmp, sizeof(tmp), narrowSpec, aStars[0], v)
                    : snprintf(tmp, sizeof(tmp), narrowSpec, v);
            }
        }

        if ( len > (int)sizeof(tmp) - 1 ) len = (int)sizeof(tmp) - 1;
        for ( int i = 0; i < len; ++i ) J3D_PUTW((unsigned char)tmp[i]);
    }

    if ( buf && n ) buf[o] = 0;
#undef J3D_PUTW
    return (buf && (size_t)total >= n) ? -1 : total;
}

int J3D_snwprintf(wchar_t* buf, size_t n, const wchar_t* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int r = J3D_vsnwprintf(buf, n, fmt, args);
    va_end(args);
    return r;
}

// ---- time --------------------------------------------------------------------------------------------------

DWORD J3D_GetTickCount(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

void J3D_GetLocalTime(SYSTEMTIME* pTime)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm t;
    localtime_r(&ts.tv_sec, &t);
    pTime->wYear         = (WORD)(t.tm_year + 1900);
    pTime->wMonth        = (WORD)(t.tm_mon + 1);
    pTime->wDayOfWeek    = (WORD)t.tm_wday;
    pTime->wDay          = (WORD)t.tm_mday;
    pTime->wHour         = (WORD)t.tm_hour;
    pTime->wMinute       = (WORD)t.tm_min;
    pTime->wSecond       = (WORD)t.tm_sec;
    pTime->wMilliseconds = (WORD)(ts.tv_nsec / 1000000);
}

void J3D_Sleep(DWORD msec)
{
    usleep((useconds_t)msec * 1000u);
}

// ---- misc ----------------------------------------------------------------------------------------------------

BOOL J3D_CloseHandle(HANDLE h)
{
    (void)h; // no kernel handles on native builds (the Win32 single-instance mutex isn't created)
    return TRUE;
}

BOOL J3D_GetComputerName(LPSTR pBuffer, LPDWORD pSize)
{
    if ( !pBuffer || !pSize || !*pSize || gethostname(pBuffer, *pSize) != 0 ) return FALSE;
    pBuffer[*pSize - 1] = 0;
    *pSize = (DWORD)strlen(pBuffer);
    return TRUE;
}
