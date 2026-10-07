project(std C)

file(GLOB_RECURSE SOURCES *.c)
file(GLOB_RECURSE HEADERS *.h)

# Remove DirectX 6/9 specific files based on the build option
if(J3D_DIRECTX6)
  # Remove DX9 files.
    list(FILTER SOURCES EXCLUDE REGEX ".*DX9\\.c$")
    list(FILTER SOURCES EXCLUDE REGEX "stdComm.c$")
    list(FILTER SOURCES EXCLUDE REGEX "/SDL/|/Posix/")

    set(SHADERS "")
    set(SHADER_INCLUDES "")
elseif(J3D_DIRECTX9)
    # Remove DX6 files.
    list(FILTER SOURCES EXCLUDE REGEX ".*DX6\\.c$")
    list(FILTER SOURCES EXCLUDE REGEX "/SDL/|/Posix/")

    # Collect shader files
    set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Win95/DX9/Shaders")
    file(GLOB SHADERS
        "${SHADER_DIR}/*.hlsl"
    )
    file(GLOB SHADER_INCLUDES
        "${SHADER_DIR}/*.hlsli"
    )
elseif(J3D_GLES)
    # Native build (SDL3, OpenGL ES 3): no DirectX, DirectPlay or Win32 console; SDL/ and Posix/ replace the rest
    list(FILTER SOURCES EXCLUDE REGEX ".*DX6\\.c$|/DX9/(std3DX9|stdDisplayDX9|stdShaderDX9)\\.c$|/stdComm\\.c$|/stdConsole\\.c$|/General/stdPlatform\\.c$")
    # stdControlDX9.c is shared: DirectInput and XInput run on SDL3 (SDL/stdDInputSDL.c, SDL/stdXInputSDL.c)
endif()

# Make project target
add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS} ${SHADERS} ${SHADER_INCLUDES})

# Add global includes
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Link libraries
target_link_libraries(${PROJECT_NAME} PUBLIC j3dcore)
if (WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC Winmm) # timeGetTime
endif()

set(EXTRA_LIBS
    jansson::jansson
)

if (J3D_DIRECTX6)
    target_link_libraries(${PROJECT_NAME} PUBLIC
        DirectX61c::ddraw
        DirectX61c::dxguid
        DirectX61c::dinput
        DirectX61c::dplayx
        ${EXTRA_LIBS}
    )
elseif(J3D_DIRECTX9) 
    target_link_libraries(${PROJECT_NAME} PUBLIC
        d3d9
        dxguid
        dinput8
        ${EXTRA_LIBS}
    )

    # Define HLSL shaders to be compiled
    foreach(SHADER ${SHADERS})
        get_filename_component(FILE_NAME ${SHADER} NAME_WE) # name without extension
        set(HEADER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Win95/DX9/Shaders/std_${FILE_NAME}.h")

        # Get shader type from filename
        set(SHADER_MODEL "3.0")
        if(FILE_NAME MATCHES "_vs$")
            set(SHADER_TYPE "Vertex")
        elseif(FILE_NAME MATCHES "_ps$")
            set(SHADER_TYPE "Pixel")
        else()
            message(FATAL_ERROR "Unknown shader type for ${FILE_NAME} - expected _vs or _ps suffix!")
        endif()

        # Adjust shader compilation properties
        set_source_files_properties(${SHADER} PROPERTIES
            VS_SHADER_TYPE ${SHADER_TYPE}
            VS_SHADER_MODEL ${SHADER_MODEL}
            VS_SHADER_ENTRYPOINT "main"
            VS_SHADER_OUTPUT_HEADER_FILE "${HEADER_FILE}"
            VS_SHADER_OBJECT_FILE_NAME ""
            VS_SHADER_VARIABLE_NAME "std_${FILE_NAME}"
        )

        # Add dependency so headers are built before your game compiles
        list(APPEND GENERATED_HEADERS ${HEADER_FILE})
    endforeach()
endif()

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC
        dbghelp # for minidump & stack trace
    )
endif()

if (J3D_GLES)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${EXTRA_LIBS} SDL3::SDL3)
endif()

if (WIN32)
target_link_options(${PROJECT_NAME} INTERFACE "/SAFESEH:NO") # due to DirectX 6.1 libs
endif()

# Organize source files in Visual Studio
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS} ${SHADERS} ${SHADER_INCLUDES} ${GENERATED_HEADERS})
