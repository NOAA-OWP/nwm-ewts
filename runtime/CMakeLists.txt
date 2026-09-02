#
# nwm-ewts/runtime/CMakeLists.txt
#

cmake_minimum_required(VERSION 3.20)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(EWTS_BUILD_C       "Build EWTS C runtime"       ON)
option(EWTS_BUILD_CPP     "Build EWTS C++ runtime"     ON)
option(EWTS_BUILD_FORTRAN "Build EWTS Fortran runtime" ON)
option(EWTS_WITH_NGEN     "Build EWTS ngen bridge"     ON)
option(EWTS_BUILD_SHARED  "Build shared libs"          ON)

set(BUILD_SHARED_LIBS ${EWTS_BUILD_SHARED})

# Provide this path to consumers via the package config file.
set(EWTS_FORTRAN_CONFIG_DIR "\${PACKAGE_PREFIX_DIR}/${EWTS_FORTRAN_CONFIG_INSTALL_DIR}")

# ----- Targets -----
if(EWTS_BUILD_C)
  add_subdirectory(c)
endif()

if(EWTS_BUILD_CPP)
  add_subdirectory(cpp)
endif()

if(EWTS_BUILD_FORTRAN)
  add_subdirectory(fortran)
endif()

if(EWTS_WITH_NGEN)
  add_subdirectory(../integrations/ngen integrations_ngen_build)
endif()

# ----- Package export -----
# Collect and export targets created in subdirectories.
install(EXPORT ewtsTargets
  NAMESPACE ewts::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ewts"
)

configure_package_config_file(
  "${CMAKE_CURRENT_LIST_DIR}/cmake/ewtsConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ewtsConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ewts"
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/ewtsConfigVersion.cmake"
  VERSION ${CMAKE_PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/ewtsConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/ewtsConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ewts"
)

# ----- Python wheel -----

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(EWTS_PY_PKG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/python/ewts")
set(EWTS_PY_DIST_DIR "${EWTS_PY_PKG_DIR}/dist")

add_custom_target(ewts_python_dist ALL
  COMMAND "${Python3_EXECUTABLE}" -m pip install --upgrade build setuptools==81.0.0 wheel
  COMMAND "${Python3_EXECUTABLE}" -m build --wheel --no-isolation
  WORKING_DIRECTORY "${EWTS_PY_PKG_DIR}"
  COMMENT "Building EWTS Python wheel"
)

# Install only the most recent wheel into the EWTS install tree
install(CODE "
  file(MAKE_DIRECTORY \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/python/dist\")

  file(GLOB EWTS_WHEELS \"${EWTS_PY_DIST_DIR}/*.whl\")

  set(LATEST_WHEEL \"\")
  set(LATEST_WHEEL_TIME 0)

  foreach(f IN LISTS EWTS_WHEELS)
    file(TIMESTAMP \"\${f}\" TS \"%s\")
    if(TS GREATER LATEST_WHEEL_TIME)
      set(LATEST_WHEEL \"\${f}\")
      set(LATEST_WHEEL_TIME \"\${TS}\")
    endif()
  endforeach()

  if(LATEST_WHEEL)
    message(STATUS \"Installing latest EWTS wheel: \${LATEST_WHEEL}\")
    file(INSTALL
      DESTINATION \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/python/dist\"
      TYPE FILE
      FILES \"\${LATEST_WHEEL}\"
    )
  else()
    message(WARNING \"No EWTS wheel found in ${EWTS_PY_DIST_DIR}\")
  endif()
")
