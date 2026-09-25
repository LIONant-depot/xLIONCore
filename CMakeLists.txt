cmake_minimum_required(VERSION 3.10)
project(xLIONCore_unit_test LANGUAGES C CXX)

# Standalone buildable project for xLIONCore on its own - generates its own solution the same way
# dependencies/xGPU's own top-level CMakeLists.txt does. A consuming project (xLION) instead fetches
# this depot via FetchAndPopulate, which picks up Build/dependency/CMakeLists.txt directly - this file
# add_subdirectory's that same file rather than duplicating its add_library block, so there is exactly
# one copy of the shared-library recipe regardless of which mode is building it.
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

#
# Fetch Common.cmake from xcmake_tools - identical bootstrap to xGPU's/xLION's own root CMakeLists.txt
#
include(FetchContent)
FetchContent_Declare(
  xcmake_tools
  GIT_REPOSITORY https://github.com/LIONant-depot/xcmake_tools.git
  GIT_TAG main
  GIT_SHALLOW TRUE
  SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/xcmake_tools"
)
FetchContent_MakeAvailable(xcmake_tools)
include(${CMAKE_BINARY_DIR}/_deps/xcmake_tools/Common.cmake)

#
# The transitive dependencies xecs.cpp/xtextfile.cpp/my_properties.cpp need - same list xLION's and
# xGPU's own top-level CMakeLists.txt already fetch directly; everything else (xerr, xresource_guid,
# xmath, xbits, xresource_pipeline_v2, ...) comes in transitively through these depots' own
# Build/dependency/CMakeLists.txt files, the same way it already does for xLION/xGPU.
#
FetchAndPopulate("https://github.com/LIONant-depot/xresource_mgr.git"    "main")
FetchAndPopulate("https://github.com/LIONant-depot/xcontainer.git"       "main")
FetchAndPopulate("https://github.com/LIONant-depot/xdelegate.git"        "main")
FetchAndPopulate("https://github.com/LIONant-depot/xprim_geom.git"       "main")
FetchAndPopulate("https://github.com/LIONant-depot/xproperty.git"        "master")
FetchAndPopulate("https://github.com/LIONant-depot/xstrtool.git"         "main")
FetchAndPopulate("https://github.com/LIONant-depot/xserializer.git"      "main")
FetchAndPopulate("https://github.com/LIONant-depot/xscheduler.git"       "main")
FetchAndPopulate("https://github.com/LIONant-depot/xundo.git"            "main")
FetchAndPopulate("https://github.com/LIONant-depot/xsource_control.git"  "main")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/Build/dependency" "xLIONCore_build")
