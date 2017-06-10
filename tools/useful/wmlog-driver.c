#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

//#define MULLVM_WM_LOG_OUTPUT __WM__OUTPUT__PATH_TO_PROGRAM__TEMPLATE.WM.covlabels

#define str(x) # x
#define xstr(x) str(x)

static unsigned muLLVM_WM_Log__Highest_Mutant_ID = 0;  //XXX Must be overriden by mutation tool when linking
static char *muLLVM_WM_Log__Mutants_Weakly_Killed_Cache = (void*) 0;    //bool[]
static char muLLVM_WM_Log__Newly_Weakly_Killed_Mutants = 0;      //bool

static FILE* muLLVM_WM_Log__file = (void*) 0;

void muLLVM_WM_Log__Function(unsigned id, char cond /*bool*/) 
{
    if (!muLLVM_WM_Log__file) 
    {
        char* MULLVM_WM_LOG_OUTPUT_file = getenv(xstr(MULLVM_WM_LOG_OUTPUT));
        if (! MULLVM_WM_LOG_OUTPUT_file)
            MULLVM_WM_LOG_OUTPUT_file = "defaultFileName.WM.covlabels";
        muLLVM_WM_Log__file = fopen(MULLVM_WM_LOG_OUTPUT_file, "a");  //In case many processes(fork). The user shoul make sure to delete this before run
        if (!muLLVM_WM_Log__file) 
        {
            printf("[TEST HARNESS] cannot init weak mutation output file (%s)\n", MULLVM_WM_LOG_OUTPUT_file);
            return;
        }
        muLLVM_WM_Log__Mutants_Weakly_Killed_Cache = (char *) malloc ((1+muLLVM_WM_Log__Highest_Mutant_ID) * sizeof (char));
        unsigned ui=0;
        for (;ui <= muLLVM_WM_Log__Highest_Mutant_ID;++ui)
            muLLVM_WM_Log__Mutants_Weakly_Killed_Cache[ui] = 0;   //false
    }

    // with caching
    if (cond && muLLVM_WM_Log__Mutants_Weakly_Killed_Cache[id] == 0)
    {
        fprintf(muLLVM_WM_Log__file, "%u\n", id);
        muLLVM_WM_Log__Mutants_Weakly_Killed_Cache[id] = 1;
        muLLVM_WM_Log__Newly_Weakly_Killed_Mutants = 1;
        //fflush(muLLVM_WM_Log__file); 
    } 
}

void muLLVM_WM_Log__Function_Explicit_FFlush()
{
    if (muLLVM_WM_Log__Newly_Weakly_Killed_Mutants)
    {
        fflush(muLLVM_WM_Log__file);
        muLLVM_WM_Log__Newly_Weakly_Killed_Mutants = 0;
    }
}

