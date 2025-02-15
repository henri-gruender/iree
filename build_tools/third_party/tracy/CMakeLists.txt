# Copyright 2021 The IREE Authors
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.16.3)

project(IREETracyServer C CXX)

set(TRACY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party/tracy")

find_package(Threads REQUIRED)

#-------------------------------------------------------------------------------
# Detect package manager
#-------------------------------------------------------------------------------

message(STATUS "Checking for Tracy dependencies...")
find_program(PKGCONFIG pkg-config)
if(NOT PKGCONFIG)
  message(STATUS "Could not find pkg-config (on Ubuntu/Debian, 'apt install pkg-config')")
else()
  include(FindPkgConfig)
  pkg_check_modules(TRACY_DEPS
    tbb
    libzstd
  )
  pkg_check_modules(TRACY_CAPSTONE_DEPS
    capstone
  )
  pkg_check_modules(TRACY_GTK_DEPS
    freetype2
    glfw3
    gtk+-3.0
    dbus-1
  )
endif()


if(NOT TRACY_DEPS_FOUND)
  message(STATUS "Could not find Tracy dependencies (Tracy server will not be built).")
  message(STATUS "To build Tracy, install packages libzstd, and tbb (on Ubuntu/Debian, 'apt install libcapstone-dev libtbb-dev libzstd-dev')")
  return()
endif()

if(NOT TRACY_CAPSTONE_DEPS_FOUND)
  message(STATUS "Could not find capstone, a Tracy dependency (Tracy server will not be built).")
  message(STATUS "To build Tracy, install capstone (on Ubuntu/Debian, 'apt install libcapstone-dev')")
  return()
endif()

if(NOT TRACY_GTK_DEPS_FOUND)
  message(STATUS
    "Could not find deps required to build graphical programs: "
    "Tracy graphical profiler will not be built (on Ubuntu/Debian, 'apt install libglfw3-dev libfreetype-dev libgtk-3-dev libdbus-1-dev')")
endif()

#-------------------------------------------------------------------------------
# Configuration
#-------------------------------------------------------------------------------

function(setup_cxx_options name)
  set_target_properties(${name}
    PROPERTIES
      CXX_STANDARD 17
  )
  target_compile_options(${name}
    PRIVATE
      $<$<CXX_COMPILER_ID:GNU,Clang>:-Wno-unused-result>
  )
  target_include_directories(${name}
    PUBLIC
      ${TRACY_SOURCE_DIR}/imgui
      ${TRACY_DEPS_INCLUDE_DIRS}
      ${TRACY_CAPSTONE_DEPS_INCLUDE_DIRS}
      # capstone-next moved capstone.h to a capstone/ subdirectory, but the
      # pkg-config isn't updated yet as of April 2022.
      ${TRACY_CAPSTONE_DEPS_INCLUDE_DIRS}/capstone
  )
  target_link_libraries(${name}
    PRIVATE
      ${TRACY_DEPS_LIBRARIES}
      ${TRACY_CAPSTONE_DEPS_LIBRARIES}
      ${CMAKE_DL_LIBS}
      ${CMAKE_THREAD_LIBS_INIT}
  )
  target_link_directories(${name}
    PRIVATE
      ${TRACY_DEPS_LIBRARY_DIRS}
      ${TRACY_CAPSTONE_DEPS_LIBRARY_DIRS}
  )
endfunction()

function(setup_graphics_deps name)
  target_compile_definitions(${name}
  PRIVATE
    DISPLAY_SERVER_X11
  )
  target_include_directories(${name}
    PUBLIC
      ${TRACY_GTK_DEPS_INCLUDE_DIRS}
  )
  target_link_libraries(${name}
    PRIVATE
      ${TRACY_GTK_DEPS_LIBRARIES}
  )
  target_link_directories(${name}
    PRIVATE
      ${TRACY_GTK_DEPS_LIBRARY_DIRS}
  )
endfunction()

#-------------------------------------------------------------------------------
# Common library
#-------------------------------------------------------------------------------

file(GLOB COMMON_SRCS ${TRACY_SOURCE_DIR}/common/*.cpp)
add_library(IREETracyCommon
  ${COMMON_SRCS}
)
setup_cxx_options(IREETracyCommon)

#-------------------------------------------------------------------------------
# Server library
#-------------------------------------------------------------------------------

file(GLOB SERVER_SRCS ${TRACY_SOURCE_DIR}/server/*.cpp)
add_library(IREETracyServer
  ${SERVER_SRCS}
)
setup_cxx_options(IREETracyServer)
target_link_libraries(IREETracyServer
  PRIVATE
    IREETracyCommon
)

#-------------------------------------------------------------------------------
# IMGUI library
#-------------------------------------------------------------------------------

file(GLOB IMGUI_SOURCES ${TRACY_SOURCE_DIR}/imgui/*.cpp)
add_library(IREETracyIMGUI
  ${IMGUI_SOURCES}
)
setup_cxx_options(IREETracyServer)

#-------------------------------------------------------------------------------
# Standalone capture server
#-------------------------------------------------------------------------------

add_executable(IREETracyCaptureServer
  ${TRACY_SOURCE_DIR}/capture/src/capture.cpp
)
set_target_properties(IREETracyCaptureServer
  PROPERTIES
    OUTPUT_NAME "iree-tracy-capture"
)
setup_cxx_options(IREETracyCaptureServer)
target_link_libraries(IREETracyCaptureServer
  PRIVATE
    IREETracyCommon
    IREETracyServer
)

#-------------------------------------------------------------------------------
# Graphical frontends
#-------------------------------------------------------------------------------

if(TRACY_GTK_DEPS_FOUND)
  #-----------------------------------------------------------------------------
  # NFD library
  #-----------------------------------------------------------------------------

  set(NFD_SOURCES
    ${TRACY_SOURCE_DIR}/nfd/nfd_portal.cpp
  )
  add_library(IREETracyNFD
    ${NFD_SOURCES}
  )
  setup_cxx_options(IREETracyNFD)
  setup_graphics_deps(IREETracyNFD)

  #-----------------------------------------------------------------------------
  # Profiler
  #-----------------------------------------------------------------------------

  file(GLOB PROFILER_SRCS ${TRACY_SOURCE_DIR}/profiler/src/*.cpp)
  add_executable(IREETracyProfiler
    ${PROFILER_SRCS}
  )
  set_target_properties(IREETracyProfiler
    PROPERTIES
      OUTPUT_NAME "iree-tracy-profiler"
  )
  setup_cxx_options(IREETracyProfiler)
  setup_graphics_deps(IREETracyProfiler)
  target_link_libraries(IREETracyProfiler
    PRIVATE
      IREETracyIMGUI
      IREETracyCommon
      IREETracyNFD
      IREETracyServer
      ${CMAKE_THREAD_LIBS_INIT}
  )
endif()
