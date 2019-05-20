## I. Specifying the mutation scope (source files and functions to mutate)
The scope configuration file is a JSON file where the source files and functions to mutate can be specified in two list. The template is the following:
```json
{
    "Source-Files": [<function1>, <function2>,...],
    "Functions": [<src_file1>, <src_file2>,...]
}
```
Not specifying mutation scope will mean nutating the whole LLVM bitcode module.

---

## II. Specifying Mutants operators
Teh mutation operators to be applied may be specified using a simple description language. The Specification of the language follows.

### 1. Syntax Diagram
![sample](mutop_desc_lang.jpg?raw=true "Mutation Operator Description Language Syntax Diagram")

The above Diagram represent the Syntax Diagram of the Mutation operator description language. 
The Bottom diagram is the representation of both the `Matched Fragment` and the `Compatible Replacement Fragment`.  
__*Note*__: The argument (`@, C, V, A, P`) for the `Compatible Replacing Fragment` must be subset of those specified for the corresponding `Matched Fragment`.

Following is an example of mutation operators configuration:
```
ADD(@1,@2) --> Mutop1.1, SUB(@1, @2); Mutop1.2, SUB(@2, @1); Mutop1.3 TRAPSTMT;
LEFTINF(V) --> Mutop2.1, RIGHTDEC(V); Mutop2.2, ASSIGN(V, CONSTVAL(0))
```
In this example we have 3 mutation operators:
- _Mutop1.1_: Replace Sum of two expressions by Their Difference.
- _Mutop1.2_: Replace Sum of two expressions by The negation of their difference Difference.
- _Mutop1.3_: Replace Statement having a Sum by a Trap Statement (Failure).
- _Mutop2.1_: Replace Variable left increment by its right decrement.
- _Mutop2.2_: Replace Variable left increment by an assignement of `0` to it.

The currently Supported _Fragment Elements_ are listed bellow.

### 2. Fragment Elements
The following table gives information about the currently Supported Fragment Elements.
| **Fragment Element** | **Description** |**Matching Fragment**|**Replacing Fragment**| **Fragment Argument**|
| ------------- |:-------------:| ------------- |:-------------:| ------------- |
|  @  | Any Scalar Expression|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  C  | Any Constant|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  V  | Any Scalar Variable|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  A  | Any Address|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  P  | Any Pointer Variable|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  ABS  | Absolute value of the argument (single argument) |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  ADD  | Sum of two values |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  AND  | Logical conjunction|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  ASSIGN  | variable assignement: 2 args (var, expr)|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  BITAND  | Bit level _and_ :2 args|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  BITNOT  | bit level _not_ :1 arg|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  BITOR  | bit level _or_ :2 args|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  BITSHL  | bit level _shift left_ :2 args (val, shift)|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  BITSHR  | bit level _shift right_: 2 args (val, shift)|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  BITXOR  | bit level _xor_: 2 args (val, shift)|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  CALL  | function call: 1 arg (callee)|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  CONSTVAL  |specific constant value: 1 arg (const val)|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  DELSTMT  | remove statement: no args|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  DIV  | Scalar division: 2 args (numerator, denominator)|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  EQ  | relational equal: 2 args (left expr, right expr)|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  GE  | relational greater or equal: 2 args (left expr, right expr)|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  GT  | relational greater than: 2 args (left expr, right expr)|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  LE  | relational less or equal: 2 args (left expr, right expr)| |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  LEFTDEC  | Scalar left hand decrement: 1 arg (var)|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  LEFTINC  | Scalar left hand increment: 1 arg (var) |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  LT  | relational less than: 2 args (left expr, right expr)|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  MOD  | scalar modulo operation: 2 args (left expr, right expr)|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  MUL  | scalar multiplication: 2 args|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  NEG  | Scalar rithmetic negation|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  NEQ  | relational not equal: 2 args (left expr, right expr) |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  NEWCALLEE  | Set new callee of function call: 1 arg (new callee)|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  OPERAND  | return an operand of an expression: 1 arg (operand class) |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  OR  | Logical or: 2 args (left expr, right expr))|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PADD  | Pointer addition operation with integer: 2 args (address, integer)|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PADD_DEREF  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PDEREF_ADD  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PDEREF_LEFTDEC  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PDEREF_LEFTINC  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PDEREF_RIGHTDEC  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PDEREF_RIGHTINC  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PDEREF_SUB  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PEQ  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PGE  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PGT  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PLE  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PLEFTDEC  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PLEFTDEC_DEREF  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PLEFTINC  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PLEFTINC_DEREF  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PLT  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PNEQ  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PRIGHTDEC  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PRIGHTDEC_DEREF  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PRIGHTINC  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PRIGHTINC_DEREF  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PSUB  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  PSUB_DEREF  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  REMOVECASES  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  RETURN_BREAK_CONTINUE  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  RIGHTDEC  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  RIGHTINC  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  SHUFFLEARGS  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  SHUFFLECASESDESTS  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  STMT  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  SUB  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  SWITCH  | |:heavy_check_mark:|:x:| :heavy_check_mark: |
|  TRAPSTMT  | |:heavy_check_mark:|:x:| :heavy_check_mark: |

### 3. Programmatically Generate Mutants Operators Configuration
In order to programmatically create the mutant operators configuration file, there is a helper python script, located in `<path to build dir>/tools/useful/create_mconf.py`, that implement a class named `GlobalDefs`.
The objects of the class `GlobalDefs` have the following iportant properties:
1) `FORMATS`: Is a key value (python dict) of each relevant Fragment element as key, and the key's value is the fragment element's operands as list of operand classes.
   
2) `RULES`: Is a key value (python dict) of each relevant Fragment element as key, and the key's value is the list of compatible fragment element (that can replace this).
   
3) `CLASSES`: Define Fragment elements groups. This is a key value (python dict) having as key, each fragment element and as value the corresponding operator groups. 
Group granularity is represented by different levels separated by the character `'/'`, similarly to the file system path structure on UNIX systems. e.g. 'EXPRESSION-MUTATION/SCALAR/BINARY/AO' is used for Arithmetic binary operations such as '+', '-', '*', '/' as these form the same group. 
Nevertheless, '>', '<' are 'EXPRESSION-MUTATION/SCALAR/BINARY/RO' as they are relational binary operations (Note the common prefix).
Thus '+', '-', '*', '/', '>', '<' belong to the same group 'EXPRESSION-MUTATION/SCALAR/BINARY' for this group granularity.
