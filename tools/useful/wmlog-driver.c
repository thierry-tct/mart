#include <stdlib.h>
#include <stdio.h>

//#define MULLVM_WM_LOG_OUTPUT __WM__OUTPUT__PATH_TO_PROGRAM__TEMPLATE.WM.covlabels

#define str(x) # x
#define xstr(x) str(x)

static FILE* muLLVM_WM_Log_file = (void*) 0;

void muLLVM_WM_Log_Function(unsigned id, char cond) 
{
    if (!muLLVM_WM_Log_file) 
    {
        char* MULLVM_WM_LOG_OUTPUT_file = getenv(xstr(MULLVM_WM_LOG_OUTPUT));
        if (! MULLVM_WM_LOG_OUTPUT_file)
            MULLVM_WM_LOG_OUTPUT_file = "defaultFileName.WM.covlabels";
        muLLVM_WM_Log_file = fopen(MULLVM_WM_LOG_OUTPUT_file, "a");
        if (!muLLVM_WM_Log_file) 
        {
            printf("[TEST HARNESS] cannot init weak mutation output file (%s)\n", MULLVM_WM_LOG_OUTPUT_file);
            return;
        }
    }

    // no caching
    if (cond)
    {
        fprintf(muLLVM_WM_Log_file, "%u\n", id);
        fflush(muLLVM_WM_Log_file); 
    } 
}

