cmake_minimum_required(VERSION 3.21)
project(kof98 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# The neogeorecomp toolkit: the in-tree copy, or a checkout next to this
# project.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/neogeorecomp/CMakeLists.txt)
    set(ng_default ${CMAKE_CURRENT_SOURCE_DIR}/neogeorecomp)
else()
    set(ng_default ${CMAKE_CURRENT_SOURCE_DIR}/../neogeorecomp)
endif()
set(NEOGEORECOMP_DIR ${ng_default} CACHE PATH "neogeorecomp checkout")
add_subdirectory(${NEOGEORECOMP_DIR} neogeorecomp_build)

# Your unzipped MAME "kof98" set and "neogeo" system ROMs.  The build
# decrypts the program into ${CMAKE_BINARY_DIR}/roms; nothing derived from
# it is committed.  Point this at an already-decrypted directory (one with
# 242-pd.bin) and the decrypt step is skipped.
set(KOF98_ROM_DIR "" CACHE PATH "Directory with your kof98 and Neo Geo BIOS ROM files")
# Optional: reuse generated C produced by another build of this project.
set(KOF98_PREGENERATED "" CACHE PATH "Existing generated/ directory; skip the recompiler")
# Optional: entry points a previous run found at runtime (--dump-misses).
set(KOF98_ENTRIES "${CMAKE_BINARY_DIR}/entries.txt" CACHE FILEPATH "Extra entry points for the recompiler")
set(KOF98_GEN_FILES 16 CACHE STRING "Number of recompiled C translation units")

# Stage 0: ROM decryption (PROGSF1 program).
add_executable(kof98_decrypt tools/decrypt.c)

# Stage 1: the generator.
add_executable(kof98_recomp src/recomp_main.c src/game.c)
target_link_libraries(kof98_recomp PRIVATE m68krecomp_core)
target_include_directories(kof98_recomp PRIVATE src)

set(GEN_DIR ${CMAKE_BINARY_DIR}/generated)
set(HAVE_GENERATED 0)

if(KOF98_PREGENERATED)
    set(GEN_DIR ${KOF98_PREGENERATED})
    set(HAVE_GENERATED 1)
    if(KOF98_ROM_DIR)
        set(DEC_DIR ${KOF98_ROM_DIR})
    endif()
elseif(KOF98_ROM_DIR)
    if(EXISTS "${KOF98_ROM_DIR}/242-pd.bin")
        set(DEC_DIR ${KOF98_ROM_DIR})
    else()
        set(DEC_DIR ${CMAKE_BINARY_DIR}/roms)
        set(DEC_STAMP ${DEC_DIR}/.decrypted)
        add_custom_command(
            OUTPUT ${DEC_STAMP}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${DEC_DIR}
            COMMAND kof98_decrypt --rom-path ${KOF98_ROM_DIR} --out ${DEC_DIR}
            COMMAND ${CMAKE_COMMAND} -E touch ${DEC_STAMP}
            DEPENDS kof98_decrypt
            COMMENT "Decrypting KOF '98 ROMs (PROGSF1)")
    endif()

    set(GEN_SOURCES ${GEN_DIR}/recomp_table.c)
    math(EXPR last "${KOF98_GEN_FILES} - 1")
    foreach(i RANGE ${last})
        if(i LESS 10)
            list(APPEND GEN_SOURCES ${GEN_DIR}/recomp_0${i}.c)
        else()
            list(APPEND GEN_SOURCES ${GEN_DIR}/recomp_${i}.c)
        endif()
    endforeach()
    if(NOT EXISTS ${KOF98_ENTRIES})
        file(WRITE ${KOF98_ENTRIES} "")
    endif()
    file(MAKE_DIRECTORY ${GEN_DIR})
    add_custom_command(
        OUTPUT ${GEN_SOURCES} ${GEN_DIR}/recomp_common.h
        COMMAND kof98_recomp --rom-path ${DEC_DIR} --out ${GEN_DIR} --files ${KOF98_GEN_FILES}
                --entries ${KOF98_ENTRIES}
        DEPENDS kof98_recomp ${KOF98_ENTRIES} ${DEC_STAMP}
        COMMENT "Recompiling KOF '98 from ${DEC_DIR}")
    set(HAVE_GENERATED 1)
endif()

set(GAME_SOURCES src/main.c src/rom_select.c)
if(HAVE_GENERATED)
    set(GEN_SOURCES ${GEN_DIR}/recomp_table.c)
    math(EXPR last "${KOF98_GEN_FILES} - 1")
    foreach(i RANGE ${last})
        if(i LESS 10)
            list(APPEND GEN_SOURCES ${GEN_DIR}/recomp_0${i}.c)
        else()
            list(APPEND GEN_SOURCES ${GEN_DIR}/recomp_${i}.c)
        endif()
    endforeach()
    list(APPEND GAME_SOURCES ${GEN_SOURCES})
endif()

add_executable(kof98 ${GAME_SOURCES} src/game.c)
set_target_properties(kof98 PROPERTIES OUTPUT_NAME "KOF98")
target_link_libraries(kof98 PRIVATE neogeorecomp)
target_include_directories(kof98 PRIVATE src ${GEN_DIR})
if(WIN32)
    target_link_libraries(kof98 PRIVATE ole32 shell32)
endif()
if(HAVE_GENERATED)
    target_compile_definitions(kof98 PRIVATE KOF98_HAVE_RECOMP)
    if(MSVC)
        set_source_files_properties(${GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "/bigobj;/O1")
    else()
        set_source_files_properties(${GEN_SOURCES} PROPERTIES COMPILE_OPTIONS "-O1")
    endif()
endif()

if(KOF98_ROM_DIR AND NOT KOF98_PREGENERATED)
    add_custom_target(profile
        COMMAND kof98 --headless --rom-path ${DEC_DIR} --frames 6000 --dump-misses ${KOF98_ENTRIES}
        COMMAND kof98 --headless --rom-path ${DEC_DIR} --frames 19000
                --input ${CMAKE_CURRENT_SOURCE_DIR}/tests/play.txt --dump-misses ${KOF98_ENTRIES}
        DEPENDS kof98
        COMMENT "Profiling a scripted KOF '98 session for runtime-only entry points")
endif()

if(MSVC)
    target_compile_definitions(kof98 PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(kof98_recomp PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(kof98_decrypt PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(KOF98_ROM_DIR AND Python3_Interpreter_FOUND)
    add_custom_target(audio-check
        COMMAND ${CMAKE_COMMAND} -E env "NG_RTC=1998-01-01"
                $<TARGET_FILE:kof98> --headless --rom-path ${DEC_DIR}
                --frames 12000 --input ${CMAKE_CURRENT_SOURCE_DIR}/tests/play.txt
                --wav ${CMAKE_BINARY_DIR}/audio-check.wav
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-audio.py
                ${CMAKE_BINARY_DIR}/audio-check.wav --from 10 --max-silence 3
        DEPENDS kof98
        COMMENT "Checking KOF '98 audio through select and the match")
endif()

# Ship the SDL2 DLL (when the window is built) next to the executable.
if(WIN32 AND TARGET SDL2::SDL2)
    add_custom_command(TARGET kof98 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:kof98> $<TARGET_FILE_DIR:kof98>
        COMMAND_EXPAND_LISTS)
endif()
