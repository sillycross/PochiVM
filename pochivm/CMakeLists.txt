add_library(pochivm 
  arith_expr_llvm.cpp  
  common_expr_llvm.cpp     
  dummy_hook.cpp      
  logical_operator_llvm.cpp
  cast_expr_llvm.cpp   
  function_proto_llvm.cpp  
  lang_constructs_llvm.cpp
  ast_variable_llvm.cpp
  destructor_helper_llvm.cpp
  exception_helper_llvm.cpp
  ast_catch_throw_llvm.cpp
  codegen_context.cpp
  arith_expr_fastinterp.cpp
  ast_variable_fastinterp.cpp
  function_proto_fastinterp.cpp
  common_expr_fastinterp.cpp
  $<TARGET_OBJECTS:fastinterp>
)

add_dependencies(pochivm runtime)
add_dependencies(pochivm fastinterp)

