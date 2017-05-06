
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
    
    llvm::Function * currentFunction;
    llvm::BasicBlock * currentBasicBlock;
    
    llvm::Module * currentInputModule;
    llvm::Module * currentMetaMutantModule;
    
    unsigned curMutantID;
    
    //std::vector<std::string> mutantsInfos;
    MutantInfoList mutantsInfos;
    
    unsigned preTCENumMuts;
    unsigned postTCENumMuts;
    
    UserMaps usermaps;
    
    ModuleUserInfos moduleInfo;
    
    const char * wmLogFuncName = "llGenMu_WM_Log_Function"; //This fuction take the mutant ID and the condition and print the mutant ID if the condition is true.
    
public:
    typedef bool (* DumpMutFunc_t)(std::map<unsigned, std::vector<unsigned>>*, std::vector<llvm::Module *>*, llvm::Module *);
    Mutation (llvm::Module &module, std::string mutConfFile, DumpMutFunc_t writeMutsF, std::string scopeJsonFile="");
    ~Mutation ();
    bool doMutate ();   //Transforms module
    void doTCE (bool writeMuts=false, llvm::Module *modWMLog=nullptr);      //Transforms module
    void computeWeakMutation(llvm::Module * cmodule, llvm::Module &modWMLog);     //Compute WM of the module passed (pass a cloned module)
    bool getMutant (llvm::Module &module, unsigned mutanatID);
    unsigned getHighestMutantID (llvm::Module &module);
    
    void loadMutantInfos (std::string filename);
    void dumpMutantInfos (std::string filename);
    //llvm::Module & getMetaMutantModule() {return currentMetaMutantModule;}
    
private:
    bool getConfiguration(std::string &mutconfFile);
    void getanothermutantIDSelectorName();
    llvm::BasicBlock *getOriginalStmtBB (std::vector<llvm::Value *> &stmtIR, unsigned stmtcount);
    void getMutantsOfStmt (std::vector<llvm::Value *> const &stmtIR, MutantsOfStmt &ret_mutants, ModuleUserInfos const &moduleInfos);
    llvm::Function * createGlobalMutIDSelector_Func(llvm::Module &module, bool bodyOnly=false);
    void updateMutantsInfos (std::vector<llvm::Value *> &stmtIR, std::vector<std::string> &mutNameList, unsigned startID, unsigned num);
    DumpMutFunc_t writeMutantsCallback;
    llvm::Value * getWMCondition (llvm::BasicBlock *orig, llvm::BasicBlock *mut, llvm::Instruction * insertBeforeInst);
};
