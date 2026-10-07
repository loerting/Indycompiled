# Indycompiled: build OpenJones3D with clang + MinGW-w64 on Linux, hosted by Indy3D.exe v1.2 (PROJECT.md §5).
# Included from upstream's top-level CMakeLists.txt for non-MSVC compilers. Upstream files stay untouched apart
# from INDY-marked lines; everything else happens here.

set(INDY_ROOT "${CMAKE_SOURCE_DIR}")
set(INDY_GEN "${CMAKE_BINARY_DIR}/indy")
set(INDY_HOST_EXE "${INDY_ROOT}/game/original/Resource/Indy3D.exe" CACHE FILEPATH "Host Indy3D.exe (v1.2)")
set(INDY_HOST_EXE_SHA256 4075e655e0cf0db2d352265ba83a19a51fb373156cba8b3e43107fd6c6129ebb)
set(INDY_ADDRESS_MAP "${INDY_ROOT}/Scripts/indy/rti_v12.csv")
set(INDY_O0_TARGETS "" CACHE STRING "Debug aid: targets compiled with -O0 in optimised builds")
set(INDY_O0_SOURCES "" CACHE STRING "Debug aid: source globs (repo-relative) compiled with -O0 in optimised builds")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# --- Stage 4: standalone Jones3D.exe -------------------------------------------------------------------------
# ON: Jones3D.exe is the game itself. Indy3D.exe's code is never loaded: no injection, no hooks, the exe's globals are
# our own (j3dhook.h). Indy3D.exe is only read at run time, as a data file, for its dialog and icon resources.
# The define is a compile definition rather than part of the generated j3d.h, which both build directories share.
option(JONES3D_STANDALONE "Jones3D.exe is the game, without Indy3D.exe's code (still a Windows build)" OFF)
if(JONES3D_STANDALONE)
    add_compile_definitions(J3D_STANDALONE)
else()
    # --- host exe ---------------------------------------------------------------------------------------------
    if(NOT EXISTS "${INDY_HOST_EXE}")
        message(FATAL_ERROR "Host exe not found: ${INDY_HOST_EXE} (set INDY_HOST_EXE)")
    endif()
    file(SHA256 "${INDY_HOST_EXE}" _indy_hash)
    if(NOT _indy_hash STREQUAL INDY_HOST_EXE_SHA256)
        message(FATAL_ERROR "${INDY_HOST_EXE} is not Indy3D.exe v1.2 (sha256 ${_indy_hash})")
    endif()
    add_compile_definitions(INDY_HOST_EXE_SHA256="${INDY_HOST_EXE_SHA256}") # launcher hash check (exemain.cpp)

    # --- v1.2 address headers (generated at configure time; regenerated when the map changes) ------------------
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -I "${INDY_ROOT}/Scripts/indy/gen_rti_headers.py"
                "${INDY_ROOT}" "${INDY_ADDRESS_MAP}" "${INDY_HOST_EXE}" "${INDY_GEN}/rti"
        RESULT_VARIABLE _indy_rc OUTPUT_VARIABLE _indy_out ERROR_VARIABLE _indy_err
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT _indy_rc EQUAL 0)
        message(FATAL_ERROR "gen_rti_headers.py failed: ${_indy_err}")
    endif()
    message(STATUS "${_indy_out}")
    file(GLOB _indy_rti_sources "${INDY_ROOT}/Libs/*/RTI/addresses.h")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${INDY_ADDRESS_MAP}" "${INDY_ROOT}/Scripts/indy/gen_rti_headers.py" ${_indy_rti_sources}
        "${INDY_ROOT}/Jones3D/RTI/addresses.h" "${INDY_ROOT}/Libs/smush/SmushPlay.h")
endif()

# external definitions for upstream's header inline functions (MSVC vs. C99 inline semantics)
execute_process(
    COMMAND "${Python3_EXECUTABLE}" -I "${INDY_ROOT}/Scripts/indy/gen_inline_externals.py"
            "${INDY_ROOT}" "${INDY_GEN}/indy_inline_externals.c"
    RESULT_VARIABLE _indy_rc OUTPUT_VARIABLE _indy_out OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _indy_rc EQUAL 0)
    message(FATAL_ERROR "gen_inline_externals.py failed")
endif()
message(STATUS "${_indy_out}")

# generated v1.2 headers first, then lowercase aliases for MSVC-style system header names
include_directories(BEFORE "${INDY_GEN}/rti" "${INDY_ROOT}/cmake/compat/include")

# --- compiler and linker ------------------------------------------------------------------------------------
add_compile_options(
    -fms-extensions -fasm-blocks                    # MSVC-style __asm and extensions used upstream
    -fno-strict-aliasing -fwrapv                    # MSVC semantics: no type-based alias analysis, no signed-overflow UB
    "$<$<COMPILE_LANGUAGE:C>:SHELL:-include ${INDY_ROOT}/cmake/compat/indy_prelude.h>"
    -Wno-microsoft -Wno-pragma-pack -Wno-unknown-pragmas -Wno-ignored-attributes
    # MSVC accepts these mismatches (e.g. int- vs. void-returning callbacks; same cdecl ABI): warn, don't fail
    -Wno-error=incompatible-pointer-types -Wno-error=incompatible-function-pointer-types
)
add_link_options(-static)  # libgcc, winpthread and libstdc++ into the binaries: no extra DLLs next to the game

# Libraries requested by "#pragma comment(lib, ...)" with MSVC capitalisation: the linker looks for lib<Name>.a,
# MinGW-w64's are lowercase. Provide correctly named copies (Xinput.lib = XINPUT1_4.dll, as in the Windows SDK).
foreach(_indy_alias "Xinput:xinput1_4")
    string(REPLACE ":" ";" _indy_pair "${_indy_alias}")
    list(GET _indy_pair 0 _indy_name)
    list(GET _indy_pair 1 _indy_real)
    find_library(_indy_lib_${_indy_real} NAMES ${_indy_real} REQUIRED)
    configure_file("${_indy_lib_${_indy_real}}" "${INDY_GEN}/lib/lib${_indy_name}.a" COPYONLY)
endforeach()
link_directories("${INDY_GEN}/lib")

# --- fix-ups once upstream's targets exist --------------------------------------------------------------------
function(_indy_collect_targets dir out)
    get_property(targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach(sub IN LISTS subdirs)
        _indy_collect_targets("${sub}" sub_targets)
        list(APPEND targets ${sub_targets})
    endforeach()
    set(${out} ${targets} PARENT_SCOPE)
endfunction()

function(_indy_fixup_targets)
    _indy_collect_targets("${CMAKE_SOURCE_DIR}" targets)
    foreach(t IN LISTS targets)
        get_target_property(type ${t} TYPE)
        set(props INTERFACE_LINK_LIBRARIES INTERFACE_LINK_OPTIONS)
        if(NOT type STREQUAL "INTERFACE_LIBRARY")
            list(APPEND props LINK_LIBRARIES)
        endif()
        foreach(prop IN LISTS props)
            get_target_property(values ${t} ${prop})
            if(values)
                # MinGW-w64 import libraries are lowercase; drop the MSVC-only /SAFESEH:NO
                list(TRANSFORM values REPLACE "^Comctl32(\\.lib)?$" "comctl32")
                list(TRANSFORM values REPLACE "^Winmm$" "winmm")
                list(REMOVE_ITEM values "/SAFESEH:NO")
                set_property(TARGET ${t} PROPERTY ${prop} "${values}")
            endif()
        endforeach()
    endforeach()

    target_sources(Jones3D_DLL PRIVATE "${INDY_GEN}/indy_inline_externals.c")

    # our own module (PROJECT.md §7.2): enhancement toggles and other Indycompiled code
    file(GLOB indy_sources "${INDY_ROOT}/Libs/indy/*.c")
    if(JONES3D_STANDALONE)
        list(FILTER indy_sources EXCLUDE REGEX "indyDiff\\.c$") # differential tests run the exe's code
    endif()
    target_sources(Jones3D_DLL PRIVATE ${indy_sources})

    # debug aid: compile the listed targets without optimisation (e.g. to bisect optimisation-only bugs)
    foreach(t IN LISTS INDY_O0_TARGETS)
        set_property(TARGET ${t} APPEND PROPERTY COMPILE_OPTIONS -O0)
    endforeach()
    foreach(pattern IN LISTS INDY_O0_SOURCES)   # globs relative to the repo root, e.g. Libs/sith/Cog/*.c
        file(GLOB files "${INDY_ROOT}/${pattern}")
        foreach(t IN LISTS targets)
            get_target_property(type ${t} TYPE)
            if(NOT type STREQUAL "INTERFACE_LIBRARY")
                set_source_files_properties(${files} TARGET_DIRECTORY ${t} PROPERTIES COMPILE_OPTIONS -O0)
            endif()
        endforeach()
    endforeach()

    # the launcher injects "Jones3D.dll" (MinGW would name it libJones3D.dll)
    set_target_properties(Jones3D_DLL PROPERTIES PREFIX "" IMPORT_PREFIX "")
    if(NOT JONES3D_STANDALONE) # standalone: no launcher, Jones3D_DLL is Jones3D.exe
        # the launcher is a Unicode program with wmain() (Visual Studio's "Unicode" character set)
        target_compile_definitions(Jones3D PRIVATE UNICODE)
        target_link_options(Jones3D PRIVATE -municode)
        set_property(TARGET Jones3D APPEND PROPERTY LINK_LIBRARIES pthread)  # MinGW's static libstdc++ is built on winpthreads
    endif()

    # DirectX 9 shaders: vkd3d-compiler instead of Visual Studio's fxc (PROJECT.md §5.3)
    if(J3D_DIRECTX9)
        set(shader_dir "${INDY_ROOT}/Libs/std/Win95/DX9/Shaders")
        file(GLOB shaders "${shader_dir}/*.hlsl")
        file(GLOB shader_includes "${shader_dir}/*.hlsli")
        set(outputs)
        foreach(src IN LISTS shaders)
            get_filename_component(name "${src}" NAME_WE)
            if(name MATCHES "_vs$")
                set(profile vs_3_0)
            else()
                set(profile ps_3_0)
            endif()
            set(out "${INDY_GEN}/shaders/Shaders/std_${name}.h")
            add_custom_command(OUTPUT "${out}"
                COMMAND "${Python3_EXECUTABLE}" -I "${INDY_ROOT}/Scripts/indy/compile_shader.py"
                        "${src}" ${profile} "std_${name}" "${out}"
                DEPENDS "${src}" ${shader_includes} "${INDY_ROOT}/Scripts/indy/compile_shader.py"
                COMMENT "Compiling shader ${name}.hlsl (${profile})" VERBATIM)
            list(APPEND outputs "${out}")
        endforeach()
        add_custom_target(indy_dx9_shaders DEPENDS ${outputs})
        add_dependencies(std indy_dx9_shaders)
        target_include_directories(std PRIVATE "${INDY_GEN}/shaders")
    endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _indy_fixup_targets)
