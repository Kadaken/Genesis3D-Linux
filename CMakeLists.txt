# The contents of this file are subject to the Genesis3D Public License
# Version 1.01. A copy is provided in g3dlicense.txt. Software is distributed
# WITHOUT WARRANTY OF ANY KIND. The Original Code is Genesis3D, released
# March 25, 1999. Copyright (C) 1996-1999 Eclipse Entertainment, L.L.C.
# All Rights Reserved. Contributor(s): Kadaken (native Linux port, 2026).

cmake_minimum_required(VERSION 3.16)
project(Genesis3D_Linux LANGUAGES C CXX)
include(CTest)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(X11 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(OPENAL REQUIRED IMPORTED_TARGET openal)

option(GENESIS3D_BUILD_TOOLS "Build standalone asset conversion tools" ON)

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
# backends are excluded from this core graph. The inherited OpenGL driver is
# built separately below with an SDL host.
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/Engine/Drivers/")
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/Engine/Logo/")
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/VFile/fsdos\\.c$")
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/(CSNetMgr|Netplay|Sound)\\.c$")
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
# implementation source from the legacy driver directories enters the core.
# The inherited OpenGL driver is built as a separate module below.
list(APPEND ENGINE_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Drivers")
list(REMOVE_DUPLICATES ENGINE_INCLUDE_DIRS)
list(SORT ENGINE_INCLUDE_DIRS)
list(REMOVE_ITEM ENGINE_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
list(PREPEND ENGINE_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")

# Reusable in-process native renderer. The public C ABI owns world loading,
# SDL/OpenGL presentation, input polling, fixed-step collision, and explicit
# camera submission without embedding application/game logic.
add_library(genesis3d_render SHARED
    Engine/linux_render_api.cpp
    ${ENGINE_SOURCES}
)
set_target_properties(genesis3d_render PROPERTIES
    OUTPUT_NAME genesis3d_render
    VERSION 1.0.0
    SOVERSION 1
)
target_compile_definitions(genesis3d_render PRIVATE GL_GLEXT_PROTOTYPES)
target_compile_options(genesis3d_render PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Werror=pointer-to-int-cast>
    $<$<COMPILE_LANGUAGE:C>:-Werror=int-to-pointer-cast>
    $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>
    $<$<COMPILE_LANGUAGE:C>:-Werror=incompatible-pointer-types>
)
target_include_directories(genesis3d_render PUBLIC
    ${ENGINE_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
)
target_link_libraries(genesis3d_render PUBLIC
    OpenGL::GL
    PkgConfig::SDL2
    PkgConfig::OPENAL
    ${X11_LIBRARIES}
    ${CMAKE_DL_LIBS}
    pthread
)

if(UNIX AND NOT APPLE)
    target_link_libraries(genesis3d_render PUBLIC m)
endif()

if(GENESIS3D_BUILD_TOOLS)
    find_package(PNG REQUIRED)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cgltf/cgltf.h")
        message(FATAL_ERROR
            "cgltf submodule missing; run: git submodule update --init --recursive")
    endif()
    add_executable(gltf2act tools/gltf2act.cpp)
    target_include_directories(gltf2act PRIVATE
        ${ENGINE_INCLUDE_DIRS}
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cgltf")
    target_link_libraries(gltf2act PRIVATE genesis3d_render PNG::PNG)
    set_target_properties(gltf2act PROPERTIES BUILD_RPATH "$ORIGIN")
endif()

# Historical DRV_Driver ABI, separate from the high-level LinuxRender API.
add_library(OglDrv SHARED
    Engine/Drivers/OpenGl/OGLDrv.c
    Engine/Drivers/OpenGl/OglMisc.c
    Engine/Drivers/OpenGl/Render.c
    Engine/Drivers/OpenGl/THandle.c
    Engine/Drivers/OpenGl/SDLWindow.c
)
target_include_directories(OglDrv PRIVATE ${ENGINE_INCLUDE_DIRS}
    Engine/Drivers/OpenGl)
target_link_libraries(OglDrv PRIVATE OpenGL::GL OpenGL::GLU PkgConfig::SDL2 m)
set_target_properties(OglDrv PROPERTIES C_VISIBILITY_PRESET hidden)

# Preserve the original standalone validation executable. It now resolves the
# engine implementation through the same shared object consumed by clients.
add_executable(genesis3d_linux linux_main.cpp)
target_compile_definitions(genesis3d_linux PRIVATE GL_GLEXT_PROTOTYPES)
target_include_directories(genesis3d_linux PRIVATE
    ${ENGINE_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
)
target_link_libraries(genesis3d_linux PRIVATE genesis3d_render)
set_target_properties(genesis3d_linux PROPERTIES BUILD_RPATH "$ORIGIN")

if(BUILD_TESTING)
    add_executable(genesis3d_keyed_mip_edge_test
        tests/keyed_mip_edge_test.c
        Engine/Drivers/OpenGl/OglMisc.c)
    target_include_directories(genesis3d_keyed_mip_edge_test PRIVATE
        ${ENGINE_INCLUDE_DIRS} Engine/Drivers/OpenGl)
    target_link_libraries(genesis3d_keyed_mip_edge_test PRIVATE OpenGL::GL m)
    add_test(NAME genesis3d_keyed_mip_edge COMMAND genesis3d_keyed_mip_edge_test)

    add_executable(genesis3d_pixel_composer_test tests/pixel_composer_test.c)
    target_include_directories(genesis3d_pixel_composer_test PRIVATE ${ENGINE_INCLUDE_DIRS})
    target_link_libraries(genesis3d_pixel_composer_test PRIVATE genesis3d_render)
    add_test(NAME genesis3d_pixel_composers COMMAND genesis3d_pixel_composer_test)
    add_executable(genesis3d_actor_rejection_test tests/actor_rejection_test.c)
    target_include_directories(genesis3d_actor_rejection_test PRIVATE ${ENGINE_INCLUDE_DIRS})
    target_link_libraries(genesis3d_actor_rejection_test PRIVATE genesis3d_render ${CMAKE_DL_LIBS})
    add_test(NAME genesis3d_actor_rejection COMMAND genesis3d_actor_rejection_test)

    add_executable(genesis3d_actor_pose_chain_test tests/actor_pose_chain_test.c)
    target_include_directories(genesis3d_actor_pose_chain_test PRIVATE ${ENGINE_INCLUDE_DIRS})
    target_link_libraries(genesis3d_actor_pose_chain_test PRIVATE genesis3d_render)
    add_test(NAME genesis3d_actor_pose_chain COMMAND genesis3d_actor_pose_chain_test)
    add_executable(genesis3d_smooth_skinning_test tests/smooth_skinning_test.c)
    target_include_directories(genesis3d_smooth_skinning_test PRIVATE ${ENGINE_INCLUDE_DIRS})
    target_link_libraries(genesis3d_smooth_skinning_test PRIVATE genesis3d_render)
    add_test(NAME genesis3d_smooth_skinning COMMAND genesis3d_smooth_skinning_test)
    if(GENESIS3D_BUILD_TOOLS)
        find_program(PYTHON3_EXECUTABLE python3 REQUIRED)
        add_test(NAME genesis3d_gltf2act
            COMMAND "${PYTHON3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/gltf2act_fixture.py"
                "$<TARGET_FILE:gltf2act>")
    endif()
    add_executable(genesis3d_entity_schema_test tests/entity_schema_test.c)
    target_include_directories(genesis3d_entity_schema_test PRIVATE ${ENGINE_INCLUDE_DIRS})
    target_link_libraries(genesis3d_entity_schema_test PRIVATE genesis3d_render)
    add_test(NAME genesis3d_entity_schema COMMAND genesis3d_entity_schema_test)
    add_test(NAME genesis3d_entity_names COMMAND genesis3d_entity_schema_test names)
    add_executable(genesis3d_entity_reserve_test tests/entity_reserve_test.c)
    target_include_directories(genesis3d_entity_reserve_test PRIVATE ${ENGINE_INCLUDE_DIRS})
    target_link_libraries(genesis3d_entity_reserve_test PRIVATE genesis3d_render)
    add_test(NAME genesis3d_entity_reserve COMMAND genesis3d_entity_reserve_test)
    add_executable(genesis3d_vfile_test tests/vfile_contract_test.c)
    target_include_directories(genesis3d_vfile_test PRIVATE ${ENGINE_INCLUDE_DIRS})
    target_link_libraries(genesis3d_vfile_test PRIVATE genesis3d_render)
    add_test(NAME genesis3d_vfile_lines COMMAND genesis3d_vfile_test lines)
    add_test(NAME genesis3d_vfile_const_write COMMAND genesis3d_vfile_test writes)
    add_test(NAME genesis3d_vfile_reads COMMAND genesis3d_vfile_test reads)
    add_executable(genesis3d_sound_test tests/sound_openal_test.c)
    target_include_directories(genesis3d_sound_test PRIVATE ${ENGINE_INCLUDE_DIRS})
    target_link_libraries(genesis3d_sound_test PRIVATE genesis3d_render)
    set_target_properties(genesis3d_sound_test PROPERTIES BUILD_RPATH "$ORIGIN")
    add_test(NAME genesis3d_sound_null_backend COMMAND genesis3d_sound_test)
    set_tests_properties(genesis3d_sound_null_backend PROPERTIES
        ENVIRONMENT "ALSOFT_DRIVERS=null"
    )

    add_executable(genesis3d_texture_name_test tests/world_texture_name_test.c)
    target_include_directories(genesis3d_texture_name_test PRIVATE ${ENGINE_INCLUDE_DIRS})
    target_link_libraries(genesis3d_texture_name_test PRIVATE genesis3d_render)
    set_target_properties(genesis3d_texture_name_test PROPERTIES BUILD_RPATH "$ORIGIN")
    add_test(NAME genesis3d_texture_name_fixture COMMAND genesis3d_texture_name_test)

    add_executable(genesis3d_legacy_frame_test tests/legacy_frame_test.c)
    target_link_libraries(genesis3d_legacy_frame_test PRIVATE genesis3d_render)
    add_dependencies(genesis3d_legacy_frame_test OglDrv)
    find_program(XVFB_RUN_EXECUTABLE xvfb-run)
    add_executable(genesis3d_sdl_window_test tests/sdl_window_contract.c
        Engine/Drivers/OpenGl/SDLWindow.c)
    target_include_directories(genesis3d_sdl_window_test PRIVATE ${ENGINE_INCLUDE_DIRS}
        "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Drivers/OpenGl")
    target_link_libraries(genesis3d_sdl_window_test PRIVATE PkgConfig::SDL2)
    if(XVFB_RUN_EXECUTABLE)
        add_test(NAME genesis3d_face_capacity_fixture
            COMMAND "${XVFB_RUN_EXECUTABLE}" -a -s "-screen 0 640x480x24"
            $<TARGET_FILE:genesis3d_legacy_frame_test> $<TARGET_FILE_DIR:OglDrv> --face-capacity)
        set_tests_properties(genesis3d_face_capacity_fixture PROPERTIES
            TIMEOUT 30 RUN_SERIAL TRUE ENVIRONMENT "SDL_VIDEODRIVER=x11;LIBGL_ALWAYS_SOFTWARE=1")
        add_test(NAME genesis3d_sdl_window_contract
            COMMAND "${XVFB_RUN_EXECUTABLE}" -a -s "-screen 0 640x480x24"
            $<TARGET_FILE:genesis3d_sdl_window_test>)
        set_tests_properties(genesis3d_sdl_window_contract PROPERTIES
            TIMEOUT 30 ENVIRONMENT "SDL_VIDEODRIVER=x11;LIBGL_ALWAYS_SOFTWARE=1")
        add_test(NAME genesis3d_legacy_frame_fixture
            COMMAND "${XVFB_RUN_EXECUTABLE}" -a -s "-screen 0 640x480x24"
            $<TARGET_FILE:genesis3d_legacy_frame_test> $<TARGET_FILE_DIR:OglDrv>)
        set_tests_properties(genesis3d_legacy_frame_fixture PROPERTIES
            TIMEOUT 30 ENVIRONMENT "SDL_VIDEODRIVER=x11;LIBGL_ALWAYS_SOFTWARE=1")
        add_test(NAME genesis3d_flat_color_fixture
            COMMAND "${XVFB_RUN_EXECUTABLE}" -a -s "-screen 0 640x480x24"
            $<TARGET_FILE:genesis3d_legacy_frame_test> $<TARGET_FILE_DIR:OglDrv> --flat-colors)
        set_tests_properties(genesis3d_flat_color_fixture PROPERTIES
            TIMEOUT 30 RUN_SERIAL TRUE ENVIRONMENT "SDL_VIDEODRIVER=x11;LIBGL_ALWAYS_SOFTWARE=1")
        add_test(NAME genesis3d_palettized_texture_fixture
            COMMAND "${XVFB_RUN_EXECUTABLE}" -a -s "-screen 0 640x480x24"
            $<TARGET_FILE:genesis3d_legacy_frame_test> $<TARGET_FILE_DIR:OglDrv> --palette-colors)
        set_tests_properties(genesis3d_palettized_texture_fixture PROPERTIES
            TIMEOUT 30 RUN_SERIAL TRUE ENVIRONMENT "SDL_VIDEODRIVER=x11;LIBGL_ALWAYS_SOFTWARE=1")
        add_test(NAME genesis3d_legacy_bsp_fixture
            COMMAND "${XVFB_RUN_EXECUTABLE}" -a -s "-screen 0 640x480x24"
            $<TARGET_FILE:genesis3d_legacy_frame_test> $<TARGET_FILE_DIR:OglDrv> --bsp)
        set_tests_properties(genesis3d_legacy_bsp_fixture PROPERTIES
            TIMEOUT 30 ENVIRONMENT "SDL_VIDEODRIVER=x11;LIBGL_ALWAYS_SOFTWARE=1")
    else()
        message(WARNING "xvfb-run unavailable: legacy graphics fixtures are not registered; install Xvfb and xauth to validate the driver")
    endif()
endif()

message(STATUS "Genesis3D native canvas: 1920x1080")
message(STATUS "Genesis3D fixed-step rates: 60 FPS and 120 FPS")
list(LENGTH ENGINE_SOURCES ENGINE_SOURCE_COUNT)
message(STATUS "Genesis3D native implementation modules: ${ENGINE_SOURCE_COUNT}")
message(STATUS "Legacy OpenGL driver: libOglDrv.so with SDL host; other Win32 drivers excluded")
message(STATUS "Genesis3D audio backend: OpenAL (pitch, looping, spatial pan, and per-source/master gain)")
