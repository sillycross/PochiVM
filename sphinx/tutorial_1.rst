
########################
  PochiVM Tutorial 101
########################

.. note::
  This section assumes you have completed the steps in :doc:`Build and Run Unit Test </build_and_test>` section.

We are now ready to write our first generated function using PochiVM!

Fibonacci Sequence
===================

As our first task, we will dynamically create a function with signature ``uint64_t fib(int n)``: and yes, it computes the n-th term of the Fibonacci sequence.

Of course, there is not much value in generating this function dynamically: 
we already know all of its logic, so we could have directly implemented it in C++,
and have it compiled by the C++ compiler at build time. However, it is a good demonstration of the basic usages of PochiVM and the overall structure of the project.

First of all, let's create a new C++ source file named ``learn_pochivm.cpp`` in the project root directory. 
PochiVM uses CMake build system. To add our new file to the project, we need to put it into the ``CMakeLists.txt`` file, also in the project root directory. For simplicity, we will just piggy-back on the existing GoogleTest infrastructure. Open ``CMakeLists.txt``, search for ``add_executable(main``, and put our source file ``learn_pochivm.cpp`` at the end of the list.

Now open ``learn_pochivm.cpp``. To use PochiVM, we just need to include ``pochivm.h``. PochiVM uses a thread-local model: in a multi-thread environment, each thread is supposed to build its own module, not interfering with each other. Each thread holds its global contexts in ``thread_local`` global variables. Before we use PochiVM, the thread needs to initialize its global contexts. 

.. note::
  Currently there are 3 different global contexts, but clean-up work is in progress to merge all those contexts into one.

.. highlight:: c++

So the initial (boilerplate) code would look like this::

  #include "gtest/gtest.h"          // For GoogleTest infrastructure
  #include "pochivm.h"              // For all PochiVM APIs
  #include "test_util_helper.h"     
  
  using namespace PochiVM;          // All PochiVM APIs live in this namespace

  TEST(Tutorial, Fibonacci)
  {
      // Initialize PochiVM global contexts for this thread
      // The contexts are automatically destructed on destruction of these variables
      AutoThreadPochiVMContext apv;
      AutoThreadErrorContext arc;
      AutoThreadLLVMCodegenContext alc;

      // code in snippets below are inserted here
  }
  
We will now create a new module, and create our function::

  NewModule("test" /*name*/);
  using FnPrototype = uint64_t(*)(int);
  auto [fn, n] = NewFunction<FnPrototype>("fib");

There is implicitly one module being built by each thread, 
so we just use ``NewModule`` to create a new module, and ``NewFunction`` to create a new function in that module. 
Now ``fn`` refers to our function with the given prototype and its name is ``"fib"``, and ``n`` refers to its first parameter,
which is a variable of type ``int``: these are automatically deduced from the function signature.

We will now give a body to this function, which is the traditional (brute-force) recursive logic to compute the fibonacci sequence::

  fn.SetBody(
    If(n <= Literal<int>(2)).Then(
      Return(Literal<uint64_t>(1))
    ).Else(
      Return(Call<FnPrototype>("fib", n - Literal<int>(1))
             + Call<FnPrototype>("fib", n - Literal<int>(2)))
    )
  );

I hope there's not much need to explain what each line of the above code does -- any one who knows C/C++ should be fairly easy to understand using the intuition.
This is one of the core design philosophies of PochiVM: 
the syntax should look intuitive to any one who knows C/C++, and its behavior should match that intuition as well.

Now we are ready to JIT compile the function to native binary code::

  // Validate that the module does not contain errors.
  // We require doing this in test build before JIT'ing the module 
  TestAssert(thread_pochiVMContext->m_curModule->Validate());
  
  // Unfortunately the API for JIT part is not done yet, so we still need some ugly code 
  thread_pochiVMContext->m_curModule->EmitIR();
  thread_pochiVMContext->m_curModule->OptimizeIR();
  SimpleJIT jit;
  jit.SetModule(thread_pochiVMContext->m_curModule);
  
  // Retrive the built function, and execute it.
  FnPrototype jitFn = jit.GetFunction<FnPrototype>("fib");
  std::cout << "The 20th number in fibonacci sequence is " << jitFn(20) << std::endl;

PochiVM catches most of the static type errors by ``static_assert``, 
so if you made a static type error in your program (e.g. dereference an integer), 
your project will fail to build, and you can immediately know the issue. 
However, not all errors can be caught by ``static_assert`` (e.g. use of an undeclared variable), 
so we require ``Validate()`` the program, at least in test builds. 
If an error is detected, human-readable error message will be stored in ``thread_errorContext->m_errorMsg``.

.. highlight:: bash

Now we are ready to run our first generated function::

  python3 pochivm-build make debug
  ./main --gtest_filter=Tutorial.Fibonacci

.. highlight:: text

and you shoud see the output::

  The 20th number in fibonacci sequence is 6765
  
.. highlight:: c++

.. _ref_after_adding_files_to_runtime:

Call C++ Fn from Generated Code 
=====================================

Doing everything in generated code is hard. 
Almost every project involving JIT has a runtime library which is statically compiled, 
and generated code may call functions provided by the runtime library to use its functionalities. 

Fortunately, one of the main strengths of PochiVM is its seamless and efficient integration 
with the C++ codebase. Generated code can access almost any C++ functions and classes using an intuitive syntax, 
even if they are templated, overloaded, virtual, or have non-primitive parameter/return types.
Furthermore, calls to C++ functions **can be inlined** into generated code, 
so you don't even have to pay the cost of a function call, and the optimizer could also work better. 

We will now demonstrate how to use a C++ class inside the generated code. 

In PochiVM, the folder ``runtime`` holds all the headers and implementations of the runtime library 
which is accessible to the generated code. Let's create a new C++ header file ``tutorial101.h`` 
and a new C++ source file ``tutorial101.cpp`` in that directory. 

There are two steps that we have to do after adding files to ``runtime`` folder: 

 - First, we must add the CPP file to the ``CMakeLists.txt`` in that folder, so it could be built. 
   Open ``CMakeList.txt``, look for ``SET(SOURCES``, add ``tutorial101.cpp`` to the end of that list, and you are all set.
 - Second, we must add the header file to ``pochivm_runtime_headers.h``. 
   This is required by the build infrastructure, so it can have access to all the declarations of the classes.
   In our case, we should add ``#include "tutorial101.h"`` to the end of ``pochivm_runtime_headers.h``. 

Now we are ready to implement our C++ class. Inside ``tutorial101.h``, write::

  #pragma once
  #include <cstdio>
  
  class Tutorial101
  {
  public:
      Tutorial101() : m_x(123) {}
      ~Tutorial101() {
          printf("The class is destructed!\n");
      }
      void Increment(int value);
      void Print() {
          printf("m_x has value %d\n", m_x);
      }
      int m_x;
  };
  
And inside ``tutorial101.cpp``, write::

  #include "tutorial101.h"
  
  Tutorial101::Increment(int value) {
      m_x += value;
  }

Well, it's not doing much useful things, all for demonstration purposes. 
There is one last important step to make the class accessible to generated code. 
For every constructor, member function, member object, or whatever that we want to access from generated code, 
we need to **register** it inside ``pochivm_register_runtime.cpp``.
To register it, in general, we just need its function pointer, or member function pointer, 
or member object pointer (these are just C++ terminologies for different kinds of pointers, 
check |cppref_pointer_link| if you are unfamiliar). So open ``pochivm_register_runtime.cpp``, 
and inside the function body of ``RegisterRuntimeLibrary`` (anywhere is fine), add::

  // Register the constructor, which takes no additional parameters 
  RegisterConstructor<Tutorial101>();
  // Register member function Increment(int value)
  RegisterMemberFn<&Tutorial101::Increment>();
  // Register member function Print()
  RegisterMemberFn<&Tutorial101::Print>();
  // Register member object 'm_x'
  RegisterMemberObject<&Tutorial101::m_x>();
  
.. |cppref_pointer_link| raw:: html

   <a href="https://en.cppreference.com/w/cpp/language/pointer" target="_blank">here</a>

.. highlight:: bash

You don't have to register the destructor manually, it is automatically registered as needed.
You also do not have to register everything in the class. 
If something is not registered, it is just not accessible to generated code. Now build the repository::

  python3 pochivm-build make debug

.. highlight:: c++

It is recommended that you build the repository after adding in new runtime 
library classes and before you write generated code that uses them, since that would allow your IDE to give
you auto-completion hints when you write your generated code (if your IDE is good enough, of course).
Now move back to ``learn_pochivm.cpp`` in the project root, and add a new test::

  TEST(Tutorial, CallCppFunction)
  {
      AutoThreadPochiVMContext apv;
      AutoThreadErrorContext arc;
      AutoThreadLLVMCodegenContext alc;

      NewModule("test" /*name*/);
      using FnPrototype = void(*)();
      auto [fn] = NewFunction<FnPrototype>("call_cpp_fn");
      // Create a new local variable of type 'Tutorial101' for use in the function.
      // The variable is not constructed here. It is constructed by 'Declare'.
      auto v = fn.NewVariable<Tutorial101>();
      
      fn.SetBody(
        Declare(v),
        v.Print(),
        v.Increment(Literal<int>(1)),
        v.Print(),
        Assign(v.m_x(), v.m_x() + Literal<int>(2)),
        v.Print()
      );
      
      TestAssert(thread_pochiVMContext->m_curModule->Validate());

      thread_pochiVMContext->m_curModule->EmitIR();
      thread_pochiVMContext->m_curModule->OptimizeIR();
      SimpleJIT jit;
      jit.SetModule(thread_pochiVMContext->m_curModule);
  
      FnPrototype jitFn = jit.GetFunction<FnPrototype>("call_cpp_fn");
      jitFn();
  }
  
.. highlight:: bash

It should still be fairly straightforward what the code is doing. 
we constructed a local variable ``v`` of type ``Tutorial101`` in the function using the default constructor,
then called its ``Print()`` member method, and then called its ``Increment()`` member method, etc. 
It is just as if you were writing the same logic in C++. Now build and run the test::

  python3 pochivm-build make debug
  ./main --gtest_filter=Tutorial.CallCppFunction

.. highlight:: text

and you should see the following::

  m_x has value 123
  m_x has value 124
  m_x has value 126
  The class is destructed!
  
Yes, when the local variable goes out of the scope (at the end of the function), 
the destructor is automatically called, printing out the last line.
Furthermore, if you investigate the generated LLVM IR  in ``release`` or ``production`` build mode,
you would notice that there are no calls to the C++ functions at all. There are only 4 calls to ``printf``, 
printing out the lines. Everything else, including the local variable ``v`` and the class ``Tutorial101``, is optimized out.
This is because we are able to inline all the calls to the C++ functions (including the ``Increment`` which implementation resides 
in a CPP file), and after they are inlined, the optimizer will be able to further figure out that the values passed to ``printf``
are constants as well, and optimize out everything except ``printf``.

Now you should be familiar with the basic usages of PochiVM as well as its project directory structure. 
You can find the full list of language construction APIs documentation :doc:`here </lang_construct>` 
and the full documentation on interacting with C++ code :doc:`here </cpp_interact>`.

The next and final part of this tutorial will be a more detailed explanation of PochiVM's internals, 
as well as a guide on how to use PochiVM in your own project.

