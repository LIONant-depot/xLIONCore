include(${CMAKE_BINARY_DIR}/_deps/xcmake_tools/Common.cmake)

# Defensive - FetchAndPopulate is idempotent, a no-op if the host project already fetched this (xLION's
# own top-level CMakeLists.txt already does).
FetchAndPopulate("https://github.com/LIONant-depot/xECSV2.git" "main")

# box3d (Erin Catto's 3D physics engine) - a real, non-LIONant CMake project (ships its own
# add_subdirectory(src) with a box3d::box3d target), so this uses plain FetchContent directly rather
# than FetchAndPopulate's LIONant-depot "build/dependency/CMakeLists.txt" convention. Samples/unit
# tests default ON upstream and pull in extra dependencies xLIONCore has no use for - forced OFF.
include(FetchContent)
set(BOX3D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX3D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(box3d
  GIT_REPOSITORY https://github.com/erincatto/box3d.git
  GIT_TAG main
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/dependencies/box3d"
)
FetchContent_MakeAvailable(box3d)

# xLIONCore's own shared library. xtextfile.cpp/my_properties.cpp are compiled in here (not just
# xecs.cpp) because their types cross the DLL boundary through xecs.h/xecs_api.h - Game.dll and this
# DLL must share the one compiled instance of each, not two disconnected copies (relocated verbatim
# from xLION's own CMakeLists.txt, where this block used to live directly).
#
# src/physics/xlioncore_physics_world.cpp: the first non-ECS engine system, proving xLIONCore is a
# real home for more than just the ECS - box3d itself, and every box3d call, stays private to this
# DLL, wrapped behind the xecs-agnostic `world` class (see xlioncore_physics_world.h's own comment on
# why it - and NOT the xecs-facing system/component code - is what actually lives in this DLL).
add_library(xLIONCore SHARED
  "${CMAKE_SOURCE_DIR}/dependencies/xECSV2/src/xecs.cpp"
  "${CMAKE_SOURCE_DIR}/dependencies/xtextfile/source/xtextfile.cpp"
  "${CMAKE_SOURCE_DIR}/dependencies/xproperty/source/xcore/my_properties.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/physics/xlioncore_physics_world.cpp"
)
set_target_properties(xLIONCore PROPERTIES OUTPUT_NAME "LIONCore")
target_include_directories(xLIONCore PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/dependencies/xECSV2/src"
  "${CMAKE_SOURCE_DIR}/dependencies/xerr"
  "${CMAKE_SOURCE_DIR}/dependencies/xresource_guid"
  "${CMAKE_SOURCE_DIR}/dependencies/xtextfile"
  "${CMAKE_SOURCE_DIR}/dependencies/xproperty"
  "${CMAKE_SOURCE_DIR}/dependencies/xresource_pipeline_v2"
  "${CMAKE_SOURCE_DIR}/dependencies/xstrtool"
  "${CMAKE_SOURCE_DIR}/dependencies/xcontainer"
  "${CMAKE_SOURCE_DIR}/dependencies/xdelegate"
  "${CMAKE_SOURCE_DIR}/dependencies/xscheduler"
  "${CMAKE_SOURCE_DIR}/dependencies/xmath"
  "${CMAKE_SOURCE_DIR}/dependencies/xbits"
)
target_compile_definitions(xLIONCore PRIVATE XECS_BUILD_SHARED XECS_EXPORTS)
target_link_libraries(xLIONCore PRIVATE box3d::box3d)
if(WIN32)
  target_link_libraries(xLIONCore PRIVATE Advapi32.lib)
endif()
