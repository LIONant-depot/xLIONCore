include(${CMAKE_BINARY_DIR}/_deps/xcmake_tools/Common.cmake)

# Defensive - FetchAndPopulate is idempotent, a no-op if the host project already fetched this (xLION's
# own top-level CMakeLists.txt already does).
FetchAndPopulate("https://github.com/LIONant-depot/xECSV2.git" "main")

# xLIONCore's own shared library. xtextfile.cpp/my_properties.cpp are compiled in here (not just
# xecs.cpp) because their types cross the DLL boundary through xecs.h/xecs_api.h - Game.dll and this
# DLL must share the one compiled instance of each, not two disconnected copies (relocated verbatim
# from xLION's own CMakeLists.txt, where this block used to live directly).
add_library(xLIONCore SHARED
  "${CMAKE_SOURCE_DIR}/dependencies/xECSV2/src/xecs.cpp"
  "${CMAKE_SOURCE_DIR}/dependencies/xtextfile/source/xtextfile.cpp"
  "${CMAKE_SOURCE_DIR}/dependencies/xproperty/source/xcore/my_properties.cpp"
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
if(WIN32)
  target_link_libraries(xLIONCore PRIVATE Advapi32.lib)
endif()
