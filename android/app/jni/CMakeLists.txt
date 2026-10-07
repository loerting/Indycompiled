# Native part of the Indycompiled Android app.
#   libSDL3.so  - vendored SDL3 (Libs/external/SDL3), the platform layer
#   libmain.so  - the app (demo_main.c); SDLActivity loads it and calls SDL_main() on its own thread
# Built by the Android Gradle plugin (externalNativeBuild) once per ABI.
cmake_minimum_required(VERSION 3.22)
project(indycompiled_android C CXX)

set(INDY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

# Cap the parallel compile jobs per ABI build: the machine runs other jobs at the same time.
set(INDY_NINJA_JOBS 4 CACHE STRING "Parallel compile jobs (ninja job pool size)")
set_property(GLOBAL APPEND PROPERTY JOB_POOLS indy_compile=${INDY_NINJA_JOBS} indy_link=2)
set(CMAKE_JOB_POOL_COMPILE indy_compile)
set(CMAKE_JOB_POOL_LINK indy_link)

# SDL3 as a shared library (SDLActivity loads "SDL3" then "main"); no tests, examples or install rules.
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${INDY_ROOT}/Libs/external/SDL3" SDL3)

# Demo: plays the intro movie with the platform-neutral SMUSH decoder
add_library(main SHARED demo_main.c "${INDY_ROOT}/Libs/smush/smushDecoder.c")
target_include_directories(main PRIVATE "${INDY_ROOT}/Libs/smush")
target_compile_options(main PRIVATE -Wall)
target_link_libraries(main PRIVATE SDL3::SDL3)
