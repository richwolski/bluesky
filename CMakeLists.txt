cmake_minimum_required(VERSION 2.6)
project(bluesky)
set(CMAKE_BUILD_TYPE Debug)

include(FindPkgConfig)
pkg_check_modules(GLIB REQUIRED glib-2.0 gthread-2.0)

set(LIBS3_BUILD_DIR "${CMAKE_SOURCE_DIR}/libs3/build")

add_definitions(-D_FILE_OFFSET_BITS=64)
add_subdirectory(kvstore)
add_subdirectory(bluesky)
add_subdirectory(nfs3)
add_subdirectory(microbench)
add_subdirectory(cloudbench)
add_subdirectory(logbench)
add_subdirectory(simplestore)
