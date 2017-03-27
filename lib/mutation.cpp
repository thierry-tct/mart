
#include <vector>
#include <set>
#include <queue>
#include <sstream>
#include <regex>
#include <fstream>

#include "mutation.h"
#include "matchercreator.h"
#include "tce.h"    //Trivial Compiler Equivalence

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/GlobalVariable.h"
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
#include "llvm/PassManager.h"    //This is needed for 3rd param of SplitBlock
#include "llvm/Analysis/RegionInfo.h"   //This is needed for 3rd param of SplitBlock
#include "llvm/Analysis/Verifier.h"
#else
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#endif
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Transforms/Utils/Cloning.h"  //for CloneModule

Mutation::Mutation(std::string mutConfFile, DumpMutFunc_t writeMutsF): forKLEESEMu(true), writeMutantsCallback(writeMutsF)
{
    //get mutation config (operators)
    assert(getConfiguration(mutConfFile) && "@Mutation(): getConfiguration(mutconfFile) Failed!");
    
    //initialize mutantIDSelectorName
    getanothermutantIDSelectorName();
    currentFunction = nullptr;
    currentBasicBlock = nullptr;
    curMutantID = 0;
}


bool Mutation::getConfiguration(std::string &mutConfFile)
{
    //TODO
    std::vector<unsigned> reploprd;
    std::vector<llvmMutationOp> mutationOperations;
    std::vector<std::string> matchoprd;
    std::vector<enum ExpElemKeys> *correspKeysMatch;
    std::vector<enum ExpElemKeys> *correspKeysMutant;
    
    std::ifstream infile(mutConfFile);
    if (infile)
    {
        std::string linei;
        std::vector<std::string> matchop_oprd;
        unsigned confLineNum = 0;
        while (infile)
        {
            confLineNum++;
            std::getline(infile, linei); 
            
            //remove comment (Everything after the '#' character)
            size_t commentInd = linei.find_first_of('#');
            if (commentInd != std::string::npos)
                linei.erase(commentInd, std::string::npos);
            
            //skip white(empty) line
            if (std::regex_replace(linei, std::regex("^\\s+|\\s+$"), std::string("$1")).length() == 0)
            {
                //llvm::errs() << "#"<<linei <<"#\n";
                continue;
            }
            
            std::regex rgx("\\s+\-\->\\s+");        // Matcher --> Replacors
            std::sregex_token_iterator iter(linei.begin(),linei.end(), rgx, -1);
            std::sregex_token_iterator end;
            unsigned short matchRepl = 0;
            for ( ; iter != end; ++iter)
            {
                if (matchRepl == 0)
                {
                    std::string matchstr2(*iter);
                    std::regex rgx2("\\s*,\\s*|\\s*\\(\\s*|\\s*\\)\\s*");        // operation (operand1,operand2,...) 
                    std::sregex_token_iterator iter2(matchstr2.begin(), matchstr2.end(), rgx2, -1);
                    std::sregex_token_iterator end2;
                    
                    matchoprd.clear();
                    for ( ; iter2 != end2; ++iter2)
                    {
                        //Position 0 for operation, rest for operands
                        matchoprd.push_back(std::regex_replace(std::string(*iter2), std::regex("^\\s+|\\s+$"), std::string("$1")));
                    }
                    
                    //when we match @ or C or V or A or P, they are their own params
                    if (matchoprd.size() == 1 && usermaps.isConstValOPRD(matchoprd.back()))
                    {
                        //I am my own parameter
                        matchoprd.push_back(matchoprd.back());
                    }
                    
                    mutationOperations.clear();
                    
                    correspKeysMatch = usermaps.getExpElemKeys (matchoprd.front(), matchstr2, confLineNum);   //floats then ints. EX: OF, UF, SI, UI
                    for (unsigned i=0; i < correspKeysMatch->size(); i++)
                    {
                        mutationOperations.push_back(llvmMutationOp());
                        mutationOperations.back().setMatchOp(correspKeysMatch->at(i), matchoprd, 1);
                    }
                }
                else if (matchRepl == 1)
                {
                    std::string matchstr3(*iter);
                    std::regex rgx3("\\s*;\\s*");        // Matcher --> Replacors
                    std::sregex_token_iterator iter3(matchstr3.begin(),matchstr3.end(), rgx3, -1);
                    std::sregex_token_iterator end3;
                    for ( ; iter3 != end3; ++iter3)     //For each replacor
                    {
                        std::string tmprepl(std::regex_replace(std::string(*iter3), std::regex("^\\s+|\\s+$"), std::string("$1")));
                        std::regex rgx4("\\s*,\\s*|\\s*\\(\\s*|\\s*\\)\\s*");        // MutName, operation(operand1,operand2,...)
                        std::sregex_token_iterator iter4(tmprepl.begin(),tmprepl.end(), rgx4, -1);
                        std::sregex_token_iterator end4;
                        std::string mutName(*iter4);
                        ++iter4; assert (iter4 != end4 && "only Mutant name, no info!");
                        std::string mutoperation(*iter4);
                        
                        //If replace with constant number, add it here
                        auto contvaloprd = llvmMutationOp::insertConstValue (mutoperation, true);   //numeric=true
                        if (contvaloprd > llvmMutationOp::maxOprdNum)
                        {
                            reploprd.clear();
                            reploprd.push_back(contvaloprd); 
                            for (auto & mutOper:mutationOperations)
                                mutOper.addReplacor(mCONST_VALUE_OF, reploprd, mutName);
                            ++iter4; assert (iter4 == end4 && "Expected no mutant operand for const value replacement!");
                            continue;
                        }
                           
                        ++iter4; assert (iter4 != end4 && "no mutant operands");
                        
                        reploprd.clear();
                        
                        for ( ; iter4 != end4; ++iter4)     //for each operand
                        {
                            if ((*iter4).length() == 0)
                            {
                                if (! usermaps.isDeleteStmtConfName(mutoperation))
                                { 
                                    llvm::errs() << "missing operand at line " << confLineNum << "\n";
                                    assert (false && "");
                                }
                                continue;
                            }
                                
                            bool found = false;
                            unsigned pos4 = 0;
                            
                            //If constant number, directly add it
                            if ((pos4 = llvmMutationOp::insertConstValue (std::string(*iter4), true)) > llvmMutationOp::maxOprdNum)     //numeric=true
                            {
                                reploprd.push_back(pos4); 
                                continue;
                            }
                            
                            // If CALLED Function replacement
                            if (mutationOperations.back().matchOp == mCALL)
                            {
                                for (auto &obj: mutationOperations)
                                    assert (obj.matchOp == mCALL && "All 4 types should be mCALL here");
                                pos4 = llvmMutationOp::insertConstValue (std::string(*iter4), false);
                                assert (pos4 > llvmMutationOp::maxOprdNum && "Insertion should always work here");
                                reploprd.push_back(pos4); 
                                continue;
                            }
                            
                            //Make sure that the parameter is either anything or any variable or any pointer or any constant
                            usermaps.validateNonConstValOPRD(llvm::StringRef(*iter4), confLineNum);
                            
                            //search the operand position
                            for (std::vector<std::string>::iterator it41 = matchoprd.begin()+1; it41 != matchoprd.end(); ++it41)
                            {
                                if (llvm::StringRef(*it41).equals_lower(llvm::StringRef(*iter4)))
                                {
                                    found = true;
                                    break;
                                }
                                pos4++;
                            }
                            
                            if (found)
                            {
                                reploprd.push_back(pos4);    //map to Matcher operand
                            }
                            else
                            {
                                llvm::errs() << "Error in the replacor parameters (do not match any of the Matcher's): '"<<tmprepl<<"', confLine("<<confLineNum<<")!\n";
                                return false;
                            }
                        }
                        
                        correspKeysMutant = usermaps.getExpElemKeys (mutoperation, tmprepl, confLineNum);   //floats then ints. EX: FDiv, SDiv, UDiv
                        
                        assert (correspKeysMutant->size() == mutationOperations.size() && "Incompatible types Mutation oprerator");
                        
                        unsigned iM=0;
                        while (iM < correspKeysMutant->size() && (mutationOperations[iM].matchOp == mFORBIDEN_TYPE || correspKeysMutant->at(iM) == mFORBIDEN_TYPE))
                        {
                            iM++;
                        }
                        if (iM >= correspKeysMutant->size())
                            continue;
                            
                        mutationOperations[iM].addReplacor(correspKeysMutant->at(iM), reploprd, mutName);
                        enum ExpElemKeys prevMu = correspKeysMutant->at(iM);
                        enum ExpElemKeys prevMa = mutationOperations[iM].matchOp;
                        for (iM=iM+1; iM < correspKeysMutant->size(); iM++)
                        {
                            if (mutationOperations[iM].matchOp != mFORBIDEN_TYPE && correspKeysMutant->at(iM) != mFORBIDEN_TYPE)
                            {
                                if (correspKeysMutant->at(iM) != prevMu || mutationOperations[iM].matchOp != prevMa)
                                {    
                                    mutationOperations[iM].addReplacor(correspKeysMutant->at(iM), reploprd, mutName);
                                    prevMu = correspKeysMutant->at(iM);
                                    prevMa = mutationOperations[iM].matchOp;
                                }
                            }
                        }
                    }
                    
                    //Remove the matcher without replacors
                    for (unsigned itlM=0; itlM < mutationOperations.size();)
                    {
                        if (mutationOperations.at(itlM).getNumReplacor() < 1)
                            mutationOperations.erase(mutationOperations.begin() + itlM);
                        else
                            ++itlM;
                    }
                    
                    //Finished extraction for a line, Append to cofiguration
                    for (auto &opsofmatch: mutationOperations)
                    {
                        configuration.mutators.push_back(opsofmatch);
                    }
                }
                else
                {   //should not be reached
                    llvm::errs() << "Invalid Line: '" << linei << "'\n";
                    return false; //assert (false && "Invalid Line!!!")
                }
                
                matchRepl++;
            }
        }
    }
    else
    {
        llvm::errs() << "Error while opening (or empty) mutant configuration '" << mutConfFile << "'\n";
        return false;
    }
    
    /*//DEBUG
    for (llvmMutationOp oops: configuration.mutators)   //DEBUG
        llvm::errs() << oops.toString();    //DEBUG
    return false;   //DEBUG*/
    
    return true; 
}

void Mutation::getanothermutantIDSelectorName()
{
     static unsigned tempglob = 0;
     mutantIDSelectorName.assign("klee_semu_GenMu_Mutant_ID_Selector");
     if (tempglob > 0)
        mutantIDSelectorName.append(std::to_string(tempglob));
     tempglob++;
     
     mutantIDSelectorName_Func.assign(mutantIDSelectorName + "_Func");
}

/* Assumes that the IRs instructions in stmtIR have same order as the initial original IR code*/
//@@TODO Remove this Function : UNSUSED
llvm::BasicBlock * Mutation::getOriginalStmtBB (std::vector<llvm::Value *> &stmtIR, unsigned stmtcount)
{
    llvm::SmallDenseMap<llvm::Value *, llvm::Value *> pointerMap;
    llvm::BasicBlock* block = llvm::BasicBlock::Create(llvm::getGlobalContext(), std::string("KS.original_Mut0.")+std::to_string(stmtcount), currentFunction, currentBasicBlock);
    for (llvm::Value * I: stmtIR)
    { 
        //clone instruction
        llvm::Value * newI = llvm::dyn_cast<llvm::Instruction>(I)->clone();
        
        //set name
        if (I->hasName())
            newI->setName((I->getName()).str()+"_Mut0");
        
        if (! pointerMap.insert(std::pair<llvm::Value *, llvm::Value *>(I, newI)).second)
        {
            llvm::errs() << "Error (Mutation::getOriginalStmtBB): inserting an element already in the map\n";
            return nullptr;
        }
    };
    for (llvm::Value * I: stmtIR)
    {
        for(unsigned opos = 0; opos < llvm::dyn_cast<llvm::User>(I)->getNumOperands(); opos++)
        {
            auto oprd = llvm::dyn_cast<llvm::User>(I)->getOperand(opos);
            if (llvm::isa<llvm::Instruction>(oprd))
            {
                if (auto newoprd = pointerMap.lookup(oprd))    //TODO:Double check the use of lookup for this map
                {
                    llvm::dyn_cast<llvm::User>(pointerMap.lookup(I))->setOperand(opos, newoprd);  //TODO:Double check the use of lookup for this map
                }
                else
                {
                    bool fail = false;
                    switch (opos)
                    {
                        case 0:
                            {
                                if (llvm::isa<llvm::StoreInst>(I))
                                    fail = true;
                                break;
                            }
                        case 1:
                            {
                                if (llvm::isa<llvm::LoadInst>(I))
                                    fail = true;
                                break;
                            }
                        default:
                            fail = true;
                    }
                    
                    if (fail)
                    {
                        llvm::errs() << "Error (Mutation::getOriginalStmtBB): lookup an element not in the map -- "; 
                        llvm::dyn_cast<llvm::Instruction>(I)->dump();
                        return nullptr;
                    }
                }
            }
        }
        block->getInstList().push_back(llvm::dyn_cast<llvm::Instruction>(pointerMap.lookup(I)));   //TODO:Double check the use of lookup for this map
    }
    
    if (!block)
    {
         llvm::errs() << "Error (Mutation::getOriginalStmtBB): original Basic block is NULL";
    }
    
    return block;
}

// @Name: Mutation::mutantsOfStmt
// This function takes a statement as a list of IR instruction, using the 
// mutation model specified for this class, generate a list of all possible mutants
// of the statement
void Mutation::mutantsOfStmt (std::vector<llvm::Value *> &stmtIR, std::vector<std::vector<llvm::Value *>> &ret_mutants, llvm::Module &module)
{
    assert ((ret_mutants.size() == 0) && "Error (Mutation::mutantsOfStmt): mutant list result vector is not empty!\n");
    
    bool deleted = false;
    for (llvmMutationOp mutator: configuration.mutators)
    {
        unsigned curNumGenMu = ret_mutants.size();
        switch (mutator.matchOp)
        {
            case mALLSTMT:
            {
                assert ((mutator.mutantReplacorsList.size() == 1 && mutator.mutantReplacorsList.front().first == mDELSTMT) && "only Delete Stmt affect whole statement and match anything");
                if (!deleted && ! llvm::dyn_cast<llvm::Instruction>(stmtIR.back())->isTerminator())     //This to avoid deleting if condition or 'ret'
                {
                    ret_mutants.push_back(std::vector<llvm::Value *>());
                    deleted = true;
                }
                break;
            }
            default:    //Anything beside math anything and delete whole stmt
            {
                llvmMutationOp::setModule(&module);
                usermaps.getMatcherFunc(mutator.matchOp) (stmtIR, mutator, ret_mutants);
                llvmMutationOp::unsetModule();
                // Verify that no constant is considered as instruction in the mutant (inserted in replacement vector)
                for (auto &mutInsVec: ret_mutants)
                {    
                    for (auto *mutIns: mutInsVec)
                    {
                        if(llvm::dyn_cast<llvm::Constant>(mutIns))
                        {
                            llvm::errs() << "\nError: A constant is considered as Instruction (inserted in 'replacement') for mutator (enum ExpElemKeys): " << mutator.matchOp << "\n\n";
                            mutIns->dump();
                            assert (false);
                        }
                    }
                }
            }
        }
        if (unsigned numAddedMu = (ret_mutants.size() - curNumGenMu))
        {
            updateMutantsInfos (stmtIR, mutator.mutOpNameList, curNumGenMu, numAddedMu);
        }
    }
}//~Mutation::mutantsOfStmt

llvm::Function * Mutation::createGlobalMutIDSelector_Func(llvm::Module &module, bool bodyOnly)
{
    llvm::Function *funcForKS = nullptr;
    if (!bodyOnly)
    {
        llvm::Constant* c = module.getOrInsertFunction(mutantIDSelectorName_Func,
                                         llvm::Type::getVoidTy(llvm::getGlobalContext()),
                                         llvm::Type::getInt32Ty(llvm::getGlobalContext()),
                                         llvm::Type::getInt32Ty(llvm::getGlobalContext()),
                                         NULL);
        funcForKS = llvm::cast<llvm::Function>(c);
        assert (funcForKS && "Failed to create function 'GlobalMutIDSelector_Func'");
    }
    else
    {
        funcForKS = module.getFunction(mutantIDSelectorName_Func);
        assert (funcForKS && "function 'GlobalMutIDSelector_Func' is not existing");
    }
    llvm::BasicBlock* block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", funcForKS);
    llvm::IRBuilder<> builder(block);
    llvm::Function::arg_iterator args = funcForKS->arg_begin();
    llvm::Value* x = llvm::dyn_cast<llvm::Value>(args++);
    x->setName("mutIdFrom");
    llvm::Value* y = llvm::dyn_cast<llvm::Value>(args++);
    y->setName("mutIdTo");
    //builder.CreateBinOp(llvm::Instruction::Mul, x, y, "tmp");*/
    builder.CreateRetVoid();
    
    return funcForKS;
}

// @Name: doMutate
// This is the main method of the class Mutation, Call this to mutate a module
bool Mutation::doMutate(llvm::Module &module)
{
    //Instert Mutant ID Selector Global Variable
    while (module.getNamedGlobal(mutantIDSelectorName))
    {
        llvm::errs() << "The gobal variable '" << mutantIDSelectorName << "' already present in code!\n";
        assert (false && "ERROR: Module already mutated!"); //getanothermutantIDSelectorName();
    }
    module.getOrInsertGlobal(mutantIDSelectorName, llvm::Type::getInt32Ty(llvm::getGlobalContext()));
    llvm::GlobalVariable *mutantIDSelectorGlobal = module.getNamedGlobal(mutantIDSelectorName);
    //mutantIDSelectorGlobal->setLinkage(llvm::GlobalValue::CommonLinkage);             //commonlinkage require 0 as initial value
    mutantIDSelectorGlobal->setAlignment(4);
    mutantIDSelectorGlobal->setInitializer(llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, 0, false)));
    
    //TODO: Insert definition of the function whose call argument will tell KLEE-SEMU which mutants to fork
    if (forKLEESEMu)
    {
        funcForKLEESEMu = createGlobalMutIDSelector_Func(module);
    }
    
    //mutate
    unsigned mod_mutstmtcount = 0;
    for (auto &Func: module)
    {
        
        //Skip Function with only Declaration (External function -- no definition)
        if (Func.isDeclaration())
            continue;
            
        if (forKLEESEMu && funcForKLEESEMu == &Func)
            continue;
        
        currentFunction = &Func;
        
        for (auto itBBlock = Func.begin(), F_end = Func.end(); itBBlock != F_end; ++itBBlock)
        { 
            currentBasicBlock = &*itBBlock;
            
            std::vector<std::vector<llvm::Value *>> sourceStmts;
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            std::set<llvm::Value *> visited;
#else
            llvm::SmallPtrSet<llvm::Value *, 5> visited;
#endif
            std::queue<llvm::Value *> curUses;
            
            //Use to maintain same order of instruction as originally when extracting source statement
            int stmtIRcount = 0;
            
            for (auto &Instr: *itBBlock)
            {
                /*if (auto *op = llvm::dyn_cast<llvm::BinaryOperator>(&Instr))
                {
                    llvm::IRBuilder<> builder(op);
                    llvm::Value *lhs = op->getOperand(0);
                    llvm::Value *rhs = op->getOperand(1);
                    llvm::Value *mul = builder.CreateMul(lhs, rhs);
                    
                    for (auto &U: op->uses())
                    {
                        llvm::User *user = U.getUser();
                        user->setOperand(U.getOperandNo(), mul);
                    }
                } */
                
                //Skip llvm debugging functions void @llvm.dbg.declare and void @llvm.dbg.value
                if (auto * callinst = llvm::dyn_cast<llvm::CallInst>(&Instr))
                {
                    if (llvm::Function *fun = callinst->getCalledFunction())    //TODO: handle function alias
                    {
                        if(fun->getName().startswith("llvm.dbg.") && fun->getReturnType()->isVoidTy())
                        {
                            if((/*callinst->getNumArgOperands()==2 && */fun->getName().equals("llvm.dbg.declare")) ||
                                (/*callinst->getNumArgOperands()==3 && */fun->getName().equals("llvm.dbg.value")))
                            {
                                if (visited.count(&Instr)) 
                                    sourceStmts.pop_back();
                                continue;
                            }
                        }
                        if (forKLEESEMu && fun->getName().equals("klee_make_symbolic") && callinst->getNumArgOperands() == 3 && fun->getReturnType()->isVoidTy())
                        {
                            if (visited.count(&Instr)) 
                            {
                                sourceStmts.pop_back();
                                stmtIRcount = 0;
                            }
                            continue;
                        }
                    }
                }
               
                if (visited.count(&Instr))  //is it visited
                {
                    if (stmtIRcount <= 0)
                    {
                        if (stmtIRcount < 0)
                            llvm::errs() << "Error (Mutation::doMutate): 'stmtIRcount' should neve be less than 0; Possibly bug in the porgram.\n";
                        else
                            llvm::errs() << "Error (Mutation::doMutate): Instruction appearing multiple times.\n";
                        return false;
                    }
                    sourceStmts.back().push_back(&Instr);
                    stmtIRcount--;
                    
                    continue;
                }
                
                //Check that Statements are atomic (all IR of stmt1 before any IR of stmt2, except Alloca)
                if (stmtIRcount > 0)
                {
                    llvm::errs() << "Error (Mutation::doMutate): Problem with IR - statements are not atomic (" << stmtIRcount << ").\n";
                    for(auto * rr:sourceStmts.back()) //DEBUG
                        llvm::dyn_cast<llvm::Instruction>(rr)->dump(); //DEBUG
                    return false;
                }
                
                //Do not mind Alloca
                if (llvm::isa<llvm::AllocaInst>(&Instr))
                    continue;
                
                /* //Commented because the mutating function do no delete stmt with terminator instr (to avoid misformed while)
                //make the final unconditional branch part of this statement (to avoid multihop empty branching)
                if (llvm::isa<llvm::BranchInst>(&Instr))
                {
                    if (llvm::dyn_cast<llvm::BranchInst>(&Instr)->isUnconditional() && !visited.empty())
                    {
                        sourceStmts.back().push_back(&Instr);
                        continue;
                    }
                }*/
                
                visited.clear();
                sourceStmts.push_back(std::vector<llvm::Value *>());
                sourceStmts.back().push_back(&Instr); 
                visited.insert(&Instr);
                curUses.push(&Instr);
                while (! curUses.empty())
                {
                    llvm::Value *popInstr = curUses.front();
                    curUses.pop();
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                    for (llvm::Value::use_iterator ui=popInstr->use_begin(), ue=popInstr->use_end(); ui!=ue; ++ui)
                    {
                        auto &U = ui.getUse();
#else
                    for (auto &U: popInstr->uses())
                    {
#endif
                        if (visited.insert(U.getUser()).second)
                        {
                            curUses.push(U.getUser());
                            stmtIRcount++;
                        }
                    }
                    //consider only operands when more than 1 (popInstr is a user or operand or Load or Alloca)
                    //if (llvm::dyn_cast<llvm::User>(popInstr)->getNumOperands() > 1)
                    if (!(llvm::isa<llvm::AllocaInst>(popInstr)))
                    {
                        for(unsigned opos = 0; opos < llvm::dyn_cast<llvm::User>(popInstr)->getNumOperands(); opos++)
                        {
                            auto oprd = llvm::dyn_cast<llvm::User>(popInstr)->getOperand(opos);
                            
                            //@ Check that oprd is not Alloca (done already above 'if')..
                            
                            if (!oprd || llvm::isa<llvm::AllocaInst>(oprd)) // || llvm::isa<llvm::LoadInst>(oprd))
                                continue;
                                
                            if (llvm::dyn_cast<llvm::Instruction>(oprd) && visited.insert(oprd).second)
                            {
                                curUses.push(oprd);
                                stmtIRcount++;
                            }
                        }
                    }
                }
                //curUses is empty here
            }
            
            //actual mutation
            llvm::BasicBlock * sstmtCurBB = &*itBBlock;
            for (auto sstmt: sourceStmts)
            {
                /*llvm::errs() << "\n";   
                for (auto ins: sstmt) 
                {
                    llvm::errs() << ">> ";ins->dump();//U.getUser()->dump();
                }*/
                
                //llvm::BasicBlock * original = getOriginalStmtBB(sstmt, mod_mutstmtcount++);
                //if (! original)
                //    return false;
                
                // Find all mutants and put into 'mutants_vects'
                std::vector<std::vector<llvm::Value *>> mutants_vects;
                mutantsOfStmt (sstmt, mutants_vects, module);
                unsigned nMuts = mutants_vects.size();
                
                //Mutate only when mutable: at least one mutant (nMuts > 0)
                if (nMuts > 0)
                {
                    llvm::Instruction * firstInst = llvm::dyn_cast<llvm::Instruction>(sstmt.front());

#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                    llvm::PassManager PM;
                    llvm::RegionInfo * tmp_pass = new llvm::RegionInfo();
                    PM.add(tmp_pass);   //tmp_pass must be created with 'new'
                    llvm::BasicBlock * original = llvm::SplitBlock(sstmtCurBB, firstInst, tmp_pass);
#else                
                    llvm::BasicBlock * original = llvm::SplitBlock(sstmtCurBB, firstInst);
#endif
                    original->setName(std::string("KS.original_Mut0.Stmt")+std::to_string(mod_mutstmtcount));
                
                    llvm::Instruction * linkterminator = sstmtCurBB->getTerminator();    //this cannot be nullptr because the block just got splitted
                    llvm::IRBuilder<> sbuilder(linkterminator);
                    
                    //TODO: Insert definition of the function whose call argument will tell KLEE-SEMU which mutants to fork
                    if (forKLEESEMu)
                    {
                        std::vector<llvm::Value*> argsv;
                        argsv.push_back(llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, (uint64_t)(curMutantID+1), false)));
                        argsv.push_back(llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, (uint64_t)(curMutantID+nMuts), false)));
                        sbuilder.CreateCall(funcForKLEESEMu, argsv);
                    }
                    
                    llvm::SwitchInst * sstmtMutants = sbuilder.CreateSwitch (sbuilder.CreateAlignedLoad(mutantIDSelectorGlobal, 4), original, nMuts);
                    
                    //Remove old terminator link
                    linkterminator->eraseFromParent();
                    
                    //Separate Mutants(including original) BB from rest of instr
                    if (! llvm::dyn_cast<llvm::Instruction>(sstmt.back())->isTerminator())    //if we have another stmt after this in this BB
                    {
                        firstInst = llvm::dyn_cast<llvm::Instruction>(sstmt.back())->getNextNode();
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                        llvm::BasicBlock * nextBB = llvm::SplitBlock(original, firstInst, tmp_pass);
#else                         
                        llvm::BasicBlock * nextBB = llvm::SplitBlock(original, firstInst);
#endif
                        nextBB->setName(std::string("KS.BBafter.Stmt")+std::to_string(mod_mutstmtcount));
                        
                        sstmtCurBB = nextBB;
                    }
                    else
                    {
                        //llvm::errs() << "Error (Mutation::doMutate): Basic Block '" << original->getName() << "' has no terminator!\n";
                        //return false;
                        sstmtCurBB = original;
                    }
                    
                    //TODO: Insert mutant blocks here
                    //@# MUTANTS (see ELSE bellow)
                    for (auto &mut_vect: mutants_vects)
                    {
                        std::string mutIDstr(std::to_string(++curMutantID));
                        
                        //construct Basic Block and insert before original
                        llvm::BasicBlock* mutBlock = llvm::BasicBlock::Create(llvm::getGlobalContext(), std::string("KS.Mutant_Mut")+mutIDstr, &Func, original);
                        
                        //Add to mutant selection switch
                        sstmtMutants->addCase(llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, (uint64_t)curMutantID, false)), mutBlock);
                        
                        for (auto mutIRIns: mut_vect)
                        {
                            mutBlock->getInstList().push_back(llvm::dyn_cast<llvm::Instruction>(mutIRIns));
                        }
                        
                        if (! llvm::dyn_cast<llvm::Instruction>(sstmt.back())->isTerminator())    //if we have another stmt after this in this BB
                        {
                            //clone original terminator
                            llvm::Instruction * mutTerm = original->getTerminator()->clone();
                
                            //set name
                            if (original->getTerminator()->hasName())
                                mutTerm->setName((original->getTerminator()->getName()).str()+"_Mut"+mutIDstr);
                            
                            //set as mutant terminator
                            mutBlock->getInstList().push_back(mutTerm);
                        }
                    }
                    
                    /*//delete previous instructions
                    auto rit = sstmt.rbegin();
                    for (; rit!= sstmt.rend(); ++rit)
                    {
                        llvm::dyn_cast<llvm::Instruction>(*rit)->eraseFromParent();
                    }*/
                    
                    //Help name the labels for mutants
                    mod_mutstmtcount++;
                }//~ if(nMuts > 0)
            }
            
            //Got to the right block
            while (&*itBBlock != sstmtCurBB)
            {
                itBBlock ++;
            }
            
        }   //for each BB in Function
        

#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
        if (llvm::verifyFunction (Func, llvm::AbortProcessAction))
#else
        if (llvm::verifyFunction (Func, &llvm::errs()))
#endif
        {
            llvm::errs() << "ERROR: Misformed Function('" << Func.getName() << "') After mutation!\n";//module.dump();
            assert(false); //return false;
        }
         
    }   //for each Function in Module
    
    //@ Set the Initial Value of mutantIDSelectorGlobal to '<Highest Mutant ID> + 1' (which is equivalent to original)
    mutantIDSelectorGlobal->setInitializer(llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, (uint64_t)1+curMutantID, false)));
    
    //module.dump();
    
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    if (llvm::verifyModule (module, llvm::AbortProcessAction))
#else
    if (llvm::verifyModule (module, &llvm::errs()))
#endif
    {
        llvm::errs() << "ERROR: Misformed Module after mutation!\n"; 
        assert(false); //return false;
    }
    
    return true;
}//~Mutation::doMutate


void Mutation::doTCE (llvm::Module &module, bool writeMuts)
{
    llvm::GlobalVariable *mutantIDSelGlob = module.getNamedGlobal(mutantIDSelectorName);
    assert (mutantIDSelGlob && "Unmutated module passed to TCE");
    
    unsigned highestMutID = getHighestMutantID (module);
    
    TCE tce;
    
    std::map<unsigned, std::vector<unsigned>> duplicateMap;
    std::vector<llvm::Module *> mutModules;
    for (unsigned id=0; id <= highestMutID; id++)
    {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
        llvm::Module *clonedM = llvm::CloneModule(&module);
#else
        llvm::Module *clonedM = llvm::CloneModule(&module).get();
#endif
        mutModules.push_back(clonedM);  
        mutantIDSelGlob = clonedM->getNamedGlobal(mutantIDSelectorName);
        mutantIDSelGlob->setInitializer(llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, (uint64_t)id, false)));
        mutantIDSelGlob->setConstant(true);
        tce.optimize(*clonedM);
        bool hasEq = false;
        for (auto &M: duplicateMap)
        {
            if (! tce.moduleDiff(mutModules.at(M.first), clonedM))
            {
                hasEq = true;
                duplicateMap.at(M.first).push_back(id);
                break;
            }
        }
        if (! hasEq)
            duplicateMap[id];   //insert id into the map
    }
    
    // Store some statistics about the mutants
    preTCENumMuts = highestMutID;
    postTCENumMuts = duplicateMap.size() - 1;
    //
    
    /*for (auto &p :duplicateMap)
    {
        llvm::errs() << p.first << " <--> {"; 
        for (auto eq: p.second)
            llvm::errs() << eq << " ";
        llvm::errs() << "}\n";
    }*/
    
    //re-assign the ids of mutants
    duplicateMap.erase(0);
    unsigned newmutIDs = 1;
    
    //The keys of duplicateMap are (must be) sorted in increasing order: helpful when enabled 'forKLEESEMu'
    for (auto &mm :duplicateMap)    
    {
        mm.second.clear();
        mm.second.push_back(newmutIDs++);
    }
    
    //
    if (writeMuts)
    {
        assert (writeMutantsCallback(duplicateMap, mutModules) && "Failed to dump mutants IRs");
    }
    
    for (auto &Func: module)
    {
        for (auto &BB: Func)
        {
            for (llvm::BasicBlock::iterator Iit = BB.begin(), Iie = BB.end(); Iit != Iie;)
            {
                llvm::Instruction &Inst = *Iit++;   //we increment here so that 'eraseFromParent' bellow do not cause crash
                if (auto *callI = llvm::dyn_cast<llvm::CallInst>(&Inst))
                {
                    if (forKLEESEMu && callI->getCalledFunction() == module.getFunction(mutantIDSelectorName_Func))
                    {
                        uint64_t fromMID = llvm::dyn_cast<llvm::ConstantInt>(callI->getArgOperand(0))->getZExtValue();
                        uint64_t toMID = llvm::dyn_cast<llvm::ConstantInt>(callI->getArgOperand(1))->getZExtValue();
                        unsigned newFromi = 0, newToi = 0;
                        for (auto i=fromMID; i <= toMID; i++)
                        {
                            if (duplicateMap.count(i) != 0)
                            {
                                newToi = i;
                                if (newFromi == 0)
                                    newFromi = i;
                            }
                        }
                        if (newToi == 0)
                        {
                            callI->eraseFromParent();
                        }
                        else
                        {   
                            callI->setArgOperand (0, llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, (uint64_t)(duplicateMap.at(newFromi).front()), false)));
                            callI->setArgOperand (1, llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, (uint64_t)(duplicateMap.at(newToi).front()), false)));
                        }
                    }
                }
                if (auto *sw = llvm::dyn_cast<llvm::SwitchInst>(&Inst))
                {
                    if (auto *ld = llvm::dyn_cast<llvm::LoadInst>(sw->getCondition()))
                    {
                        if (ld->getOperand(0) == module.getNamedGlobal(mutantIDSelectorName))
                        {
                            uint64_t fromMID=highestMutID;
                            uint64_t toMID=0; 
                            for (llvm::SwitchInst::CaseIt i = sw->case_begin(), e = sw->case_end(); i != e; ++i) 
                            {
                                uint64_t curcase = i.getCaseValue()->getZExtValue();
                                if (curcase > toMID)
                                    toMID = curcase;
                                if (curcase < fromMID)
                                    fromMID = curcase;
                            }
                            for (unsigned i = fromMID; i <= toMID; i++)
                            {
                                llvm::SwitchInst::CaseIt cit = sw->findCaseValue(llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, (uint64_t)(i), false)));
                                if (duplicateMap.count(i) == 0)
                                    sw->removeCase (cit);
                                else
                                    cit.setValue(llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, (uint64_t)(duplicateMap.at(i).front()), false)));
                                
                            }
                        }
                    }
                }
            }
        }
    }
    
    //@ Set the Initial Value of mutantIDSelectorGlobal to '<Highest Mutant ID> + 1' (which is equivalent to original)
    highestMutID = duplicateMap.size();     //here 'duplicateMap' contain only remaining mutant (excluding original)
    mutantIDSelGlob = module.getNamedGlobal(mutantIDSelectorName);
    
    if (highestMutID == 0)
    {       //No mutants
        mutantIDSelGlob->eraseFromParent();
    }
    else
    {
        mutantIDSelGlob->setInitializer(llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, (uint64_t)1+highestMutID, false)));
    }
    
    //create the final version of the meta-mutant file
    if (forKLEESEMu)
    {
        llvm::Function *funcForKS = module.getFunction(mutantIDSelectorName_Func);
        funcForKS->deleteBody();
        tce.optimize(module);
        
        if (highestMutID > 0)   //if There are mutants
            createGlobalMutIDSelector_Func(module, true);
        
        //reduce consecutive range (of selector func) into one //TODO
    }
    else
    {
        tce.optimize(module);
    }
    
}

bool Mutation::getMutant (llvm::Module &module, unsigned mutantID)
{
    unsigned highestMutID = getHighestMutantID (module);
    if (mutantID > highestMutID)
        return false;
    
    llvm::GlobalVariable *mutantIDSelGlob = module.getNamedGlobal(mutantIDSelectorName);    
    TCE tce;
    
    mutantIDSelGlob->setInitializer(llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, (uint64_t)mutantID, false)));
    mutantIDSelGlob->setConstant(true);
    tce.optimize(module);
    
    return true;
}
unsigned Mutation::getHighestMutantID (llvm::Module &module)
{
    llvm::GlobalVariable *mutantIDSelectorGlobal = module.getNamedGlobal(mutantIDSelectorName);
    assert (mutantIDSelectorGlobal && mutantIDSelectorGlobal->getInitializer()->getType()->isIntegerTy() && "Unmutated module passed to TCE");
    return llvm::dyn_cast<llvm::ConstantInt>(mutantIDSelectorGlobal->getInitializer())->getZExtValue() - 1;
}

void Mutation::updateMutantsInfos (std::vector<llvm::Value *> &stmtIR, std::vector<std::string> &mutNameList, unsigned startID, unsigned num)
{
    if (!(num >= mutNameList.size() && num % mutNameList.size() == 0)) return;  //temporary
    assert ((num >= mutNameList.size() && num % mutNameList.size() == 0) && "number of mutants generated and number of operator mismatch");
    std::stringstream ss;
    std::string location;
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
//FIXME
#else
    for (auto * val: stmtIR)
    {
        if (const llvm::Instruction *I = llvm::dyn_cast<llvm::Instruction>(val)) 
        {
            const llvm::DebugLoc& Loc = I->getDebugLoc();
            if (Loc) 
            {
                std::string tstr;
                llvm::raw_string_ostream ross(tstr);
                Loc.print(llvm::cast<llvm::raw_ostream>(ross));
                //ross.flush();
                location.assign(ross.str());
                break;
            }
        }
    }
#endif
    
    startID += 1 + curMutantID;
    for (unsigned id = startID; id < startID + num; id++)
    {
        ss.str("");     //reset content
        ss.clear();     //reset content
        ss << id << ", " << mutNameList.at((id - startID) % mutNameList.size()) << ", " << location;
        mutantsInfos.push_back(ss.str());
    }
}

Mutation::~Mutation ()
{
    llvm::errs() << "\n~~~~~~~~~ MUTANTS INFOS ~~~~~~~~~\n\nID, Name, Location\n-------------------\n";
    for (std::string & str: mutantsInfos)
    {
        llvm::errs() << str << "\n";
    }
    
    llvm::errs() << "\nNumber of Mutants:   PreTCE: " << preTCENumMuts << ", PostTCE: " << postTCENumMuts << "\n";
    
    //Clear the constant map to avoid double free
    llvmMutationOp::destroyPosConstValueMap();
}


