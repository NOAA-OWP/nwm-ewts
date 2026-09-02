#
# nwm-ewts/CMakeLists.txt
#

cmake_minimum_required(VERSION 3.20)

# Top-level build that includes EWTS runtime
#
# Build example (same knobs as before, now from repo root):
#   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DEWTS_WITH_NGEN=ON -DEWTS_BUILD_SHARED=OFF
#   cmake --build build -j
#   cmake --install build --prefix /tmp/ewts_install
#

set(EWTS_VERSION_FILE
    "${CMAKE_SOURCE_DIR}/runtime/python/ewts/src/ewts/_version.py"
)

file(READ "${EWTS_VERSION_FILE}" EWTS_VERSION_CONTENT)

string(REGEX MATCH "__version__ = \"([^\"]+)\"" _ "${EWTS_VERSION_CONTENT}")
set(EWTS_PY_VERSION "${CMAKE_MATCH_1}")

string(REGEX MATCH "NGWPC_VERSION = \"([^\"]+)\"" _ "${EWTS_VERSION_CONTENT}")
set(EWTS_NGWPC_VERSION "${CMAKE_MATCH_1}")

project(ewts VERSION ${EWTS_PY_VERSION} LANGUAGES C CXX Fortran)

message(STATUS "EWTS version is ${EWTS_PY_VERSION}")
message(STATUS "EWTS NGWPC VERSION is ${EWTS_NGWPC_VERSION}")

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ewtsConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Build/install/export the core runtime libraries + package config.
add_subdirectory(runtime)
