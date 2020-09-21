FROM ubuntu:18.04

# install misc dependency
#
RUN apt update && apt install -y git wget tar xz-utils sudo cmake make ninja-build python ccache

RUN apt update && apt-get install -y g++ libtinfo-dev 

# install llvm 10.0.0
#
RUN wget -O llvm.tar.xz https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
RUN tar xf llvm.tar.xz
RUN cp -r clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04/* /usr/local/
RUN rm -rf clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04
RUN rm -f llvm.tar.xz

# set user
#
RUN useradd -ms /bin/bash u
RUN usermod -aG sudo u
RUN echo 'u ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

COPY ccache.conf /etc/ccache.conf

USER u
WORKDIR /home/u

