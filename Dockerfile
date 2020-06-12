FROM ubuntu:18.04

# install misc dependency
#
RUN apt update && apt install -y git wget tar xz-utils sudo cmake make ninja-build python

RUN apt update && apt-get install -y g++ libtinfo-dev 

# install llvm 10.0.0
#
RUN wget -O llvm.tar.xz https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
RUN tar xf llvm.tar.xz
RUN cp -r clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04/* /usr/local/
RUN rm -rf clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04
RUN rm -f llvm.tar.xz

# build and install LLVMgold.so
#
RUN git clone --depth 1 git://sourceware.org/git/binutils-gdb.git binutils
RUN wget -O llvm_src.tar.xz https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/llvm-10.0.0.src.tar.xz
RUN tar xf llvm_src.tar.xz
RUN cd llvm-10.0.0.src && mkdir build && cd build && cmake -GNinja -DLLVM_BINUTILS_INCDIR=../../binutils/include -DCMAKE_BUILD_TYPE=Release .. && ninja
RUN cp llvm-10.0.0.src/build/lib/LLVMgold.so /usr/local/lib
RUN rm -f llvm_src.tar.xz
RUN rm -rf llvm-10.0.0.src
RUN rm -rf binutils

# set user
#
RUN useradd -ms /bin/bash u
RUN usermod -aG sudo u
RUN echo 'u ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER u
WORKDIR /home/u

