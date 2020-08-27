### PochiVM -- lightweight framework for easy and efficient code generation
------
PochiVM is a JIT (just-in-time) code-generation framework backed by LLVM C++ API. <br>The goal of PochiVM is to enable any ordinary C++ user with no prior knowledge of LLVM or code-generation to enjoy the benefits of code-generation technology. 

JIT code-generation, the technique that generate binary code on-the-fly, has a wide range of applications from SQL servers to web browsers, and often gives a large performance boost. However, the steep learning curve and the large gap between low-level LLVM APIs and high-level application might have prevented many developers from trying it. This framework aims at bridging this gap, by providing:
 - **Intuitive C-like language interface** to describe the logic of the generated code. Most of the core C language constructs, and certain C++ features including `constructor`/`destructor`/`scoped variable`/`exception` are supported. 
 - **Seamless and efficient interaction** with your C++ codebase. The generated program can access any C++ classes and functions using an intuitive syntax, even if they are templated, overloaded, virtual, or have non-primitive parameter/return types. Moreover, calls to C++ functions **can be inlined** into generated code, allowing the mix of C++ infrastructure and generated code with minimal overhead. 
 - You write everything in completely valid C++ code. No fragile external text-preprocessor magic or C macro magic involved. Even IDE code hinting works (provided a decent IDE like QtCreator).
 - **User-friendly error handling**. Almost all static type errors will be caught by `static_assert`. Like C, all programming errors are caught when you compile the generated-code, never at execution phase.

### Toy Example
In this example, We will codegen a function that takes a `std::vector<int>*` as parameter, and returns the number of distinct positive values in the vector. The generated function will construct a `std::set<int>` on the stack, then iterate through the vector and insert the positive values into the set, and finally return the `size()` of the set. We will demonstrate the basic language constructs, and the seamless interaction between C++ codebase (in this case, the C++ STL, a heavily templated libary) and the generated code. 

```cpp
// Our name and prototype for the generated function.
const char* funcName = "count_distinct_positive_values";
using FnPrototype = int(*)(std::vector<int>*);

// Create a function 'fn' of given prototype and name, with 'v' binding to its first parameter.
auto [fn, v] = NewFunction<FnPrototype>(funcName);
// Create a local variable of type 'std::set<int>'.
// It is not constructed yet -- this is just to specify its type and allow us to cite it later.
auto s = fn.NewVariable<std::set<int>>();
// Create a local variable of type 'std::vector<int>::iterator'
auto it = fn.NewVariable<std::vector<int>::iterator>();

// Create the body of the function
fn.SetBody(
  // Declare and construct local variable 's': default constructor of std::set<int> is called.
  Declare(s),
  // Write a for-loop -- the syntax is almost identical to C++.
  // We invoked C++ methods 'begin()', 'end()' and the overloaded operators '!=', '++'. 
  // We also constructed the variable 'it' using 'v->begin()': C++17 guaranteed-copy-elision
  // will happen. As you can see, the syntax and behavior is "just as if" you were writing C++.
  For(Declare(it, v->begin()), it != v->end(), it++).Do(
    // Write a if-statement, again the syntax is almost identical to C++.
    If(*it > Literal<int>(0)).Then(
      // Unlike C++, we disallow implicitly discarding return values.
      // We explicitly ignore the return value of 'set<int>::insert' using 'IgnoreRet'.
      IgnoreRet(s.insert(*it))
      // Call to 'insert' may throw -- don't worry, the exception will just propagate to the 
      // caller, and all local variables will be destructed, just as you would expect in C++.
    )
  ),
  // Variable 'it' is declared in the for-loop, and at this point it goes out of scope.
  // As you would expect in C++, its destructor will be automatically called at this point. 
  
  // std::set<int>::size() returns 'size_t', while this function returns 'int'. 
  // Both unsigned-to-signed conversion and narrowing conversion are potentially unsafe,  
  // so we require an explicit static_cast for such type conversions.
  Return(s.size().StaticCast<int>())
  // After the return statement, 's' also goes out of scope, and will be destructed.
);

// Validate that the module contains no errors. If an error is detected, 
// human-readable error message can be retrieved in thread_errorContext->m_errorMsg 
TestAssert(ValidateModule());
// Build the module, and retrieve the built function
BuildModule();
FnPrototype generatedFn = GetFunction<FnPrototype>(funcName);

// Invoke the generated function
std::vector<int> vec { -1, -1, 1, 1, 2, 2 };
printf("The vector contains %d distinct positive values.", generatedFn(&vec));  // outputs 2
```

Thanks to the inlining feature, the generated code will be just as efficient as writing the same logic in C++: the calls to the cheap C++ methods (constructors, destructors, `begin()`, `end()`, `std::vector<int>::iterator`'s overloaded `*`, `!=` and `++` operators, etc) will be inlined, and after they are inlined, LLVM optimizer will be able to figure out that `v->end()` is a loop invariant and will only evaluate it once. In the end, you will get the same optimized binary code as you can get by writing the same logic in C++.

Of course, the above example is not really useful -- all logic of the generated function is already known at compile time, so you could have implemented it in C++ directly. However, if the logic is only known at runtime (for example, a SQL engine executing a query from user, or a web browser executing a Javascript snippet on a website), being able to JIT-compile the user logic into native binary code would often be much faster than an interpretative approach.

### Documentation / Getting Started 

For the full documentation, check here. [TODO]

### License

PochiVM uses the same license as LLVM ("Apache License v2.0 with LLVM Exceptions"), check [here](https://releases.llvm.org/10.0.0/LICENSE.TXT).

### Q & A

Q: What types can be used in generated code?

A: There are a few notable limitations in the typesystem, made by intentional design decisions to trade a bit expressiveness for simplicity.
 - The typesystem does not have the concept of `const`-ness or `volatile`-ness. 
 - The typesystem does not support C++11 rvalue-reference type.
 - The typesystem only supports rvalue non-fundamental type (temporary objects) in a few well-defined cases, mostly for interaction with C++ codebase. This is to make the behavior of the code straightforward (no hidden constructor/move constructor/destructors behind the scene due to temporary objects). It also greatly simplifies the implementation and cases needed to test.
 - The generated function's prototype may only have fundamental-typed parameters and return values.
 
Q: Any limitations in accessing C++ infrastructure from generated code?

A: The limitations in the generated code's typesystem have a few direct implications.
 - Since the generated code's typesystem does not have `const`-ness, the`const`-qualifier is ignored in all parameters and return value. For example, if a C++ function returns `const int* const**`, it would become a `int***` in the generated code's typesystem. It is your responsibility to make sure the generated code would not write into unwritable memory.
 - `volatile`-qualifier, and C++11 rvalue-reference is not supported. You cannot call C++ function which parameters or return value contain such types (using them inside the C++ function is completely fine though).
 - Since the generated code's typesystem does not have `const`-ness, the generated code cannot distinguish between overloaded C++ function prototypes that only differs in `const`-ness. One common example is class member `Getter`s, E.g., `const int* SomeClass::Get() const` vs `int* SomeClass::Get()`: you can only make one of them accessible to generated code.

Q: The naming conventions and macros remind me of the codebase in M***QL.

A: Just to clarify any potential concerns, none of the code in this repo was written while I was affiliated to there. I never worked on their internal codegen framework, nor am I aware of any of their design/implementation choices or technical detail. All code in this repo are either independently written by me, or publicly available on the Internet and properly cited if applicable.

Q: What does the name 'PochiVM' mean?

A: 'Pochi' is a Japanese word that means 'Mini' (amongst other meanings). I named it for two reasons:
 - The framework is a bit higher-level than LLVM ("Low-Level Virtual Machine"), but still not so high-level that limits the potential use case. 
 - Design decisions are made with a spirit of minimalism: to provide a minimal set of intuitive APIs while still being expressive enough for complex applications.

