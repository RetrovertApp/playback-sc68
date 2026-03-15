# SC68 Playback Plugin
#
# This plugin uses SC68 to decode Atari ST and Amiga music formats (SNDH, SC68).
# Based on SC68 by Benjamin Gerard - http://sc68.atari.org/
#
# Supports:
#   - SC68 format (native SC68 music file format)
#   - SNDH format (Atari ST sound format)
#
# Build structure matches the original autoconf build:
#   - unice68 (static library)
#   - file68 (static library)
#   - emu68 (static library)
#   - io68 (static library)
#   - dial68 (static library)
#   - sc68_playback (shared library linking the above)

cmake_minimum_required(VERSION 3.16)
project(playback_sc68 C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PlaybackPlugin)

# Download SC68 3.0.0b
download_library(
    NAME sc68
    URL "https://github.com/Zeinok/sc68/archive/refs/tags/3.0.0b.tar.gz"
    FILENAME "sc68-3.0.0b.tar.gz"
    HASH "468f78560dd8db8723182e6098f15b3faaed8c7f3637efdcdbef1186832ffb38"
    EXTRACT_DIR "sc68-3.0.0b"
    PATCHES patches/scope-capture.patch
)

set(LIBSC68_ROOT "${sc68_SOURCE_DIR}/libsc68")
set(FILE68_ROOT "${sc68_SOURCE_DIR}/file68")
set(UNICE68_ROOT "${sc68_SOURCE_DIR}/unice68")

# Copy pre-generated file68_features.h (upstream uses autoconf to generate from .h.in)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/include/sc68/file68_features.h"
     DESTINATION "${FILE68_ROOT}/sc68")

# =============================================================================
# unice68 static library (ICE! decompression)
# Original: libunice68_la_CFLAGS = $(gb_CFLAGS)
# No special CPPFLAGS in original
# =============================================================================
add_library(unice68 STATIC
    ${UNICE68_ROOT}/unice68_unpack.c
    ${UNICE68_ROOT}/unice68_version.c
)
target_include_directories(unice68 PRIVATE
    ${UNICE68_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(unice68 PRIVATE HAVE_CONFIG_H)
if(NOT MSVC)
    target_compile_options(unice68 PRIVATE "-include${CMAKE_CURRENT_SOURCE_DIR}/config.h")
endif()
suppress_external_warnings(unice68)
set_target_properties(unice68 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# =============================================================================
# file68 static library (file I/O, message handling, etc.)
# Original: libfile68_la_CPPFLAGS = -I$(top_srcdir)/sc68
# =============================================================================
set(FILE68_SOURCES
    ${FILE68_ROOT}/src/endian68.c
    ${FILE68_ROOT}/src/error68.c
    ${FILE68_ROOT}/src/file68.c
    ${FILE68_ROOT}/src/gzip68.c
    ${FILE68_ROOT}/src/ice68.c
    ${FILE68_ROOT}/src/init68.c
    ${FILE68_ROOT}/src/msg68.c
    ${FILE68_ROOT}/src/option68.c
    ${FILE68_ROOT}/src/registry68.c
    ${FILE68_ROOT}/src/replay68.c
    ${FILE68_ROOT}/src/rsc68.c
    ${FILE68_ROOT}/src/string68.c
    ${FILE68_ROOT}/src/timedb68.c
    ${FILE68_ROOT}/src/uri68.c
    ${FILE68_ROOT}/src/vfs68.c
    ${FILE68_ROOT}/src/vfs68_ao.c
    ${FILE68_ROOT}/src/vfs68_curl.c
    ${FILE68_ROOT}/src/vfs68_fd.c
    ${FILE68_ROOT}/src/vfs68_file.c
    ${FILE68_ROOT}/src/vfs68_mem.c
    ${FILE68_ROOT}/src/vfs68_null.c
    ${FILE68_ROOT}/src/vfs68_z.c
)
add_library(file68 STATIC ${FILE68_SOURCES})
# Original: -I$(top_srcdir)/sc68 where top_srcdir is file68 root
target_include_directories(file68 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FILE68_ROOT}/sc68
    ${FILE68_ROOT}/src
    ${UNICE68_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(file68 PRIVATE
    HAVE_CONFIG_H
    FILE68_UNICE68=1
)
if(NOT MSVC)
    target_compile_options(file68 PRIVATE "-include${CMAKE_CURRENT_SOURCE_DIR}/config.h")
endif()
suppress_external_warnings(file68)
set_target_properties(file68 PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(file68 PRIVATE unice68)

# =============================================================================
# emu68 static library (68000 CPU emulator)
# Original: libemu68_la_CPPFLAGS = -I$(top_srcdir)
# Original uses split line files (default, not monolithic)
# Compile command: gcc -DHAVE_CONFIG_H -I. -I.. -I../../file68
# =============================================================================
set(EMU68_SOURCES
    ${LIBSC68_ROOT}/emu68/emu68.c
    ${LIBSC68_ROOT}/emu68/error68.c
    ${LIBSC68_ROOT}/emu68/getea68.c
    ${LIBSC68_ROOT}/emu68/inst68.c
    ${LIBSC68_ROOT}/emu68/ioplug68.c
    ${LIBSC68_ROOT}/emu68/mem68.c
    # Split line files (default mode, not monolithic)
    ${LIBSC68_ROOT}/emu68/line0_68.c
    ${LIBSC68_ROOT}/emu68/line1_68.c
    ${LIBSC68_ROOT}/emu68/line2_68.c
    ${LIBSC68_ROOT}/emu68/line3_68.c
    ${LIBSC68_ROOT}/emu68/line4_68.c
    ${LIBSC68_ROOT}/emu68/line5_68.c
    ${LIBSC68_ROOT}/emu68/line6_68.c
    ${LIBSC68_ROOT}/emu68/line7_68.c
    ${LIBSC68_ROOT}/emu68/line8_68.c
    ${LIBSC68_ROOT}/emu68/line9_68.c
    ${LIBSC68_ROOT}/emu68/lineA_68.c
    ${LIBSC68_ROOT}/emu68/lineB_68.c
    ${LIBSC68_ROOT}/emu68/lineC_68.c
    ${LIBSC68_ROOT}/emu68/lineD_68.c
    ${LIBSC68_ROOT}/emu68/lineE_68.c
    ${LIBSC68_ROOT}/emu68/lineF_68.c
    ${LIBSC68_ROOT}/emu68/table68.c
)
add_library(emu68 STATIC ${EMU68_SOURCES})
# Match original: -I. -I.. -I../../file68
target_include_directories(emu68 PRIVATE
    ${LIBSC68_ROOT}/emu68
    ${LIBSC68_ROOT}
    ${FILE68_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(emu68 PRIVATE HAVE_CONFIG_H)
if(NOT MSVC)
    target_compile_options(emu68 PRIVATE "-include${CMAKE_CURRENT_SOURCE_DIR}/config.h")
endif()
suppress_external_warnings(emu68)
set_target_properties(emu68 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# =============================================================================
# io68 static library (Sound chip emulators: YM2149, Paula, MFP, etc.)
# Original: libio68_la_CPPFLAGS = -I$(top_srcdir) $(file68_CPPFLAGS)
# where top_srcdir is libsc68 root
# =============================================================================
set(IO68_SOURCES
    ${LIBSC68_ROOT}/io68/io68.c
    ${LIBSC68_ROOT}/io68/mfpemul.c
    ${LIBSC68_ROOT}/io68/mfp_io.c
    ${LIBSC68_ROOT}/io68/mwemul.c
    ${LIBSC68_ROOT}/io68/mw_io.c
    ${LIBSC68_ROOT}/io68/paulaemul.c
    ${LIBSC68_ROOT}/io68/paula_io.c
    ${LIBSC68_ROOT}/io68/shifter_io.c
    ${LIBSC68_ROOT}/io68/ymemul.c
    ${LIBSC68_ROOT}/io68/ym_blep.c
    ${LIBSC68_ROOT}/io68/ym_dump.c
    ${LIBSC68_ROOT}/io68/ym_envel.c
    ${LIBSC68_ROOT}/io68/ym_io.c
    ${LIBSC68_ROOT}/io68/ym_puls.c
)
add_library(io68 STATIC ${IO68_SOURCES})
# Original: -I$(top_srcdir) $(file68_CPPFLAGS)
target_include_directories(io68 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIBSC68_ROOT}
    ${FILE68_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(io68 PRIVATE HAVE_CONFIG_H)
if(NOT MSVC)
    target_compile_options(io68 PRIVATE "-include${CMAKE_CURRENT_SOURCE_DIR}/config.h")
endif()
suppress_external_warnings(io68)
set_target_properties(io68 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# =============================================================================
# dial68 static library (dialogs - without Windows UI)
# Original: libdial68_la_CPPFLAGS = -I$(top_srcdir) $(file68_CPPFLAGS)
# =============================================================================
set(DIAL68_SOURCES
    ${LIBSC68_ROOT}/dial68/dial68.c
    ${LIBSC68_ROOT}/dial68/dial_conf.c
    ${LIBSC68_ROOT}/dial68/dial_finf.c
    ${LIBSC68_ROOT}/dial68/dial_tsel.c
)
add_library(dial68 STATIC ${DIAL68_SOURCES})
# Original: -I$(top_srcdir) $(file68_CPPFLAGS)
target_include_directories(dial68 PRIVATE
    ${LIBSC68_ROOT}
    ${FILE68_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_definitions(dial68 PRIVATE HAVE_CONFIG_H)
if(NOT MSVC)
    target_compile_options(dial68 PRIVATE "-include${CMAKE_CURRENT_SOURCE_DIR}/config.h")
endif()
suppress_external_warnings(dial68)
set_target_properties(dial68 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# =============================================================================
# sc68_playback shared library (the final plugin)
# Original: libsc68_la_CPPFLAGS = -I$(top_srcdir)/sc68 $(file68_CPPFLAGS)
# =============================================================================
set(SC68_API_SOURCES
    ${LIBSC68_ROOT}/src/api68.c
    ${LIBSC68_ROOT}/src/conf68.c
    ${LIBSC68_ROOT}/src/libsc68.c
    ${LIBSC68_ROOT}/src/mixer68.c
)

add_library(sc68_playback SHARED
    sc68_plugin.c
    ${SC68_API_SOURCES}
)

# Original: -I$(top_srcdir)/sc68 $(file68_CPPFLAGS)
# Plus plugin include path for replay API
target_include_directories(sc68_playback PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIBSC68_ROOT}/sc68
    ${LIBSC68_ROOT}
    ${FILE68_ROOT}
    ${UNICE68_ROOT}
)

target_compile_definitions(sc68_playback PRIVATE
    HAVE_CONFIG_H
    PACKAGE="sc68"
    PACKAGE_NAME="libsc68"
    PACKAGE_VERSION="3.0.0"
    VERSION="3.0.0"
    FILE68_UNICE68=1
)

if(NOT MSVC)
    target_compile_options(sc68_playback PRIVATE "-include${CMAKE_CURRENT_SOURCE_DIR}/config.h")
endif()

# Link all the static libraries
target_link_libraries(sc68_playback PRIVATE
    file68
    emu68
    io68
    dial68
    unice68
)

rv_set_plugin_properties(sc68_playback)
