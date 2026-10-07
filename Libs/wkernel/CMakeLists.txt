project(wkernel C)

file(GLOB_RECURSE SOURCES *.c)
file(GLOB_RECURSE HEADERS *.h)
if (WIN32)
    list(FILTER SOURCES EXCLUDE REGEX "SDL\\.c$")
else()
    list(FILTER SOURCES EXCLUDE REGEX "/wkernel\\.c$")
endif()

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(${PROJECT_NAME} PUBLIC j3dcore std)
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC Comctl32.lib)
else()
    target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3)
endif()

# Organize source files in Visual Studio
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})

