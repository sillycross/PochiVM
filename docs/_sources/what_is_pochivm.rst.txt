
####################
  What is PochiVM?
####################

PochiVM is a JIT (just-in-time) code-generation framework backed by LLVM C++ API. 

JIT code-generation, the technique of compiling user logic (e.g. a SQL query, or a Javascript snippet) into native binary code on-the-fly, offers drastically improved performance compared with traditional interpretative approach, and is widely used in applications from SQL servers to web browsers. However, the steep learning curve of LLVM, and the large gap between "*what the low-level LLVM APIs provide*" and "*what a high-level application needs*" might have prevented many developers from trying it. 

The goal of PochiVM is to enable any ordinary C++ user with no prior knowledge of LLVM or code-generation to enjoy the benefits of code-generation technology. It provides:

 - **Intuitive C-like language interface** to describe the logic of the generated code. Most of the core C language constructs, and certain C++ features including constructor, destructor, scoped variable and exceptions are supported. You write generated code just like you write the same logic in C/C++.
 - **Seamless and efficient interaction** with your C++ codebase. The generated program can access any C++ classes and functions using an intuitive syntax, even if they are templated, overloaded, virtual, or have non-primitive parameter/return types. Moreover, calls to C++ functions **can be inlined** into generated code, allowing the mix of C++ infrastructure and generated code with minimal overhead. 
 - You write everything in completely valid C++ code. No fragile external text-preprocessor magic or C macro magic involved. Even IDE code hinting works.
 - **User-friendly error handling**. Almost all static type errors will be caught by ``static_assert``. Like C, all programming errors are caught when you compile the generated-code, never at execution phase.
 
In the rest of this tutorial, we will go through the basic usages of PochiVM. For a full documentation of the APIs, check :doc:`Language Construction API </lang_construct>` and :doc:`Interacting with C++ Code </cpp_interact>`.

