
#include <vector>
#include <string>

#include <llvm/IR/Module.h>

#include "typesops.h"

struct mutationConfig
{
	std::vector<llvmMutationOp> mutators;
};

class Mutation
{
    mutationConfig configuration;
    std::string mutantIDSelectorName;
    std::string mutantIDSelectorName_Func;
    
    //Tell wheter the mutation is to be used with KLEE-SEMu
    //If so, insert at mutation point a function call whose 2 arguments
    //are respectively the minID and MaxID of mutants at that point (mutant IDs are seuentially attributed)
    bool forKLEESEMu;
    llvm::Function* funcForKLEESEMu;
    
    llvm::Function * currentFunction;
    llvm::BasicBlock * currentBasicBlock;
    
    unsigned curMutantID;
    
    std::vector<std::string> mutantsInfos;
    
    unsigned preTCENumMuts;
    unsigned postTCENumMuts;
    
    UserMaps usermaps;
    
public:
    typedef bool (* DumpMutFunc_t)(std::map<unsigned, std::vector<unsigned>>&, std::vector<llvm::Module *>&);
    Mutation (std::string mutConfFile, DumpMutFunc_t writeMutsF);
    ~Mutation ();
    bool doMutate (llvm::Module &module);   //Transforms module
    void doTCE (llvm::Module &module, bool writeMuts=false);      //Transforms module
    bool getMutant (llvm::Module &module, unsigned mutanatID);
    unsigned getHighestMutantID (llvm::Module &module);
    
private:
    bool getConfiguration(std::string &mutconfFile);
    void getanothermutantIDSelectorName();
    llvm::BasicBlock *getOriginalStmtBB (std::vector<llvm::Value *> &stmtIR, unsigned stmtcount);
    void mutantsOfStmt (std::vector<llvm::Value *> &stmtIR, std::vector<std::vector<llvm::Value *>> &ret_mutants, llvm::Module &module);
    llvm::Function * createGlobalMutIDSelector_Func(llvm::Module &module, bool bodyOnly=false);
    void updateMutantsInfos (std::vector<llvm::Value *> &stmtIR, std::vector<std::string> &mutNameList, unsigned startID, unsigned num);
    DumpMutFunc_t writeMutantsCallback;
};
