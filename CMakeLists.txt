# The contents of this file are subject to the Genesis3D Public License
# Version 1.01. A copy is provided in g3dlicense.txt. Software is distributed
# WITHOUT WARRANTY OF ANY KIND. The Original Code is Genesis3D, released
# March 25, 1999. Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C.
# All Rights Reserved. Contributor(s): Kadaken (native Linux port, 2026).

cmake_minimum_required(VERSION 3.16)
project(Genesis3D_Linux LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(X11 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

# Discover the native-port implementation graph. CONFIGURE_DEPENDS makes CMake
# regenerate when a source module is added or removed.
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Math/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Math/*.C"
    "${CMAKE_CURRENT_SOURCE_DIR}/Bitmap/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Bitmap/*.C"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/*.C"
    "${CMAKE_CURRENT_SOURCE_DIR}/Entities/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Entities/*.C"
    "${CMAKE_CURRENT_SOURCE_DIR}/Font/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Font/*.C"
    "${CMAKE_CURRENT_SOURCE_DIR}/Physics/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Physics/*.C"
    "${CMAKE_CURRENT_SOURCE_DIR}/Support/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Support/*.C"
    "${CMAKE_CURRENT_SOURCE_DIR}/VFile/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/VFile/*.C"
    "${CMAKE_CURRENT_SOURCE_DIR}/World/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/World/*.C"
    "${CMAKE_CURRENT_SOURCE_DIR}/Actor/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Actor/*.C"
)

file(GLOB ROOT_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.C"
)
list(APPEND ENGINE_SOURCES ${ROOT_ENGINE_SOURCES})

# The Linux bootstrap owns the active OpenGL/X11 context. The repository's
# OpenGl directory is an older Win32-hosted driver, so all historical driver
# backends are excluded together with optional Win32-only subsystems.
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/Engine/Drivers/")
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/Engine/Logo/")
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/VFile/fsdos\\.c$")
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/(CSNetMgr|Netplay|Sound|Sound3d)\\.c$")
list(REMOVE_DUPLICATES ENGINE_SOURCES)
list(SORT ENGINE_SOURCES)

# On case-sensitive hosts CMake treats .C as C++; these are legacy C modules.
set(UPPERCASE_C_SOURCES ${ENGINE_SOURCES})
list(FILTER UPPERCASE_C_SOURCES INCLUDE REGEX "\\.C$")
if(UPPERCASE_C_SOURCES)
    set_source_files_properties(${UPPERCASE_C_SOURCES} PROPERTIES
        LANGUAGE C
        COMPILE_FLAGS "-x c"
    )
endif()

# Derive include directories from the retained source/header graph. Keep the
# project root first so every module resolves the canonical lowercase types.
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.H"
    "${CMAKE_CURRENT_SOURCE_DIR}/*._h"
)
list(FILTER ENGINE_HEADERS EXCLUDE REGEX "/Engine/Drivers/")
list(FILTER ENGINE_HEADERS EXCLUDE REGEX "/CMakeFiles/")
list(FILTER ENGINE_HEADERS EXCLUDE REGEX "/cmake-build-[^/]+/")
list(FILTER ENGINE_HEADERS EXCLUDE REGEX "/\\.git/")
set(ENGINE_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(ENGINE_HEADER IN LISTS ENGINE_HEADERS)
    get_filename_component(ENGINE_HEADER_DIR "${ENGINE_HEADER}" DIRECTORY)
    list(APPEND ENGINE_INCLUDE_DIRS "${ENGINE_HEADER_DIR}")
endforeach()
# Core world/engine headers consume the renderer-neutral Dcommon.h ABI, but no
# implementation source from the legacy driver directories is compiled.
list(APPEND ENGINE_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Drivers")
list(REMOVE_DUPLICATES ENGINE_INCLUDE_DIRS)
list(SORT ENGINE_INCLUDE_DIRS)
list(REMOVE_ITEM ENGINE_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
list(PREPEND ENGINE_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")

# Native Linux bootstrap executable.  It provides the 1920x1080 OpenGL/X11
# canvas and a fixed-step render loop while the historical engine modules are
# ported away from DirectDraw/Direct3D and Win32.
add_executable(genesis3d_linux linux_main.cpp ${ENGINE_SOURCES})
target_compile_definitions(genesis3d_linux PRIVATE GL_GLEXT_PROTOTYPES)
target_compile_options(genesis3d_linux PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Werror=pointer-to-int-cast>
    $<$<COMPILE_LANGUAGE:C>:-Werror=int-to-pointer-cast>
    $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>
    $<$<COMPILE_LANGUAGE:C>:-Werror=incompatible-pointer-types>
)
target_include_directories(genesis3d_linux PRIVATE
    ${ENGINE_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
)
target_link_libraries(genesis3d_linux PRIVATE
    OpenGL::GL
    PkgConfig::SDL2
    ${X11_LIBRARIES}
    ${CMAKE_DL_LIBS}
    pthread
)

if(UNIX AND NOT APPLE)
    target_link_libraries(genesis3d_linux PRIVATE m)
endif()

message(STATUS "Genesis3D native canvas: 1920x1080")
message(STATUS "Genesis3D fixed-step rates: 60 FPS and 120 FPS")
list(LENGTH ENGINE_SOURCES ENGINE_SOURCE_COUNT)
message(STATUS "Genesis3D native implementation modules: ${ENGINE_SOURCE_COUNT}")
message(STATUS "Legacy Win32 driver and optional subsystem sources are excluded")
