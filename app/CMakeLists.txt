# defaultmp - BO2 multiplayer on rexglue-sdk development (bo2-dev P0 baseline).
#
# Layout follows `rexglue init` (resources/templates/init/cmakelists.inja): find the SDK, call
# rexglue_project, build the host sources and the recompiled image into one executable.
# No -march anywhere: the SDK adds its -msse4.1 floor through rexglue_apply_target_settings.

cmake_minimum_required(VERSION 3.25)
project(defaultmp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (REXSDK_DIR)
  add_subdirectory("${REXSDK_DIR}" rexglue-sdk)
else ()
  find_package(rexglue 0.11.0 REQUIRED CONFIG)
endif ()
rexglue_project(defaultmp OUTPUT_DIR generated/default_mp)

set(DEFAULTMP_HOST_SOURCES src/stubs.cpp src/compat_math.cpp src/dvar_force.cpp src/vpad.cpp src/scene_res.cpp src/live_settings.cpp src/lua_patches.cpp src/gsc_patches.cpp)

# OBJECT and never STATIC, an archive drops the REX_EXPORT symbols.
add_library(defaultmp_common OBJECT ${DEFAULTMP_HOST_SOURCES})
rexglue_setup_host_library(defaultmp_common)

if (WIN32)
  add_executable(defaultmp WIN32 src/main.cpp)
else ()
  add_executable(defaultmp src/main.cpp)
endif ()

rexglue_setup_target(defaultmp)
target_link_libraries(defaultmp PRIVATE defaultmp_common)

if (WIN32)
  target_link_options(defaultmp PRIVATE "-Wl,/MAP:${CMAKE_CURRENT_BINARY_DIR}/defaultmp.map")
endif ()
