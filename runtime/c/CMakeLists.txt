#
# nwm-ewts/runtime/c/CMakeLists.txt 
#

add_library(ewts_c
  src/ewts/logger.c
  src/ewts/module_keys.c
)

target_include_directories(ewts_c
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(ewts_c PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(ewts_c PRIVATE dl)

add_library(ewts::c ALIAS ewts_c)

install(TARGETS ewts_c
  EXPORT ewtsTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
