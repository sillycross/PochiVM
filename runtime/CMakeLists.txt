# KNOWN LIMITATION: Due to a CMake limitation,
# all filenames in this list must be different, even if they are in different directories!
# (i.e. dir1/a.cpp and dir2/a.cpp would not work!)
#
SET(SOURCES
  test_classes.cpp
  test_classes_2.cpp
)

# instruct clang++ to generate .bc intermediate product (which we need) 
# along with the normal object file product
#
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --save-temps ")

SET(ALL_SYMFILE "")
foreach(_file ${SOURCES})
  string(REGEX REPLACE "\\.[^.]*$" "" _file_without_ext ${_file})	# remove file extension
  set(_bc "${_file_without_ext}.bc")
  set(_syms "${_file_without_ext}.syms")
  # the clang++ command is always run from PROJECT_BINARY_DIR, not CURRENT_BINARY_DIR,
  # and --save-temps option would spill output into the current directory.
  # It is unfortunate that the .bc files will be spilled into the build root, but there
  # is no way to change this behavior.. (--save-temps=obj won't work because object directory is 
  # intentionally designed to be CMake implementation detail and not accessible in CMakeFiles.txt)
  #
  set_source_files_properties(${_file} PROPERTIES OBJECT_OUTPUTS "${PROJECT_BINARY_DIR}/${_bc}")
  
  # generate *.sym from *.bc for each file
  #
  add_custom_command(
    OUTPUT ${PROJECT_BINARY_DIR}/${_syms}
    COMMAND ${PROJECT_BINARY_DIR}/runtime_lib_builder/dump_symbols --dump "${PROJECT_BINARY_DIR}/${_bc}" "${PROJECT_BINARY_DIR}/${_syms}"
    # update_symbol_matches is a dependency because if it changed, we need to empty all .syms.matches file
    # For simplicity we just call dump_symbol to do this (it empties the .syms.matches file corresponding to the input)
    #
    DEPENDS ${PROJECT_BINARY_DIR}/${_bc} dump_symbols update_symbol_matches
  )
  LIST(APPEND ALL_SYMFILE "${PROJECT_BINARY_DIR}/${_syms}")
endforeach()

add_library(runtime_internal OBJECT
  ${SOURCES}
)

add_library(runtime
  pochivm_register_runtime.cpp
  $<TARGET_OBJECTS:runtime_internal>
)

set_source_files_properties(pochivm_register_runtime.cpp PROPERTIES OBJECT_OUTPUTS "${PROJECT_BINARY_DIR}/pochivm_register_runtime.bc")

add_custom_command(
  OUTPUT ${PROJECT_BINARY_DIR}/pochivm_register_runtime.syms
  OUTPUT ${PROJECT_BINARY_DIR}/__pochivm_all_needed_symbols__.txt
  OUTPUT ${GENERATED_FILES_DIR}/pochivm_runtime_headers.generated.h
  COMMAND ${PROJECT_BINARY_DIR}/runtime_lib_builder/dump_symbols --dump-list "${PROJECT_BINARY_DIR}/pochivm_register_runtime.bc" "${PROJECT_BINARY_DIR}/pochivm_register_runtime.syms" "${PROJECT_BINARY_DIR}/__pochivm_all_needed_symbols__.txt" "${GENERATED_FILES_DIR}/pochivm_runtime_headers.generated.h"
  # update_symbol_matches is a dependency because if it changed, we need to empty all .syms.matches file
  # For simplicity we just call dump_symbol to do this (it empties the .syms.matches file corresponding to the input)
  #
  DEPENDS "${PROJECT_BINARY_DIR}/pochivm_register_runtime.bc" dump_symbols update_symbol_matches
)

add_custom_command(
  OUTPUT ${GENERATED_FILES_DIR}/pochivm_runtime_library.generated.cpp
  COMMAND ${PROJECT_BINARY_DIR}/runtime_lib_builder/build_runtime_lib "${PROJECT_BINARY_DIR}/runtime_lib_builder/update_symbol_matches" "${PROJECT_BINARY_DIR}/__pochivm_all_needed_symbols__.txt" "${GENERATED_FILES_DIR}" ${ALL_SYMFILE} "${PROJECT_BINARY_DIR}/pochivm_register_runtime.syms"
  DEPENDS ${ALL_SYMFILE} "${PROJECT_BINARY_DIR}/pochivm_register_runtime.syms" "${PROJECT_BINARY_DIR}/__pochivm_all_needed_symbols__.txt" build_runtime_lib update_symbol_matches
)

add_custom_target(fake ALL DEPENDS
  ${GENERATED_FILES_DIR}/pochivm_runtime_library.generated.cpp
)


