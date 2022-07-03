# MART mutants features and selection

This feature is enabled when the variable `MART_MUTANT_SELECTION` is defined during `cmake` build, as following: `-DMART_MUTANT_SELECTION`.

The Mart's generated mutants' code features can be extracted, after running mart, using the tool `mart-selection` located in the folder `tools`. The same tool can be used to predicts the mutans as described on the following paper:

[x] Titcheu Chekam, T., Papadakis, M., Bissyandé, T.F. et al. Selecting fault revealing mutants. Empir Software Eng 25, 434–487 (2020). https://doi.org/10.1007/s10664-019-09778-7 

In this same context, the tool `mart-training` can be used to train the gradient boosted decision tree model described on the paper, which can be used to make the selection.

View the help of the tools using:
```
mart-training --help
``` 
and 
```
mart-selection --help
```

## TODO
- Fix compiling third-parties/dg (CMake file when pull), should point to right version