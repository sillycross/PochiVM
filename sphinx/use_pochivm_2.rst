###################################
 Integration Into Existing Project
###################################

PochiVM's intuitive syntax and powerful inlining features is 
at the cost of a somewhat complicated build process. 
Further complicated by the nature of LLVM's sensitive version requirements, 
it is impossible that PochiVM could be provided as a plug-and-use library.

So unfortunately, integrating PochiVM into an existing project with an existing build system is more complicated, 
and requires understanding of PochiVM's internal build process and requirements.
In the rest of the section, we will go through the internals of PochiVM's build system,
so one can figure out how to best integrate PochiVM into an existing project.

Build Process Behind the Scene
===============================

Get LLVM IR of Runtime Library
-------------------------------

We start with the ``.cpp`` files in the ``runtime`` folder.
The first step of the build process is to let ``clang++`` emit the LLVM IR for those source files.
This is done using the clang-specific compiler flag ``-emit-llvm``.
After this step we get a bunch of ``.bc`` files (though CMake still names them with the extension of ``.o``),
which are the LLVM IR bitcode for the C++ sources.

Extract Information from IR
----------------------------

The runtime library builder (which source code resides in ``runtime_lib_builder`` folder) is 
responsible for a list of things (these are skippable technical details):

 - Figure out which C++ entities (functions, member functions, member objects, etc) are registered 
   in ``pochivm_register_runtime.cpp`` by JIT-executing the entry-point function ``__pochivm_register_runtime_library__`` in a sandbox.
 - Extract LLVM metadata and IR information of the runtime library for all registered entities, 
   generate the extracted IR into data files, which will be linked into the main binary, 
   so PochiVM can access them at runtime.
 - Generate C++ header files, so users can express the use of the registered C++ entities in an intuitive syntax
   (that header file is what allows static type-checking of function paramaters, 
   and allows one to write calls in generated code just as if writing in C++).
 - Generate ``libruntime.a`` by invoking ``llc`` to lower the LLVM IR bitcode to ``.o`` object files, 
   and invoking ``ar`` to archive the ``.o`` object files into a library. 
   It is important that we use ``llc`` to convert the ``.bc`` files to object file, 
   not ``clang++`` to re-compile the source file, otherwise we could get inconsistent inlining 
   decisions which result in missing symbols.   
   
In short, the runtime library ``libruntime.a`` must be built using a special build process, 
which as a side-product, also generates certain C++ header and source files needed to build the PochiVM library.
When integrating PochiVM into your own project's build system, you have to keep this build step unchanged.

Build the Rest of the Project
------------------------------

Once the ``runtime`` library is built, the ``PochiVM`` library may be built. 
Then, the rest of the project which depends on ``PochiVM`` may be built. 
It is important that you keep the build dependency that

 - ``runtime`` library depends on ``runtime_lib_builder`` utility.
 - ``PochiVM`` library depends on ``runtime`` library.
 - All code using PochiVM depends on ``PochiVM`` library.

Final Linking Step
-------------------

.. highlight:: cmake

The following CMake link flags and libraries are needed for any binary file that uses PochiVM::
  
  SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -rdynamic ")
  SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${LLVM_EXTRA_LINK_FLAGS} ")
  
  target_link_libraries([your_target_name] PUBLIC
    "-Wl,--whole-archive" runtime "-Wl,--no-whole-archive" 
    runtime_bc  
    pochivm
    ${LLVM_EXTRA_LINK_LIBRARIES} 
  )

The ``-Wl,--whole-archive`` linker flag is important: any function in ``runtime`` library, 
even if not referenced directly by the rest of the codebase, may be called at runtime by generated code, 
so we must tell the linker to keep all symbols.

.. warning::

  PochiVM does not work with ``-flto`` (clang's link-time-optimization feature), 
  since there is a known bug that clang LTO does not respect ``-Wl,--whole-archive`` linker flag.
  
Post-Build Validation Pass
---------------------------

PochiVM also provides a post-build validation utility to validate that nothing has gone wrong on the PochiVM side. 
While this is optional, it is recommended that you run the pass just in case you hit a bug in PochiVM. 
The utility for this validation is in folder ``post_build_verifier``, 
and the validator program takes a single argument, the built binary file to validate. 
You can replicate the CMake logic by looking at how the post-build-pass is implemented in the project root's ``CMakeLists.txt``.

.. note::

  If ``post_build_verifier`` returns an error and you have not modified the build process, 
  it is most likely a bug in PochiVM itself. Please report a bug.

Why PochiVM is Built in Docker
===============================

As you can see, the build steps involve a reflective step: 
the C++ source file is compiled by ``clang++`` to LLVM IR, 
then the LLVM IR is parsed by the ``LLVM`` library and linked back into our application. 

By design of LLVM, each version of ``LLVM`` is **explicitly version-locked with** the ``clang++`` having the same version number. 
This implies that LLVM of version ``A.B.C`` can only understand the LLVM IR output of clang++ of **exactly the same** version number ``A.B.C``. 
Furthermore, LLVM library has no backward or forward API compatibility. 
This means that PochiVM (which is currently using LLVM 10.0.0) may or may not work with another LLVM version. 

So in short, the ``PochiVM`` library and the ``runtime`` library must be built using precisely ``LLVM 10.0.0`` and ``clang++ 10.0.0``.
Of course it is impractical to ask one to install a specific version of clang++ and LLVM in order to build PochiVM. 
This is why we used ``docker`` to provide a virtualized build environment in which we have precise control over the version of the toolchain.

Nonetheless, you don't have to switch your whole project to use ``docker``, or getting locked on a specific compiler version 
(although it would be easier to integrate if you are fine with it). 
Only the source code in ``runtime`` folder must be compiled by that specific version of clang++.
The rest of your project (including the ones using ``pochivm.h``) is free to use whatever compiler that supports C++17 (since ``pochivm.h`` employed many C++17 features), and does not need access to any LLVM headers, and may or may not use docker in the build system. 
Of course the final binary still needs to be linked against LLVM 10.0.0, but one can just store the LLVM libraries statically somewhere in the project by copying them out from docker.



