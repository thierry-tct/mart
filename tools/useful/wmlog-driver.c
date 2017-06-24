#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

//#define MARTLLVM_WM_LOG_OUTPUT __WM__OUTPUT__PATH_TO_PROGRAM__TEMPLATE.WM.covlabels

#define str(x) # x
#define xstr(x) str(x)

static unsigned martLLVM_WM_Log__Highest_Mutant_ID = 0;  //XXX Must be overriden by mutation tool when linking
static char *martLLVM_WM_Log__Mutants_Weakly_Killed_Cache = (void*) 0;    //bool[]
static char martLLVM_WM_Log__Newly_Weakly_Killed_Mutants = 0;      //bool

static FILE* martLLVM_WM_Log__file = (void*) 0;

void martLLVM_WM_Log__Function(unsigned id, char cond /*bool*/) 
{
    if (!martLLVM_WM_Log__file) 
    {
        char* MARTLLVM_WM_LOG_OUTPUT_file = getenv(xstr(MART_WM_LOG_OUTPUT));
        if (! MARTLLVM_WM_LOG_OUTPUT_file)
            MARTLLVM_WM_LOG_OUTPUT_file = "mart.defaultFileName.WM.covlabels";
        martLLVM_WM_Log__file = fopen(MARTLLVM_WM_LOG_OUTPUT_file, "a");  //In case many processes(fork). The user shoul make sure to delete this before run
        if (!martLLVM_WM_Log__file) 
        {
            printf("[TEST HARNESS] cannot init weak mutation output file (%s)\n", MARTLLVM_WM_LOG_OUTPUT_file);
            return;
        }
        martLLVM_WM_Log__Mutants_Weakly_Killed_Cache = (char *) malloc ((1+martLLVM_WM_Log__Highest_Mutant_ID) * sizeof (char));
        unsigned ui=0;
        for (;ui <= martLLVM_WM_Log__Highest_Mutant_ID;++ui)
            martLLVM_WM_Log__Mutants_Weakly_Killed_Cache[ui] = 0;   //false
    }

    // with caching
    if (cond && martLLVM_WM_Log__Mutants_Weakly_Killed_Cache[id] == 0)
    {
        fprintf(martLLVM_WM_Log__file, "%u\n", id);
        martLLVM_WM_Log__Mutants_Weakly_Killed_Cache[id] = 1;
        martLLVM_WM_Log__Newly_Weakly_Killed_Mutants = 1;
        //fflush(martLLVM_WM_Log__file); 
    } 
}

void martLLVM_WM_Log__Function_Explicit_FFlush()
{
    if (martLLVM_WM_Log__Newly_Weakly_Killed_Mutants)
    {
        fflush(martLLVM_WM_Log__file);
        martLLVM_WM_Log__Newly_Weakly_Killed_Mutants = 0;
    }
}

