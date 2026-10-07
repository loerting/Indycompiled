project(smush C)

file(GLOB_RECURSE SOURCES *.c)
file(GLOB_RECURSE HEADERS *.h)
if (WIN32)
    list(FILTER SOURCES EXCLUDE REGEX "SDL\\.c$")
else()
    list(FILTER SOURCES EXCLUDE REGEX "Win32\\.c$")
endif()

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS}) # INDY: was OBJECT (header-only); static so the DLL links the player
target_link_libraries(${PROJECT_NAME} PUBLIC j3dcore std) # INDY: std: SmushPlay.h includes std headers (DirectX 6 SDK include dirs)
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC dsound winmm) # INDY: smushPlayWin32.c
else()
    target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3)
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Organize source files in Visual Studio
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})
