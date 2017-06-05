
#include <vector>
#include <set>
#include <queue>
#include <stack>
#include <tuple>
#include <sstream>
#include <regex>
#include <fstream>

#include "ReadWriteIRObj.h"

#include "mutation.h"
#include "usermaps.h"
#include "typesops.h"
#include "tce.h"    //Trivial Compiler Equivalence
#include "operatorsClasses/GenericMuOpBase.h"

#include "llvm/ADT/SmallVector.h"
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
#include "llvm/Linker.h" //for Linker
#else
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h" //for Linker
#endif
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Transforms/Utils/Cloning.h"  //for CloneModule

#include "llvm/Transforms/Utils/Local.h"  //for llvm::DemotePHIToStack   used to remove PHI nodes after mutation
#include "llvm/Analysis/CFG.h"  // llvm::GetSuccessorNumber and  llvm::GetSuccessorNumber  

/*#ifdef ENABLE_OPENMP_PARALLEL
#include "omp.h"
#endif*/

Mutation::Mutation(llvm::Module &module, std::string mutConfFile, DumpMutFunc_t writeMutsF, std::string scopeJsonFile): forKLEESEMu(true), funcForKLEESEMu(nullptr), writeMutantsCallback(writeMutsF), moduleInfo (&module, &usermaps)
{
    // tranform the PHI Node with any non-constant incoming value with reg2mem
    preprocessVariablePhi(module);
    
    //set module
    currentInputModule = &module;
    currentMetaMutantModule = currentInputModule;       //for now the input is transformed (mutated to become mutant)
    
    //get mutation config (operators)
    assert(getConfiguration(mutConfFile) && "@Mutation(): getConfiguration(mutconfFile) Failed!");
    
    //Get scope info
    mutationScope.Initialize(module, scopeJsonFile);
    
    //initialize mutantIDSelectorName
    getanothermutantIDSelectorName();
    curMutantID = 0;
}

/**
 * \brief PREPROCESSING - Remove PHI Nodes, replacing by reg2mem, for every funtion in module
 */
void Mutation::preprocessVariablePhi (llvm::Module &module)
{
    // Replace the PHI node with memory, to avoid error with verify, as it don't get the relation of mutant ID...
    for (auto &Func: module)
    {
        if (skipFunc (Func))
            continue;
            
        llvm::CastInst *AllocaInsertionPoint = nullptr;
        llvm::BasicBlock * BBEntry = &(Func.getEntryBlock());
        llvm::BasicBlock::iterator I = BBEntry->begin();
        while (llvm::isa<llvm::AllocaInst>(I)) ++I;
 
        AllocaInsertionPoint = new llvm::BitCastInst(
           llvm::Constant::getNullValue(llvm::Type::getInt32Ty(Func.getContext())),
           llvm::Type::getInt32Ty(Func.getContext()), "my reg2mem alloca point", &*I);
                       
        std::vector<llvm::PHINode *> phiNodes;
        for (auto &bb: Func)
            for (auto &instruct: bb)
                if (auto *phiN = llvm::dyn_cast<llvm::PHINode>(&instruct))
                    phiNodes.push_back(phiN);
        for (auto it=phiNodes.rbegin(), ie=phiNodes.rend(); it!=ie; ++it)
        {
            auto *phiN = *it;
            bool hasNonConstIncVal = true; /*false;
            for (unsigned pind=0, pe=phiN->getNumIncomingValues(); pind < pe; ++pind)
            {
                if (! llvm::isa<llvm::Constant>(phiN->getIncomingValue(pind)))
                {
                    hasNonConstIncVal = true;
                    break;
                }
            }*/
            if (hasNonConstIncVal)
            {
                /***if (! AllocaInsertionPoint) 
                {
                    llvm::BasicBlock * BBEntry = &(Func.getEntryBlock());
                    llvm::BasicBlock::iterator I = BBEntry->begin();
                    while (llvm::isa<llvm::AllocaInst>(I)) ++I;
             
                    AllocaInsertionPoint = new llvm::BitCastInst(
                       llvm::Constant::getNullValue(llvm::Type::getInt32Ty(Func.getContext())),
                       llvm::Type::getInt32Ty(Func.getContext()), "my reg2mem alloca point", &*I);
                }****/
                auto * allocaPN = MYDemotePHIToStack (phiN, AllocaInsertionPoint);
                assert(allocaPN && "Failed to transform phi node (Maybe PHI Node has no 'uses')");
            }
        }
        
        //Now take care of the values used on different Blocks: there is no more Phi Nodes
        std::vector<llvm::Instruction *> crossBBInsts;
        for (auto &bb: Func)
        {
            for (auto &instruct: bb)
            {
                if (llvm::isa<llvm::AllocaInst>(&instruct))
                    continue;
                    
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                for (llvm::Value::use_iterator ui=instruct.use_begin(), ue=instruct.use_end(); ui!=ue; ++ui)
                {
                    auto &Usr = ui.getUse();
#else
                for (auto &Usr: instruct.uses())
                {
#endif
                    llvm::Instruction *U = llvm::dyn_cast<llvm::Instruction>(Usr.getUser());
                    if (U->getParent() != &bb)
                    {
                        crossBBInsts.push_back(&instruct);
                        break;
                    }
                }
            } 
        }
        for (auto * Inst: crossBBInsts)
            MyDemoteRegToStack(*Inst, false/*volitile?*/, AllocaInsertionPoint);
            
        if (AllocaInsertionPoint)
            AllocaInsertionPoint->eraseFromParent();
        
        Mutation::checkFunctionValidity(Func, "ERROR: PreProcessing ERROR: Preprocessing Broke Function!");
    }
}

/// DemotePHIToStack - This function takes a virtual register computed by a PHI
/// node and replaces it with a slot in the stack frame allocated via alloca.
/// The PHI node is deleted. It returns the pointer to the alloca inserted.
/// @MuLL: edited from "llvm/lib/Transforms/Utils/DemoteRegToStack.cpp"
///TODO: @todo Update this as LLVM evolve
llvm::AllocaInst *Mutation::MYDemotePHIToStack(llvm::PHINode *P, llvm::Instruction *AllocaPoint) 
{
    if (P->use_empty()) 
    {
        P->eraseFromParent();
        return nullptr;
    }

    // Create a stack slot to hold the value.
    llvm::AllocaInst *Slot;
    if (AllocaPoint) 
    {
        Slot = new llvm::AllocaInst(P->getType(), nullptr,
                            P->getName()+".reg2mem", AllocaPoint);
    } 
    else 
    {
        /*llvm::Function *F = P->getParent()->getParent();
        Slot = new llvm::AllocaInst(P->getType(), nullptr, P->getName() + ".reg2mem",
                              &F->getEntryBlock().front());*/
        assert (false && "must have non-null insertion point, which if after all allocates of entry BB");
    }

    // Iterate over each operand inserting a store in each predecessor.
    for (unsigned i = 0, e = P->getNumIncomingValues(); i < e; ++i) 
    {
        if (llvm::InvokeInst *II = llvm::dyn_cast<llvm::InvokeInst>(P->getIncomingValue(i))) 
        {
            assert(II->getParent() != P->getIncomingBlock(i) &&
                    "Invoke edge not supported yet"); (void)II;
        }
        llvm::Instruction *storeInsertPoint=nullptr;
        llvm::Value *incomingValue = P->getIncomingValue(i);
        llvm::BasicBlock *incomingBlock = P->getIncomingBlock(i);
        //avoid atomicity problem of high level stmts
        if ((storeInsertPoint = llvm::dyn_cast<llvm::Instruction>(incomingValue)) && (storeInsertPoint->getParent() == incomingBlock))
        {
            storeInsertPoint = storeInsertPoint->getNextNode();
            assert(storeInsertPoint && "storeInsertPoint is null");
        }
        else //if (llvm::isa<llvm::Constant>(incomingValue) || incomingBlock != incomingValue->getParent())  
        {
            if (AllocaPoint->getParent() == incomingBlock)
                storeInsertPoint = AllocaPoint;
            else
                storeInsertPoint = &*(incomingBlock->begin());  
        }
        new llvm::StoreInst(incomingValue, Slot, storeInsertPoint);
                      //P->getIncomingBlock(i)->getTerminator());
    }

    // Insert a load in place of the PHI and replace all uses.
    llvm::BasicBlock::iterator InsertPt = P->getIterator();

    for (; llvm::isa<llvm::PHINode>(InsertPt) || InsertPt->isEHPad(); ++InsertPt)
        /* empty */;   // Don't insert before PHI nodes or landingpad instrs.

    llvm::Value *V = new llvm::LoadInst(Slot, P->getName() + ".reload", &*InsertPt);
    P->replaceAllUsesWith(V);

    // Delete PHI.
    P->eraseFromParent();
    return Slot;
}

/// DemoteRegToStack - This function takes a virtual register computed by an
/// Instruction and replaces it with a slot in the stack frame, allocated via
/// alloca.  This allows the CFG to be changed around without fear of
/// invalidating the SSA information for the value.  It returns the pointer to
/// the alloca inserted to create a stack slot for I.
/// @MuLL: edited from "llvm/lib/Transforms/Utils/DemoteRegToStack.cpp"
///TODO: @todo Update this as LLVM evolve
llvm::AllocaInst *Mutation::MyDemoteRegToStack(llvm::Instruction &I, bool VolatileLoads, llvm::Instruction *AllocaPoint) 
{
    /*if (I.use_empty()) 
    {
        I.eraseFromParent();
        return nullptr;
    }*/

    //Change the users from different BB to read from stack.
    std::vector<llvm::Instruction *> crossBBUsers;
    llvm::BasicBlock *Ibb = I.getParent();
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    for (llvm::Value::use_iterator ui=I.use_begin(), ue=I.use_end(); ui!=ue; ++ui)
    {
        auto &Usr = ui.getUse();
#else
    for (auto &Usr: I.uses())
    {
#endif
        llvm::Instruction *U = llvm::dyn_cast<llvm::Instruction>(Usr.getUser());
        if (U->getParent() != Ibb)
            crossBBUsers.push_back(U);
    }
    
    llvm::AllocaInst *Slot = nullptr;
    if (!crossBBUsers.empty())
    {
        // Create a stack slot to hold the value.
        if (AllocaPoint) 
        {
            Slot = new llvm::AllocaInst(I.getType(), nullptr,
                                I.getName()+".reg2mem", AllocaPoint);
        } 
        else 
        {
            /*llvm::Function *F = I.getParent()->getParent();
            Slot = new llvm::AllocaInst(I.getType(), nullptr, I.getName() + ".reg2mem",
                                  &F->getEntryBlock().front());*/
            assert (false && "must have non-null insertion point, which if after all allocates of entry BB");
        }

        // We cannot demote invoke instructions to the stack if their normal edge
        // is critical. Therefore, split the critical edge and create a basic block
        // into which the store can be inserted.  TODO TODO: Check out whether this is needed here (Invoke..)
        if (llvm::InvokeInst *II = llvm::dyn_cast<llvm::InvokeInst>(&I)) 
        {
            if (!II->getNormalDest()->getSinglePredecessor()) 
            {
                unsigned SuccNum = llvm::GetSuccessorNumber(II->getParent(), II->getNormalDest());
                assert(llvm::isCriticalEdge(II, SuccNum) && "Expected a critical edge!");
                llvm::BasicBlock *BB = llvm::SplitCriticalEdge(II, SuccNum);
                assert(BB && "Unable to split critical edge.");
                (void)BB;
            }
        }
        
        for (auto *U: crossBBUsers)
        {
            //XXX: No need to care for PHI Nodes, since they are already all removed
            llvm::Value *V = new llvm::LoadInst(Slot, I.getName()+".reload", VolatileLoads, U);
            U->replaceUsesOfWith(&I, V);
        }

        // Insert stores of the computed value into the stack slot. We have to be
        // careful if I is an invoke instruction, because we can't insert the store
        // AFTER the terminator instruction.
        llvm::BasicBlock::iterator InsertPt;
        if (!llvm::isa<llvm::TerminatorInst>(I)) 
        {
            InsertPt = ++I.getIterator();
            for (; llvm::isa<llvm::PHINode>(InsertPt) || InsertPt->isEHPad(); ++InsertPt)
              /* empty */;   // Don't insert before PHI nodes or landingpad instrs.
        } 
        else 
        {
            llvm::InvokeInst &II = llvm::cast<llvm::InvokeInst>(I);
            InsertPt = II.getNormalDest()->getFirstInsertionPt();
        }

        new llvm::StoreInst(&I, Slot, &*InsertPt);
    }
    return Slot;
}

/**
 * \brief The function that mutation will skip
 */
inline bool Mutation::skipFunc (llvm::Function &Func)
{
    assert ((funcForKLEESEMu == nullptr || Func.getParent() == currentMetaMutantModule) && "calling skipFunc on the wrong module. Must be called only in doMutate");
    
    //Skip Function with only Declaration (External function -- no definition)
    if (Func.isDeclaration())
        return true;
        
    if (forKLEESEMu && funcForKLEESEMu == &Func)
        return true;
        
    if (mutationScope.isInitialized() && ! mutationScope.functionInMutationScope(&Func))
        return true;
        
    return false;
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
                    std::regex rgx3("\\s*;\\s*");        // Replacors
                    std::sregex_token_iterator iter3(matchstr3.begin(),matchstr3.end(), rgx3, -1);
                    std::sregex_token_iterator end3;
                    for ( ; iter3 != end3; ++iter3)     //For each replacor
                    {
                        std::string tmprepl(std::regex_replace(std::string(*iter3), std::regex("^\\s+|\\s+$"), std::string("$1")));
                        std::regex rgx4("\\s*,\\s*|\\s*\\(\\s*|\\s*\\)\\s*");        // MutName, operation(operand1,operand2,...)
                        std::sregex_token_iterator iter4(tmprepl.begin(),tmprepl.end(), rgx4, -1);
                        std::sregex_token_iterator end4;
                        std::string mutName(*iter4);
                        ++iter4; 
                        if (!(iter4 != end4))
                        {
                            llvm::errs() << "only Mutant name, no info at line " << confLineNum << ", mutantion op name: " <<  mutName << "\n";
                            assert (iter4 != end4 && "only Mutant name, no info!");
                        }
                        std::string mutoperation(*iter4);
                        
                        //If replace with constant number, add it here
                        auto contvaloprd = llvmMutationOp::insertConstValue (mutoperation, true);   //numeric=true
                        if (llvmMutationOp::isSpecifiedConstIndex(contvaloprd))
                        {
                            reploprd.clear();
                            reploprd.push_back(contvaloprd); 
                            for (auto & mutOper:mutationOperations)
                                mutOper.addReplacor(mCONST_VALUE_OF, reploprd, mutName);
                            ++iter4; assert (iter4 == end4 && "Expected no mutant operand for const value replacement!");
                            continue;
                        }
                           
                        ++iter4; 
                        if (!(iter4 != end4))
                        {
                            if (! usermaps.isDeleteStmtConfName(mutoperation))
                            {
                                llvm::errs() << "no mutant operands at line " << confLineNum << ", replacor: " << mutoperation << "\n";
                                assert (iter4 != end4 && "no mutant operands");
                            }
                        }
                        
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
                            if (llvmMutationOp::isSpecifiedConstIndex(pos4 = llvmMutationOp::insertConstValue (std::string(*iter4), true)))     //numeric=true
                            {
                                reploprd.push_back(pos4); 
                                continue;
                            }
                            
                            // If CALLED Function replacement
                            if (mutationOperations.back().getMatchOp() == mCALL)
                            {
                                for (auto &obj: mutationOperations)
                                    assert (obj.getMatchOp() == mCALL && "All 4 types should be mCALL here");
                                pos4 = llvmMutationOp::insertConstValue (std::string(*iter4), false);
                                assert (llvmMutationOp::isSpecifiedConstIndex(pos4) && "Insertion should always work here");
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
                        while (iM < correspKeysMutant->size() && (isExpElemKeys_ForbidenType(mutationOperations[iM].getMatchOp()) || isExpElemKeys_ForbidenType(correspKeysMutant->at(iM))))
                        {
                            iM++;
                        }
                        if (iM >= correspKeysMutant->size())
                            continue;
                            
                        mutationOperations[iM].addReplacor(correspKeysMutant->at(iM), reploprd, mutName);
                        enum ExpElemKeys prevMu = correspKeysMutant->at(iM);
                        enum ExpElemKeys prevMa = mutationOperations[iM].getMatchOp();
                        for (iM=iM+1; iM < correspKeysMutant->size(); iM++)
                        {
                            if (! isExpElemKeys_ForbidenType(mutationOperations[iM].getMatchOp()) && ! isExpElemKeys_ForbidenType(correspKeysMutant->at(iM)))
                            {
                                if (correspKeysMutant->at(iM) != prevMu || mutationOperations[iM].getMatchOp() != prevMa)
                                {    
                                    mutationOperations[iM].addReplacor(correspKeysMutant->at(iM), reploprd, mutName);
                                    prevMu = correspKeysMutant->at(iM);
                                    prevMa = mutationOperations[iM].getMatchOp();
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

// @Name: Mutation::getMutantsOfStmt
// This function takes a statement as a list of IR instruction, using the 
// mutation model specified for this class, generate a list of all possible mutants
// of the statement
void Mutation::getMutantsOfStmt (MatchStmtIR const &stmtIR, MutantsOfStmt &ret_mutants, ModuleUserInfos const &moduleInfo)
{
    assert ((ret_mutants.getNumMuts() == 0) && "Error (Mutation::getMutantsOfStmt): mutant list result vector is not empty!\n");
    
    bool isDeleted = false;
    for (llvmMutationOp &mutator: configuration.mutators)
    {
        /*switch (mutator.getMatchOp())
        {
            case mALLSTMT:
            {
                assert ((mutator.getNumReplacor() == 1 && mutator.getReplacor(0).first == mDELSTMT) && "only Delete Stmt affect whole statement and match anything");
                if (!isDeleted && ! llvm::dyn_cast<llvm::Instruction>(stmtIR.back())->isTerminator())     //This to avoid deleting if condition or 'ret'
                {
                    ret_mutants.push_back(std::vector<llvm::Value *>());
                    isDeleted = true;
                }
                break;
            }
            default:    //Anything beside math anything and delete whole stmt
            {*/
        usermaps.getMatcherObject(mutator.getMatchOp())->matchAndReplace (stmtIR, mutator, ret_mutants, isDeleted, moduleInfo);
        
        // Check that load and stores type are okay
        for (MuLL::MutantIDType i=0, ie=ret_mutants.getNumMuts(); i < ie; i++)
        {
            if (! ret_mutants.getMutantStmtIR(i).checkLoadAndStoreTypes())
            {
                llvm::errs() << "\nMutation: " << ret_mutants.getTypeName(i);
                //for (auto &mn: mutator.getMutantReplacorsList())
                //    llvm::errs() << mn.getMutOpName() << "; ";
                llvm::errs() << "\n\n";
                //for (auto *xx: stmtIR.getIRList()) xx->dump();
                assert (false);
            }
        }
        
        // Verify that no constant is considered as instruction in the mutant (inserted in replacement vector)  TODO: Remove commented bellow
        /*# llvm::errs() << "\n@orig\n";   //DBG
        for (auto *dd: stmtIR)          //DBG
            dd->dump();                 //DBG*/
        /*for (auto ind = 0; ind < ret_mutants.getNumMuts(); ind++)
        {    
            auto &mutInsVec = ret_mutants.getMutantStmtIR(ind);
            /*# llvm::errs() << "\n@Muts\n";    //DBG* /
            for (auto *mutIns: mutInsVec)
            {
                /*# mutIns->dump();     //DBG* /
                if(llvm::dyn_cast<llvm::Constant>(mutIns))
                {
                    llvm::errs() << "\nError: A constant is considered as Instruction (inserted in 'replacement') for mutator (enum ExpElemKeys): " << mutator.getMatchOp() << "\n\n";
                    mutIns->dump();
                    assert (false);
                }
            }
        }*/
            //}
        //}
    }
}//~Mutation::getMutantsOfStmt

llvm::Function * Mutation::createGlobalMutIDSelector_Func(llvm::Module &module, bool bodyOnly)
{
    llvm::Function *funcForKS = nullptr;
    if (!bodyOnly)
    {
        llvm::Constant* c = module.getOrInsertFunction(mutantIDSelectorName_Func,
                                         llvm::Type::getVoidTy(moduleInfo.getContext()),
                                         llvm::Type::getInt32Ty(moduleInfo.getContext()),
                                         llvm::Type::getInt32Ty(moduleInfo.getContext()),
                                         NULL);
        funcForKS = llvm::cast<llvm::Function>(c);
        assert (funcForKS && "Failed to create function 'GlobalMutIDSelector_Func'");
    }
    else
    {
        funcForKS = module.getFunction(mutantIDSelectorName_Func);
        assert (funcForKS && "function 'GlobalMutIDSelector_Func' is not existing");
    }
    llvm::BasicBlock* block = llvm::BasicBlock::Create(moduleInfo.getContext(), "entry", funcForKS);
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

void Mutation::checkModuleValidity(llvm::Module &Mod, const char *errMsg)
{
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    if (verifyFuncModule && llvm::verifyModule (Mod, llvm::AbortProcessAction))
#else
    if (verifyFuncModule && llvm::verifyModule (Mod, &llvm::errs()))
#endif
    {
        llvm::errs() << errMsg << "\n"; 
        assert(false); //return false;
    }
}

void Mutation::checkFunctionValidity(llvm::Function &Func, const char *errMsg)
{
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
        if (verifyFuncModule && llvm::verifyFunction (Func, llvm::AbortProcessAction))
#else
        if (verifyFuncModule && llvm::verifyFunction (Func, &llvm::errs()))
#endif
        {
            llvm::errs() << errMsg << "(" << Func.getName() << ")\n";
            assert(false); //return false;
        }
}

// @Name: doMutate
// This is the main method of the class Mutation, Call this to mutate a module
bool Mutation::doMutate()
{
    llvm::Module &module = *currentMetaMutantModule;
    
    //Instert Mutant ID Selector Global Variable
    while (module.getNamedGlobal(mutantIDSelectorName))
    {
        llvm::errs() << "The gobal variable '" << mutantIDSelectorName << "' already present in code!\n";
        assert (false && "ERROR: Module already mutated!"); //getanothermutantIDSelectorName();
    }
    module.getOrInsertGlobal(mutantIDSelectorName, llvm::Type::getInt32Ty(moduleInfo.getContext()));
    llvm::GlobalVariable *mutantIDSelectorGlobal = module.getNamedGlobal(mutantIDSelectorName);
    //mutantIDSelectorGlobal->setLinkage(llvm::GlobalValue::CommonLinkage);             //commonlinkage require 0 as initial value
    mutantIDSelectorGlobal->setAlignment(4);
    mutantIDSelectorGlobal->setInitializer(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, 0, false)));
    
    //XXX: Insert definition of the function whose call argument will tell KLEE-SEMU which mutants to fork
    if (forKLEESEMu)
    {
        funcForKLEESEMu = createGlobalMutIDSelector_Func(module);
    }
    
    //mutate
    struct SourceStmtsSearchList
    {
        std::vector<StatementSearch *> sourceOrderedStmts;
        llvm::BasicBlock *curBB=nullptr;
        StatementSearch * createNewElem(llvm::BasicBlock *bb)
        {
            if (bb != curBB)
            {
                //if (curBB != nullptr)
                sourceOrderedStmts.push_back(nullptr);
                curBB = bb;
            }
            sourceOrderedStmts.push_back(new StatementSearch);
            return sourceOrderedStmts.back();
        }
        void remove (StatementSearch *ss)
        {
            delete ss;
            bool found = false;
            for (auto i=sourceOrderedStmts.size()-1; i>=0; i--)
                if (sourceOrderedStmts[i] == ss)
                {
                    sourceOrderedStmts.erase(sourceOrderedStmts.begin()+i);
                    found = true;
                }
            assert (found && "removing a statement not inserted");
        }
        void appendOrder(llvm::BasicBlock *bb, StatementSearch *ss)
        {
            if (bb != curBB)
            {
                assert (curBB != nullptr && "calling append before calling createNewElem");
                sourceOrderedStmts.push_back(nullptr);
                curBB = bb;
            }
            sourceOrderedStmts.push_back(ss);
        }
        void doneSearch() {sourceOrderedStmts.push_back(nullptr);}
        void clear() 
        {
            std::unordered_set<StatementSearch *> stmp(sourceOrderedStmts.begin(), sourceOrderedStmts.end());
            for (auto *ss: stmp)
                if (ss)
                    delete ss;
            sourceOrderedStmts.clear();
            curBB=nullptr;
        }
        std::vector<StatementSearch *> & getSourceOrderedStmts () {return sourceOrderedStmts;}
    } srcStmtsSearchList;
    
    /**
     * \brief This class create a proxy BB for each incoming BB of PHI nodes whose corresponding value is a CONSTANT (independent on value computed on previous BB)
     */
    struct ProxyForPHI
    {
        std::unordered_set<llvm::BasicBlock *> proxies;
        std::unordered_map<llvm::PHINode *, std::unordered_map<llvm::BasicBlock * /*a Basic Block*/, llvm::BasicBlock * /*its Proxy*/>> phiBBProxy;
        llvm::Function *curFunc = nullptr;
        
        void clear(llvm::Function *f) {proxies.clear(); phiBBProxy.clear(); curFunc = f;} 
        bool isProxy(llvm::BasicBlock *bb) {return (proxies.count(bb)>0);}
        void getProxiesTerminators(llvm::PHINode *phi, std::vector<llvm::Instruction *> &terms)
        {
            auto itt = phiBBProxy.find(phi);
            assert (itt != phiBBProxy.end() && "looking for missing phi node");
            for (auto inerIt: itt->second)
                terms.push_back (inerIt.second->getTerminator());
        }
        void handleBB (llvm::BasicBlock *bb, ModuleUserInfos const & MI)
        {   // see http://llvm.org/docs/doxygen/html/BasicBlock_8cpp_source.html#l00401
            llvm::TerminatorInst *TI = bb->getTerminator();
            if (!TI)
                return;
            for (auto i=0; i < TI->getNumSuccessors(); i++)
            {
                llvm::BasicBlock *Succ = TI->getSuccessor(i); 
                for (llvm::BasicBlock::iterator II = Succ->begin(), IE = Succ->end(); II != IE; ++II) 
                {
                    llvm::PHINode *PN = llvm::dyn_cast<llvm::PHINode>(II);
                    if (!PN)
                        break;
                    handlePhi (PN, MI);
                }
            }
        }
        void handlePhi (llvm::PHINode * phi, ModuleUserInfos const & MI)
        {
            static unsigned proxyBBNum = 0;
            llvm::BasicBlock *phiBB = nullptr;
            std::pair<std::unordered_map<llvm::PHINode *, std::unordered_map<llvm::BasicBlock *, llvm::BasicBlock *>>::iterator, bool> ittmp;
            unsigned pind = 0;
            /*for (; pind < phi->getNumIncomingValues(); ++pind)
            {
                if (llvm::isa<llvm::Constant>(phi->getIncomingValue(pind)))
                {
                    ittmp = phiBBProxy.emplace (phi, std::unordered_map<llvm::BasicBlock *, llvm::BasicBlock *>());
                    if (ittmp.second==false)
                        return;
                    phiBB = phi->getParent();
                    break;
                }
            }*/
            ittmp = phiBBProxy.emplace (phi, std::unordered_map<llvm::BasicBlock *, llvm::BasicBlock *>());
            if (ittmp.second==false)
                return;
            phiBB = phi->getParent();
            
            for (; pind < phi->getNumIncomingValues(); ++pind)
            {
                /*if (! llvm::isa<llvm::Constant>(phi->getIncomingValue(pind)))
                    continue;*/
                    
                ///The incoming value is a constant
                llvm::BasicBlock * bb = phi->getIncomingBlock(pind);
                //create proxy
                llvm::BasicBlock * proxyBlock = llvm::BasicBlock::Create(MI.getContext(), std::string("MuLL.PHI_N_Proxy")+std::to_string(proxyBBNum++), curFunc, bb->getNextNode());
                llvm::BranchInst::Create(phiBB, proxyBlock);    //create unconditional branch to phiBB and insert at the end of proxyBlock
                if((ittmp.first->second.emplace(bb, proxyBlock)).second == false)
                {
                    proxyBlock->eraseFromParent();
                }
                else
                {
                    proxies.insert(proxyBlock);
                    
                    phi->setIncomingBlock(pind, proxyBlock);
                    llvm::TerminatorInst *TI = bb->getTerminator();
                    bool found=false;     //DEBUG
                    for (auto i=0; i < TI->getNumSuccessors(); i++)
                    {
                        if (phiBB == TI->getSuccessor(i))
                        {
                            TI->setSuccessor(i, proxyBlock);
                            found=true;     //DEBUG
                        }
                    }
                    assert (found && "bb must have phiBB as a successor");  //DEBUG
                }
            }            
        }
    } phiProxy;
    
    std::set<StatementSearch *> remainMultiBBLiveStmts;     //pos in sourceStmts of the statement spawning multiple BB. The actual mutation happend only when this is empty at the end of a BB.
    unsigned mod_mutstmtcount = 0;
    StatementSearch *curLiveStmtSearch = nullptr;   //set to null after each stmt search completion
    
    /******************************************************
     **** Search for high level statement (source level) **
     ******************************************************/
    for (auto &Func: module)
    {
        
        //Skip Function with only Declaration (External function -- no definition)
        if (skipFunc (Func))
            continue;
            
        phiProxy.clear(&Func);
        
        ///\brief This hel recording the IR's LOC: index in the function it belongs
        unsigned instructionPosInFunc = 0;
        
        ///\brief In case we have multiBB stmt, this say which is the first BB to start mutation from. this is equal to itBBlock below if only sigle BB stmts
        /// Set to null after each actual mutation take place
        llvm::BasicBlock * mutationStartingAtBB = nullptr;   
        
        for (auto itBBlock = Func.begin(), F_end = Func.end(); itBBlock != F_end; ++itBBlock)
        { 
            /// Do not mutate the inserted proxy for PHI nodes
            if (phiProxy.isProxy(&*itBBlock))
                continue;
            
            /// set the Basic block from which the actual mutation should start
            if (! mutationStartingAtBB)
                mutationStartingAtBB = &*itBBlock;
                
            //make sure that in case this BB has phi node as successor, proxy BB will be created and added.
            phiProxy.handleBB(&*itBBlock, moduleInfo);
            
            std::queue<llvm::Value *> curUses;
            
            for (auto &Instr: *itBBlock)
            {
                instructionPosInFunc++;     // This should always be before anything else in this loop
                
                // For Now do not mutate Exeption handling code, TODO later. TODO (http://llvm.org/docs/doxygen/html/Instruction_8h_source.html#l00393)
                if (Instr.isEHPad())
                {
                    llvm::errs() << "(msg) Exception handling not mutated for now. TODO\n";
                    continue;
                }
                
                //Do not mind Alloca
                if (llvm::isa<llvm::AllocaInst>(&Instr))
                    continue;
                    
                //If PHI node and wasn't processed by proxy, add proxies
                if (auto *phiN = llvm::dyn_cast<llvm::PHINode>(&Instr))
                    phiProxy.handlePhi(phiN, moduleInfo);
                    
                //In case this is not the beginig of a stmt search (there are live stmts)
                if (curLiveStmtSearch)
                {
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
                                    if (curLiveStmtSearch->isVisited(&Instr)) 
                                    {
                                        assert (false && "The debug statement should not have been in visited (cause no dependency on others stmts...)");
                                        srcStmtsSearchList.remove(curLiveStmtSearch);
                                    }
                                    continue;
                                }
                            }
                            if (forKLEESEMu && fun->getName().equals("klee_make_symbolic") && callinst->getNumArgOperands() == 3 && fun->getReturnType()->isVoidTy())
                            {
                                if (curLiveStmtSearch->isVisited(&Instr)) 
                                {
                                    srcStmtsSearchList.remove(curLiveStmtSearch);     //do not mutate klee_make_symbolic
                                }
                                continue;
                            }
                        }
                    }
                   
                    if (curLiveStmtSearch->isVisited(&Instr))  //is it visited?
                    {
                        curLiveStmtSearch->checkCountLogic();
                        curLiveStmtSearch->appendIRToStmt(&*itBBlock, &Instr, instructionPosInFunc - 1);
                        curLiveStmtSearch->countDec();
                        
                        continue;
                    }
                }
                
                bool foundd = false;
                for (auto *remMStmt: remainMultiBBLiveStmts)
                {
                    if(remMStmt->isVisited(&Instr))
                    {
                        //Check that Statements are atomic (all IR of stmt1 before any IR of stmt2, except Alloca - actually all allocas are located at the beginning of the function)
                        remMStmt->checkAtomicityInBB(&*itBBlock);
                        
                        curLiveStmtSearch = StatementSearch::switchFromTo(&*itBBlock, curLiveStmtSearch, remMStmt);
                        srcStmtsSearchList.appendOrder(&*itBBlock, curLiveStmtSearch);
                        remainMultiBBLiveStmts.erase(remMStmt);
                        foundd = true;
                        
                        // process as for visited Inst, as above
                        curLiveStmtSearch->checkCountLogic();
                        curLiveStmtSearch->appendIRToStmt(&*itBBlock, &Instr, instructionPosInFunc - 1);
                        curLiveStmtSearch->countDec();
                        
                        break;
                    }
                }
                if (foundd)
                {
                    continue;
                }
                else
                {
                    if (curLiveStmtSearch && ! curLiveStmtSearch->isCompleted())
                    {
                        remainMultiBBLiveStmts.insert(curLiveStmtSearch);
                        curLiveStmtSearch = StatementSearch::switchFromTo(&*itBBlock, curLiveStmtSearch, srcStmtsSearchList.createNewElem(&*itBBlock));     //(re)initialize
                    }
                    else
                    {
                        curLiveStmtSearch = StatementSearch::switchFromTo(&*itBBlock, nullptr, srcStmtsSearchList.createNewElem(&*itBBlock));     //(re)initialize
                    }
                }
                
                /* //Commented because the mutating function do no delete stmt with terminator instr (to avoid misformed while), but only delete for return break and continue in this case
                //make the final unconditional branch part of this statement (to avoid multihop empty branching)
                if (llvm::isa<llvm::BranchInst>(&Instr))
                {
                    if (llvm::dyn_cast<llvm::BranchInst>(&Instr)->isUnconditional() && !visited.empty())
                    {
                        curLiveStmtSearch->appendIRToStmt(&*itBBlock, &Instr, instructionPosInFunc - 1); 
                        continue;
                    }
                }*/
                
                curLiveStmtSearch->appendIRToStmt(&*itBBlock, &Instr, instructionPosInFunc - 1); 
                if (! curLiveStmtSearch->visit(&Instr))
                {
                    //Func.dump();
                    llvm::errs() << "\nInstruction: ";
                    Instr.dump();
                    assert (false && "first time seing an instruction but present in visited. report bug");
                }
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
                        if (curLiveStmtSearch->visit(U.getUser()))  //wasn't visited? insert
                        {
                            curUses.push(U.getUser());
                            curLiveStmtSearch->countInc();
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
                                
                            if (llvm::dyn_cast<llvm::Instruction>(oprd) && curLiveStmtSearch->visit(oprd))
                            {
                                curUses.push(oprd);
                                curLiveStmtSearch->countInc();
                            }
                        }
                    }
                }
                    //curUses is empty here
            }   //for (auto &Instr: *itBBlock)
            
            curLiveStmtSearch = nullptr;
            
            //Check if we can mutate now or not (seach completed all live stmts)
            if (! remainMultiBBLiveStmts.empty())
                continue;
                
           
             
            /***********************************************************
            // \brief Actual mutation **********************************
            /***********************************************************/
            
            /// \brief mutate all the basic blocks between 'mutationStartingAtBB' and '&*itBBlock'
            srcStmtsSearchList.doneSearch();       //append the last nullptr to order...
            auto changingBBIt = mutationStartingAtBB->getIterator();
            auto stopAtBBIt = itBBlock->getIterator(); 
            ++stopAtBBIt;   //pass the current block
            llvm::BasicBlock * sstmtCurBB = nullptr;    //The loop bellow will be executed at least once
            auto curSrcStmtIt = srcStmtsSearchList.getSourceOrderedStmts().begin();
            
            /// Get all the mutants
            for (auto *sstmt: srcStmtsSearchList.getSourceOrderedStmts())
            {
                if (sstmt && sstmt->mutantStmt_list.isEmpty())   //not yet mutated
                {
                    // Find all mutants and put into 'mutantStmt_list'
                    getMutantsOfStmt (sstmt->matchStmtIR, sstmt->mutantStmt_list, moduleInfo);
                    
                    //set the mutant IDs
                    for (auto mind=0; mind < sstmt->mutantStmt_list.getNumMuts(); mind++)
                    {
                        sstmt->mutantStmt_list.setMutID(mind, ++curMutantID);
                        //for(auto &xx:sstmt->mutantStmt_list.getMutantStmtIR(mind).origBBToMutBB)
                        //    for(auto *bb: xx.second)
                        //        bb->dump();   
                    }
                }
            }
            
            //for each BB place in the muatnts 
            for (; changingBBIt != stopAtBBIt; ++changingBBIt)
            {
                /// Do not mutate the inserted proxies for PHI nodes
                if (phiProxy.isProxy(&*changingBBIt))
                    continue;
                
                sstmtCurBB = &*changingBBIt;
                
                for (++curSrcStmtIt/*the 1st is nullptr*/; *curSrcStmtIt != nullptr; ++curSrcStmtIt)   //different BB stmts are delimited by nullptr
                {
                    unsigned nMuts = (*curSrcStmtIt)->mutantStmt_list.getNumMuts();
                    
                    //Mutate only when mutable: at least one mutant (nMuts > 0)
                    if (nMuts > 0)
                    {
                        llvm::Instruction * firstInst, *lastInst;
                        (*curSrcStmtIt)->matchStmtIR.getFirstAndLastIR (&*changingBBIt, firstInst, lastInst);

                        /// If the firstInst (intended basic block plit point) isPHI Node, instead of splitting, directly add the mutant selection switch on the Proxy BB.
                        bool usePhiProxy_NoSplitBB = false;
                        if (llvm::isa<llvm::PHINode>(firstInst)) 
                            usePhiProxy_NoSplitBB = true;
                        
                        llvm::BasicBlock * original = nullptr;
                        std::vector<llvm::Instruction *> linkterminators;
                        std::vector<llvm::SwitchInst *> sstmtMutants;
                        
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                        llvm::PassManager PM;
                        llvm::RegionInfo * tmp_pass = new llvm::RegionInfo();
                        PM.add(tmp_pass);   //tmp_pass must be created with 'new'
                        if (!usePhiProxy_NoSplitBB)
                        {
                            original = llvm::SplitBlock(sstmtCurBB, firstInst, tmp_pass);
#else                
                        if (!usePhiProxy_NoSplitBB)
                        {
                            original = llvm::SplitBlock(sstmtCurBB, firstInst);
#endif
                            original->setName(std::string("MuLL.original_Mut0.Stmt")+std::to_string(mod_mutstmtcount));
                    
                            linkterminators.push_back(sstmtCurBB->getTerminator());    //this cannot be nullptr because the block just got splitted
                        }
                        else
                        {   //PHI Node is always the first non PHI instruction of its BB
                            original = sstmtCurBB;
                            
                            phiProxy.getProxiesTerminators(llvm::dyn_cast<llvm::PHINode>(firstInst), linkterminators);
                        }
                        
                        for (auto *lkt: linkterminators)
                        {
                            llvm::IRBuilder<> sbuilder(lkt);
                            
                            //XXX: Insert definition of the function whose call argument will tell KLEE-SEMU which mutants to fork (done elsewhere)
                            if (forKLEESEMu)
                            {
                                std::vector<llvm::Value*> argsv;
                                argsv.push_back(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)((*curSrcStmtIt)->mutantStmt_list.getMutID(0)), false)));
                                argsv.push_back(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)((*curSrcStmtIt)->mutantStmt_list.getMutID(nMuts-1)), false)));
                                sbuilder.CreateCall(funcForKLEESEMu, argsv);
                            }
                            
                            sstmtMutants.push_back(sbuilder.CreateSwitch (sbuilder.CreateAlignedLoad(mutantIDSelectorGlobal, 4), original, nMuts));
                            
                            //Remove old terminator link
                            lkt->eraseFromParent();
                        }
                        
                        //Separate Mutants(including original) BB from rest of instr
                        if (! llvm::dyn_cast<llvm::Instruction>(lastInst)->isTerminator())    //if we have another stmt after this in this BB
                        {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                            llvm::BasicBlock * nextBB = llvm::SplitBlock(original, lastInst->getNextNode(), tmp_pass);
#else                         
                            llvm::BasicBlock * nextBB = llvm::SplitBlock(original, lastInst->getNextNode());
#endif
                            nextBB->setName(std::string("MuLL.BBafter.Stmt")+std::to_string(mod_mutstmtcount));
                            
                            sstmtCurBB = nextBB;
                        }
                        else
                        {
                            //llvm::errs() << "Error (Mutation::doMutate): Basic Block '" << original->getName() << "' has no terminator!\n";
                            //return false;
                            sstmtCurBB = original;
                        }
                        
                        //XXX: Insert mutant blocks here
                        //@# MUTANTS (see ELSE bellow)
                        for (auto ms_ind = 0; ms_ind < (*curSrcStmtIt)->mutantStmt_list.getNumMuts(); ms_ind++)
                        {
                            auto &mut_stmt_ir = (*curSrcStmtIt)->mutantStmt_list.getMutantStmtIR(ms_ind);
                            std::string mutIDstr(std::to_string((*curSrcStmtIt)->mutantStmt_list.getMutID(ms_ind)));
                            
                            // Store mutant info
                            mutantsInfos.add((*curSrcStmtIt)->mutantStmt_list.getMutID(ms_ind), (*curSrcStmtIt)->matchStmtIR.toMatchIRs, (*curSrcStmtIt)->mutantStmt_list.getTypeName(ms_ind),\
                                                                                 (*curSrcStmtIt)->mutantStmt_list.getIRRelevantPos(ms_ind), &Func, (*curSrcStmtIt)->matchStmtIR.posIRsInOrigFunc);
                            
                            //construct Basic Block and insert before original
                            std::vector<llvm::BasicBlock *> &mutBlocks = mut_stmt_ir.getMut(&*changingBBIt);
                            
                            //Add to mutant selection switch
                            for (auto *swches: sstmtMutants)
                                swches->addCase(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)(*curSrcStmtIt)->mutantStmt_list.getMutID(ms_ind), false)), \
                                                                                                                                                                            mutBlocks.front());
                            
                            for (auto *subBB: mutBlocks)
                            {
                                subBB->setName(std::string("MuLL.Mutant_Mut")+mutIDstr/*+"-"+(*curSrcStmtIt)->mutantStmt_list.getTypeName(ms_ind)*/);
                                subBB->insertInto (&Func, original);
                            }
                            
                            if (! llvm::dyn_cast<llvm::Instruction>(lastInst)->isTerminator())    //if we have another stmt after this in this BB
                            {
                                //clone original terminator
                                llvm::Instruction * mutTerm = original->getTerminator()->clone();
                    
                                //set name
                                if (original->getTerminator()->hasName())
                                    mutTerm->setName((original->getTerminator()->getName()).str()+"_Mut"+mutIDstr);
                                
                                //set as mutant terminator
                                mutBlocks.back()->getInstList().push_back(mutTerm);
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
                changingBBIt = sstmtCurBB->getIterator();   //make 'changeBBIt' foint to the last BB before the next one to explore
            } //Actual mutation for
            
            //Get to the right block
            /*while (&*itBBlock != sstmtCurBB)
            {
                itBBlock ++;
            }*/
            
            // Do not use changingBBIt here because it is advanced
            itBBlock = sstmtCurBB->getIterator();   //make 'changeBBIt' foint to the last BB before the next one to explore
            
            ///\brief Mutation over for the current set of BB, reinitialize 'mutationStartingAtBB' for the coming ones
            mutationStartingAtBB = nullptr;
            
            srcStmtsSearchList.clear();
            
        }   //for each BB in Function
        
        assert (remainMultiBBLiveStmts.empty() && "Something wrong with function (missing IRs) or bug!");
        
        //repairDefinitionUseDomination(Func);  //TODO: when we want to directly support cross BB use
        
        //Func.dump();
        
        Mutation::checkFunctionValidity(Func, "ERROR: Misformed Function After mutation!");
    }   //for each Function in Module
    
    //@ Set the Initial Value of mutantIDSelectorGlobal to '<Highest Mutant ID> + 1' (which is equivalent to selecting the original program)
    mutantIDSelectorGlobal->setInitializer(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)1+curMutantID, false)));
    
    //module.dump();
    
    Mutation::checkModuleValidity(module, "ERROR: Misformed Module after mutation!");
    
    return true;
}//~Mutation::doMutate

/**
 * \brief for the statements spawning multiple BB, make sure that the IR used in other BB will not make verifier complain. We fix by adding a phi node at the convergence point of mutants and original.
 * TODO: When we want do support cross BB use
 */
/*void Mutation::repairDefinitionUseDomination(llvm::Function &Func);
{
    llvm::SwitchInst *mutSelSw = nullptr;
    for (auto &BB: Func)
    {
        for (auto &Inst: BB)
        {
            if (mutSelSw = llvm::dyn_cast<llvm::SwitchInst>(&Inst))
            {
                
            }
        }
    }
}*/

/**
 * \brief obtain the Weak Mutant kill condition and insert it before the instruction @param insertBeforeInst and return the result of the comparison 'original' != 'mutant'
 */
llvm::Value * Mutation::getWMCondition (llvm::BasicBlock *orig, llvm::BasicBlock *mut, llvm::Instruction * insertBeforeInst)
{
    // Look for difference
    
    //TODO: Put the real condition here (original != Mutant)
    //return llvm::ConstantInt::getTrue(moduleInfo.getContext());
    return llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(8, 1, true)); 
}

/**
 * \brief transform non optimized meta-mutant module into weak mutation module. 
 * @param cmodule is the meta mutant module. @note: it will be transformed into WM module, so clone module before this call
 */
void Mutation::computeWeakMutation(std::unique_ptr<llvm::Module> &cmodule, std::unique_ptr<llvm::Module> &modWMLog)
{
    /// Link cmodule with the corresponding driver module (actually only need c module)
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    llvm::Linker linker(cmodule.get());
    std::string ErrorMsg;
    if (linker.linkInModule(modWMLog.get(), &ErrorMsg))
    {
        llvm::errs() << "Failed to link weak mutation module with log function module: " << ErrorMsg << "\n";
        assert (false);
    }
    modWMLog.reset(nullptr);
#else
    llvm::Linker linker(*cmodule);
    if (linker.linkInModule (std::move(modWMLog)))
    {
        assert (false && "Failed to link weak mutation module with log function module");
    }
#endif
    
    llvm::Function *funcWMLog = cmodule->getFunction (wmLogFuncName);
    assert (funcWMLog && "Weak Mutation Log Function absent in WM Module. Was it liked properly?");
    for (auto &Func: *cmodule)
    {
        if (&Func == funcWMLog)
            continue;
        for (auto &BB: Func)
        {
            std::vector<llvm::BasicBlock *> toBeRemovedBB;
            for (llvm::BasicBlock::iterator Iit = BB.begin(), Iie = BB.end(); Iit != Iie;)
            {
                llvm::Instruction &Inst = *Iit++;   //we increment here so that 'eraseFromParent' bellow do not cause crash
                if (auto *callI = llvm::dyn_cast<llvm::CallInst>(&Inst))
                {
                    if (forKLEESEMu && callI->getCalledFunction() == cmodule->getFunction(mutantIDSelectorName_Func))
                    {   //Weak mutation is not seen by KLEE, no need to keep KLEE's function...
                        callI->eraseFromParent();
                    }
                }
                if (auto *sw = llvm::dyn_cast<llvm::SwitchInst>(&Inst))
                {
                    if (auto *ld = llvm::dyn_cast<llvm::LoadInst>(sw->getCondition()))
                    {
                        if (ld->getOperand(0) == cmodule->getNamedGlobal(mutantIDSelectorName))
                        {
                            std::vector<llvm::ConstantInt *> cases;
                            auto *defaultBB = sw->getDefaultDest ();    //original
                            for (llvm::SwitchInst::CaseIt i = sw->case_begin(), e = sw->case_end(); i != e; ++i) 
                            {
                                auto *mutIDConstInt = i.getCaseValue();
                                cases.push_back(mutIDConstInt);     //to be removed later
                                
                                /// Now create the call to weak mutation log func.
                                auto *caseiBB = i.getCaseSuccessor();   //mutant
                                
                                toBeRemovedBB.push_back(caseiBB);
                                
                                llvm::Value *condVal = getWMCondition (defaultBB, caseiBB, sw);
                                llvm::IRBuilder<> sbuilder(sw);
                                std::vector<llvm::Value*> argsv;
                                argsv.push_back(mutIDConstInt);     //mutant ID
                                argsv.push_back(condVal);           //weak kill condition
                                sbuilder.CreateCall(funcWMLog, argsv);  //call WM log func
                            }
                            for (auto *caseval: cases)
                            {
                                llvm::SwitchInst::CaseIt cit = sw->findCaseValue(caseval);
                                sw->removeCase (cit);
                            }
                        }
                    }
                }
            }
            for (auto *bbrm: toBeRemovedBB)
                bbrm->eraseFromParent();
        }
    }
    if (forKLEESEMu)
    {
        llvm::Function *funcForKS = cmodule->getFunction(mutantIDSelectorName_Func);
        funcForKS->eraseFromParent();
    }
    llvm::GlobalVariable *mutantIDSelGlob = cmodule->getNamedGlobal(mutantIDSelectorName);
    //mutantIDSelGlob->setInitializer(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)0, false)));    //Not needed because there is no case anyway, only default
    if (mutantIDSelGlob)
        mutantIDSelGlob->setConstant(true); // only original...
    
    //verify WM module
    Mutation::checkModuleValidity(*cmodule, "ERROR: Misformed WM Module!");
}//~Mutation::computeWeakMutation

/**
 * \brief If targetF is non nul, it should be the function corresponding to srcF in Mod
 */
void Mutation::setModFuncToFunction (llvm::Module *Mod, llvm::Function *srcF, llvm::Function *targetF)
{
    if (targetF == nullptr)
        targetF = Mod->getFunction(srcF->getName());
    else
        assert (targetF->getParent() == Mod && "passed targetF which is not a function of module");
    assert (targetF && "the function Fun do not have an instance in module Mod");
    llvm::ValueToValueMapTy vmap;
    llvm::Function::arg_iterator destI = targetF->arg_begin();
    for (const llvm::Argument &aII: srcF->args())
        if (vmap.count(&aII) == 0)
            vmap[&aII] = &*destI++;
    llvm::SmallVector<llvm::ReturnInst*, 8> Returns;
    targetF->dropAllReferences(); //deleteBody();
    llvm::CloneFunctionInto(targetF, srcF, vmap, true, Returns);
    //verify post clone into module
    //Mutation::checkModuleValidity(*Mod, "ERROR: Misformed module after cloneFunctionInto!");
}

void Mutation::doTCE (std::unique_ptr<llvm::Module> &modWMLog, bool writeMuts, bool isTCEFunctionMode)
{
    assert (currentMetaMutantModule && "Running TCE before mutation");
    llvm::Module &module = *currentMetaMutantModule;    
    
    llvm::GlobalVariable *mutantIDSelGlob = module.getNamedGlobal(mutantIDSelectorName);
    assert (mutantIDSelGlob && "Unmutated module passed to TCE");
    
    unsigned highestMutID = getHighestMutantID (&module);
    
    TCE tce;
    
    std::map<unsigned, std::vector<unsigned>> duplicateMap;
    
    ///\brief mutant's function or module. that which will be used when serializing mutants to file.
    std::vector<llvm::Module *> mutModules;
    std::vector<llvm::Function *> mutFunctions;
    
    /// \brief the key of the map is the index (staring from 0) of the function in the original's optimized module, or -1 for any function not present in original optimized
    /// \brief the value is a vector of the ids of all the mutants (non dup) in duplicateMap for which the function is different than the orig's.
    std::unordered_map<llvm::Function *, std::vector<unsigned>> diffFuncs2Muts; 
    
    /// \brief map to lookup the module(value) cleaned for each function (key) 
    std::unordered_map<llvm::Function *, ReadWriteIRObj> inMemIRModBufByFunc;
    std::unordered_map<llvm::Function *, llvm::Module *> clonedModByFunc;
    
    /// \brief map for the mode using 'clonedModByFunc' (function tce). 
    /// the key is the module from 'clonedModByFunc', and the value is a clone of the corresponding function, initially, from the key module. 
    std::unordered_map<llvm::Module *, llvm::Function *> backedFuncsByMods;
    
    /// \brief list of functions, according to meta-mutant module (current 'module'). 
    /// here we just need the function name, or the function to lookup module in 'clonedModByFunc' or 'inMemIRModBufByFunc'
    std::vector<llvm::Function *> funcMutByMutID(highestMutID+1, nullptr);   //nullptr mean more than 1 function mutated, or for the original(0 function mutated)
    
    std::vector<llvm::SmallVector<llvm::BasicBlock *, 2>> diffBBWithOrig(highestMutID+1);
    
    ///\brief keep the original's module up to end of this function
    llvm::Module *clonedOrig = nullptr;
    
    if (isTCEFunctionMode)
    {
        mutFunctions.clear();
        mutFunctions.resize(highestMutID+1, nullptr);
        llvm::errs() << "Cloning...\n";   //////DBG
        computeModuleBufsByFunc(module, nullptr, &clonedModByFunc, funcMutByMutID);
        
        clonedOrig = ReadWriteIRObj::cloneModuleAndRelease (clonedModByFunc.at(funcMutByMutID[0]));

        // The original
        assert (getMutant (*clonedOrig, 0, funcMutByMutID[0], 'A'/*optimizeAllFunctions*/) && "error: failed to get original");
        
        // populate 'backedFuncsByMods'
        for (auto &xxit: clonedModByFunc)
        {
            if (xxit.first == nullptr)
                continue;
            llvm::ValueToValueMapTy vmap;
            llvm::Function *clonedFunc = llvm::CloneFunction(xxit.second->getFunction(xxit.first->getName()), vmap, false/*moduleLevelChanges*/);
            backedFuncsByMods[xxit.second] = clonedFunc;
        }
    }
    else
    {
        mutModules.clear();
        mutModules.resize(highestMutID+1, nullptr);
        llvm::errs() << "Cloning...\n";   //////DBG
        computeModuleBufsByFunc(module, &inMemIRModBufByFunc, nullptr, funcMutByMutID);
        // Read all the mutantmodules possibly in parallel
       /*#ifdef ENABLE_OPENMP_PARALLEL
            omp_set_num_threads(std::min(atoi(std::getenv("OMP_NUM_THREADS")), std::max(omp_get_max_threads()/2, 1)));
            #pragma omp parallel for 
       #endif*/
        for (unsigned id=0; id <= highestMutID; id++)       //id==0 is the original
        {
            mutModules[id] = inMemIRModBufByFunc.at(funcMutByMutID[id]).readIR();
            if (id % 100 == 0)
                llvm::errs() << "Cloned up to " << id << ". ";
        }
        
        // The original
        clonedOrig = mutModules[0];
        assert (getMutant (*clonedOrig, 0, funcMutByMutID[0], 'M'/*optimizeModule*/) && "error: failed to get original");
    }
    
    // The original cont.
    duplicateMap[0];   //insert 0 into the map
    //initialize 'diffFuncs2Muts'
    diffFuncs2Muts[nullptr];    //for function not in original
    for (auto & origFunc: *clonedOrig)
        diffFuncs2Muts[&origFunc];
            
    //The mutants
    
    /// \brief since the mutants of the same function have sequential ID, we use this to trac function change and do some operation only then...
    llvm::Function *curFunc_ForBetterPerf  = nullptr;    ////DBG
    
    std::vector<llvm::Function *> mutatedFuncsOfMID;
    for (unsigned id=1; id <= highestMutID; id++)       //id==0 is the original
    {
        llvm::Module *clonedM;
        
/*#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
        llvm::Module *clonedM = llvm::CloneModule(&module);
#else
        llvm::Module *clonedM = llvm::CloneModule(&module).release();
#endif*/

        //Currently only support a mutant in a single funtion. TODO TODO: extent to mutant cros function
        assert(funcMutByMutID[id] != nullptr && "//Currently only support a mutant in a single funtion (funcMutByMutID[id]). TODO TODO: extent to mutant cros function");
        
        //Check with original
        mutatedFuncsOfMID.clear();
        if (isTCEFunctionMode)
        {
            clonedM = clonedModByFunc.at(funcMutByMutID[id]);
            llvm::Function *targetF = clonedM->getFunction(funcMutByMutID[id]->getName());
            llvm::Function *srcF = backedFuncsByMods.at(clonedM);
            
            //set the correct function for this mutant in the corresponding function module
            assert (getMutant (*clonedM, id, funcMutByMutID[id], 'F') && "error: failed to get mutant");
            if (tce.functionDiff(clonedOrig->getFunction(funcMutByMutID[id]->getName()), targetF, &(diffBBWithOrig[id])))
                mutatedFuncsOfMID.push_back (clonedOrig->getFunction(funcMutByMutID[id]->getName()));
                
            llvm::ValueToValueMapTy vmap;
            mutFunctions[id] = llvm::CloneFunction(targetF, vmap, true/*moduleLevelChanges*/);
            
            /// function has changed, restore clonedM module
            if (curFunc_ForBetterPerf != funcMutByMutID[id])
            {
                /// restore clonedM module (bring back the backed function)
                /// XXX: We do this once we are done with the function
                if (curFunc_ForBetterPerf != nullptr)
                {
                    llvm::Module *correspM = clonedModByFunc.at(curFunc_ForBetterPerf);
                    llvm::Function *corresptargetF = correspM->getFunction(curFunc_ForBetterPerf->getName());
                    llvm::Function *correspsrcF = backedFuncsByMods.at(correspM);
                    setModFuncToFunction (correspM, correspsrcF, corresptargetF);
                }
                curFunc_ForBetterPerf = funcMutByMutID[id];
                llvm::errs()<<"processing Func: " << curFunc_ForBetterPerf->getName() << ", Starting at mutant: "<<id<<"/"<< highestMutID << "\n";
            }
            
            //If there was difference with original process bellow
        }
        else
        {
            clonedM = mutModules[id];
            assert (getMutant (*clonedM, id, funcMutByMutID[id], 'M') && "error: failed to get mutant");
            tce.checkWithOrig(clonedOrig, clonedM, mutatedFuncsOfMID);
            if (curFunc_ForBetterPerf != funcMutByMutID[id])
            {
                curFunc_ForBetterPerf = funcMutByMutID[id];
                llvm::errs()<<"processing Func: " << curFunc_ForBetterPerf->getName() << ", Starting at mutant: "<<id<<"/"<< highestMutID << "\n";
            }
        }
        
        if (mutatedFuncsOfMID.empty()) //equivalent with orig
        {
            duplicateMap.at(0).push_back(id);
            //llvm::errs() << id << " is equivalent\n"; /////DBG
        }
        else
        {
            //Currently only support a mutant in a single funtion. TODO TODO: extent to mutant cros function
            assert(mutatedFuncsOfMID.size() == 1 && "//Currently only support a mutant in a single funtion. TODO TODO: extent to mutant cros function");
            
            bool hasEq = false;
            for (auto *mF: mutatedFuncsOfMID)
            {
                llvm::Function *subjFunc;
                if (isTCEFunctionMode)
                    subjFunc = mutFunctions[id];
                else
                    subjFunc = clonedM->getFunction(mF->getName());
                    
                for (auto candID: diffFuncs2Muts.at(mF))
                {
                    llvm::Function *candFunc;
                    if (isTCEFunctionMode)
                    {
                        //prune useless comparisons
                        if (diffBBWithOrig[id].size() != diffBBWithOrig[candID].size())
                            continue;
                        bool dbbdf = false;
                        for (unsigned dbbInd=0, dbbe=diffBBWithOrig[id].size(); dbbInd < dbbe; dbbInd++)
                        {
                            if (diffBBWithOrig[id][dbbInd] != diffBBWithOrig[candID][dbbInd])
                            {
                                dbbdf = true;
                                break;
                            }
                        }
                        if (dbbdf)
                            continue;
                            
                        candFunc = mutFunctions[candID];
                    }
                    else
                    {    
                        candFunc = mutModules.at(candID)->getFunction(mF->getName());
                    }
                        
                    if (!subjFunc)
                    {
                        if (candFunc)
                            continue;
                            
                        hasEq = true;
                        duplicateMap.at(candID).push_back(id);
                        break;
                    }
                    else
                    {
                        if (!candFunc)
                            continue;
                        
                        if (! tce.functionDiff(candFunc, subjFunc, nullptr))
                        {
                            hasEq = true;
                            duplicateMap.at(candID).push_back(id);
                            break;
                        }
                    }
                }
            }
            if (! hasEq)
            {
                duplicateMap[id];   //insert id into the map
                for (auto *mF: mutatedFuncsOfMID)
                    diffFuncs2Muts.at(mF).push_back(id);
            }
            //else    llvm::errs() << id << " is duplicate\n"; /////DBG
        }
    }
    
    // Store some statistics about the mutants
    preTCENumMuts = highestMutID;
    postTCENumMuts = duplicateMap.size() - 1;       // -1 because the original is also in
    numEquivalentMuts = duplicateMap.at(0).size();
    numDuplicateMuts = preTCENumMuts - postTCENumMuts - numEquivalentMuts;
    //
    
    /*for (auto &p :duplicateMap)
    {
        llvm::errs() << p.first << " <--> {"; 
        for (auto eq: p.second)
            llvm::errs() << eq << " ";
        llvm::errs() << "}\n";
    }*/
    
    //re-assign the ids of mutants
    duplicateMap.erase(0);      //Remove original
    unsigned newmutIDs = 1;
    
    //The keys of duplicateMap are (must be) sorted in increasing order: helpful when enabled 'forKLEESEMu'
    for (auto &mm :duplicateMap)    
    {
        mm.second.clear();
        mm.second.push_back(newmutIDs++);
    }
    
    // update mutants infos
    mutantsInfos.postTCEUpdate(duplicateMap);
    
    for (auto &Func: module)
    {
        for (auto &BB: Func)
        {
            std::vector<llvm::BasicBlock *> toBeRemovedBB;
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
                            callI->setArgOperand (0, llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)(duplicateMap.at(newFromi).front()), false)));
                            callI->setArgOperand (1, llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)(duplicateMap.at(newToi).front()), false)));
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
                                llvm::SwitchInst::CaseIt cit = sw->findCaseValue(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)(i), false)));
                                if (duplicateMap.count(i) == 0)
                                {
                                    toBeRemovedBB.push_back(cit.getCaseSuccessor());
                                    sw->removeCase (cit);
                                }
                                else
                                {
                                    cit.setValue(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)(duplicateMap.at(i).front()), false)));
                                }
                            }
                        }
                    }
                }
            }
            for (auto *bbrm: toBeRemovedBB)
                bbrm->eraseFromParent();
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
        mutantIDSelGlob->setInitializer(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)1+highestMutID, false)));
    }
    
    // Write mutants files and weak mutation file
    if (writeMuts || modWMLog)
    {
        std::unique_ptr<llvm::Module> wmModule(nullptr);
        if (modWMLog)
        {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            wmModule.reset (llvm::CloneModule(&module));
#else
            wmModule = llvm::CloneModule(&module);
#endif
            computeWeakMutation(wmModule, modWMLog);
        }
        
        if (writeMuts)
        {
            if (isTCEFunctionMode)
            {
                assert (mutModules.empty() && "mutModules must be empty here");
                unsigned tmpHMID = mutFunctions.size() - 1;    //initial highestMutID
                mutModules.resize(tmpHMID+1, nullptr);
                mutModules[0] = clonedOrig;
                for (unsigned tmpid=1; tmpid <= tmpHMID; tmpid++)       //id==0 is the original
                {
                    mutModules[tmpid] = clonedModByFunc.at(funcMutByMutID[tmpid]);
                }
                assert (writeMutantsCallback(this, &duplicateMap, &mutModules, wmModule.get(), &mutFunctions, &backedFuncsByMods) && "Failed to dump mutants IRs");
                mutModules.clear();
                mutModules.resize(0);
            }
            else
            {
                assert (writeMutantsCallback(this, &duplicateMap, &mutModules, wmModule.get(), nullptr, nullptr) && "Failed to dump mutants IRs");
            }
        }
        else
        {
            assert (writeMutantsCallback(this, nullptr, nullptr, wmModule.get(), nullptr, nullptr) && "Failed to dump weak mutantion IR. (can be null)");
        }
    }
    
    //create the final version of the meta-mutant file
    if (forKLEESEMu)
    {
        llvm::Function *funcForKS = module.getFunction(mutantIDSelectorName_Func);
        funcForKS->deleteBody();
        //tce.optimize(module);
        
        if (highestMutID > 0)   //if There are mutants
            createGlobalMutIDSelector_Func(module, true);
        
        ///reduce consecutive range (of selector func) into one //TODO
    }
    else
    {
        //tce.optimize(module);
    }
    
    //verify post TCE Meta-module
    Mutation::checkModuleValidity(module, "ERROR: Misformed post-TCE Meta-Module!");
    
    // free mutModules' cloned modules.
    if (isTCEFunctionMode)
    {
        for (auto *ff: mutFunctions)
            delete ff;
        for (auto &ffIt: backedFuncsByMods)
            delete ffIt.second;
        delete clonedOrig;
        for (auto &mmIt: clonedModByFunc)
            delete mmIt.second;
    }
    else
    {
        for (auto *mm: mutModules)
            delete mm;
    }
}

void Mutation::cleanFunctionToMut (llvm::Function &Func, MuLL::MutantIDType mutantID, llvm::GlobalVariable *mutantIDSelGlob, llvm::Function *mutantIDSelGlob_Func)
{
    if (Func.isDeclaration())
        return;
    for (auto &BB: Func)
    {
        std::vector<llvm::BasicBlock *> toBeRemovedBB;
        for (llvm::BasicBlock::iterator Iit = BB.begin(), Iie = BB.end(); Iit != Iie;)
        {
            llvm::Instruction &Inst = *Iit++;   //we increment here so that 'eraseFromParent' bellow do not cause crash
            if (auto *callI = llvm::dyn_cast<llvm::CallInst>(&Inst))
            {
                if (forKLEESEMu && callI->getCalledFunction() == mutantIDSelGlob_Func)
                {
                    callI->eraseFromParent();   //remove all calls to the heler function for KLEE integration
                }
            }
            if (auto *sw = llvm::dyn_cast<llvm::SwitchInst>(&Inst))
            {
                if (auto *ld = llvm::dyn_cast<llvm::LoadInst>(sw->getCondition()))
                {
                    if (ld->getOperand(0) == mutantIDSelGlob)
                    {
                        llvm::SwitchInst::CaseIt cit = sw->findCaseValue(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)(mutantID), false)));
                        llvm::BasicBlock * citSucc = nullptr;
                        llvm::BasicBlock * swBB = sw->getParent();
                        if (cit == sw->case_default())  
                        {   //Not the mutants (mutants is same as orig here)
                            citSucc = cit.getCaseSuccessor();
                            for (auto csit = sw->case_begin(), cse = sw->case_end(); csit != cse; ++csit)
                                if (csit.getCaseSuccessor() != citSucc)
                                    toBeRemovedBB.push_back(csit.getCaseSuccessor());
                        }
                        else
                        {   //mutants
                            citSucc = cit.getCaseSuccessor();
                            for (auto csit = sw->case_begin(), cse = sw->case_end(); csit != cse; ++csit)
                                if (csit.getCaseSuccessor() != citSucc)
                                    toBeRemovedBB.push_back(csit.getCaseSuccessor());
                            if (sw->getDefaultDest() != citSucc)
                                toBeRemovedBB.push_back(sw->getDefaultDest());
                        }
                        
                        // Make all PHI nodes that referred to BB now refer to Pred as their
                        // source...
                        citSucc->replaceAllUsesWith(swBB);
                        // Move all definitions in the successor to the predecessor...
                        sw->eraseFromParent();
                        ld->eraseFromParent();
                        
                        swBB->getInstList().splice(swBB->end(), citSucc->getInstList());
                        toBeRemovedBB.push_back(citSucc);
                        
                        //llvm::BranchInst::Create (citSucc, sw);
                        //sw->eraseFromParent();
                    }
                }
            }
        }
        for (auto *bbrm: toBeRemovedBB)
            bbrm->eraseFromParent();
    }
    //Check validity
    Mutation::checkFunctionValidity(Func, "ERROR: Misformed Function after cleanToMut!");
}

void Mutation::computeModuleBufsByFunc(llvm::Module &module, std::unordered_map<llvm::Function *, ReadWriteIRObj> *inMemIRModBufByFunc, \
                                            std::unordered_map<llvm::Function *, llvm::Module *> *clonedModByFunc, std::vector<llvm::Function *> &funcMutByMutID)
{
    if (inMemIRModBufByFunc == nullptr && clonedModByFunc == nullptr)
        assert (false && "Both 'inMemIRModBufByFunc' and 'clonedModByFunc' are NULL, should choose one");
    if (inMemIRModBufByFunc != nullptr && clonedModByFunc != nullptr)
        assert (false && "Both 'inMemIRModBufByFunc' and 'clonedModByFunc' are NOT NULL, should choose one");
        
    std::unordered_set<MuLL::MutantIDType> mutHavingMoreThan1Func;
    llvm::GlobalVariable *mutantIDSelGlob = module.getNamedGlobal(mutantIDSelectorName); 
    llvm::Function *mutantIDSelGlob_Func = module.getFunction(mutantIDSelectorName_Func);
    //llvm::errs() << "XXXCloning...\n";   //////DBG
    /// \brief First pass to get the functions mutated for each mutant (nullptr when more than one function mutated).
    for (auto &Func: module)
    {
        bool funcMutated = false;
        for (auto &BB: Func)
        {
            for (llvm::BasicBlock::iterator Iit = BB.begin(), Iie = BB.end(); Iit != Iie;)
            {
                llvm::Instruction &Inst = *Iit++;
                if (auto *sw = llvm::dyn_cast<llvm::SwitchInst>(&Inst))
                {
                    if (auto *ld = llvm::dyn_cast<llvm::LoadInst>(sw->getCondition()))
                    {
                        if (ld->getOperand(0) == mutantIDSelGlob)
                        {
                            for (llvm::SwitchInst::CaseIt cit = sw->case_begin(), ce = sw->case_end(); cit != ce; ++cit)
                            {
                                MuLL::MutantIDType mid = cit.getCaseValue()->getZExtValue();
                                if (funcMutByMutID[mid] == nullptr)
                                {
                                    funcMutByMutID[mid] = &Func;
                                    funcMutated = true;
                                }
                                else if(funcMutByMutID[mid] != &Func)
                                {
                                    mutHavingMoreThan1Func.insert(mid);
                                }
                            }
                        }
                    }
                }
            }
        }
        if (funcMutated)
        {
            if(inMemIRModBufByFunc != nullptr)
                (*inMemIRModBufByFunc)[&Func];  //insert Func to map
            else
                (*clonedModByFunc)[&Func] = nullptr;
        }
    }
    
    //set mutant taking more than one function's function to null
    for (auto mid: mutHavingMoreThan1Func)
        funcMutByMutID[mid] = nullptr;
    
    TCE tce;
    
    /// \brief get the module for each function (when cleaning all other funcs)
    std::stack<std::tuple<llvm::Module *, unsigned/*From*/, unsigned/*To*/>> workStack;
    std::vector<std::string> mutFuncList;
    if (clonedModByFunc != nullptr)
    {
        (*clonedModByFunc)[nullptr] = ReadWriteIRObj::cloneModuleAndRelease (&module);
        for (auto &funcmodIt: *clonedModByFunc)
            if (funcmodIt.first != nullptr)
                mutFuncList.push_back(funcmodIt.first->getName());
    }
    else
    {
        (*inMemIRModBufByFunc)[nullptr].setToModule(&module);
        for (auto &funcmodIt: *inMemIRModBufByFunc)
            if (funcmodIt.first != nullptr)
                mutFuncList.push_back(funcmodIt.first->getName());
    }
    if (! mutFuncList.empty())
        workStack.emplace(ReadWriteIRObj::cloneModuleAndRelease(&module), 0, mutFuncList.size()-1);
    
    /// \brief Use binary approach(divide and conquer) to quickly obtain the module for each function
    while (! workStack.empty())
    {
        auto &stackElem = workStack.top();
        unsigned min = std::get<1>(stackElem), max = std::get<2>(stackElem);
        llvm::Module *cloneML = std::get<0>(stackElem);
        workStack.pop();
        
        if (min == max)
        {
            if (clonedModByFunc != nullptr)
            {
                (*clonedModByFunc)[module.getFunction(mutFuncList[min])] = cloneML;
            }
            else
            {
                (*inMemIRModBufByFunc)[module.getFunction(mutFuncList[min])].setToModule(cloneML);
                delete cloneML;
            }
            continue;
        }
        
        unsigned mid = min + (max-min)/2;
        
        llvm::Module *cloneMR = ReadWriteIRObj::cloneModuleAndRelease(cloneML);
        
        //llvm::errs() << "Func - " << funcPtr->getName() << "\n";   //////DBG
        assert ((min <= mid && mid < max) && "shlould reach here only if we have at least 2 functions uncleaned");
        
        llvm::GlobalVariable *mutantIDSelGlobMM = cloneML->getNamedGlobal(mutantIDSelectorName); 
        llvm::Function *mutantIDSelGlob_FuncMM = cloneML->getFunction(mutantIDSelectorName_Func);
        for (auto fit = min; fit <= mid; fit++)
        {
            auto *Func = cloneML->getFunction(mutFuncList[fit]);
            cleanFunctionToMut (*Func, 0, mutantIDSelGlobMM, mutantIDSelGlob_FuncMM);
            tce.optimize(*Func, Mutation::funcModeOptLevel);
        }
        //left side has been cleaned, now add remaining right side to be processed next
        workStack.emplace(cloneML, mid+1, max);
        
        mutantIDSelGlobMM = cloneMR->getNamedGlobal(mutantIDSelectorName); 
        mutantIDSelGlob_FuncMM = cloneMR->getFunction(mutantIDSelectorName_Func);
        for (auto fit = mid+1; fit <= max; fit++)
        {
            auto *Func = cloneMR->getFunction(mutFuncList[fit]);
            cleanFunctionToMut (*Func, 0, mutantIDSelGlobMM, mutantIDSelGlob_FuncMM);
            tce.optimize(*Func, Mutation::funcModeOptLevel);
        }
        //right side have been cleaned now add the remaining left side to be processed next
        workStack.emplace(cloneMR, min, mid);
    }
}

bool Mutation::getMutant (llvm::Module &module, unsigned mutantID, llvm::Function * mutFunc, char optimizeModFuncNone)
{
    unsigned highestMutID = getHighestMutantID (&module);
    if (mutantID > highestMutID)
        return false;
    
    llvm::GlobalVariable *mutantIDSelGlob = module.getNamedGlobal(mutantIDSelectorName); 
    llvm::Function *mutantIDSelGlob_Func = module.getFunction(mutantIDSelectorName_Func);   
    TCE tce;
    
    if (optimizeModFuncNone == 'F')
        assert (mutFunc && "optimize function but function is NULL");
        
    /// \brief remove every case of switches that are irrelevant, as well as all call of 'forKleeSeMU'
    /// XXX: This help to make optimization faster
    llvm::Function *mutFuncInThisModule = nullptr;
    llvm::Module::iterator modIter, modend;
    if (mutFunc)
    {
        mutFuncInThisModule = module.getFunction(mutFunc->getName());
        modIter = mutFuncInThisModule->getIterator();
        modend = modIter;
        ++modend;
    }
    else
    {
        modIter = module.begin();
        modend = module.end();
    }
    
    for ( ; modIter != modend; ++modIter)
    {
        if (modIter->isDeclaration())
            continue;
        llvm::Function &Func = *modIter;
        cleanFunctionToMut (Func, mutantID, mutantIDSelGlob, mutantIDSelGlob_Func);
    } 
    
    //llvm::errs() << "Optimizing...\n";   /////DBG
    ///~
    
    if (optimizeModFuncNone == 'F')
    {
        tce.optimize(*mutFuncInThisModule, Mutation::funcModeOptLevel);
    }
    else if (optimizeModFuncNone == 'M')
    {
        mutantIDSelGlob->setInitializer(llvm::ConstantInt::get(moduleInfo.getContext(), llvm::APInt(32, (uint64_t)mutantID, false)));
        mutantIDSelGlob->setConstant(true);
        tce.optimize(module, Mutation::modModeOptLevel);
    }
    else if (optimizeModFuncNone == 'A')    //optimize all the functions
    {
        for (auto &ffunc: module)
            if(! ffunc.isDeclaration())
                tce.optimize(ffunc, Mutation::funcModeOptLevel); 
    }
    else
    {   
        assert (optimizeModFuncNone == '0' && "invalid optimization mode in getMutant: expect 'M', 'F', 'A' or '0'");
    }
    
    return true;
}

unsigned Mutation::getHighestMutantID (llvm::Module const *module)
{
    if (!module)
        module = currentMetaMutantModule;
    llvm::GlobalVariable const *mutantIDSelectorGlobal = module->getNamedGlobal(mutantIDSelectorName);
    assert (mutantIDSelectorGlobal && mutantIDSelectorGlobal->getInitializer()->getType()->isIntegerTy() && "Unmutated module passed to TCE");
    return llvm::dyn_cast<llvm::ConstantInt>(mutantIDSelectorGlobal->getInitializer())->getZExtValue() - 1;
}

void Mutation::loadMutantInfos (std::string filename)
{
    mutantsInfos.loadFromJsonFile(filename);
}

void Mutation::dumpMutantInfos (std::string filename)
{
    mutantsInfos.printToJsonFile(filename);
}

Mutation::~Mutation ()
{
    //mutantsInfos.printToStdout();
    
    llvm::errs() << "\n# Number of Mutants:   PreTCE: " << preTCENumMuts << ", PostTCE: " << postTCENumMuts << ", ";
    llvm::errs() << "Equivalent: " << numEquivalentMuts << ", Duplicates: " << numDuplicateMuts << "\n\n";
    
    //Clear the constant map to avoid double free
    llvmMutationOp::destroyPosConstValueMap();
}


