.. highlight:: c++

#####################
 Declaring Variables
#####################

Same as in C++, a local variable must be ``declared`` before it could be used. 
When the declaration goes :doc:`out of scope</block_scope>`, the lifetime of the variable ends and its destructor is called.

Use of an undeclared or out-of-scope variable is a programming error and will be caught when you ``Validate()`` the generated program.
 
Default Initialization
=======================

.. cpp:function:: template<typename T> Value<void> Declare(Variable<T> var)
  
Declare the variable ``var`` with default initialization. 
Same as you would expect in C++,
if ``T`` is a :term:`primitive type<Primitive Type>`, its initial value is not defined;
if ``T`` is a :term:`C++ type<C++ Type>`, the default constructor of ``T`` will be called. 
If the default constructor is not registered in the runtime, a ``static_assert`` is triggered.

Value / PRValue Initialization
===============================

.. cpp:function:: template<typename T> Value<void> Declare(Variable<T> var, Value<T> value)
  
If ``T`` is a :term:`primitive type<Primitive Type>`, declare and initialize ``var`` with ``value``.
if ``T`` is a :term:`C++ type<C++ Type>`, 
since the only way to get such a ``Value<T>`` is from the return value of a C++ function 
(check :doc:`PochiVM typesystem documentation</typesystem>` for more information), 
the return value is always *constructed-into* the variable. In other words, 
C++17 guaranteed-copy-elision will take place, it is guaranteed that no copy or move constructors will be called. 
The behavior is equivalent to C++::

  new (&var) T(fn(....));
  
where ``fn(....)`` is the C++ function call that returns type ``T``, and ``var`` is an uninitialized variable of type ``T``, 
into which the return value is directly placement-constructed.

Constructor Initialization
===========================

.. cpp:function:: template<typename C> Value<void> Declare(Variable<C> var, Constructor<C> constructorParams)

``C`` must be a :term:`C++ type<C++ Type>`. 
Initialize ``var`` using constructor of ``C``, with constructor parameters specified in ``constructorParams``.
The constructor must be registered, otherwise a ``static_assert`` is triggered.
The below example demonstrates calling the constructors of ``std::vector<int>``::

  auto [fn] = NewFunction<size_t(*)()>("fn");
  auto v1 = fn.NewVariable<std::vector<int>>();
  auto v2 = fn.NewVariable<std::vector<int>>();
  fn.SetBody(
    // call constructor std::vector<int>(size_t count)
    // which initializes 'v' to be a vector of length 100, filled with 0.
    Declare(v1, Constructor<std::vector<int>>(Literal<size_t>(100))),
    // call copy constructor std::vector<int>(const std::vector<int>& other)
    Declare(v2, Constructor<std::vector<int>>(v1)),
    Return(v2.size())	// returns 100
  );
  

