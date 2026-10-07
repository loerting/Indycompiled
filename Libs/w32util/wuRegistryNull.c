// Native builds (Linux, Android): no registry. The settings live in the JSON config (stdConfig); this module
// only keeps its interface: every read returns the default, every write succeeds without storing anything.
#include <w32util/wuRegistry.h>
#include <std/General/stdUtil.h>

static bool wuRegistry_bStarted = false;

int J3DAPI wuRegistry_Startup(HKEY hKey, LPCSTR lpSubKey)
{
    J3D_UNUSED(hKey);
    J3D_UNUSED(lpSubKey);
    wuRegistry_bStarted = true;
    return 0;
}

bool wuRegistry_HasStarted(void)
{
    return wuRegistry_bStarted;
}

void J3DAPI wuRegistry_Shutdown()
{
    wuRegistry_bStarted = false;
}

int J3DAPI wuRegistry_SaveInt(const char* pKey, int value) { J3D_UNUSED(pKey); J3D_UNUSED(value); return 0; }
int J3DAPI wuRegistry_GetInt(const char* pKey, int defaultValue) { J3D_UNUSED(pKey); return defaultValue; }
int J3DAPI wuRegistry_SaveFloat(const char* pKey, float value) { J3D_UNUSED(pKey); J3D_UNUSED(value); return 0; }
float J3DAPI wuRegistry_GetFloat(const char* pKey, float defaultValue) { J3D_UNUSED(pKey); return defaultValue; }
int J3DAPI wuRegistry_SaveBool(const char* pKey, int value) { J3D_UNUSED(pKey); J3D_UNUSED(value); return 0; }
int J3DAPI wuRegistry_GetBool(const char* pKey, int defaultValue) { J3D_UNUSED(pKey); return defaultValue; }
int J3DAPI wuRegistry_SaveStr(const char* pKey, const char* pStr) { J3D_UNUSED(pKey); J3D_UNUSED(pStr); return 0; }

bool J3DAPI wuRegistry_SaveBinary(const char* pKey, const uint8_t* pData, size_t size)
{
    J3D_UNUSED(pKey); J3D_UNUSED(pData); J3D_UNUSED(size);
    return true;
}

bool J3DAPI wuRegistry_GetBinary(const char* pKey, uint8_t* pData, size_t size)
{
    J3D_UNUSED(pKey); J3D_UNUSED(pData); J3D_UNUSED(size);
    return false;
}

int J3DAPI wuRegistry_GetStr(const char* pKey, char* pDstStr, size_t size, const char* pDefaultValue)
{
    J3D_UNUSED(pKey);
    if ( pDefaultValue )
    {
        stdUtil_StringCopy(pDstStr, size, pDefaultValue);
    }
    return 1;
}

void wuRegistry_InstallHooks(void) {}
void wuRegistry_ResetGlobals(void) {}
