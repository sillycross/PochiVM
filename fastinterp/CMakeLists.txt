# remove -O0 -g flag, and add -O3 flag
# 
string(REPLACE " -O0 " " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
string(REPLACE " -g " " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fconstexpr-steps=100000000 ")

add_library(fastinterp_tpl_obj OBJECT
  fastinterp_tpl.cpp
)

# important compiler flags for the object file, must not be changed. see comments in dynamic_specialization_utils.h
# TODO: clang++ currently does not support -mlarge-data-threshold which would hurt a bit of performance
#
set_target_properties(fastinterp_tpl_obj PROPERTIES COMPILE_FLAGS " -mcmodel=medium -fno-pic -fno-pie ")

# Unlike runtime_library_builder, it is fine we compile fastinterp_tpl twice
# since the IR is only for extracting symbol names.
# The only source of truth of binary data is the object file.
#
add_library(fastinterp_tpl_ir OBJECT
  fastinterp_tpl.cpp
)

set_target_properties(fastinterp_tpl_ir PROPERTIES COMPILE_FLAGS " -emit-llvm ")

add_executable(build_fast_interp_lib
  build_fast_interp_lib.cpp   
  $<TARGET_OBJECTS:fake_symbol_resolver_util>
) 
# for some reason, get a weird 'typeinfo for ErrorInfoBase does not exist' link error in debug
# compile it in -fno-rtti for now, consider build llvm with rtti if we really need to fix it
#
set_target_properties(build_fast_interp_lib PROPERTIES COMPILE_FLAGS " -fno-rtti ")

target_link_libraries(build_fast_interp_lib PUBLIC
  ${LLVM_EXTRA_LINK_LIBRARIES}
)

set_source_files_properties(${GENERATED_FILES_DIR}/fastinterp_fwd_declarations.generated.h PROPERTIES GENERATED true)
set_source_files_properties(${GENERATED_FILES_DIR}/fastinterp_library.generated.h PROPERTIES GENERATED true)
set_source_files_properties(${GENERATED_FILES_DIR}/fastinterp_library.generated.cpp PROPERTIES GENERATED true)

add_library(fastinterp OBJECT
  fast_interp_helper.cpp
  get_fs_base_helper.cpp
  ${GENERATED_FILES_DIR}/fastinterp_library.generated.cpp
)

add_custom_command(
  OUTPUT ${GENERATED_FILES_DIR}/fastinterp_fwd_declarations.generated.h
  OUTPUT ${GENERATED_FILES_DIR}/fastinterp_library.generated.h
  OUTPUT ${GENERATED_FILES_DIR}/fastinterp_library.generated.cpp
  COMMAND ${PROJECT_BINARY_DIR}/fastinterp/build_fast_interp_lib '$<TARGET_OBJECTS:fastinterp_tpl_ir>' '$<TARGET_OBJECTS:fastinterp_tpl_obj>' "${GENERATED_FILES_DIR}"
  DEPENDS $<TARGET_OBJECTS:fastinterp_tpl_ir> $<TARGET_OBJECTS:fastinterp_tpl_obj> build_fast_interp_lib 
)

