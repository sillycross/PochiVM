.. highlight:: c++

############
 Functions
############

Synopsis
=========

A generated ``Function`` is a the primary interface between generated code and host program. 
You define functions in generated code, which may execute logics, call each other, call C++ functions in host program,
or get called by the host program. 
Same as in C++, each ``Function`` has a **function signature** and a **function body**. 

Like all API classes in PochiVM, the ``Function`` class is a trivially copyable proxy class that may be passed around by value. 
It is just the reference handler to the underlying function object.

Function Signature
-------------------

The signature of a generated function is represented by either a C-style function pointer type 
(e.g. ``int(*)(double, std::vector<int>*)``),
or a ``std::function`` type (e.g. ``std::function<void(int)>``). 
Most of the APIs support both interface.

Inherently limited by the PochiVM typesystem, 
a generated function can only take parameters and return values of types :doc:`supported by the typesystem</typesystem>`,
with the additional exclusion of :term:`C++ types<C++ Type>` that is passed or returned by value. 
Additionally, they currently cannot be reference types, although this limitation may be removed in future.

Function Body
--------------

The function body is the definition of the function. 
It contains a sequence of :term:`statements<Statement>`.

APIs
=====

Create a New Function
----------------------

.. cpp:function:: template<typename FnPrototype> std::tuple<Function, Variable<ParamTypes>...> NewFunction(const std::string& name)

Create a generated function with the given function prototype and function name.
Returns a ``Function``, and a list of ``Variable<ParamType>`` references to its parameters, 
which types are automatically deduced from the function prototype.

The recommended usage is to leverage the C++17 structured-binding syntax::

  using FnPrototype = int(*)(double, std::set<int>*, double***);
  auto [fn, param1, param2, param3] = NewFunction<FnPrototype>("my_generated_function");
  // 'fn' is of type 'Function'
  // 'param1' is of type 'Variable<double>'
  // 'param2' is of type 'Variable<std::set<int>*>'
  // 'param3' is of type 'Variable<double***>'
  
Create Local Variables
-----------------------

.. cpp:function:: template<typename T> Variable<T> Function::NewVariable()

Create a local variable of type ``T``. 
This give you a ``Variable<T>`` handler so you can cite the variable later in the function body to declare and use it.
See also :doc:`Declaring Variables</declare_var>`.

Set Function Body
------------------

.. cpp:function:: void Function::SetBody(Value<void>... statements)

Set the function body to consist of a list of statements. 

.. cpp:function:: Scope Function::GetBody()
  
Get the function body. The function body is always a :doc:`Scope</block_scope>`.

Examples
=========

Below is a minimal example to create a generated function that given integer ``a, b``, returns ``a + b``.

.. code-block::

  // create a function of prototype 'int(*)(int, int)'
  using FnPrototype = int(*)(int, int);
  auto [fn, a, b] = NewFunction<FnPrototype>("a_plus_b");
  // create a local variable of type 'int', just for demonstration purpose
  auto c = fn.NewVariable<int>();
  // set function body
  // could also just write 'Return(a + b)'
  fn.SetBody(
    Declare(c, a + b),
    Return(c)
  );

