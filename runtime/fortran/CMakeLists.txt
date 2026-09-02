add_library(ewts_fortran
  src/ewts/log_levels.f90
  src/ewts/module_constants.f90
  src/ewts/module_keys.f90
  src/ewts/payload_status.f90
  src/ewts/logger.f90
)

# Force free-form (defensive) and avoid line truncation being fatal
set_source_files_properties(
  src/ewts/log_levels.f90
  src/ewts/module_constants.f90
  src/ewts/module_keys.f90
  src/ewts/payload_status.f90
  src/ewts/logger.f90
  PROPERTIES Fortran_FORMAT FREE
)

target_compile_options(ewts_fortran PRIVATE
  $<$<COMPILE_LANGUAGE:Fortran>:-Wno-error=line-truncation>
  $<$<AND:$<COMPILE_LANGUAGE:Fortran>,$<Fortran_COMPILER_ID:GNU>>:-ffree-line-length-none>
  $<$<AND:$<COMPILE_LANGUAGE:Fortran>,$<Fortran_COMPILER_ID:Intel>>:-extend-source>
)

# Fortran preprocessing is needed because you have #ifdef EWTS_HAVE_NGEN_BRIDGE
set_source_files_properties(src/ewts/logger.f90 PROPERTIES Fortran_PREPROCESS ON)

# Put .mod files somewhere predictable in build + install
set(EWTS_FORTRAN_MODDIR "${CMAKE_CURRENT_BINARY_DIR}/mod")
set_target_properties(ewts_fortran PROPERTIES
  Fortran_MODULE_DIRECTORY "${EWTS_FORTRAN_MODDIR}"
  POSITION_INDEPENDENT_CODE ON
)

# Consumers need the installed .mod directory
# We'll install mods to: include/ewts/fortran
set(EWTS_FORTRAN_MOD_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}/ewts/fortran")

target_include_directories(ewts_fortran
  PUBLIC
    $<BUILD_INTERFACE:${EWTS_FORTRAN_MODDIR}>
    $<INSTALL_INTERFACE:${EWTS_FORTRAN_MOD_INSTALL_DIR}>
)

# Enable/disable ngen bridge at compile time
if(EWTS_WITH_NGEN)
  target_compile_definitions(ewts_fortran PRIVATE EWTS_HAVE_NGEN_BRIDGE=1)
  target_link_libraries(ewts_fortran PRIVATE ewts_ngen_bridge)
endif()

add_library(ewts::fortran ALIAS ewts_fortran)

install(TARGETS ewts_fortran
  EXPORT ewtsTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install the .mod files produced by this compiler
install(DIRECTORY "${EWTS_FORTRAN_MODDIR}/"
  DESTINATION "${EWTS_FORTRAN_MOD_INSTALL_DIR}"
  FILES_MATCHING PATTERN "*.mod" PATTERN "*.smod" PATTERN "*.MOD"
)
