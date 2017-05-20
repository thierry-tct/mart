#ifndef __KLEE_SEMU_GENMU_operatorClasses__FunctionCallCallee__
#define __KLEE_SEMU_GENMU_operatorClasses__FunctionCallCallee__

/**
 * -==== FunctionCallCallee.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief   Generic abstract base class that match Specific function call callee. 
 * \todo    Extend to support calls to 'Invoke' and make use of the methods 'matchIRs()' and 'prepareCloneIRs()'.
 */
 
#include "../MatchOnly_Base.h"

class FunctionCallCallee: public MatchOnly_Base
{
  public:
    bool matchIRs (MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp, unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) 
    {
        llvm::errs() << "Unsuported yet: 'matchIRs' mathod of FunctionCallCallee should not be called \n";
        assert (false);
    }
    
    void prepareCloneIRs (MatchStmtIR const &toMatch, unsigned pos,  MatchUseful const &MU, llvmMutationOp::MutantReplacors const &repl, DoReplaceUseful &DRU, ModuleUserInfos const &MI) 
    {
        llvm::errs() << "Unsuported yet: 'prepareCloneIRs' mathod of FunctionCallCallee should not be called \n";
        assert (false);
    }
    
    void matchAndReplace (MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp, MutantsOfStmt &resultMuts, bool &isDeleted, ModuleUserInfos const &MI)
    {
        MutantsOfStmt::MutantStmtIR toMatchMutant;
        int pos = -1;
        for (auto *val:toMatch.getIRList())
        {
            pos++;
            
            ///MATCH
            if (llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(val))
            {
                llvm::Function *cf = call->getCalledFunction();
                for (auto &repl: mutationOp.getMutantReplacorsList())
                {
                    if (isDeletion(repl.getExpElemKey()))
                    {
                        doDeleteStmt (toMatch, repl, resultMuts, isDeleted, MI);
                    }
                    else
                    {
                        assert (repl.getExpElemKey() == mNEWCALLEE && "Error: CALL should have only 'NEWCALLEE as replacor'");
                        assert (llvmMutationOp::isSpecifiedConstIndex(repl.getOprdIndexList()[0]));  //repl.getOprdIndexList()[0,1,...] should be >  llvmMutationOp::maxOprdNum
                        if (cf->getName() == llvmMutationOp::getConstValueStr(repl.getOprdIndexList()[0]))    //'repl.getOprdIndexList()[0]' is it the function to match?
                        {
                            for (auto i=1; i < repl.getOprdIndexList().size(); i++)
                            {
                                toMatchMutant.clear();
                                toMatchMutant.setToCloneStmtIROf(toMatch, MI);
                                llvm::Function *repFun = MI.getModule()->getFunction(llvmMutationOp::getConstValueStr(repl.getOprdIndexList()[i]));
                                if (!repFun)
                                {
                                    llvm::errs() << "\n# Error: Replacing function not found in module: " << llvmMutationOp::getConstValueStr(repl.getOprdIndexList()[i]) << "\n";
                                    assert (false);
                                }
                                // Make sure that the two functions are compatible
                                if (cf->getFunctionType() != repFun->getFunctionType())
                                {
                                    llvm::errs() << "\n#Error: the function to be replaced and de replacing are of different types:" << cf->getName() << " and " << repFun->getName() << "\n";
                                    assert (false);
                                }                                
                                llvm::dyn_cast<llvm::CallInst>(toMatchMutant.getIRAt(pos))->setCalledFunction(repFun);
                                resultMuts.add(/*toMatch, */toMatchMutant, repl, std::vector<unsigned>({pos})); 
                            }
                        }
                    }
                }
            }
        }
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__FunctionCallCallee__
