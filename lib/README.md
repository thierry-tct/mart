# Libraries Directory

```llvm-diff``` is helpful to check wheter llvm code differ. 
We made some change on the ```llvm-diff``` from ```llvm``` repo (see _MART_GenMu_PRINTDIFF_).
    - llvm-diff/DiffConsumer.cpp
    - llvm-diff/DiffLog.cpp
    - llvm-diff/DifferenceEngine.cpp
***TODO*** : Make sure that we have such folder for each of the ```llvm``` suported versions (use patches to select the corresponding version at compile time) 


After any change in the folder `operatorsClasses`, fore regeneration of AUTOGEN header using the command: `make CollectMutOpHeaders`.