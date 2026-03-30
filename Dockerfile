FROM ubuntu:24.04
RUN apt-get update && apt-get install -y build-essential cmake git curl wget unzip pkg-config libssl-dev libcurl4-openssl-dev clang-format libfmt-dev gdb htop valgrind libopentracing-dev prometheus-cpp-dev libgtest-dev

# clang-format -i -style=llvm `find . -name '*.cpp' -o -name '*.h'` && cmake --build build
