add_library(pochivm 
  arith_expr_llvm.cpp  
  common_expr_llvm.cpp     
  dummy_hook.cpp      
  logical_operator_llvm.cpp
  cast_expr_llvm.cpp   
  function_proto_llvm.cpp  
  lang_constructs_llvm.cpp
  ast_variable_llvm.cpp
  scoped_variable_manager_llvm.cpp
  exception_helper_llvm.cpp
  ast_catch_throw_llvm.cpp
  codegen_context.cpp
  arith_expr_fastinterp.cpp
  ast_variable_fastinterp.cpp
  function_proto_fastinterp.cpp
  lang_constructs_fastinterp.cpp
  common_expr_fastinterp.cpp
  logical_operator_fastinterp.cpp
  cast_expr_fastinterp.cpp
  scoped_variable_manager_fastinterp.cpp
  ast_catch_throw_fastinterp.cpp
  pochivm_function_pointer.cpp
  $<TARGET_OBJECTS:fastinterp>
)

add_dependencies(pochivm runtime)
add_dependencies(pochivm fastinterp)

