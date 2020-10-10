add_library(pochivm 
  arith_expr.cpp  
  common_expr.cpp     
  dummy_hook.cpp      
  logical_operator.cpp
  cast_expr.cpp   
  function_proto.cpp  
  lang_constructs.cpp
  ast_variable.cpp
  destructor_helper.cpp
  exception_helper.cpp
  ast_catch_throw.cpp
  codegen_context.cpp
  arith_expr_fastinterp.cpp
  $<TARGET_OBJECTS:fastinterp>
)

add_dependencies(pochivm runtime)
add_dependencies(pochivm fastinterp)

