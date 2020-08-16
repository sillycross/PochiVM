# fake_symbol_resolver class inherits a LLVM class,
# so we must compile it with -fno-rtti
#
add_library(fake_symbol_resolver_util OBJECT
  fake_symbol_resolver.cpp
)
set_target_properties(fake_symbol_resolver_util PROPERTIES COMPILE_FLAGS " -fno-rtti ")

add_library(runtime_lib_builder_util OBJECT
  symbol_list_util.cpp
  sha1.cpp
)

add_executable(dump_symbols
  dump_symbols.cpp   
  $<TARGET_OBJECTS:runtime_lib_builder_util>
  $<TARGET_OBJECTS:fake_symbol_resolver_util>
) 

target_link_libraries(dump_symbols PUBLIC
  ${LLVM_EXTRA_LINK_LIBRARIES}
)

add_executable(update_symbol_matches
  update_symbol_matches.cpp   
  $<TARGET_OBJECTS:runtime_lib_builder_util>
) 

target_link_libraries(update_symbol_matches PUBLIC
  ${LLVM_EXTRA_LINK_LIBRARIES}
)

add_executable(build_runtime_lib
  build_runtime_lib.cpp   
  $<TARGET_OBJECTS:runtime_lib_builder_util>
)

target_link_libraries(build_runtime_lib PUBLIC
  ${LLVM_EXTRA_LINK_LIBRARIES}
)

add_executable(build_lib_wrapper
  build_lib_wrapper.cpp   
)

