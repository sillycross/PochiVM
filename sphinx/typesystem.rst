
################
 The Typesystem
################

Supported Types
================

Like C++, PochiVM is static-typed. All type information must be known at the time a generated program is compiled into native binary.

PochiVM currently supports the following types:

 - Most of the C++ fundamental types, specifically ``void``, ``bool``, ``uint8_t``, ``int8_t``, ``uint16_t``, ``int16_t``, ``uint32_t``, ``int32_t``, ``uint64_t``, ``int64_t``, ``float``, ``double``
 - Any ``C++ class`` type registered in ``pochivm_register_runtime.cpp``
 - Pointers to all above types, and pointers to pointers
 
.. note::
  Support for remaining C++ fundamental types (the most important being ``char``) is a TODO.

Notable valid C++ types that do not fall into the above category include:

 - ``const``-qualified or ``volatile``-qualified types.
 - C-style function-pointer, member-function-pointer, or member-object-pointer types.

Note that the template-parameter part in a C++ class is never subject to the above restriction. 
For example, ``std::function<void(int)>`` is just a C++ class type (as a whole). 
PochiVM does not care and is unaware of whatever is in the template parameter. 

Terminology
============

Throughout the documentation, we will use the following terminology for types.

.. glossary::

  C++ Type
    A ``C++ class`` type registered in ``pochivm_register_runtime.cpp`` thus accessible to generated code, 
    typically denoted by ``C`` in relavent contexts. 
    For example, ``std::set<int>`` is a C++ type.
    
  Primitive Type
    A type that is not a :term:`C++ type<C++ Type>`. 
    For example, ``int**``, ``std::set<int>*``, ``bool`` are primitive types.

Value, Reference and Variable
==============================

.. cpp:class:: template<typename T> Value

``Value<T>`` is probably the most important class in PochiVM. 
It is analogous to a RValue of type ``T`` in C++, which resulted from some computation. 
for example, the result of expression ``a+b+c``.
The APIs of this class are documented in a :doc:`standalone section<basic_class_value>`.
For now, it's sufficient to understand that it represents a RValue of type ``T``.

.. cpp:class:: template<typename T> Reference

Similarily, a ``Reference<T>`` represents a LValue-reference of type ``T``. 
It is analogous to a RValue of type ``T&`` in C++.

For a :term:`primitive type<Primitive Type>` ``T``, ``Reference<T>`` inherits ``Value<T>``, 
which implies that a ``Reference<T>`` can be implicitly converted to a ``Value<T>`` and passed 
to any API that accepts a ``Value<T>`` parameter.
The resulted ``Value<T>`` contains the value stored in the reference at the time the dereference is done in program execution. 

However, for a :term:`C++ type<C++ Type>` ``C``, 
dereferencing a LValue-reference to obtain a RValue involves a call to the copy constructor of class ``C``,
This is an operation hidden behind the scene. 
We want our generated program to have straight-forward behavior, 
so we disallow such conversions. Therefore for :term:`C++ type<C++ Type>` ``C``, ``Reference<C>`` **does not** inherit ``Value<C>``.

``Reference<C>`` is also specialized to provide APIs to access any member functions and objects of class ``C`` 
that are registered in ``pochivm_register_runtime.cpp``.
E.g., suppose the member function ``void std::vector<int>::push_back(const int&)`` is registered, 
class ``Reference<std::vector<int>>`` would be specialized to have a member function ``Value<void> push_back(Value<int>)``.
Then, given a variable ``a`` of type ``Reference<std::vector<int>>``,
one can write ``a.push_back(Literal<int>(1))`` to call the ``push_back`` member function and push a ``1`` to the end of the vector.
For more information, see :doc:`Interacting with C++ Runtime</cpp_interact>`.

The "operation hidden behind the scene" issue also applies for ``Value<C>`` itself.
A RValue of a C++ class ``C`` (aka a temporary object) has to be destructed when the |cppref_temp_lifetime_link| -- 
an undesirable hidden operation behind the scene, 
not even considering the additional trickiness to define "full expression" in PochiVM in a precise and intuitive way.
Therefore, we do not in general support ``Value<C>``. 
There is only one way you can get an instance of ``Value<C>``: 
from the return value of a C++ function. And this ``Value<C>`` has only one usage:
either being move-assigned or being in-place-constructed (C++17 guaranteed copy-elision applies here) into a ``Reference<C>``. 
In this sense, ``Value<C>`` is analogous to a |cppref_prvalue_link| of type ``C`` under C++17 definition.

.. |cppref_temp_lifetime_link| raw:: html

   <a href="https://en.cppreference.com/w/cpp/language/lifetime" target="_blank">full-expression is evaluated</a>
   
.. |cppref_prvalue_link| raw:: html

   <a href="https://en.cppreference.com/w/cpp/language/value_category" target="_blank">prvalue</a>

.. cpp:class:: template<typename T> Variable

The last important class in PochiVM is ``Variable<T>``. It is analogous to a local variable of type ``T`` in C++.
As one would expect, it must be declared (and initialized by a constructor if it has a :term:`C++ type<C++ Type>`) before it can be used,
and when the declaration goes out of scope, its destructor will be called. 

This class inherits ``Reference<T>``, so it may be passed to any API that takes a ``Reference<T>`` 
(or ``Value<T>``, if T is not a :term:`C++ type<C++ Type>`). The semantics is clear: a local variable is obviously a reference.

Finally, C++11 rvalue-reference type (``T&&``) is not supported in PochiVM, thus does not have an analogy in the typesystem. 

The table below summarizes the PochiVM typesystem: 

.. list-table:: 
   :widths: 25 25 50
   :header-rows: 1

   * - PochiVM Type
     - C++ Analogy
     - Inheritance
   * - ``Value<T>`` for primitive type ``T`` 
     - rvalue of type ``T``
     - None
   * - ``Reference<T>`` for primitive type ``T`` 
     - rvalue of type ``T&``
     - Inherits ``Value<T>``
   * - ``Value<C>`` for C++ type ``C`` 
     - |cppref_prvalue_link| of type ``C``
     - None
   * - ``Reference<C>`` for C++ type ``C`` 
     - rvalue of type ``C&``
     - **None**
   * - ``Variable<T>`` for any type ``T`` 
     - local variable of type ``T``
     - Inherits ``Reference<T>``

     
