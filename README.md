# MART Framework for Multi-Programming Language Mutation Testing based on LLVM.

```MART``` is a framework for multi-programming language mutation testing based on the [LLVM compiler infrastructure](http://llvm.org/). It enables the application of mutation testing on newly created languages and for programming languages lacking robust mutation tools. Currently, the MART (_LLVM Mutation Artisant_) framework:

1. Generate mutants according to [user specification](docs/mutation_configuration.md) (type of mutation and section of the source code) for any programming language supporting compilation to LLVM.)

2. Can approximate redundant mutants using data dependency in order to improve the cost and effectiveness of mutatiion testing ([mutant selection](https://arxiv.org/abs/1803.07901)).

3. Provides an API for new mutation operations by extending the base class for mutation operators and registering the operation.

4. Run from [Docker](https://docs.docker.com).
---
_Please use the following reference to cite this tool_ 

[1] Thierry Titcheu Chekam, Mike Papadakis, and Yves Le Traon. 2019. Mart: A Mutant Generation Tool for LLVM. In Proceedings of the 27th ACM Joint European Software Engineering Conference and Symposium on the Foundations of Software Engineering (ESEC/FSE �19), August 26�30, 2019, Tallinn, Estonia. ACM, New York, NY, USA, 5 pages. https://doi.org/10.1145/3338906.3341180

---

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
---

## Build
### From Docker
Use [docker](https://docs.docker.com) image to run _Mart_.
#### Get the image Ready
Pull the docker image or build the docker image locally
1. Pulling the image 
```
docker pull thierrytct/mart
```

2. Building the image locally
```
git clone --recursive https://github.com/thierry-tct/mart.git mart/src
docker build --tag thierrytct/mart mart/src
```
#### RunDocker container
run docker container for demo
```
docker run --rm thierrytct/mart
```
or interactively
```
docker run --rm -it thierrytct/mart /bin/bash
```

### From Source 
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
```
git clone --recursive https://github.com/thierry-tct/mart.git src
```

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
 -DLLVM_SRC_PATH=/home/mart/llvm-3.7.1/src \
 -DLLVM_BUILD_PATH=/home/mart/llvm-3.7.1/build \
 -DLLVM_DIR=/home/mart/llvm-3.7.1/build/share/llvm/cmake \
  ../src
```
`Note`: with llvm 6, the `LLVM_DIR` is set to `... build/lib/cmake/llvm` instead of `... build/share/llvm/cmake`.
* make:
compile using make while in the _build_ folder.
```
make CollectMutOpHeaders
make
```
---

## Usage
Checkout the usage demo video [here](https://youtu.be/V2Hvi_iqiVE).
### Compile your code into LLVM bitcode (.bc) file
Use `clang` to build the C language program (<SourceFile>.c) into LLVM bitcode (<BitCode>.c) with a command of the form:
``` bash
clang -c -emit-llvm -g <SourceFile>.c -o <BitFile>.bc
```
You may use [wllvm](https://github.com/travitch/whole-program-llvm) for large C/C++ projects. 
Compile with `debug` flag enable (`-g` option for C/C++ compilers gcc and clang) and without optimization to have mutants closer to source code mutants.

### Generate the mutants
Use Mart through command line. The usage format is the following:
```bash
<path to mart build dir>/tools/mart [OPTIONS] <bitcode file to mutate>
```

View the help on usage with the command:
```bash
<path to mart build dir>/tools/mart --help
```
### Mutation Generation Configuration
Mutant generation configuration consist in 2 configurations: 
1. **Code locations to mutates (mutation scope):**
This specifies the source files and functions to mutate.
This is done using the option: `-mutant-scope <path/to/mutant scope file>`
2. **Mutation operators to apply:**
This specifies the mutation operator to apply. Mart's way of specifying mutants is flexible. For example, the user has control on the constant to replace when replacing an expression with a constant value.
This is done using the option: `-mutant-config <path./to/mutant config file>`
**_Note_**:_If mutants operators configuration is not specified, the default configuration of 816 transformation rule is used. That default configuration file is located in `<path to build dir>/tools/useful/mconf-scope/default_allmax.mconf`_

Find the details about the format and language to specify the configuration [here](docs/mutation_configuration.md). 

---

## TODO
- Fix auto creation of AUTOGEN headers ".inc"
- Fix compiling third-parties/dg (CMake file when pull), should point to right version
- Give build option to enable mutant selection


