
###########################
  Build and Run Unit Test
###########################

.. highlight:: bash
.. warning::

  Only Linux is supported at the moment.
  
PochiVM uses ``docker`` to create a distribution-independent build environment. 
All build dependencies are managed by ``docker`` in its virtualized build environment,
and statically linked to the binary.
So just install ``docker`` on your machine and you are ready to build PochiVM.

Clone the repository::

  git clone https://github.com/sillycross/PochiVM PochiVM
  cd PochiVM
  
The build script is a Python script named ``pochivm_build``. 
There are 3 possible build flavors: ``debug``, ``release`` and ``production``.
The debug and release builds are testing builds with assertions enabled, 
while ``production`` is intended for production use, with full optimizations enabled
and all assertions disabled.

To get a ``debug`` build, run::

  python3 pochivm_build cmake debug
  python3 pochivm_build make debug

The built artifact is located in ``./build/[flavor]/main``, it is the Testing Suite for PochiVM. 
The built artifact is also copied to the project root by the build script.
Run the built artifact::

  ./main

It should finish in a few seconds with no test failures. If not, please report a bug.

To get a release or production build, just change the parameter passed to ``pochivm_build``.
As is all cmake projects, the ``camke`` command is usually only needed for the first time you build the project. 
Later only running ``python3 pochivm_build make [flavor]`` should be enough.

.. note::
  
  The test suite currently does not work for ``production`` build. Failure is expected.
  For ``debug`` and ``release`` build, all tests should pass.
  
Now you are ready to write your first generated program using PochiVM. We will do it in the next part of the tutorial.
 

