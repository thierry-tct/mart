#ifndef __KLEE_SEMU_GENMU_operatorClasses__ReturnBreakContinue__
#define __KLEE_SEMU_GENMU_operatorClasses__ReturnBreakContinue__

/**
 * -==== ReturnBreakContinue.h
 *
 *                LLGenMu LLVM Mutation Tool
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief   Generic abstract base class that match Return, Break and Continue stmts. 
 * \todo    Make use of the methods 'matchIRs()' and 'prepareCloneIRs()'.
 */
 
#include "../MatchOnly_Base.h"

class ReturnBreakContinue: public MatchOnly_Base
{
  public:
    bool matchIRs (std::vector<llvm::Value *> const &toMatch, llvmMutationOp const &mutationOp, unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) 
    {
        llvm::errs() << "Unsuported yet: 'matchIRs' mathod of ReturnBreakContinue should not be called \n";
        assert (false);
    }
    
    void prepareCloneIRs (std::vector<llvm::Value *> const &toMatch, unsigned pos,  MatchUseful const &MU, llvmMutationOp::MutantReplacors const &repl, DoReplaceUseful &DRU, ModuleUserInfos const &MI) 
    {
        llvm::errs() << "Unsuported yet: 'prepareCloneIRs' mathod of ReturnBreakContinue should not be called \n";
        assert (false);
    }
        
    void matchAndReplace (std::vector<llvm::Value *> const &toMatch, llvmMutationOp const &mutationOp, MutantsOfStmt &resultMuts, bool &isDeleted, ModuleUserInfos const &MI)
    {
        static llvm::Function *prevFunc = nullptr;   
        static const llvm::Value *structRetUse = nullptr;
        //Here toMatch should have only one elem for 'br' and for ret, the last elem should be 'ret'
        llvm::Instruction * retbr = llvm::dyn_cast<llvm::Instruction>(toMatch.back());
        std::vector<llvm::Value *> toMatchClone;
        assert (mutationOp.getMutantReplacorsList().size() == 1 && "Return, Break and Continue can only be deleted");
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
        llvm::Function *curFunc = retbr->getParent()->getParent(); 
#else
        llvm::Function *curFunc = retbr->getFunction();
#endif
        if (prevFunc != curFunc)
        {
            prevFunc = curFunc;
            assert (!structRetUse && "Error, Maybe the return struct was not mutated for perv funtion!");
            // Check whether the function returns a struct
            if (curFunc->getReturnType()->isVoidTy())
            {
                for (llvm::Function::const_arg_iterator II = curFunc->arg_begin(), EE = curFunc->arg_end(); II != EE; ++II)
                {
                    const llvm::Argument *param = &*II;
                    if (param->hasStructRetAttr())
                    {
                        structRetUse = param;
                        break;
                    }
                }
            }
        }
        if(structRetUse && toMatch.size() == 3)     //1: bitcast dests, 2: bitcast src, 3: copy src to dest (llvm.memcpy)
        {
            bool found = false;
            for (auto *ins: toMatch)
                if (llvm::isa<llvm::BitCastInst>(ins) && llvm::dyn_cast<llvm::User>(ins)->getOperand(0) == structRetUse)
                    found = true;
            if (found)
            {
                for (auto &repl: mutationOp.getMutantReplacorsList())
                {
                    if (isDeletion(repl.getExpElemKey()))
                    {
                        doDeleteStmt (toMatch, repl, resultMuts, isDeleted, MI);  //doDeleteStmt may call this function but they can't be infinite recursion because here the stmt do not have terminator
                    }
                    else
                        assert ("The replacement for return break and continue should be delete stmt");
                }
            }
        }
        
        if (auto *br = llvm::dyn_cast<llvm::BranchInst>(retbr))
        {
            if (br->isConditional())
                return;
            assert (toMatch.size() == 1 && "unconditional break should be the only instruction in the statement");
            llvm::BasicBlock *targetBB = br->getSuccessor(0);
    #if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            llvm::Function::iterator FI = br->getParent()->getNextNode();//Iterator();
            if (FI) //!= br->getFunction()->end() && FI )
            {
    #else
            llvm::Function::iterator FI = br->getParent()->getIterator();
            FI++;
            if (FI != br->getFunction()->end() && FI )
            {
    #endif
                llvm::BasicBlock *followBB = &*FI;
                if (followBB != targetBB)
                {
                    for (auto &repl: mutationOp.getMutantReplacorsList())
                    {
                        if (isDeletion(repl.getExpElemKey()))
                        {
                            toMatchClone.clear();
                            cloneStmtIR (toMatch, toMatchClone);
                            llvm::dyn_cast<llvm::BranchInst>(toMatchClone.back())->setSuccessor(0, followBB);
                            resultMuts.add(toMatch, toMatchClone, repl, std::vector<unsigned>({0})/*toMatch size here is 1 (see assert above)*/);
                            isDeleted = true;
                        }
                        else
                            assert ("The replacement for return break and continue should be delete stmt");
                    }
                }
            }
        }
        else if (auto *ret = llvm::dyn_cast<llvm::ReturnInst>(retbr))
        {
            if (ret->getReturnValue() == nullptr)   //void type
                return;
                
            llvm::Type *retType = curFunc->getReturnType();
            llvm::IRBuilder<> builder(llvm::getGlobalContext());
            //Case of main (delete lead to ret 0 - integer)
            if (curFunc->getName().equals(MI.G_MAIN_FUNCTION_NAME))
            {
                assert (retType->isIntegerTy() && "Main function must return an integer");
                for (auto &repl: mutationOp.getMutantReplacorsList())
                {
                    if (isDeletion(repl.getExpElemKey()))
                    {
                        // Skip this if the return value is already 0
                        if (auto *rve = llvm::dyn_cast<llvm::ConstantInt>(ret->getReturnValue()))
                            if (rve->equalsInt(0))
                                continue;
                        toMatchClone.clear();  //w do not clone here, just create new ret
                        llvm::ReturnInst *newret = builder.CreateRet(llvm::ConstantInt::get(retType, 0));
                        toMatchClone.push_back(newret);
                        std::vector<unsigned> relpos;
                        for (auto i=0; i<toMatch.size();i++)
                            relpos.push_back(i);
                        resultMuts.add(toMatch, toMatchClone, repl, relpos);
                        isDeleted = true;
                    }
                    else
                        assert ("The replacement for return break and continue should be delete stmt");
                }
            }
            else if (retType->isIntOrIntVectorTy() || retType->isFPOrFPVectorTy () || retType->isPtrOrPtrVectorTy())
            {//Case of primitive and pointer(delete lead to returning of varable value without reaching definition - uninitialized)
                for (auto &repl: mutationOp.getMutantReplacorsList())
                {
                    if (isDeletion(repl.getExpElemKey()))
                    {
                        toMatchClone.clear();
                        llvm::AllocaInst *alloca = builder.CreateAlloca(retType);
                        alloca->setAlignment( MI.getDataLayout().getTypeStoreSize(retType));
                        toMatchClone.push_back(alloca);
                        llvm::LoadInst *load = builder.CreateAlignedLoad(alloca, MI.getDataLayout().getTypeStoreSize(retType));
                        toMatchClone.push_back(load);
                        llvm::ReturnInst *newret = builder.CreateRet(load);
                        toMatchClone.push_back(newret);
                        std::vector<unsigned> relpos;
                        for (auto i=0; i<toMatch.size();i++)
                            relpos.push_back(i);
                        resultMuts.add(toMatch, toMatchClone, repl, relpos);
                        isDeleted = true;
                    }
                    else
                        assert ("The replacement for return val should be delete stmt");
                }
            }
        }
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__ReturnBreakContinue__
