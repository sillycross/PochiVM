add_library(fastinterp_tpl_obj OBJECT
  fastinterp_tpl.cpp
)

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

target_link_libraries(build_fast_interp_lib PUBLIC
  ${LLVM_EXTRA_LINK_LIBRARIES}
)

add_custom_command(
  OUTPUT "x.txt"  # FIXME
  COMMAND ${PROJECT_BINARY_DIR}/fastinterp/build_fast_interp_lib '$<TARGET_OBJECTS:fastinterp_tpl_ir>' '$<TARGET_OBJECTS:fastinterp_tpl_obj>'
  DEPENDS $<TARGET_OBJECTS:fastinterp_tpl_ir> $<TARGET_OBJECTS:fastinterp_tpl_obj> build_fast_interp_lib 
)

