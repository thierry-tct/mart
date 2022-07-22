# Mart Configurtion

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
LEFTINC(V) --> Mutop2.1, RIGHTDEC(V); Mutop2.2, ASSIGN(V, 0)
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
|  @  | Any Scalar Expression|&check;|&cross;| &check; |
|  C  | Any Constant|&check;|&cross;| &check; |
|  V  | Any Scalar Variable|&check;|&cross;| &check; |
|  A  | Any Address|&check;|&cross;| &check; |
|  P  | Any Pointer Variable|&check;|&cross;| &check; |
|  CONSTVAL  |specific constant value: 1 arg (const val)|&cross;|&check;| &cross; |
|  OPERAND  | return an operand of an expression: 1 arg (operand class) |&cross;|&check;| &cross; |
|  ABS  | Absolute value of the argument (single argument) |&cross;|&check;| &cross; |
|  NEG  | Scalar arithmetic negation|&check;|&check;| &cross; |
|  ADD  | Sum of two values |&check;|&check;| &cross; |
|  SUB  | Scalar substraction: 2 args (left value, right value)|&check;|&check;| &cross; |
|  MUL  | scalar multiplication: 2 args|&check;|&check;| &cross; |
|  DIV  | Scalar division: 2 args (numerator, denominator)|&check;|&check;| &cross; |
|  MOD  | scalar modulo operation: 2 args (left expr, right expr)|&check;|&check;| &cross; |
|  BITAND  | Bit level _and_ :2 args|&check;|&check;| &cross; |
|  BITNOT  | bit level _not_ :1 arg|&check;|&check;| &cross; |
|  BITOR  | bit level _or_ :2 args|&check;|&check;| &cross; |
|  BITSHL  | bit level _shift left_ :2 args (val, shift)|&check;|&check;| &cross; |
|  BITSHR  | bit level _shift right_: 2 args (val, shift)|&check;|&check;| &cross; |
|  BITXOR  | bit level _xor_: 2 args (val, shift)|&check;|&check;| &cross; |
|  LEFTDEC  | Scalar left hand decrement: 1 arg (var)|&check;|&check;| &cross; |
|  LEFTINC  | Scalar left hand increment: 1 arg (var) |&check;|&check;| &cross; |
|  RIGHTDEC  | Scalar variable right decrement: 1 arg (var))|&check;|&check;| &cross; |
|  RIGHTINC  | Scalar variable right decrement: 1 arg (var))|&check;|&check;| &cross; |
|  EQ  | relational equal: 2 args (left expr, right expr)|&check;|&check;| &cross; |
|  NEQ  | relational not equal: 2 args (left expr, right expr) |&check;|&check;| &cross; |
|  GE  | relational greater or equal: 2 args (left expr, right expr)|&check;|&check;| &cross; |
|  GT  | relational greater than: 2 args (left expr, right expr)|&check;|&check;| &cross; |
|  LE  | relational less or equal: 2 args (left expr, right expr)|&check;|&check;| &cross; |
|  LT  | relational less than: 2 args (left expr, right expr)|&check;|&check;| &cross; |
|  AND  | Logical conjunction|&check;|&check;| &cross; |
|  OR  | Logical or: 2 args (left expr, right expr))|&check;|&check;| &cross; |
|  ASSIGN  | variable assignement: 2 args (var, expr)|&check;|&check;| &cross; |
|  PADD  | Address addition operation with integer: 2 args (address, integer)|&check;|&check;| &cross; |
|  PSUB  | Address substraction operation with integer: 2 args (address, integer)|&check;|&check;| &cross; |
|  PLEFTDEC  | Pointer variable left decrement: 1 arg (pointer variable)|&check;|&check;| &cross; |
|  PLEFTINC  | Pointer variable left increment: 1 arg (pointer variable) |&check;|&check;| &cross; |
|  PRIGHTDEC  | Pointer variable right decrement: 1 arg (pointer variable) |&check;|&check;| &cross; |
|  PRIGHTINC  | Pointer variable right increment: 1 arg (pointer variable) |&check;|&check;| &cross; |
|  PEQ  | Address relational equal operation: 2 args (left addr, right addr) |&check;|&check;| &cross; |
|  PNEQ  | Address relational not equal operation|&check;|&check;| &cross; |
|  PGE  | Address relational greater or equal operation: 2 args (left addr, right addr)  |&check;|&check;| &cross; |
|  PGT  | Address relational greater than operation: 2 args (left addr, right addr) |&check;|&check;| &cross; |
|  PLE  |Address relational less or equal operation: 2 args (left addr, right addr)  |&check;|&check;| &cross; |
|  PLT  | Address relational less than operation|&check;|&check;| &cross; |
|  PADD_DEREF  | Pointer-int addition followerd by dereference.e.g. *(p+3): 2 args (addess, int) |&check;|&check;| &cross; |
|  PSUB_DEREF  | Address-int substraction followed by dereference.e.g. *(p-3): 2 args (Address, int)|&check;|&check;| &cross; |
|  PDEREF_ADD  | Pointer dereference followed by addition.e.g. (*p)+2: 2 args (address, number)|&check;|&check;| &cross; |
|  PDEREF_SUB  |Pointer dereference followed by substraction.e.g. (*p)-2: 2 args (address, number) |&check;|&check;| &cross; |
|  PLEFTDEC_DEREF  | Pointer variable left decrement followed by dereference.e.g. *(--p): 1 args (pointer variable)    |&check;|&check;| &cross; |
|  PLEFTINC_DEREF  | Pointer variable left increment followed by dereference.e.g. *(--p): 1 args (pointer variable)    |&check;|&check;| &cross; |
|  PRIGHTDEC_DEREF  | Pointer variable right decrement followed by dereference.e.g. *(p--): 1 args (pointer variable)    |&check;|&check;| &cross; |
|  PRIGHTINC_DEREF  | Pointer variable right increment followed by dereference.e.g. *(p++): 1 args (pointer variable)    |&check;|&check;| &cross; |
|  PDEREF_LEFTDEC  |Pointer dereference followed by left decrement.e.g. --(*p): 1 args (pointer variable) |&check;|&check;| &cross; |
|  PDEREF_LEFTINC  |Pointer dereference followed by left increment.e.g. ++(*p): 1 args (pointer variable)  |&check;|&check;| &cross; |
|  PDEREF_RIGHTDEC  |Pointer dereference followed by right decrement.e.g. (*p)--: 1 args (pointer variable)   |&check;|&check;| &cross; |
|  PDEREF_RIGHTINC  |Pointer dereference followed by right increment.e.g. (*p)++: 1 args (pointer variable)    |&check;|&check;| &cross; |
|  CALL  | function call: 1 arg (callee)|&check;|&cross;| &cross; |
|  NEWCALLEE  | Set new callee of function call: 1 arg (new callee)|&cross;|&check;| &cross; |
|  SHUFFLEARGS  | Shuffle the arguments of a function call: 1 arg (number of args to shuffle)|&cross;|&check;| &cross; |
|  SWITCH  | Match the a statement: no arg|&check;|&cross;| &cross; |
|  SHUFFLECASESDESTS  | Shuffle the destination BBs of a _switch_: 1 arg (number of destination BB to shuffle)|&cross;|&check;| &cross; |
|  REMOVECASES  | Remove a specific number of cases for switch instruction randomly: 1 args (number of cases to remove)|&cross;|&check;| &cross; |
|  RETURN_BREAK_CONTINUE  | match return, break or continue: no arg|&check;|&cross;| &cross; |
|  STMT  | Match any statement|&check;|&cross;| &cross; |
|  DELSTMT  | remove statement: no args|&cross;|&check;| &cross; |
|  TRAPSTMT  | a trap statement (artificial failure): no arg|&cross;|&check;| &cross; |

### 3. Programmatically Generate Mutants Operators Configuration
In order to programmatically create the mutant operators configuration file, there is a helper python script, located in `<path to build dir>/tools/useful/create_mconf.py`, that implement a class named `GlobalDefs`.

The objects of the class `GlobalDefs` have the following iportant properties:

1) `FORMATS`: Is a key value (python dict) of each relevant Fragment element as key, and the key's value is the fragment element's operands as list of operand classes.
   
2) `RULES`: Is a key value (python dict) of each relevant Fragment element as key, and the key's value is the list of compatible fragment element (that can replace this).
   
3) `CLASSES`: Define Fragment elements groups. This is a key value (python dict) having as key, each fragment element and as value the corresponding operator groups. 
Group granularity is represented by different levels separated by the character `'/'`, similarly to the file system path structure on UNIX systems. e.g. 'EXPRESSION-MUTATION/SCALAR/BINARY/AO' is used for Arithmetic binary operations such as '+', '-', '*', '/' as these form the same group. 
Nevertheless, '>', '<' are 'EXPRESSION-MUTATION/SCALAR/BINARY/RO' as they are relational binary operations (Note the common prefix).
Thus '+', '-', '*', '/', '>', '<' belong to the same group 'EXPRESSION-MUTATION/SCALAR/BINARY' for this group granularity.
