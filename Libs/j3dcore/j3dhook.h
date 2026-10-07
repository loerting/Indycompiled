
#ifndef JONES3D_HOOK_H
#define JONES3D_HOOK_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> // INDY: getenv
#include <string.h> // INDY: strstr
#include <j3dcore/j3d.h>

#ifdef J3D_STANDALONE
/**
* INDY: Stage 4 (CMake option JONES3D_STANDALONE). Jones3D.exe is the game itself: Indy3D.exe's code is never loaded,
* so nothing is hooked, nothing is called through the exe, and the exe's globals are our own C objects. A variable
* declared as `#define name J3D_DECL_FAR_VAR(name, type)` names the object `name` itself (a macro doesn't expand its
* own name again); the module's header declares it extern and its .c file defines it with the exe's initial value.
* J3D_TRAMPOLINE_CALL and J3D_CALLFUNCFAR are left undefined, so a remaining call into the exe doesn't compile.
*/
#define J3D_HOOKFUNC(func) ((void)(func))
#define INDY_AB_ORIGINAL(func, ...) do {} while ( 0 )
#define INDY_AB_ORIGINAL_VOID(func, ...) do {} while ( 0 )
#define J3D_DECL_FAR_VAR(var_name, var_type) var_name
#define J3D_DECL_FAR_ARRAYVAR(var_name, var_type) var_name
#define J3D_EXE_FUNC(func_name, func_type) ((func_type)NULL)

static inline bool J3DHookIsSkipped(const char* pName)
{
    (void)pName;
    return false;
}

static inline bool J3DHookFunction(intptr_t pFuncAddr, void* pHookFunc)
{
    (void)pFuncAddr;
    (void)pHookFunc;
    return false;
}
#else // INDY: hook build (Jones3D.dll injected into Indy3D.exe v1.2)

/**
* @brief Macro calls a function at a far address.
*
* @param func_addr Address of the function.
* @param func_type Type of the function.
* @param ... Function parameters (can be empty for functions with no parameters).
*/
#define J3D_CALLFUNCFAR(func_addr, func_type, ...) \
    _J3D_CALLFUNCFAR_HELPER(func_addr, func_type, ##__VA_ARGS__)

#define _J3D_CALLFUNCFAR_HELPER(func_addr, func_type, ...) \
    ((func_type)func_addr)(__VA_ARGS__)

/**
* @brief Simplified macro wrapper for CALLFUNCFAR.
*
* @param func_name Name of the function.
*                  The global function address variable (func_name_ADDR) and type variable (func_name_TYPE) are derived from name.
* @param ... Function parameters (can be empty for functions with no parameters).
*/
#define J3D_TRAMPOLINE_CALL(func_name, ...) \
    J3D_CALLFUNCFAR(func_name##_ADDR, func_name##_TYPE, ##__VA_ARGS__)

// INDY: the exe's entry point of a function, as a function pointer (NULL in the standalone build: no exe)
#define J3D_EXE_FUNC(func_name, func_type) ((func_type)(func_name##_ADDR))

/**
* @brief Redirects the original function to a specified hook function.
*
* This macro is used to hook a function by redirecting the original function
* to a new function. It derives the address of the original function by
* appending `_ADDR` to the function name and then replaces it with the new
* function provided as the argument.
*
* @param func The function to which the original function is redirected.
*             The macro derives the address variable of the original function
*             as `func_ADDR`, which is expected to be globally defined or
*             included in the source file where this macro is used.
*/
#define J3D_HOOKFUNC(func) \
    (J3DHookIsSkipped(#func) ? false : J3DHookFunction(func##_ADDR, (void*)func)) // INDY: INDY_NOHOOK (A/B tests)

/**
* INDY: A/B tests (Scripts/indy/test_ab.sh). INDY_NOHOOK="func,func,..." leaves those functions unhooked, so callers in
* the exe run the original. Calls from our own C code reach the C function directly; a reimplemented function under
* test starts with INDY_AB_ORIGINAL / INDY_AB_ORIGINAL_VOID to hand those to the original too.
*/
static inline bool J3DHookIsSkipped(const char* pName)
{
#ifdef _MSC_VER
#pragma warning(suppress : 4996) // getenv: only read
#endif
    const char* pList = getenv("INDY_NOHOOK");
    if ( !pList )
    {
        return false;
    }

    size_t len = strlen(pName);
    for ( const char* p = pList; (p = strstr(p, pName)) != NULL; p += len )
    {
        if ( (p == pList || p[-1] == ',') && (p[len] == '\0' || p[len] == ',') )
        {
            return true;
        }
    }
    return false;
}

#define INDY_AB_ORIGINAL(func, ...)                                                         \
    do                                                                                      \
    {                                                                                       \
        static int bOriginal_ = -1;                                                         \
        if ( bOriginal_ < 0 )                                                               \
        {                                                                                   \
            bOriginal_ = J3DHookIsSkipped(#func);                                           \
        }                                                                                   \
        if ( bOriginal_ )                                                                   \
        {                                                                                   \
            return J3D_TRAMPOLINE_CALL(func, __VA_ARGS__);                                  \
        }                                                                                   \
    } while ( 0 )

#define INDY_AB_ORIGINAL_VOID(func, ...)                                                    \
    do                                                                                      \
    {                                                                                       \
        static int bOriginal_ = -1;                                                         \
        if ( bOriginal_ < 0 )                                                               \
        {                                                                                   \
            bOriginal_ = J3DHookIsSkipped(#func);                                           \
        }                                                                                   \
        if ( bOriginal_ )                                                                   \
        {                                                                                   \
            J3D_TRAMPOLINE_CALL(func, __VA_ARGS__);                                         \
            return;                                                                         \
        }                                                                                   \
    } while ( 0 )

/**
* @brief Declares a variable at a specific memory address.
*
* This macro is used to declare a variable of a given type at a predefined
* memory address. It's typically used for accessing variables in memory-mapped
* regions or in external processes.
*
* @param var_name The name of the variable (without the _ADDR suffix).
* @param var_type The type of the variable.
*
* @return A reference to the variable at the specified address.
*
* @note The address of the variable should be defined elsewhere as var_name_ADDR.
*/
#define J3D_DECL_FAR_VAR(var_name, var_type) \
    (*(var_type *)(var_name##_ADDR))

/**
* @brief Declares an array variable at a specific memory address.
*
* This macro is used to declare an array variable of a given type at a predefined
* memory address. It's typically used for accessing array variables in memory-mapped
* regions or in external processes.
*
* @param var_name The name of the array variable (without the _ADDR suffix).
* @param var_type The type of the array, including its dimensions.
*
* @return A reference to the array at the specified address.
*
* @note The address of the array should be defined elsewhere as var_name_ADDR.
*/
#define J3D_DECL_FAR_ARRAYVAR(var_name, var_type) \
    (*(var_type)(var_name##_ADDR))
#endif // J3D_STANDALONE

J3D_EXTERN_C_START

/**
 * @brief Structure to hold the context for hooking
 */
    typedef struct {
    DWORD oldProtect;
    uintptr_t startAddress;
    size_t size;
} J3DHookContext;

/**
 * @brief Begins the hooking context by changing memory protection to read-write
 *
 * @param pCtx The pointer to a J3DHookContext structure
 * @param startAddress The start address of the region to modify
 * @param endAddress The end address of the region to modify
 * @return true if successful, false otherwise
 */
inline bool J3DStartHookContext(J3DHookContext* pCtx, uintptr_t startAddress, uintptr_t endAddress)
{
    if ( endAddress <= startAddress ) {
        return false;
    }
    pCtx->startAddress = startAddress;
    pCtx->size = endAddress - startAddress;
#ifdef _WIN32 // native builds have no exe to patch
    return VirtualProtect((LPVOID)pCtx->startAddress, pCtx->size, PAGE_EXECUTE_READWRITE, &pCtx->oldProtect);
#else
    return false;
#endif
}

/**
 * @brief Ends the hooking context by restoring memory protection
 *
 * @param pCtx The pointer to a J3DHookContext structure
 * @return true if successful, false otherwise
 */
inline bool J3DEndHookContext(J3DHookContext* pCtx)
{
#ifdef _WIN32
    DWORD temp;
    return VirtualProtect((LPVOID)pCtx->startAddress, pCtx->size, pCtx->oldProtect, &temp);
#else
    (void)pCtx;
    return false;
#endif
}

#ifndef J3D_STANDALONE
/**
 * @brief Installs hook on function to redirect all calls to the new function
 *
 * @param pFuncAddr Address of the original function
 * @param pHookFunc The new function to redirect to
 * @return true if hooking was successful, false otherwise
 */
static bool J3DHookFunction(intptr_t pFuncAddr, void* pHookFunc)
{
    // INDY: unverified v1.2 addresses are generated as 0, and every function entry in the host exe is
    // 16-byte aligned: refuse anything else, so the original function keeps running (PROJECT.md §5.7)
    if ( pFuncAddr == 0 || (pFuncAddr & 0xF) != 0 )
    {
        return false;
    }

    if ( pFuncAddr == (intptr_t)pHookFunc )
    {
        printf("WARNING J3DHookFunction: Attempted to hook function at address %x to itself!\n", pFuncAddr);
        return false;
    }

    // Install hook in-place
    *(uint8_t*)(pFuncAddr) = 0xE9; // JMP opcode
    *(uint32_t*)(pFuncAddr + 1) = ((uintptr_t)pHookFunc - pFuncAddr - 5); // relative address to h

    // Flush instruction cache to ensure the CPU picks up the change
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)pFuncAddr, 5);

    return true;
}
#endif // J3D_STANDALONE

J3D_EXTERN_C_END
#endif //JONES3D_HOOK_H
