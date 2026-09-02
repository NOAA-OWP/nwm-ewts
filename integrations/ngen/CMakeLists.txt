#
# nwm-ewts/integrations/ngen/CMakeLists.txt
#

add_library(ewts_ngen_bridge SHARED
  src/ewts_ngen/ewts_ngen_bridge.cpp
  src/ewts_ngen/logger.cpp
)

target_include_directories(ewts_ngen_bridge
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../runtime/cpp/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

find_package(MPI REQUIRED COMPONENTS CXX)

target_link_libraries(ewts_ngen_bridge PRIVATE MPI::MPI_CXX)

target_compile_features(ewts_ngen_bridge PUBLIC cxx_std_17)
set_target_properties(ewts_ngen_bridge PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(ewts::ewts_ngen_bridge ALIAS ewts_ngen_bridge)

install(TARGETS ewts_ngen_bridge
  EXPORT ewtsTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
