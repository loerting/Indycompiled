# Native build (Linux, Android): the engine with SDL3 for window, input and sound, and OpenGL ES 3 for rendering.
# No Win32 and no Indy3D.exe. Included from the top-level CMakeLists.txt when the target isn't Windows; the module
# CMakeLists.txt files pick their SDL/native sources (J3D_GLES).

cmake_policy(SET CMP0079 NEW) # link libraries to targets of other directories (Indycompiled block below)
set(JONES3D_STANDALONE ON)
add_compile_definitions(J3D_STANDALONE _GNU_SOURCE)

# SDL3 (vendored, static), built before the engine's targets and without the engine's compile options
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_SOURCE_DIR}/Libs/external/SDL3" "${CMAKE_BINARY_DIR}/SDL3" EXCLUDE_FROM_ALL)

add_compile_options(
    -fms-extensions -fno-strict-aliasing -fwrapv    # MSVC semantics the engine relies on
    -fshort-wchar # 16-bit wchar_t: the Windows layout of structs and savegames (wide functions: std/Posix/stdCrtCompat.c)
    "$<$<COMPILE_LANGUAGE:C>:SHELL:-include ${CMAKE_SOURCE_DIR}/cmake/compat/indy_prelude.h>"
    -Wno-microsoft -Wno-pragma-pack -Wno-unknown-pragmas -Wno-ignored-attributes
    -Wno-error=incompatible-pointer-types -Wno-error=incompatible-function-pointer-types
)
include_directories(BEFORE "${CMAKE_SOURCE_DIR}/cmake/compat/native") # <Windows.h>, <dinput.h>: the engine's Win32 type shim

# --- Indycompiled: our module (Libs/indy) and external definitions for header inline functions ------------------
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(INDY_GEN "${CMAKE_BINARY_DIR}/indy")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/Scripts/indy/gen_inline_externals.py"
            "${CMAKE_SOURCE_DIR}" "${INDY_GEN}/indy_inline_externals.c"
    RESULT_VARIABLE _indy_rc OUTPUT_QUIET)
if(NOT _indy_rc EQUAL 0)
    message(FATAL_ERROR "gen_inline_externals.py failed")
endif()

function(_indy_native_fixup)
    file(GLOB indy_sources "${CMAKE_SOURCE_DIR}/Libs/indy/*.c")
    list(FILTER indy_sources EXCLUDE REGEX "(indyDiff|indyDisplayDX9|indyDebug)\\.c$") # exe code, DirectX 9, Win32
    target_sources(Jones3D_DLL PRIVATE ${indy_sources} "${INDY_GEN}/indy_inline_externals.c")
    target_link_libraries(Jones3D_DLL PRIVATE m)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _indy_native_fixup)
