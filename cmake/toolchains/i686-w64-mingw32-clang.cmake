# Cross toolchain: clang + lld targeting 32-bit Windows (MinGW-w64 headers, CRT and libgcc from the
# Arch package mingw-w64-gcc). PROJECT.md §5.2. Builds Jones3D.exe/Jones3D.dll on Linux, no MSVC.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

set(INDY_MINGW_TRIPLE i686-w64-mingw32)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET ${INDY_MINGW_TRIPLE})
set(CMAKE_CXX_COMPILER_TARGET ${INDY_MINGW_TRIPLE})
set(CMAKE_RC_COMPILER llvm-rc)

foreach(kind EXE SHARED MODULE)
    set(CMAKE_${kind}_LINKER_FLAGS_INIT "-fuse-ld=lld")
endforeach()

# clang locates the MinGW-w64 sysroot itself (via i686-w64-mingw32-gcc); CMake only needs it for find_*()
set(CMAKE_FIND_ROOT_PATH /usr/${INDY_MINGW_TRIPLE})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
