# GPUDetection
An experimental LLVM Pass implementation for GPU codes' detection

# Installation
Just download the src codes from this repo and compile it as the LLVM compiling. Make sure the cmake 3.5 or higher is ready on your compiling machine.

Here are some example compiling steps(the ../llvm9 means that the root of this project's code files):
1. $ mkdir build
2. $ cmake -DCMAKE_BUILD_TYPE=Release ../llvm9
3. $ make -j12

After finishing the compiling process, the clang/clang++ toolchain will be ready under the ./build/bin/ dir.
