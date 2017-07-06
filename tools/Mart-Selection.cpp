/**
 * -==== Mart-Selection.cpp
 *
 *                Mart Multi-Language LLVM Mutation Framework
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

#include "llvm/Support/CommandLine.h"   //llvm::cl

//#include "llvm/Support/FileSystem.h"    //for llvm::sys::fs::create_link


static const std::string selectionFolder("Selection.out");
static const std::string generalInfo("info");
static std::stringstream loginfo;
static std::string outFile;

void printVersion()
{
    llvm::outs() << "\nMart (Framework for Multi-Programming Language Mutation Testing based on LLVM)\n";
    llvm::outs() << "\tMart-Selection 1.0\n";
    llvm::outs() << "\nLLVM (http://llvm.org/):\n";
    llvm::outs() << "\tLLVM version " << LLVM_VERSION_MAJOR << "." << LLVM_VERSION_MINOR << "." << LLVM_VERSION_PATCH << "\n";
    llvm::outs() << "\tLLVM tools dir: " << LLVM_TOOLS_BINARY_DIR << "\n";
    llvm::outs() << "\n";
}

template<typename T>
void mutantListAsJsON(std::vector<std::vector<T>> const &lists, std::string jsonName)
{
    std::ofstream xxx (jsonName);
    if (xxx.is_open())
    {
        xxx << "{\n";
        for (unsigned repet = 0, repeat_last = lists.size() - 1; repet <= repeat_last; ++repet)
        {
            xxx << "\t\"" << repet << "\": [";
            bool isNotFirst = false;
            for (T data: lists[repet]) 
            {
                if (isNotFirst)
                    xxx << ", ";
                else
                    isNotFirst = true;
                xxx << data;
            } 
            if (repet == repeat_last)
                xxx << "]\n";
            else
                xxx << "],\n";
        }  
        xxx << "\n}\n";
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
    //Remove the option we don't want to display in help
    llvm::StringMap<llvm::cl::Option*> &optMap = llvm::cl::getRegisteredOptions();
    for (auto &option: optMap)
    {
        auto optstr = option.getKey();
        if (!(optstr.startswith("help") || optstr.equals("version")))
            optMap[optstr]->setHiddenFlag(llvm::cl::Hidden);
    }
     
    llvm::cl::opt<std::string> inputIRfile(llvm::cl::Positional, llvm::cl::Required, llvm::cl::desc("<input preprocessed IR file>"));
    llvm::cl::opt<std::string> mutantInfoJsonfile("mutant-infos", llvm::cl::Required, llvm::cl::desc("(required) Specify the mutants info JSON file"), llvm::cl::value_desc("filename"));
    llvm::cl::opt<std::string> martOutTopDir("out-topdir", llvm::cl::Required, llvm::cl::desc("(required) Specify output topdir where the selection output forlder will be created"), llvm::cl::value_desc("directory name"));
    llvm::cl::opt<bool> mut_dep_cache("mutant-dep-cache", llvm::cl::desc("Enable caching of mutant dependence computation"));
    llvm::cl::opt<unsigned> numberOfRandomSelections("rand-repeat-num", llvm::cl::desc("(optional) Specify the number of repetitions for random selections: default is 100 times"), llvm::cl::init(100));
    
    llvm::cl::SetVersionPrinter(printVersion);
    
    llvm::cl::ParseCommandLineOptions(argc, argv, "Mart Mutant Selection");
    
    time_t totalRunTime = time(NULL);
    clock_t curClockTime;
    
    char * mutantDependencyJsonfile = nullptr;
    
    bool rundg = true;
    
    //unsigned numberOfRandomSelections = 100;  //How many time do we repead random
    
    if (mut_dep_cache)
        mutantDependencyJsonfile = "mutantDependencies.cache.json";
    
    assert (!inputIRfile.empty() && "Error: No input llvm IR file passed!");
    assert (!mutantInfoJsonfile .empty()&& "Error: No mutant infos file passed!");
    assert (!martOutTopDir.empty() && "Error: No output topdir passed!");
    
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
    
    std::string outDir(martOutTopDir);
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
    llvm::outs() << "Mart@Progress: dependencies construction took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    loginfo << "Mart@Progress: dependencies construction took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    
    std::string smartSelectionOutJson = outDir + "/" + "smartSelection.json";
    std::string scoresForSmartSelectionOutJson = outDir + "/" + "scoresForSmartSelection.json";
    std::string randomSDLelectionOutJson = outDir + "/" + "randomSDLSelection.json";
    std::string spreadRandomSelectionOutJson = outDir + "/" + "spreadRandomSelection.json";
    std::string dummyRandomSelectionOutJson = outDir + "/" + "dummyRandomSelection.json";
    
    std::vector<std::vector<MutantIDType>> selectedMutants1, selectedMutants2;
    unsigned long number=0;
    
    llvm::outs() << "Doing Smart Selection...\n";
    curClockTime = clock();
    selectedMutants1.clear();
    selectedMutants1.push_back(std::vector<MutantIDType>());
    std::vector<double> selectedScores;
    selection.smartSelectMutants(selectedMutants1.back(), selectedScores);
    mutantListAsJsON<MutantIDType>(selectedMutants1, smartSelectionOutJson);
    mutantListAsJsON<double>(std::vector<std::vector<double>>({selectedScores}), scoresForSmartSelectionOutJson);
    llvm::outs() << "Mart@Progress: smart selection took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    loginfo << "Mart@Progress: smart selection took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    
    number = selectedMutants1.back().size();
    assert (number == mutantInfo.getMutantsNumber() && "The number of mutants mismatch. Bug in Selection function!");
    
    llvm::outs() << "Doing dummy and spread random selection...\n";
    curClockTime = clock();
    selectedMutants1.clear(); selectedMutants2.clear();
    selectedMutants1.resize(numberOfRandomSelections); selectedMutants2.resize(numberOfRandomSelections);
    for (unsigned si=0; si<numberOfRandomSelections; ++si)
        selection.randomMutants (selectedMutants1[si], selectedMutants2[si], number);
    mutantListAsJsON<MutantIDType>(selectedMutants1, spreadRandomSelectionOutJson);
    mutantListAsJsON<MutantIDType>(selectedMutants2, dummyRandomSelectionOutJson);
    llvm::outs() << "Mart@Progress: dummy and spread random took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds. (" << numberOfRandomSelections << " repetitions)\n";
    loginfo << "Mart@Progress: dummy and spread random took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds. (" << numberOfRandomSelections << " repetitions)\n";
    
    llvm::outs() << "Doing random SDL selection...\n";  //select only SDL mutants
    curClockTime = clock();
    selectedMutants1.clear();
    selectedMutants1.resize(numberOfRandomSelections); 
    for (unsigned si=0; si<numberOfRandomSelections; ++si)
        selection.randomSDLMutants(selectedMutants1[si], number);
    mutantListAsJsON<MutantIDType>(selectedMutants1, randomSDLelectionOutJson);
    llvm::outs() << "Mart@Progress: random SDL took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds. (" << numberOfRandomSelections << " repetitions)\n";
    loginfo << "Mart@Progress: random SDL took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds. (" << numberOfRandomSelections << " repetitions)\n";
    
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
