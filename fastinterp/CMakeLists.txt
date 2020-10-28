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
  fastinterp_tpl_cdecl_interface.cpp
  fastinterp_tpl_partial_inline_lhs_assign.cpp
  fastinterp_tpl_inline_arith_expr.cpp
  fastinterp_tpl_outlined_arith_expr.cpp
  fastinterp_tpl_outlined_return.cpp
  fastinterp_tpl_inline_return.cpp
  fastinterp_tpl_partial_inline_arith_expr.cpp
  fastinterp_tpl_inline_assign.cpp
  fastinterp_tpl_inline_assign_arith_expr.cpp
  fastinterp_tpl_outlined_assign.cpp
  fastinterp_tpl_inline_comparison_expr.cpp
  fastinterp_tpl_inline_comparison_conditional_branch.cpp
  fastinterp_tpl_partial_inline_comparison_conditional_branch.cpp
  fastinterp_tpl_astvariable.cpp
  fastinterp_tpl_astderefvariable.cpp
  fastinterp_tpl_partial_inline_pointer_arith.cpp
  fastinterp_tpl_partial_inline_rhs_assign.cpp
  fastinterp_tpl_call_expr.cpp
  fastinterp_tpl_call_expr_check_exception.cpp
  fastinterp_tpl_call_expr_store_param.cpp
  fastinterp_tpl_call_expr_spill_stackaddr.cpp
  fastinterp_tpl_partial_inline_comparison_expr.cpp
  fastinterp_tpl_outlined_comparison_expr.cpp
  fastinterp_tpl_abort_trap.cpp
  fastinterp_tpl_inline_dereference.cpp
  fastinterp_tpl_partial_inline_rhs_deref.cpp
  fastinterp_tpl_partial_inline_lhs_deref.cpp
  fastinterp_tpl_outlined_deref.cpp
  fastinterp_tpl_astliteral_mcmedium.cpp
  fastinterp_tpl_astliteral_mcsmall.cpp
  fastinterp_tpl_noop.cpp
  fastinterp_tpl_outlined_conditional_branch.cpp
  fastinterp_tpl_inline_comparison_condbr_unpredictable.cpp
  fastinterp_tpl_partial_inline_comparison_condbr_unpredictable.cpp
  fastinterp_tpl_outlined_condbr_unpredictable.cpp
  fastinterp_tpl_logical_and_or.cpp
  fastinterp_tpl_logical_not.cpp
  fastinterp_tpl_pointer_cast.cpp
  fastinterp_tpl_static_cast.cpp
  fastinterp_tpl_call_expr_enter_cpp_fn.cpp
  fastinterp_tpl_return_spilled_location.cpp
  fastinterp_tpl_call_expr_call_dtor.cpp
  fastinterp_tpl_call_expr_cpp_const_primitive_ref.cpp
  fastinterp_tpl_throw_exception.cpp
  fastinterp_tpl_call_expr_call_dtor_caller.cpp
  fastinterp_tpl_static_cast_u64_double.cpp
  fastinterp_tpl_outlined_pointer_arithmetic.cpp
)

SET(FASTINTERP_SOURCES
  fastinterp_codegen_helper.cpp
  x86_64_get_fs_base_helper.cpp
)

add_library(fastinterp_tpl_ir OBJECT
  ${FASTINTERP_TPL_SOURCES}
)
# "-Walloca" is required: LLVM seems to generate buggy assembly in GHC calling convention 
# if variable-sized alloca is used. It failed to recognize that rbp is garbaged by the call.
# 
set_target_properties(fastinterp_tpl_ir PROPERTIES COMPILE_FLAGS " -emit-llvm -fno-exceptions -Walloca ")

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
  COMMAND ${PROJECT_BINARY_DIR}/fastinterp/build_fast_interp_lib '$<TARGET_OBJECTS:fastinterp_tpl_ir>' "${GENERATED_FILES_DIR}"
  DEPENDS $<TARGET_OBJECTS:fastinterp_tpl_ir> build_fast_interp_lib 
)

