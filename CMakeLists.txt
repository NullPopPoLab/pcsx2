cmake_policy(SET CMP0048 NEW)

# Project Name
project(Pcsx2)

# Debian-based distributions require at least 2.8.5 due to multiarch.
# Bumping up to 3.0 seems reasonable at this point, and will let us modernize
# things a bit.
#
# Setting it to a range tells it that it supports the features on the newer
# versions as well, avoiding setting policies.
cmake_minimum_required(VERSION 3.3.0...3.17)

# Variable to check that people use the good file
set(TOP_CMAKE_WAS_SOURCED TRUE)

# set module path
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# include some generic functions to ensure correctness of the env
include(Pcsx2Utils)

check_no_parenthesis_in_path()
detectOperatingSystem()
check_compiler_version("7.0" "7.0")
if(NOT MSVC)
   set(CMAKE_C_COMPILER_LAUNCHER ccache)
   set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif()

#-------------------------------------------------------------------------------
# Include specific module
# BuildParameters Must be done before SearchForStuff
include(BuildParameters)
# SearchForStuff be done before SelectPcsx2Plugins
include(SearchForStuff)
include(SelectPcsx2Plugins)

# Must be done after SearchForStuff
get_git_version_info()
write_svnrev_h()
set(CMAKE_BUILD_PO FALSE)
if (LIBRETRO)
    set(BUILD_REPLAY_LOADERS FALSE)
    add_definitions(-D__LIBRETRO__ -DDISABLE_RECORDING -DwxUSE_GUI=0)
endif()

if(NOT NO_TRANSLATION)
# make the translation
if(EXISTS "${CMAKE_SOURCE_DIR}/locales")
    add_subdirectory(locales)
endif()
endif()

add_subdirectory(3rdparty/wxwidgets3.0)

# make common
if(common_libs)
    add_subdirectory(common/src/Utilities)
    add_subdirectory(common/src/x86emitter)
endif()

# make pcsx2
if(EXISTS "${CMAKE_SOURCE_DIR}/pcsx2" AND pcsx2_core)
    add_subdirectory(pcsx2)
endif()

# make plugins
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins")
    add_subdirectory(plugins)
endif()

#-------------------------------------------------------------------------------

# Install some files to ease package creation
if(PACKAGE_MODE)
    if(NOT DISABLE_CHEATS_ZIP)
        INSTALL(FILES     "${CMAKE_SOURCE_DIR}/bin/cheats_ws.zip" DESTINATION "${GAMEINDEX_DIR}")
    endif()
    INSTALL(FILES     "${CMAKE_SOURCE_DIR}/bin/GameIndex.dbf" DESTINATION "${GAMEINDEX_DIR}")

    # set categories depending on system/distribution in pcsx2.desktop
    if(openSUSE)
        set(PCSX2_MENU_CATEGORIES "System;Emulator;")
    else()
        set(PCSX2_MENU_CATEGORIES "Game;Emulator;")
    endif()
    configure_file("${CMAKE_SOURCE_DIR}/linux_various/PCSX2.desktop.in" "${CMAKE_BINARY_DIR}/linux_various/PCSX2.desktop" @ONLY)

    INSTALL(FILES "${CMAKE_BINARY_DIR}/linux_various/PCSX2.desktop" DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/linux_various/PCSX2.xpm"     DESTINATION "${CMAKE_INSTALL_PREFIX}/share/pixmaps")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/bin/docs/PCSX2_FAQ.pdf"      DESTINATION "${DOC_DIR}")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/bin/docs/Configuration_Guide.pdf"   DESTINATION "${DOC_DIR}")
    INSTALL(FILES "${CMAKE_SOURCE_DIR}/bin/docs/PCSX2.1"            DESTINATION "${MAN_DIR}/man1")
    if(NOT DISABLE_PCSX2_WRAPPER)
        INSTALL(FILES "${CMAKE_SOURCE_DIR}/linux_various/PCSX2-linux.sh"          DESTINATION "${BIN_DIR}")
    endif()

else()
    if(NOT DISABLE_PCSX2_WRAPPER)
        # special case to avoid having linux files in windows
        INSTALL(FILES "${CMAKE_SOURCE_DIR}/linux_various/PCSX2-linux.sh"          DESTINATION "${CMAKE_SOURCE_DIR}/bin")
    endif()
endif()
