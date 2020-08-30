# Do not put 'pochivm_register_runtime.cpp' here. It is handled specially.
#
SET(SOURCES
  test_classes.cpp
  test_classes_2.cpp
  test_fn_stress.cpp
)

# !!! DO NOT MODIFY BELOW !!! 

add_library(runtime_internal1 OBJECT
  ${SOURCES}
)

add_library(runtime_internal2 OBJECT
  # DO NOT PUT ANYTHING ELSE HERE!
  pochivm_register_runtime.cpp
)

# (1) instruct clang++ to generate .bc instead of .o
#     Note that CMake will still name the .bc file as if they were an object file.
# (2) Force include file 'ir_special_function_patch.h', 
#     which contains necessary patch functions that we will need to process IR file.
#
set_target_properties(runtime_internal1 PROPERTIES COMPILE_FLAGS " -emit-llvm --include ${PROJECT_SOURCE_DIR}/pochivm/ir_special_function_patch.h ")
set_target_properties(runtime_internal2 PROPERTIES COMPILE_FLAGS " -emit-llvm --include ${PROJECT_SOURCE_DIR}/pochivm/ir_special_function_patch.h ")

add_library(runtime_bc 
  ${GENERATED_FILES_DIR}/pochivm_runtime_library.generated.cpp
)

add_library(runtime_bc_validator
  ${GENERATED_FILES_DIR}/pochivm_runtime_library_validator.generated.cpp
)

SET(RUNTIME_LIB_FILE "${CMAKE_CURRENT_BINARY_DIR}/libruntime.a")

add_custom_target(libruntime_target DEPENDS ${RUNTIME_LIB_FILE})
add_library(runtime STATIC IMPORTED GLOBAL)
add_dependencies(runtime libruntime_target)

set_target_properties(runtime
    PROPERTIES
    IMPORTED_LOCATION ${RUNTIME_LIB_FILE}
)

set_source_files_properties(${GENERATED_FILES_DIR}/pochivm_runtime_library_bitcodes.generated.h PROPERTIES GENERATED true)
set_source_files_properties(${GENERATED_FILES_DIR}/pochivm_runtime_library.generated.cpp PROPERTIES GENERATED true)
set_source_files_properties(${GENERATED_FILES_DIR}/pochivm_runtime_library_validator.generated.cpp PROPERTIES GENERATED true)

set_source_files_properties(${GENERATED_FILES_DIR}/pochivm_runtime_headers.generated.h PROPERTIES GENERATED true)
set_source_files_properties(${GENERATED_FILES_DIR}/pochivm_runtime_cpp_types.generated.h PROPERTIES GENERATED true)
set_source_files_properties(${GENERATED_FILES_DIR}/pochivm_runtime_cpp_typeinfo.generated.h PROPERTIES GENERATED true)

if(BUILD_FLAVOR STREQUAL "DEBUG")
  SET(LLC_OPT_LEVEL "0")
else()
  SET(LLC_OPT_LEVEL "3")
endif()

add_custom_command(
  OUTPUT ${RUNTIME_LIB_FILE}
  OUTPUT ${GENERATED_FILES_DIR}/pochivm_runtime_library_bitcodes.generated.h
  OUTPUT ${GENERATED_FILES_DIR}/pochivm_runtime_library.generated.cpp
  OUTPUT ${GENERATED_FILES_DIR}/pochivm_runtime_headers.generated.h
  OUTPUT ${GENERATED_FILES_DIR}/pochivm_runtime_cpp_types.generated.h
  OUTPUT ${GENERATED_FILES_DIR}/pochivm_runtime_cpp_typeinfo.generated.h
  OUTPUT ${GENERATED_FILES_DIR}/pochivm_runtime_library_validator.generated.cpp
  COMMAND ${PROJECT_BINARY_DIR}/runtime_lib_builder/build_lib_wrapper "${PROJECT_BINARY_DIR}/runtime_lib_builder/dump_symbols" "${PROJECT_BINARY_DIR}/runtime_lib_builder/update_symbol_matches" "${PROJECT_BINARY_DIR}/runtime_lib_builder/build_runtime_lib" "${GENERATED_FILES_DIR}" "${PROJECT_BINARY_DIR}" "${LLC_OPT_LEVEL}" "${RUNTIME_LIB_FILE}" '$<TARGET_OBJECTS:runtime_internal1>' '$<TARGET_OBJECTS:runtime_internal2>'
  DEPENDS $<TARGET_OBJECTS:runtime_internal1> $<TARGET_OBJECTS:runtime_internal2> dump_symbols build_runtime_lib update_symbol_matches build_lib_wrapper
)


