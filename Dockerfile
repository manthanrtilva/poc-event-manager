FROM ubuntu:24.04
RUN apt-get update && apt-get install -y build-essential cmake git curl wget unzip pkg-config libssl-dev libcurl4-openssl-dev clang-format libfmt-dev gdb htop valgrind libopentracing-dev prometheus-cpp-dev libgtest-dev

# RUN apt-get install -y autoconf automake binutils-x86-64-linux-gnu cmake libaio-dev libboost-all-dev libclang-dev libdouble-conversion-dev libdwarf-dev libevent-dev libgflags-dev libgmock-dev libgtest-dev liblz4-dev libsnappy-dev libsodium-dev libtool libzstd-dev ninja-build zlib1g-dev zstd python3-pex
# RUN git clone https://github.com/facebook/folly.git && cd folly

# clang-format -i -style=llvm `find . -name '*.cpp' -o -name '*.h'` && cmake --build build
