# Using MART mutant generation
## Test Automation
For use in the integration with other testing tools such ast automated test generation tools, [Muteria](https://github.com/muteria/muteria) is a good way to use `Mart` (Check out the `example_c` usage from [Muteria](https://github.com/muteria/muteria)).

For direct usage of `Mart`, checkout the usage demo video [here](https://youtu.be/V2Hvi_iqiVE).
## Compile your code into LLVM bitcode (.bc) file
Use `clang` to build the C language program (<SourceFile>.c) into LLVM bitcode (<BitCode>.c) with a command of the form:
``` bash
clang -c -emit-llvm -g <SourceFile>.c -o <BitFile>.bc
```
You may use [wllvm](https://github.com/travitch/whole-program-llvm) for large C/C++ projects. 
Compile with `debug` flag enable (`-g` option for C/C++ compilers gcc and clang) and without optimization to have mutants closer to source code mutants.

## Generate the mutants
Use Mart through command line. The usage format is the following:
```bash
<path to mart build dir>/tools/mart [OPTIONS] <bitcode file to mutate>
```

View the help on usage with the command:
```bash
<path to mart build dir>/tools/mart --help
```
## Mutation Generation Configuration
Mutant generation configuration consist in 2 configurations: 
1. **Code locations to mutates (mutation scope):**
This specifies the source files and functions to mutate.
This is done using the option: `-mutant-scope <path/to/mutant scope file>`
2. **Mutation operators to apply:**
This specifies the mutation operator to apply. Mart's way of specifying mutants is flexible. For example, the user has control on the constant to replace when replacing an expression with a constant value.
This is done using the option: `-mutant-config <path./to/mutant config file>`
**_Note_**:_If mutants operators configuration is not specified, the default configuration of 816 transformation rule is used. That default configuration file is located in `<path to build dir>/tools/useful/mconf-scope/default_allmax.mconf`_

Find the details about the format and language to specify the configuration [here](mutation_configuration.md). 
