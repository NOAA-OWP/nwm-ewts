#
# runtime/cpp/CMakeLists.txt
#

add_library(ewts_cpp SHARED
  src/ewts/logger.cpp
)

target_include_directories(ewts_cpp
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(ewts_cpp PUBLIC cxx_std_17)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(ewts_cpp PUBLIC stdc++fs)
endif()

set_target_properties(ewts_cpp PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

add_library(ewts::ewts_cpp ALIAS ewts_cpp)

install(TARGETS ewts_cpp
  EXPORT ewtsTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
