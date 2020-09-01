
############################
 Expressions and Statements
############################

.. glossary::

  Expression
  
    An expression is a ``Value<T>`` or ``Reference<T>`` instance (where ``T`` is not ``void``) 
    obtained as a result from a series of operations.
    It is analogous to a rvalue of type ``T`` (for ``Value<T>``) or type ``T&`` (for ``Reference<T>``) obtained as a result of some work in C++.
    Since it is the result of some work, PochiVM disallows implicitly ignoring it. 
    It must be either fed into further operations as input, or explicitly ignored by using the ``IgnoreRet`` API (TODO).  
 
  Statement
  
    A statement is a ``Value<void>`` instance obtained as a result from a series of operations.
    Its C++ analogy is an arbitrary piece of code which yields no return value
    (e.g. a function call returning ``void``, or a language construct like if-statement, for-loop).
    
Every generated function in PochiVM is constituted by a function signature, 
and a sequence of :term:`statements<Statement>` in its function body, similar to C++. 

