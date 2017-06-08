
#include <string>
#include <ctime>
#include <fstream>
#include <sstream>
#include <sys/stat.h>   //mkdir, stat
#include <sys/types.h>  //mkdir, stat
#include <libgen.h> //basename

#include "ReadWriteIRObj.h"
#include "../lib/mutation.h"

#include "llvm/Transforms/Utils/Cloning.h"  //for CloneModule
#include "llvm/Support/FileSystem.h"    //for llvm::sys::fs::create_link

#include "FunctionToModule.h"


static std::string outputDir("mull-out-"); 
static const std::string mutantsFolder("mutants");
static const std::string generalInfo("info");
static std::stringstream loginfo;
static std::string outFile;
static const std::string tmpFuncModuleFolder("tmp-func-module-dir");
    
void insertMutSelectGetenv(llvm::Module *mod)
{
    //insert getenv and atol
}

//print all the modules of mutants, sorted from mutant 0(original) to mutant max
bool dumpMutantsCallback (Mutation *mutEng, std::map<unsigned, std::vector<unsigned>> *poss, std::vector<llvm::Module *> *mods, llvm::Module *wmModule/*=nullptr*/, \
                                std::vector<llvm::Function *> const *mutFunctions)
{
    clock_t curClockTime = clock();
  //weak mutation
    if (wmModule)
    {
        llvm::outs() << "MuLL@Progress: writing weak mutation...\n";
        std::string wmFilePath = outputDir+"/"+outFile+".WM.bc";
        if (! ReadWriteIRObj::writeIR (wmModule, wmFilePath))
        {
            assert (false && "Failed to output weak mutation IR file");
        }
    }
    
  //Strong Mutants  
    if (poss && mods)
    {
        llvm::outs() << "MuLL@Progress: writing mutants to files (" << poss->size() <<")...\n";
        
        std::unordered_map<llvm::Module *, llvm::Function *> backedFuncsByMods;
        
        std::string mutantsDir = outputDir+"/"+mutantsFolder;
        if (mkdir(mutantsDir.c_str(), 0777) != 0)
            assert (false && "Failed to create mutants output directory");
        
        std::string tmpFunctionDir = outputDir + "/" + tmpFuncModuleFolder;   
        
        llvm::Module *formutsModule = mods->at(0);
        bool separateFunctionModule = (formutsModule->size() > 1); //separate only if we have more than one function XXX
        unsigned mid = 0;
        
        std::string infoFuncPathModPath;
        const std::string infoFuncPathModPath_File(tmpFunctionDir+"/"+"mapinfo");
        if (separateFunctionModule)
            if (mkdir(tmpFunctionDir.c_str(), 0777) != 0)
                assert (false && "Failed to create function temporal directory");
        
        //original
        if (mkdir((mutantsDir+"/0").c_str(), 0777) != 0)
            assert (false && "Failed to create output directory for original (0)");
        if (! ReadWriteIRObj::writeIR (formutsModule, mutantsDir+"/0/"+outFile+".bc"))
        {
            assert (false && "Failed to output post-TCE original IR file");
        }
        infoFuncPathModPath += mutantsFolder+"/0/"+outFile+".bc\n";
        
        //mutants
        for (auto &m: *poss)   
        {
            formutsModule = mods->at(m.first);
            if (mutFunctions != nullptr)
            {
                mid = m.second.front();
                llvm::Function *currFunc = mutFunctions->at(m.first);
                std::string funcName = currFunc->getName();
                std::string funcFile = tmpFunctionDir+"/"+funcName+".bc";
                std::string mutDirPath = mutantsDir+"/"+std::to_string(mid);
                if (mkdir(mutDirPath.c_str(), 0777) != 0)
                    assert (false && "Failed to create output directory for mutant");
                    
                if (backedFuncsByMods.count(formutsModule) == 0)
                {
                    llvm::ValueToValueMapTy vmap;
                    if (separateFunctionModule)
                    {
                        backedFuncsByMods[formutsModule];  //just insert function
                        formutsModule->getFunction(funcName)->dropAllReferences(); 
                        std::unique_ptr<llvm::Module> tmpM = FunctionToModule::mullSplitFunctionsOutOfModule(formutsModule, funcName);
                        // wrtite the function's IR in tmpFunctionDir
                        if (! ReadWriteIRObj::writeIR (tmpM.get(), funcFile))
                        {
                            llvm::errs() << "Function " << funcName << "...\n";
                            assert (false && "Failed to output function's module IR file");
                        }
                        
                        // now 'formutsModule' contain only the function of interest, delete it so that we can insert the mutants later
                        
                        if (! formutsModule->getFunction(funcName)->use_empty())    // The function has recursive call. all the mutants point here. Make them point on themselves
                        {
                            auto *tmpFF = formutsModule->getFunction(funcName);
                            while (!tmpFF->use_empty())
                            {
                                if (llvm::Instruction *URInst = llvm::dyn_cast<llvm::Instruction>(tmpFF->user_back()))
                                {
                                    llvm::Function *fofi = URInst->getParent()->getParent();
                                    assert (!fofi->getParent() && "the only users should be the mutant functions here, and thery have no parent.");
                                    URInst->replaceUsesOfWith(tmpFF, fofi);
                                }
                                else
                                {
                                    llvm::errs() << "\nError: Function '" << funcName << "' should have NO use here, since in its own module. But still have " << tmpFF->getNumUses() << " uses\n";
                                    llvm::errs() << "> Please report the bug.\n";
                                    //llvm::errs() << UR << "\n";
                                    //if (auto *Uinst = llvm::dyn_cast<llvm::Instruction>(UR))
                                    ////Uinst->getParent()->getParent()->dump();
                                    //    llvm::errs() << Uinst->getParent()->getParent() << "" << Uinst->getParent()->getParent()->getName() << " --- " << Uinst->getParent()->getParent()->getParent() << "\n";
                                    assert (false);
                                }
                            }
                        }
                        formutsModule->getFunction(funcName)->eraseFromParent();
                    }
                    else
                    {
                        backedFuncsByMods.emplace(formutsModule, llvm::CloneFunction(formutsModule->getFunction(funcName), vmap, true/*moduleLevelChanges*/));
                    }
                }
                
                if (separateFunctionModule)
                {
                    infoFuncPathModPath += mutantsFolder+"/"+std::to_string(mid)+"/"+outFile+".bc "+tmpFuncModuleFolder+"/"+funcName+".bc\n";
                    //std::error_code EC = llvm::sys::fs::create_link(funcFile, mutDirPath+"//"+funcName+"_");    //'_' to avaoid clash
                    //assert (!EC && "Failed to create link to function for a mutant");
                    
                    auto linkageBak = currFunc->getLinkage();
                    formutsModule->getFunctionList().push_back(currFunc);
                    currFunc->setLinkage(llvm::GlobalValue::ExternalLinkage);
                    if (! ReadWriteIRObj::writeIR (formutsModule, mutDirPath+"/"+outFile+".bc"))
                    {
                        llvm::errs() << "Mutant " << mid << "...\n";
                        assert (false && "Failed to output post-TCE mutant IR file");
                    }
                    currFunc->setLinkage(linkageBak);
                    currFunc->removeFromParent();
                }
                else
                {
                    mutEng->setModFuncToFunction (formutsModule, currFunc);
                    if (! ReadWriteIRObj::writeIR (formutsModule, mutDirPath+"/"+outFile+".bc"))
                    {
                        llvm::errs() << "Mutant " << mid << "...\n";
                        assert (false && "Failed to output post-TCE mutant IR file");
                    }
                }
            }
            else
            {
                mid = m.second.front();
                if (mkdir((mutantsDir+"/"+std::to_string(mid)).c_str(), 0777) != 0)
                    assert (false && "Failed to create output directory for mutant");
                if (! ReadWriteIRObj::writeIR (formutsModule, mutantsDir+"/"+std::to_string(mid)+"/"+outFile+".bc"))
                {
                    llvm::errs() << "Mutant " << mid << "...\n";
                    assert (false && "Failed to output post-TCE mutant IR file");
                }
            }
        }
        if (mutFunctions != nullptr)
        {
            if (separateFunctionModule)
            {
                std::ofstream xxx (infoFuncPathModPath_File);
                if (xxx.is_open())
                {
                    xxx << infoFuncPathModPath;
                    xxx.close();
                }
                else llvm::errs() << "Unable to open file for write:" << infoFuncPathModPath_File;
                
                //for (auto &itt: extractedFuncMod)
                //    delete itt.second;
            }
            else
            {   // restore
                for (auto &itt: backedFuncsByMods)
                {
                    mutEng->setModFuncToFunction (itt.first, itt.second);
                    delete itt.second;  //the fuction just created above as clone..
                }
            }
            
        }
    }
    llvm::outs() << "MuLL@Progress: writing mutants to file took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    loginfo << "MuLL@Progress: writing mutants to file took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    return true;
}

void printVersion()
{
    llvm::outs() << "\nMuLL (Framework for Multi-Programming Language Mutation Testing based on LLVM)\n";
    llvm::outs() << "\tMuLL 1.0\n";
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
    char * mutantConfigfile = nullptr;
    char * mutantScopeJsonfile = nullptr;
    const char * wmLogFuncinputIRfileName = "wmlog-driver.bc";
    const char * defaultMconfFile = "mconf-scope/default_allmax.mconf";
    
    bool dumpPreTCEMeta = false;
    bool dumpMetaIRbc = true; 
    bool dumpMutants = false;
    bool enabledWeakMutation = true;
    bool dumpMutantInfos = true;
    
    /// \brief set this to false if the module is small enough, that all mutants will fit in memory
    bool isTCEFunctionMode = true;
    
#ifdef KLEE_SEMU_GENMU_OBJECTFILE
    bool dumpMetaObj = false; 
#endif  //#ifdef KLEE_SEMU_GENMU_OBJECTFILE
    
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
    std::unique_ptr<llvm::Module> modWMLog (nullptr), _M;
    
    // Read IR into moduleM
    ///llvm::LLVMContext context;
    if (! ReadWriteIRObj::readIR(inputIRfile, _M))
        return 1;
    moduleM = _M.get();
    // ~
    
    /// Weak mutation 
    if (enabledWeakMutation)
    {
        std::string wmLogFuncinputIRfile(useful_conf_dir + wmLogFuncinputIRfileName);      
        // get the module containing the function to log WM info. to be linked with WMModule
        if (! ReadWriteIRObj::readIR(wmLogFuncinputIRfile, modWMLog))
            return 1;
    }
    else
    {
        modWMLog = nullptr;
    }
    
    /**********************  To BE REMOVED (used to extrac line num from bc whith llvm 3.8.0)
    for (auto &Func: *moduleM)
    {
        if (Func.isDeclaration())
            continue;
        for (auto &BB: Func)
        {
            for (auto &Inst:BB)
            {
                //Location in source file
                if (const llvm::Instruction *I = llvm::dyn_cast<llvm::Instruction>(&Inst)) {
                    std::string tmpStr = UtilsFunctions::getSrcLoc(I);
                    if(!tmpStr.empty()) {
                        llvm::errs() << Func.getName() << " ";
                        llvm::errs() << tmpStr();
                        llvm::errs() << "\n";
                    }
                }
            }
        }
    }
    return 0;
    **********************/
    
    //test
    ///llvm::errs() << "\n@Before\n"; moduleM->dump(); llvm::errs() << "\n============\n";
    //std::string mutconffile;
    
    // @Mutation
    Mutation mut(*moduleM, mutantConfigfile?mutantConfigfile:(useful_conf_dir + defaultMconfFile), dumpMutantsCallback, mutantScopeJsonfile ? mutantScopeJsonfile : "");
    
    // Keep Phi2Mem-preprocessed module
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    std::unique_ptr<llvm::Module> preProPhi2MemModule (llvm::CloneModule(moduleM));
#else
    std::unique_ptr<llvm::Module> preProPhi2MemModule = llvm::CloneModule(moduleM);
#endif
    
    // do mutation
    llvm::outs() << "MuLL@Progress: Mutating...\n";
    curClockTime = clock();
    if (! mut.doMutate())
    {
        llvm::errs() << "\nMUTATION FAILED!!\n\n";
        return 1;
    }
    llvm::outs() << "MuLL@Progress: Mutation took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    loginfo << "MuLL@Progress: Mutation took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    
    // @Output setup
    struct stat st;// = {0};
    int dcount = 0;
    while (stat((outputDir + std::to_string(dcount)).c_str(), &st) != -1)
        dcount++;
    
    outputDir += std::to_string(dcount);
    
    if (mkdir(outputDir.c_str(), 0777) != 0)
        assert (false && "Failed to create output directory");
    
    //@ Output file name root
    
    tmpStr = new char[1+std::strlen(inputIRfile)]; //Alocate tmpStr2
    std::strcpy(tmpStr, inputIRfile);
    outFile.assign(basename(tmpStr));   //basename changes the contain of its parameter
    delete [] tmpStr; tmpStr = nullptr;  //del tmpStr2
    if (!outFile.substr(outFile.length()-3, 3).compare(".ll") || !outFile.substr(outFile.length()-3, 3).compare(".bc"))
        outFile.replace(outFile.length()-3, 3, "");
    
    //@ Store Phi2Mem-preprocessed module with the same name of the input file
    if (! ReadWriteIRObj::writeIR (preProPhi2MemModule.get(), outputDir+"/"+outFile+".bc"))
        assert (false && "Failed to output Phi-preprocessed IR file");
        
    //@ Print pre-TCE meta-mutant
    if (dumpPreTCEMeta)
    {
        if (! ReadWriteIRObj::writeIR (moduleM, outputDir+"/"+outFile+".preTCE.MetaMu.bc"))
            assert (false && "Failed to output pre-TCE meta-mutatant IR file");
       // mut.dumpMutantInfos (outputDir+"//"+outFile+"mutantLocs-preTCE.json");
    }
    
    //@ Remove equivalent mutants and //@ print mutants in case on
    llvm::outs() << "MuLL@Progress: Removing TCE Duplicates & WM & writing mutants IRs (with initially " << mut.getHighestMutantID() <<" mutants)...\n";
    curClockTime = clock();
    mut.doTCE(modWMLog, dumpMutants, isTCEFunctionMode);
    llvm::outs() << "MuLL@Progress: Removing TCE Duplicates  & WM & writing mutants IRs took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    loginfo << "MuLL@Progress: Removing TCE Duplicates  & WM & writing mutants IRs took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    
    /// Mutants Infos into json
    if (dumpMutantInfos)
        mut.dumpMutantInfos (outputDir+"//"+outFile+"-mutantInfos.json");
    
    //@ Print post-TCE meta-mutant
    if (dumpMetaIRbc) 
    {   
        if (! ReadWriteIRObj::writeIR (moduleM, outputDir+"/"+outFile+".MetaMu.bc"))
            assert (false && "Failed to output post-TCE meta-mutatant IR file");
    }
    
    //@ print mutants
    /*if (dumpMutants)
    {
        unsigned highestMutID = mut.getHighestMutantID (*moduleM);
        std::string mutantsDir = outputDir+"//"+mutantsFolder;
        if (mkdir(mutantsDir.c_str(), 0777) != 0)
            assert (false && "Failed to create mutants output directory");
        for (unsigned mid=0; mid <= highestMutID; mid++)
        {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            llvm::Module *formutsModule = llvm::CloneModule(moduleM);
#else
            llvm::Module *formutsModule = llvm::CloneModule(moduleM).get();
#endif
            if (mkdir((mutantsDir+"/"+std::to_string(mid)).c_str(), 0777) != 0)
                assert (false && "Failed to create output directory for mutant");
            if (! mut.getMutant (*formutsModule, mid))
            {
                llvm::errs() << "\nError Failed to generate module for Mutant " << mid << "\n\n";
                assert (false && "");
            }
            if (! ReadWriteIRObj::writeIR (formutsModule, mutantsDir+"//"+std::to_string(mid)+"//"+outFile+".bc"))
            {
                llvm::errs() << "Mutant " << mid << "...\n";
                assert (false && "Failed to output post-TCE meta-mutatant IR file");
            }
        }
    }*/

#ifdef KLEE_SEMU_GENMU_OBJECTFILE    
    if (dumpMetaObj)
    {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
        std::unique_ptr<llvm::Module> forObjModule (llvm::CloneModule(moduleM));
#else
        std::unique_ptr<llvm::Module> forObjModule = llvm::CloneModule(moduleM);
#endif
        //TODO: insert mutant selection code into the cloned module
        if (! ReadWriteIRObj::writeObj (forObjModule.get(), outputDir+"/"+outFile+".MetaMu.o"))
            assert (false && "Failed to output meta-mutatant object file");
    }
#endif  //#ifdef KLEE_SEMU_GENMU_OBJECTFILE
    //llvm::errs() << "@After Mutation->TCE\n"; moduleM->dump(); llvm::errs() << "\n";
    
    llvm::outs() << "MuLL@Progress: Compiling Mutants ...\n";
    //curClockTime = clock();
    time_t timer = time(NULL);  //clock_t do not measure time when calling a script
    tmpStr = new char[1+std::strlen(argv[0])]; //Alocate tmpStr3
    std::strcpy(tmpStr, argv[0]); 
    std::string compileMutsScript(dirname(tmpStr));  //dirname change the contain of its parameter
    delete [] tmpStr; tmpStr = nullptr;  //del tmpStr3
    //llvm::errs() << "bash " + compileMutsScript+"/useful/CompileAllMuts.sh "+outputDir+" yes" <<"\n";
    assert(!system(("bash " + compileMutsScript+"/useful/CompileAllMuts.sh "+outputDir+" "+tmpFuncModuleFolder+" yes").c_str()) && "Mutants Compile script failed");
    //llvm::outs() << "MuLL@Progress:  Compiling Mutants took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    
    llvm::outs() << "MuLL@Progress:  Compiling Mutants took: "<< difftime(time(NULL), timer) <<" Seconds.\n";
    loginfo << "MuLL@Progress:  Compiling Mutants took: "<< difftime(time(NULL), timer) <<" Seconds.\n";
    
    llvm::outs() << "\nMuLL@Progress:  TOTAL RUNTIME: "<< (difftime(time(NULL), totalRunTime)/60) <<" min.\n";
    loginfo << "\nMuLL@Progress:  TOTAL RUNTIME: "<< (difftime(time(NULL), totalRunTime)/60) <<" min.\n";
    
    loginfo << mut.getMutationStats();
    
    std::ofstream xxx (outputDir+"/"+generalInfo);
    if (xxx.is_open())
    {
        xxx << loginfo.str();
        xxx.close();
    }
    else llvm::errs() << "Unable to create info file:" << outputDir+"/"+generalInfo;   
    
    return 0;
}
