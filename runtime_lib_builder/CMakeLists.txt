SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti ")

add_library(runtime_lib_builder_util OBJECT
  symbol_list_util.cpp
  sha1.cpp
)

add_executable(dump_symbols
  dump_symbols.cpp   
  $<TARGET_OBJECTS:runtime_lib_builder_util>
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

