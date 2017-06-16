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
//#include <libgen.h> //basename
//#include <unistd.h>  // fork, execl
//#include <sys/wait.h> //wait


#include "ReadWriteIRObj.h"
#include "../lib/mutantsSelection/MutantSelection.h"

//#include "llvm/Support/FileSystem.h"    //for llvm::sys::fs::create_link


static const std::string selectionFolder("Selection.out");
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

void mutantListAsJsON(std::vector<MuLL::MutantIDType> const &list, std::string jsonName)
{
    std::ofstream xxx (jsonName);
    if (xxx.is_open())
    {
        xxx << "[\n";
        bool isNotFirst = false;
        for (MuLL::MutantIDType mutant_id: list) 
        {
            if (isNotFirst)
                xxx << ",\n";
            else
                isNotFirst = true;
            xxx << mutant_id;
        }          
        xxx << "\n]\n";
        xxx.close();
    }
    else 
    {
        llvm::errs() << "Unable to create info file:" << jsonName << "\n";  
        assert(false);
    } 
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
    
    
    for (int i=1; i < argc; i++)
    {
        if (strcmp(argv[i], "-mutant-dep-cache") == 0)
        {
            mutantDependencyJsonfile = "mutantDependencies.cache.json";
        }
        else if (strcmp(argv[i], "-mutant-infos") == 0)
        {
            mutantInfoJsonfile = argv[++i];
        }
        else if (strcmp(argv[i], "-out-topdir") == 0)
        {
            mullOutTopDir = argv[++i];
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
    assert (mutantInfoJsonfile && "Error: No mutant infos file passed!");
    assert (mullOutTopDir && "Error: No output topdir passed!");
    
    llvm::Module *moduleM;
    std::unique_ptr<llvm::Module> _M;
    
    // Read IR into moduleM
    ///llvm::LLVMContext context;
    if (! ReadWriteIRObj::readIR(inputIRfile, _M))
        return 1;
    moduleM = _M.get();
    // ~
    
    MutantInfoList mutantInfo;
    mutantInfo.loadFromJsonFile(mutantInfoJsonfile);
    
    std::string outDir(mullOutTopDir);
    outDir = outDir + "/" + selectionFolder;
    struct stat st;
    if (stat(outDir.c_str(), &st) == -1)   //do not exists
    {
        if (mkdir(outDir.c_str(), 0777) != 0)
            assert (false && "Failed to create output directory");
    }
    else
    {
        llvm::errs() << "Overriding existing...\n";
    }
    
    std::string mutDepCacheName;
    if (mutantDependencyJsonfile)
    {
        mutDepCacheName = outDir + "/" + mutantDependencyJsonfile;
        if (stat(mutDepCacheName.c_str(), &st) != -1)   //exists
        {
            rundg = false;
        }
        else
        {
            rundg = true;
        }
    }
    else
    {
        rundg = true;
    }
    
    llvm::outs() << "Computing mutant dependencies...\n";
    curClockTime = clock();
    MutantSelection selection (*moduleM, mutantInfo, mutDepCacheName, rundg, false/*is flow-sensitive?*/);
    llvm::outs() << "MuLL@Progress: dependencies construction took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    loginfo << "MuLL@Progress: dependencies construction took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    
    std::string smartSelectionOutJson = outDir + "/" + "smartSelection.json";
    std::string randomSDLelectionOutJson = outDir + "/" + "randomSDLSelection.json";
    std::string spreadRandomSelectionOutJson = outDir + "/" + "spreadRandomSelection.json";
    std::string dummyRandomSelectionOutJson = outDir + "/" + "dummyRandomSelection.json";
    
    std::vector<MuLL::MutantIDType> selectedMutants1, selectedMutants2;
    unsigned long number=0;
    
    llvm::outs() << "Doing Smart Selection...\n";
    curClockTime = clock();
    selectedMutants1.clear();
    selection.smartSelectMutants(selectedMutants1, 0.5 /*treshold score*/);
    mutantListAsJsON(selectedMutants1, smartSelectionOutJson);
    llvm::outs() << "MuLL@Progress: smart selection took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    loginfo << "MuLL@Progress: smart selection took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    
    number = selectedMutants1.size();
    
    llvm::outs() << "Doing dummy and spread random selection...\n";
    curClockTime = clock();
    selectedMutants1.clear(); selectedMutants2.clear();
    selection.randomMutants (selectedMutants1, selectedMutants2, number);
    mutantListAsJsON(selectedMutants1, spreadRandomSelectionOutJson);
    mutantListAsJsON(selectedMutants2, dummyRandomSelectionOutJson);
    llvm::outs() << "MuLL@Progress: dummy and spread random took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    loginfo << "MuLL@Progress: dummy and spread random took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    
    llvm::outs() << "Doing random SDL selection...\n";  //select only SDL mutants
    curClockTime = clock();
    selectedMutants1.clear();
    selection.randomSDLMutants(selectedMutants1, number);
    mutantListAsJsON(selectedMutants1, randomSDLelectionOutJson);
    llvm::outs() << "MuLL@Progress: random SDL took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    loginfo << "MuLL@Progress: random SDL took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    
    std::ofstream xxx (outDir+"/"+generalInfo);
    if (xxx.is_open())
    {
        xxx << loginfo.str();
        xxx.close();
    }
    else 
    {
        llvm::errs() << "Unable to create info file:" << outDir+"/"+generalInfo << "\n\n"; 
        assert (false);
    }  
    
    return 0;
}
