# Building mart from source

## Requirements
- Linux (Tested on Ubuntu)
- LLVM >= 3.4 (latest tested with LLVM-13)
Note: Mart uses [JsonBox](https://github.com/anhero/JsonBox) and [dg](https://github.com/mchalupa/dg)(for mutant selection's dependence analysis), included as git submodules
- cmake >= 3.4.3 (for build)
    ```bash
    sudo apt-get install cmake3
    ```
- gcc >= 4.9.0 or clang ... For older Ubuntu versions, use the following to install vewer gcc:
    ```bash
    sudo add-apt-repository ppa:ubuntu-toolchain-r/test 
    sudo apt-get update
    sudo apt-get install gcc-4.9 g++-4.9
    which g++ || sudo ln -s /usr/bin/g++-4.9 /usr/bin/g++
    ```
- fdupes: used by mart to detect equivalent mutants
- git
- GNU Make
- pyhon3
- zlib1g-dev and libtinfo-dev: Required on some systems

You can used the following command to install dependencies.
```
apt install git cmake make llvm clang g++ python3 fdupes zlib1g-dev libtinfo-dev
```

Note: It is also possible to install LLVM from its source as following. Compile LLVM from source: (example of llvm-3.7.1)
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

---

## Build docker image
### Building the image locally
```
git clone --recursive https://github.com/thierry-tct/mart.git mart/src
docker build --tag thierrytct/mart mart/src
```
#### Run Docker container
run docker container for demo
```
docker run --rm thierrytct/mart
```
or interactively
```
docker run --rm -it thierrytct/mart /bin/bash
```

## Manually build from source 
Compile MART form source (using cmake version >= 3.4.3):
* Clone repository with git clone. (```--recursive``` to clone the submodules dg and JsonBox)
```
git clone --recursive https://github.com/thierry-tct/mart.git src
```

* configure setting LLVM variables: 
```bash
mkdir build && cd build
cmake ../src
```

If you have LLVM installed in non-standard paths, or you have several versions of LLVM and want to use a particular one, you must manually specify path to the installation:

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
make
```
---

## Testing
You can run tests with `make tests`. The command runs system tests.


## Build Documentation
This requires [MKdocs](https://www.mkdocs.org/user-guide/installation/), [Doxygen](https://doxygen.nl/index.html) and [Graphviz](https://graphviz.org/).

On Ubuntu, install using the command:
```
sudo apt-get install doxygen graphviz
pip install mkdocs mkdocs-include-dir-to-nav
```

Build the documentation using the command (the documentation wil be available in `<build-dir>/docs/site`):
```
make gen-docs
```

Push the documentation to the Mart documentation web page using the command:
```
make push-docs
```
---
