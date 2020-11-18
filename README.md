# GPUDetection
An experimental LLVM Pass implementation for GPU codes' detection

# Installation
Just download the src codes from this repo and compile it as the LLVM compiling. Make sure the cmake 3.5 or higher is ready on your compiling machine.

Here are some example compiling steps(the ../llvm9 means that the root of this project's code files):
1. $ mkdir build
2. $ cmake -DCMAKE_BUILD_TYPE=Release ../llvm9
3. $ make -j12

After finishing the compiling process, the clang/clang++ toolchain will be ready under the ./build/bin/ dir.

# Usage
After finishing compiling project, the executable bin files will be generated under ./bin dir and the clang++ is there. You can directly use clang++ to load the DetectionPass module(./build/lib/LLVMDetection.so) when compiling src code files. The commend line could be like following:
```shell
$ ./clang++  -Xclang -load -Xclang ../build/lib/LLVMDetection.so --cuda-gpu-arch=sm_61 -I/usr/local/cuda-10.1/include -I/usr/local/cuda-10.1/samples/common/inc -emit-llvm -S cuda_gpgpu_sample.cu
```
