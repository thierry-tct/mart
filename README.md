# MART Framework for Multi-Programming Language Mutation Testing based on LLVM.

```MART``` is a framework for multi-programming language mutation testing based on the [LLVM compiler infrastructure](http://llvm.org/). It enables the application of mutation testing on newly created languages and for programming languages lacking robust mutation tools. Currently, the MART (_LLVM Mutation Artisant_) framework:

1. Generate mutants according to user specification (type of mutation and section of the source code) for any programming language.)

2. Can approximate redundant mutants using data dependency in order to improve the cost and effectiveness of mutatiion testing.

3. Optimize mutant execution by filtering mutants with no impact of the output for each test case.

4. Provides an API for new mutation operations by extending the base class for mutation operators and registering the operation.

## Requirements
- Linux (Tested on Ubuntu 14.04 and 16.04)
- LLVM >= 3.4
Note: Mart uses [JsonBox](https://github.com/anhero/JsonBox) and [dg](https://github.com/mchalupa/dg)(for mutant selection's dependence analysis), included as git submodules
- cmake >= 3.4.3 (for build)
    ```bash
    sudo apt-get install cmake3
    ```
- gcc >= 4.9.0 or clang ...
    ```bash
    sudo add-apt-repository ppa:ubuntu-toolchain-r/test 
    sudo apt-get update
    sudo apt-get install gcc-4.9 g++-4.9
    which g++ || sudo ln -s /usr/bin/g++-4.9 /usr/bin/g++
    ```

## Build
1. Compile LLVM from source: (example of llvm-3.7.1)
``` bash
svn co http://llvm.org/svn/llvm-project/llvm/tags/RELEASE_371/final llvm-3.7.1/src

svn co http://llvm.org/svn/llvm-project/cfe/tags/RELEASE_371/final llvm-3.7.1/src/tools/clang

mkdir llvm-3.7.1/build && cd llvm-3.7.1/build && cmake ../src && make -j16
```
__Optional__: Install [llvm-cbe](https://github.com/JuliaComputing/llvm-cbe) which works with llvm-3.7 to convert LLVM code to C code
```
git clone https://github.com/JuliaComputing/llvm-cbe.git ../src/projects/llvm-cbe   

cmake ../src && make -j16
```

2. Compile MART form source (using cmake version >= 3.4.3):
* Clone repository with git clone. (```--recursive``` to clone the submodules dg and JsonBox)
* configure setting LLVM variables: 
```bash
mkdir build && cd build

cmake \
 -DLLVM_SRC_PATH=path_to_llvm_source_root \
 -DLLVM_BUILD_PATH=path_to_llvm_build_root \
 -DLLVM_DIR=path_to_llvm_build_root/share/llvm/cmake \
  path_to_mart_source_root
```
_Exemple with the above LLVM-3.7.1_
```bash
git clone https://github.com/thierry-tct/mart.git mart/src

cd mart && mkdir build && cd build

cmake \
 -DLLVM_SRC_PATH=../../llvm-3.7.1/src \
 -DLLVM_BUILD_PATH=../../llvm-3.7.1/build \
 -DLLVM_DIR=../../llvm-3.7.1/build/share/llvm/cmake \
  ../src
```
* make:
compile using make while in the _build_ folder.
```
make CollectMutOpHeaders
make
```

# TODO
- Fix auto creation of AUTOGEN headers ".inc"
- Fix compiling third-parties/dg (CMake file when pull), should point to right version
- Give build option to enable mutant selection


