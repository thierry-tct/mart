
#include <string>
#include <ctime>
#include <sys/stat.h>   //mkdir, stat
#include <sys/types.h>  //mkdir, stat
#include <libgen.h> //basename

#include "ReadWriteIRObj.h"
#include "../lib/mutation.h"

#include "llvm/Transforms/Utils/Cloning.h"  //for CloneModule

//#include "llvm/Support/raw_ostream.h"


static std::string outputDir("mull-out-"); 
static const std::string mutantsFolder("mutants");
static std::string outFile;
    
void insertMutSelectGetenv(llvm::Module *mod)
{
    //insert getenv and atol
}

//print all the modules of mutants, sorted from mutant 0(original) to mutant max
bool dumpMutantsCallback (Mutation *mutEng, std::map<unsigned, std::vector<unsigned>> *poss, std::vector<llvm::Module *> *mods, llvm::Module *wmModule/*=nullptr*/, \
                                std::vector<llvm::Function *> const *mutFunctions, std::unordered_map<llvm::Module *, llvm::Function *> *backedFuncsByMods)
{
    llvm::outs() << "MuLL@Progress: writing mutants to files ...\n";
    clock_t curClockTime = clock();
  //weak mutation
    if (wmModule)
    {
        std::string wmFilePath = outputDir+"//"+outFile+".WM.bc";
        if (! ReadWriteIRObj::writeIR (wmModule, wmFilePath))
        {
            assert (false && "Failed to output weak mutation IR file");
        }
    }
    
  //Strong Mutants  
    if (poss && mods)
    {
        std::string mutantsDir = outputDir+"//"+mutantsFolder;
        if (mkdir(mutantsDir.c_str(), 0777) != 0)
            assert (false && "Failed to create mutants output directory");
        
        llvm::Module *formutsModule = mods->at(0);
        unsigned mid = 0;
        
        //original
        if (mkdir((mutantsDir+"//0").c_str(), 0777) != 0)
            assert (false && "Failed to create output directory for original (0)");
        if (! ReadWriteIRObj::writeIR (formutsModule, mutantsDir+"//0//"+outFile+".bc"))
        {
            assert (false && "Failed to output post-TCE original IR file");
        }
        
        //mutants
        for (auto &m: *poss)   
        {
            formutsModule = mods->at(m.first);
            if (mutFunctions != nullptr)
                mutEng->setModFuncToFunction (formutsModule, mutFunctions->at(m.first));
            mid = m.second.front();
            if (mkdir((mutantsDir+"/"+std::to_string(mid)).c_str(), 0777) != 0)
                assert (false && "Failed to create output directory for mutant");
            if (! ReadWriteIRObj::writeIR (formutsModule, mutantsDir+"//"+std::to_string(mid)+"//"+outFile+".bc"))
            {
                llvm::errs() << "Mutant " << mid << "...\n";
                assert (false && "Failed to output post-TCE mutant IR file");
            }
        }
        if (mutFunctions != nullptr)
            // restore
            for (auto &itt: *backedFuncsByMods)
                mutEng->setModFuncToFunction (itt.first, itt.second);
    }
    llvm::outs() << "MuLL@Progress: writing mutants to file took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    return true;
}

int main (int argc, char ** argv)
{
    clock_t curClockTime;
    char * inputIRfile = nullptr;
    char * mutantConfigfile = nullptr;
    char * mutantScopeJsonfile = nullptr;
    const char * wmLogFuncinputIRfileName="useful/wmlog-driver.bc";
    
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
    
    for (int i=1; i < argc; i++)
    {
        if (strcmp(argv[i], "-print-preTCE-Meta") == 0)
        {
            dumpPreTCEMeta = true;
        }
        else if (strcmp(argv[i], "-gen-mutants") == 0)
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
        tmpStr = new char[1+std::strlen(argv[0])]; //Alocate tmpStr1
        std::strcpy(tmpStr, argv[0]);   
        std::string wmLogFuncinputIRfile(dirname(tmpStr));      //TODO: check this for install, where to put useful. (see klee's)
        delete [] tmpStr; tmpStr = nullptr;  //del tmpStr1
        wmLogFuncinputIRfile = wmLogFuncinputIRfile + "//"+wmLogFuncinputIRfileName; 
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
    Mutation mut(*moduleM, mutantConfigfile, dumpMutantsCallback, mutantScopeJsonfile ? mutantScopeJsonfile : "");
    
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
    
    llvm::errs() << "MuLL@Progress: Compiling Mutants ...\n";
    //curClockTime = clock();
    time_t timer = time(NULL);  //clock_t do not measure time when calling a script
    tmpStr = new char[1+std::strlen(argv[0])]; //Alocate tmpStr3
    std::strcpy(tmpStr, argv[0]); 
    std::string compileMutsScript(dirname(tmpStr));  //dirname change the contain of its parameter
    delete [] tmpStr; tmpStr = nullptr;  //del tmpStr3
    //llvm::errs() << "bash " + compileMutsScript+"/useful/CompileAllMuts.sh "+outputDir+" yes" <<"\n";
    assert(!system(("bash " + compileMutsScript+"/useful/CompileAllMuts.sh "+outputDir+" yes").c_str()) && "Mutants Compile script failed");
    //llvm::outs() << "MuLL@Progress:  Compiling Mutants took: "<< (float)(clock() - curClockTime)/CLOCKS_PER_SEC <<" Seconds.\n";
    llvm::outs() << "MuLL@Progress:  Compiling Mutants took: "<< difftime(time(NULL), timer) <<" Seconds.\n";
    return 0;
}
