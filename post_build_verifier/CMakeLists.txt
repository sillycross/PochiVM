add_executable(post_build_verifier
  post_build_verifier.cpp                 
)

target_link_libraries(post_build_verifier PUBLIC 
  runtime_bc 
  runtime_bc_validator
  ${LLVM_EXTRA_LINK_LIBRARIES}
)

add_dependencies(post_build_verifier runtime_bc runtime_bc_validator)

