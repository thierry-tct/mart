# MuLL Framework for Multi-Programming Language Mutation Testing based on LLVM.

```MuLL``` is a framework for multi-programming language mutation testing based on the [LLVM compiler infrastructure](http://llvm.org/). It enables the application of mutation testing on newly created languages and for programming languages lacking robust mutation tools. Currently, the MuLL framework:

1. Generate mutants according to user specification (type of mutation and section of the source code) for any programming language.)

2. Can approximate redundant mutants using data dependency in order to improve the cost and effectiveness of mutatiion testing.

3. Optimize mutant execution by filtering mutants with no impact of the output for each test case.

4. Provides an API for new mutation operations by extending the base class for mutation operators and registering the operation.

