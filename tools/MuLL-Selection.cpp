/**
 * -==== MuLL-Selection.cpp
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Main source file that will be compiled to execute mutation and possibly selection.
 */
 
#include <string>
#include <ctime>
#include <fstream>
#include <sstream>
#include <sys/stat.h>   //mkdir, stat
#include <sys/types.h>  //mkdir, stat
#include <libgen.h> //basename
#include <unistd.h>  // fork, execl
#include <sys/wait.h> //wait


#include "ReadWriteIRObj.h"
#include "../lib/mutantsSelection/MutantSelection.h"

#include "llvm/Support/FileSystem.h"    //for llvm::sys::fs::create_link


static std::string outputDir("mull-out-"); 
static const std::string selectionFolder("Selection");
static const std::string generalInfo("info");
static std::stringstream loginfo;
static std::string outFile;

void printVersion()
{
    llvm::outs() << "\nMuLL (Framework for Multi-Programming Language Mutation Testing based on LLVM)\n";
    llvm::outs() << "\tMuLL-Selection 1.0\n";
    llvm::outs() << "\nLLVM (http://llvm.org/):\n";
    llvm::outs() << "\tLLVM version " << LLVM_VERSION_MAJOR << "." << LLVM_VERSION_MINOR << "." << LLVM_VERSION_PATCH << "\n";
    llvm::outs() << "\tLLVM tools dir: " << LLVM_TOOLS_BINARY_DIR << "\n";
    llvm::outs() << "\n";
}

int main (int argc, char ** argv)
{
    time_t totalRunTime = time(NULL);
    clock_t curClockTime;
    char * inputIRfile = nullptr;
    char * mutantInfoJsonfile = nullptr;
    char * mutantDependencyJsonfile = nullptr;
    char * mullOutTopDir;
    
    bool rundg = true;
    
    
    char *  tmpStr = nullptr;
    
    std::string useful_conf_dir;
    
    tmpStr = new char[1+std::strlen(argv[0])]; //Alocate tmpStr1
    std::strcpy(tmpStr, argv[0]);   
    useful_conf_dir.assign(dirname(tmpStr));      //TODO: check this for install, where to put useful. (see klee's)
    delete [] tmpStr; tmpStr = nullptr;  //del tmpStr1
    
    useful_conf_dir += "/useful/";
    
    for (int i=1; i < argc; i++)
    {
        if (strcmp(argv[i], "-print-preTCE-Meta") == 0)
        {
            dumpPreTCEMeta = true;
        }
        else if (strcmp(argv[i], "-write-mutants") == 0)
        {
            dumpMutants = true;
        }
        else if (strcmp(argv[i], "-noWM") == 0)
        {
            enabledWeakMutation = false;
        }
        else if (strcmp(argv[i], "-mutant-config") == 0)
        {
            mutantConfigfile = argv[++i];
        }
        else if (strcmp(argv[i], "-mutant-scope") == 0)
        {
            mutantScopeJsonfile = argv[++i];
        }
        else if (strcmp(argv[i], "-version") == 0)
        {
            printVersion();
            return 0;
        }
        else
        {
            if (inputIRfile)
            {
                llvm::errs() << "Error with arguments: input file '" << inputIRfile << "' VS '" << argv[i] << "'.\n";
                return 1; 
            }
            inputIRfile = argv[i];
        }
        
    }
    
    assert (inputIRfile && "Error: No input llvm IR file passed!");
    
    llvm::Module *moduleM;
    std::unique_ptr<llvm::Module> _M;
    
    // Read IR into moduleM
    ///llvm::LLVMContext context;
    if (! ReadWriteIRObj::readIR(inputIRfile, _M))
        return 1;
    moduleM = _M.get();
    // ~
    
    MutantInfoLost mutantInfo;
    mutantInfo.loadFromJsonFile(/**TODO TODO*/);
    
    MutantSelection selection (*moduleM, mutantInfo, /*TODO: mutantDependence cache*/, rundg, false/*flow-sensitive?*/);
   
    llvm::outs() << "MuLL@Progress: Mutation took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    loginfo << "MuLL@Progress: Mutation took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    
    
    llvm::outs() << "MuLL@Progress:  Compiling Mutants took: "<< difftime(time(NULL), timer) <<" Seconds.\n";
    loginfo << "MuLL@Progress:  Compiling Mutants took: "<< difftime(time(NULL), timer) <<" Seconds.\n";
    
    llvm::outs() << "\nMuLL@Progress:  TOTAL RUNTIME: "<< (difftime(time(NULL), totalRunTime)/60) <<" min.\n";
    loginfo << "\nMuLL@Progress:  TOTAL RUNTIME: "<< (difftime(time(NULL), totalRunTime)/60) <<" min.\n";
    
    std::ofstream xxx (outputDir+"/"+generalInfo);
    if (xxx.is_open())
    {
        xxx << loginfo.str();
        xxx.close();
    }
    else llvm::errs() << "Unable to create info file:" << outputDir+"/"+generalInfo;   
    
    return 0;
}
