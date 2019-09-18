#include <stdlib.h>

#define str(x) #x
#define xstr(x) str(x)

// XXX Same as in lib/mutation.cpp
extern unsigned klee_semu_GenMu_Mutant_ID_Selector;

// global constructor mutant selector
__attribute__ ((constructor)) void martLLVM_Metamutant_mutant_selector() {
  char *MARTLLVM_mutant_id = getenv(xstr(MART_SELECTED_MUTANT_ID));
  if (MARTLLVM_mutant_id)
    klee_semu_GenMu_Mutant_ID_Selector = atoll(MARTLLVM_mutant_id);
}