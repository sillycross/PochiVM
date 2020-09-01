.. highlight:: c++

###################
 Blocks and Scopes
###################

Block
=========

A ``Block`` is a syntax-sugar construct capable of holding a sequence of :term:`statements<Statement>` (potentially zero). 
It has no effect on program execution. 
A ``Block`` itself is also a :term:`statement<Statement>`, thus it may be implicited converted to a ``Value<void>``.

Like all API classes in PochiVM, the ``Block`` class is a trivially copyable proxy class that may be passed around by value. 
It is just the reference handler to the underlying object. 
Copying a ``Block`` class does not create a new copy the logic stored in the block.

Public Methods
---------------

.. cpp:function:: Block::Block(Value<void>... statements)

Constructor. Create a ``Block`` consisting of zero, one or more statements.

.. cpp:function:: void Block::Append(Value<void> statement)
  
Append a new statement to the end of the block.

.. cpp:function:: Block::operator Value<void>() const
  
A ``Block`` itself is also a statement. Implicit conversion of ``Block`` to a ``Value<void>``. 

Examples
---------

.. code-block:: 

  // append some code to the end of function 'fn'
  auto block = Block(
    Declare(a, 1),
    Declare(b, 2),
    Declare(c, 3),
    Assign(a, b + c)
  );
  fn.GetBody().Append(block);

Scope
======

A ``Scope`` in PochiVM is analogous to a |cppref_scope_link|. 
It holds a sequence of :term:`statements<Statement>` (potentially zero). 
The lifetime of all variables declared in the scope ends when control flow leaves the scope. 
Destructors of such variables are called in reverse order of declaration.
A ``Scope`` itself is also a :term:`statement<Statement>`, thus it may be implicited converted to a ``Value<void>``.

Like all API classes in PochiVM, the ``Scope`` class is a trivially copyable proxy class that may be passed around by value. 
It is just the reference handler to the underlying object. 
Copying a ``Scope`` class does not create a new copy the logic stored in the scope.

.. |cppref_scope_link| raw:: html

   <a href="https://en.cppreference.com/w/cpp/language/scope" target="_blank">scope in C++</a>
   
Public Methods
---------------

.. cpp:function:: Scope::Scope(Value<void>... statements)
  
Constructor. Create a ``Scope`` consisting of zero, one or more statements.

.. cpp:function:: void Scope::Append(Value<void> statement)
  
Append a new statement to the end of the scope.

.. cpp:function:: Scope::operator Value<void>() const
  
A ``Scope`` itself is also a statement. Implicit conversion of ``Scope`` to a ``Value<void>``. 

Examples
---------

.. code-block:: 

  // The order of calls to constructors/destructors will be:
  // A(), B(), ~B(), C(), ~C(), ~A()
  fn.SetBody(
    Declare(A),
    Scope(
      Declare(B)
    ),
    Declare(C)
  );
  
