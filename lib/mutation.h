
#include <vector>
#include <string>

#include <llvm/IR/Module.h>

#include "usermaps.h"
#include "typesops.h"

struct mutationConfig
{
	std::vector<llvmMutationOp> mutators;
};

class Mutation
{
    mutationConfig configuration;
    MutationScope mutationScope;
    std::string mutantIDSelectorName;
    std::string mutantIDSelectorName_Func;
    
    //Tell wheter the mutation is to be used with KLEE-SEMu
    //If so, insert at mutation point a function call whose 2 arguments
    //are respectively the minID and MaxID of mutants at that point (mutant IDs are seuentially attributed)
    bool forKLEESEMu;
    llvm::Function* funcForKLEESEMu;
    
    llvm::Module * currentInputModule;
    llvm::Module * currentMetaMutantModule;
    
    unsigned curMutantID;
    
    //std::vector<std::string> mutantsInfos;
    MutantInfoList mutantsInfos;
    
    unsigned preTCENumMuts;
    unsigned postTCENumMuts;
    unsigned numDuplicateMuts;
    unsigned numEquivalentMuts;
    
    UserMaps usermaps;
    
    ModuleUserInfos moduleInfo;
    
    const char * wmLogFuncName = "muLLVM_WM_Log_Function"; //This fuction take the mutant ID and the condition and print the mutant ID if the condition is true.
    
    static const bool verifyFuncModule = true;  //Change this to enable/disable verification after mutation
    
    static const unsigned funcModeOptLevel = 1; 
    static const unsigned modModeOptLevel = 0;
    
public:
    typedef bool (* DumpMutFunc_t)(Mutation *mutEng, std::map<unsigned, std::vector<unsigned>>*, std::vector<llvm::Module *>*, llvm::Module *, \
                                        std::vector<llvm::Function *> const *mutFunctions, std::unordered_map<llvm::Module *, llvm::Function *> *backedFuncsByMods);
    Mutation (llvm::Module &module, std::string mutConfFile, DumpMutFunc_t writeMutsF, std::string scopeJsonFile="");
    ~Mutation ();
    bool doMutate ();   //Transforms module
    void doTCE (std::unique_ptr<llvm::Module> &modWMLog, bool writeMuts=false, bool isTCEFunctionMode=false);      //Transforms module
    void setModFuncToFunction (llvm::Module *Mod, llvm::Function *srcF, llvm::Function *targetF=nullptr);
    unsigned getHighestMutantID (llvm::Module const *module=nullptr);
    
    void loadMutantInfos (std::string filename);
    void dumpMutantInfos (std::string filename);
    //llvm::Module & getMetaMutantModule() {return currentMetaMutantModule;}
    
private:
    bool getConfiguration(std::string &mutconfFile);
    void getanothermutantIDSelectorName();
    void getMutantsOfStmt (MatchStmtIR const &stmtIR, MutantsOfStmt &ret_mutants, ModuleUserInfos const &moduleInfos);
    llvm::Function * createGlobalMutIDSelector_Func(llvm::Module &module, bool bodyOnly=false);
    DumpMutFunc_t writeMutantsCallback;
    llvm::Value * getWMCondition (llvm::BasicBlock *orig, llvm::BasicBlock *mut, llvm::Instruction * insertBeforeInst);
    void computeWeakMutation(std::unique_ptr<llvm::Module> &cmodule, std::unique_ptr<llvm::Module> &modWMLog);     //Compute WM of the module passed (pass a cloned module)
    void preprocessVariablePhi (llvm::Module &module);
    llvm::AllocaInst * MYDemotePHIToStack(llvm::PHINode *P, llvm::Instruction *AllocaPoint);
    llvm::AllocaInst * MyDemoteRegToStack(llvm::Instruction &I, bool VolatileLoads, llvm::Instruction *AllocaPoint);
    inline bool skipFunc (llvm::Function &Func);
    
    void cleanFunctionToMut (llvm::Function &Func, MuLL::MutantIDType mutantID, llvm::GlobalVariable *mutantIDSelGlob, llvm::Function *mutantIDSelGlob_Func);
    void computeModuleBufsByFunc(llvm::Module &module, std::unordered_map<llvm::Function *, ReadWriteIRObj> *inMemIRModBufByFunc, \
                                                    std::unordered_map<llvm::Function *, llvm::Module *> *clonedModByFunc, std::vector<llvm::Function *> &funcMutByMutID);
    bool getMutant (llvm::Module &module, unsigned mutanatID, llvm::Function *mutFunc=nullptr, char optimizeModFuncNone='M'/* 'M', 'F', 'A', '0' */);
    
    static inline void checkModuleValidity(llvm::Module &Mod, const char *errMsg="");
    static inline void checkFunctionValidity(llvm::Function &Func, const char *errMsg="");
};
