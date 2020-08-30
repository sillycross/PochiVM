
###############################
  A New Project Using PochiVM
###############################

It is easy to start a new project from scratch using the existing PochiVM's build infrastructure. 
You may download an empty project template from the Github Release pages (UNDONE). 

Alternatively, you may begin with the unit test repository, 
and remove all the ``test_*.h/cpp`` files in the project root folder and ``runtime`` folder,
and purge relavent code in ``pochivm_runtime_headers.h``, ``pochivm_register_runtime.cpp`` and ``CMakeLists.txt``. 

The repository provides 3 build modes by default:

 - ``debug`` build is a testing build for debugging purpose. The code is compiled with ``-O0 -g`` for best debugging experience, and all assertions are enabled. The emitted IR for generated code is not optimized either, for best readability.
 - ``release`` build is a **testing build** compiled with optimizations enabled (``-O3 -g -DNDEBUG``), to catch bugs that only show up when compiler optimization takes place. 
   ``assert()`` are disabled, but ``TestAssert()`` are enabled.
   Notably, by default LLVM optimization for generated code is only enabled in ``release`` or ``production`` build. The feature of inlining C++ function calls from generated code can also only happen in ``release`` or ``production`` build.
 - ``production`` build is for production use, with optimizations enabled and no debug symbols (``-O3 -DNDEBUG``). 
   Both ``assert()`` and ``TestAssert()`` are disabled. 

Also keep in mind that once you added a header file to folder ``runtime``, you need to update ``pochivm_runtime_headers.h`` to include it (check :ref:`ref_after_adding_files_to_runtime` in tutorial).  

With the above in mind, you should be able to develop your project in the PochiVM template just like any other project. 

