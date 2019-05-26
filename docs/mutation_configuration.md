## I. Specifying the mutation scope (source files and functions to mutate)
The scope configuration file is a JSON file where the source files and functions to mutate can be specified in two list. The template is the following:
```
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
LEFTINC(V) --> Mutop2.1, RIGHTDEC(V); Mutop2.2, ASSIGN(V, CONSTVAL(0))
```
In this example we have 3 mutation operators:
- _Mutop1.1_: Replace Sum of two expressions by Their Difference.
- _Mutop1.2_: Replace Sum of two expressions by The negation of their difference Difference.
- _Mutop1.3_: Replace Statement having a Sum by a Trap Statement (Failure).
- _Mutop2.1_: Replace Variable left increment by its right decrement.
- _Mutop2.2_: Replace Variable left increment by an assignement of `0` to it.

The currently Supported _Fragment Elements_ are listed bellow.

### 2. Fragment Elements
The following table gives information about the currently Supported Fragment Elements (The Fragment Element names are case insensitive).

| **Fragment Element** | **Description** |**Matching Fragment**|**Replacing Fragment**| **Fragment Argument**|
| --- | ---| --- | --- | --- |
|  @  | Any Scalar Expression|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  C  | Any Constant|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  V  | Any Scalar Variable|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  A  | Any Address|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  P  | Any Pointer Variable|:heavy_check_mark:|:x:| :heavy_check_mark: |
|  CONSTVAL  |specific constant value: 1 arg (const val)|:x:|:heavy_check_mark:| :heavy_check_mark: |
|  OPERAND  | return an operand of an expression: 1 arg (operand class) |:x:|:heavy_check_mark:| :x: |
|  ABS  | Absolute value of the argument (single argument) |:x:|:heavy_check_mark:| :x: |
|  NEG  | Scalar arithmetic negation|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  ADD  | Sum of two values |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  SUB  | Scalar substraction: 2 args (left value, right value)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  MUL  | scalar multiplication: 2 args|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  DIV  | Scalar division: 2 args (numerator, denominator)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  MOD  | scalar modulo operation: 2 args (left expr, right expr)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  BITAND  | Bit level _and_ :2 args|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  BITNOT  | bit level _not_ :1 arg|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  BITOR  | bit level _or_ :2 args|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  BITSHL  | bit level _shift left_ :2 args (val, shift)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  BITSHR  | bit level _shift right_: 2 args (val, shift)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  BITXOR  | bit level _xor_: 2 args (val, shift)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  LEFTDEC  | Scalar left hand decrement: 1 arg (var)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  LEFTINC  | Scalar left hand increment: 1 arg (var) |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  RIGHTDEC  | Scalar variable right decrement: 1 arg (var))|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  RIGHTINC  | Scalar variable right decrement: 1 arg (var))|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  EQ  | relational equal: 2 args (left expr, right expr)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  NEQ  | relational not equal: 2 args (left expr, right expr) |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  GE  | relational greater or equal: 2 args (left expr, right expr)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  GT  | relational greater than: 2 args (left expr, right expr)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  LE  | relational less or equal: 2 args (left expr, right expr)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  LT  | relational less than: 2 args (left expr, right expr)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  AND  | Logical conjunction|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  OR  | Logical or: 2 args (left expr, right expr))|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  ASSIGN  | variable assignement: 2 args (var, expr)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PADD  | Address addition operation with integer: 2 args (address, integer)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PSUB  | Address substraction operation with integer: 2 args (address, integer)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PLEFTDEC  | Pointer variable left decrement: 1 arg (pointer variable)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PLEFTINC  | Pointer variable left increment: 1 arg (pointer variable) |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PRIGHTDEC  | Pointer variable right decrement: 1 arg (pointer variable) |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PRIGHTINC  | Pointer variable right increment: 1 arg (pointer variable) |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PEQ  | Address relational equal operation: 2 args (left addr, right addr) |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PNEQ  | Address relational not equal operation|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PGE  | Address relational greater or equal operation: 2 args (left addr, right addr)  |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PGT  | Address relational greater than operation: 2 args (left addr, right addr) |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PLE  |Address relational less or equal operation: 2 args (left addr, right addr)  |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PLT  | Address relational less than operation|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PADD_DEREF  | Pointer-int addition followerd by dereference.e.g. *(p+3): 2 args (addess, int) |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PSUB_DEREF  | Address-int substraction followed by dereference.e.g. *(p-3): 2 args (Address, int)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PDEREF_ADD  | Pointer dereference followed by addition.e.g. (*p)+2: 2 args (address, number)|:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PDEREF_SUB  |Pointer dereference followed by substraction.e.g. (*p)-2: 2 args (address, number) |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PLEFTDEC_DEREF  | Pointer variable left decrement followed by dereference.e.g. *(--p): 1 args (pointer variable)    |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PLEFTINC_DEREF  | Pointer variable left increment followed by dereference.e.g. *(--p): 1 args (pointer variable)    |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PRIGHTDEC_DEREF  | Pointer variable right decrement followed by dereference.e.g. *(p--): 1 args (pointer variable)    |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PRIGHTINC_DEREF  | Pointer variable right increment followed by dereference.e.g. *(p++): 1 args (pointer variable)    |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PDEREF_LEFTDEC  |Pointer dereference followed by left decrement.e.g. --(*p): 1 args (pointer variable) |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PDEREF_LEFTINC  |Pointer dereference followed by left increment.e.g. ++(*p): 1 args (pointer variable)  |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PDEREF_RIGHTDEC  |Pointer dereference followed by right decrement.e.g. (*p)--: 1 args (pointer variable)   |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  PDEREF_RIGHTINC  |Pointer dereference followed by right increment.e.g. (*p)++: 1 args (pointer variable)    |:heavy_check_mark:|:heavy_check_mark:| :x: |
|  CALL  | function call: 1 arg (callee)|:heavy_check_mark:|:x:| :x: |
|  NEWCALLEE  | Set new callee of function call: 1 arg (new callee)|:x:|:heavy_check_mark:| :x: |
|  SHUFFLEARGS  | Shuffle the arguments of a function call: 1 arg (number of args to shuffle)|:x:|:heavy_check_mark:| :x: |
|  SWITCH  | Match the a statement: no arg|:heavy_check_mark:|:x:| :x: |
|  SHUFFLECASESDESTS  | Shuffle the destination BBs of a _switch_: 1 arg (number of destination BB to shuffle)|:x:|:heavy_check_mark:| :x: |
|  REMOVECASES  | Remove a specific number of cases for switch instruction randomly: 1 args (number of cases to remove)|:x:|:heavy_check_mark:| :x: |
|  RETURN_BREAK_CONTINUE  | match return, break or continue: no arg|:heavy_check_mark:|:x:| :x: |
|  STMT  | Match any statement|:heavy_check_mark:|:x:| :x: |
|  DELSTMT  | remove statement: no args|:x:|:heavy_check_mark:| :x: |
|  TRAPSTMT  | a trap statement (artificial failure): no arg|:x:|:heavy_check_mark:| :x: |

### 3. Programmatically Generate Mutants Operators Configuration
In order to programmatically create the mutant operators configuration file, there is a helper python script, located in `<path to build dir>/tools/useful/create_mconf.py`, that implement a class named `GlobalDefs`.
The objects of the class `GlobalDefs` have the following iportant properties:
1) `FORMATS`: Is a key value (python dict) of each relevant Fragment element as key, and the key's value is the fragment element's operands as list of operand classes.
   
2) `RULES`: Is a key value (python dict) of each relevant Fragment element as key, and the key's value is the list of compatible fragment element (that can replace this).
   
3) `CLASSES`: Define Fragment elements groups. This is a key value (python dict) having as key, each fragment element and as value the corresponding operator groups. 
Group granularity is represented by different levels separated by the character `'/'`, similarly to the file system path structure on UNIX systems. e.g. 'EXPRESSION-MUTATION/SCALAR/BINARY/AO' is used for Arithmetic binary operations such as '+', '-', '*', '/' as these form the same group. 
Nevertheless, '>', '<' are 'EXPRESSION-MUTATION/SCALAR/BINARY/RO' as they are relational binary operations (Note the common prefix).
Thus '+', '-', '*', '/', '>', '<' belong to the same group 'EXPRESSION-MUTATION/SCALAR/BINARY' for this group granularity.
