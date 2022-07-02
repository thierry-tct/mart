# MART Mutation Testing Tool for LLVM Code.

```MART``` is a mutation testing tool for [LLVM](http://llvm.org) code. Mart is based on the [LLVM compiler infrastructure](http://llvm.org/). It enables the application of mutation testing on languages that can be compiled into LLVM code. MART (_LLVM Mutation Artisant_) implements the following features:

1. Generate mutants according to [user specification](docs/mutation_configuration.md) (type of mutation and section of the source code) for any programming language supporting compilation to LLVM.)

2. Can approximate redundant mutants using data dependency in order to improve the cost and effectiveness of mutatiion testing ([mutant selection](https://link.springer.com/article/10.1007/s10664-019-09778-7)).

3. Provides an API for new mutation operations by extending the base class for mutation operators and registering the operation.

4. [Docker](https://docs.docker.com) image available.


For further information and documentation, see the [webpage](https://thierry-tct.github.io/mart).


---
_Please use the following reference to cite this tool_ 

[1] Thierry Titcheu Chekam, Mike Papadakis, and Yves Le Traon. 2019. Mart: a mutant generation tool for LLVM. In Proceedings of the 2019 27th ACM Joint Meeting on European Software Engineering Conference and Symposium on the Foundations of Software Engineering (ESEC/FSE 2019). Association for Computing Machinery, New York, NY, USA, 1080–1084. https://doi.org/10.1145/3338906.3341180
