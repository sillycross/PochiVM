# remove -O0 -g flag, and add -O3 flag
# This is very tricky: we are going to compile some crazily compile-time-intensive CPP files and 
# then JIT them to get their symbol-to-code mappings.
# So the total build time is clang's time to compile CPP file + LLVM JIT's time to JIT execute them.
# Despite this is only executed as a build step, so all it impacts is the developer's user experience, 
# we still want to make this step faster so we don't have to wait too long for every build.
#
# -O3 results in better inlining, so less symbols to resolve, so JIT can be faster (significantly). 
# On the other hand, -O3 results in the compiler doing more work, also slowing down compilation significantly 
# if inlining is done too extensively.
#
# So for fastest build time, we always use -O3, but in source code we manually add 'noinline' and 'optnone' 
# to places where inlining actually hurts overall build time, to manually tune for fastest build time.
# 
string(REPLACE " -O0 " " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
string(REPLACE " -g " " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fconstexpr-steps=100000000 -fconstexpr-depth=131072 -ftemplate-depth=131072 ")

SET(FASTINTERP_TPL_SOURCES
  fastinterp_tpl_arithmetic_expr.cpp
  fastinterp_tpl_astvariable.cpp
  fastinterp_tpl_astliteral.cpp
  fastinterp_tpl_comparison_expr.cpp
  fastinterp_tpl_astcallexpr.cpp
  fastinterp_tpl_astreturn.cpp
  fastinterp_tpl_astfunction.cpp
  fastinterp_tpl_return_arith_expr.cpp
  fastinterp_tpl_if_stmt.cpp
)

SET(FASTINTERP_SOURCES
  fast_interp_helper.cpp
  get_fs_base_helper.cpp
)

add_library(fastinterp_tpl_obj OBJECT
  ${FASTINTERP_TPL_SOURCES}
)
# important compiler flags for the object file, must not be changed. see comments in dynamic_specialization_utils.h
# TODO: clang++ currently does not support -mlarge-data-threshold which would hurt a bit of performance
#
set_target_properties(fastinterp_tpl_obj PROPERTIES COMPILE_FLAGS " -mcmodel=medium -fno-pic -fno-pie -fno-exceptions ")

# Unlike runtime_library_builder, it is fine we compile fastinterp_tpl twice
# since the IR is only for extracting symbol names.
# The only source of truth of binary data is the object file.
#
add_library(fastinterp_tpl_ir OBJECT
  ${FASTINTERP_TPL_SOURCES}
)
set_target_properties(fastinterp_tpl_ir PROPERTIES COMPILE_FLAGS " -emit-llvm -fno-exceptions ")

set(generated_src_list "")
foreach(cur_src ${FASTINTERP_TPL_SOURCES})
  set(cur_generated_src "${GENERATED_FILES_DIR}/${cur_src}.lib.generated.cpp")
  list(APPEND generated_src_list "${cur_generated_src}")
  set_source_files_properties(${cur_generated_src} PROPERTIES GENERATED true)
endforeach()

add_executable(build_fast_interp_lib
  build_fast_interp_lib.cpp   
  $<TARGET_OBJECTS:fake_symbol_resolver_util>
  $<TARGET_OBJECTS:runtime_lib_builder_util>
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

add_library(fastinterp OBJECT
  ${FASTINTERP_SOURCES}
  ${generated_src_list}
)

add_custom_command(
  OUTPUT ${GENERATED_FILES_DIR}/fastinterp_fwd_declarations.generated.h
  OUTPUT ${GENERATED_FILES_DIR}/fastinterp_library.generated.h
  OUTPUT ${generated_src_list}
  COMMAND ${PROJECT_BINARY_DIR}/fastinterp/build_fast_interp_lib '$<TARGET_OBJECTS:fastinterp_tpl_ir>' '$<TARGET_OBJECTS:fastinterp_tpl_obj>' "${GENERATED_FILES_DIR}"
  DEPENDS $<TARGET_OBJECTS:fastinterp_tpl_ir> $<TARGET_OBJECTS:fastinterp_tpl_obj> build_fast_interp_lib 
)

