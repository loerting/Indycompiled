# Native 64-bit Linux build (Stage 5): clang for the host (x86_64). The engine's on-disk formats are read through
# fixed-width structs, so savegames and level files stay compatible with the 32-bit builds.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
