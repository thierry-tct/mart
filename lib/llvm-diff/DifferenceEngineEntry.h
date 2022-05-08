#ifndef LLVM_DIFF_DIFFERENCE_ENGINE_ENTRY
#define LLVM_DIFF_DIFFERENCE_ENGINE_ENTRY

#if (LLVM_VERSION_MAJOR >= 14)
    #error Support for LLVM version >= 14.0.0 for llvm-diff to be implemented
#elif (LLVM_VERSION_MAJOR >= 11)
    #include "11.0.0/DifferenceEngine.h"
#elif (LLVM_VERSION_MAJOR >= 3) && (LLVM_VERSION_MINOR >= 4)
    #include "3.4.2/DifferenceEngine.h"
#else
    #error LLVM version < 3.4.0 is not supported
#endif

#endif  //LLVM_DIFF_DIFFERENCE_ENGINE_ENTRY