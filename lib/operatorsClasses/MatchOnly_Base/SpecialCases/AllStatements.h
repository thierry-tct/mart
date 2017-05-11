#ifndef __KLEE_SEMU_GENMU_operatorClasses__AllStatements__
#define __KLEE_SEMU_GENMU_operatorClasses__AllStatements__

/**
 * -==== AllStatements.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class that match ANY statement. 
 */
 
#include "../MatchOnly_Base.h"

class AllStatements: public MatchOnly_Base
{
  public:
    bool matchIRs (std::vector<llvm::Value *> const &toMatch, llvmMutationOp const &mutationOp, unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) {return true;}
    
    void prepareCloneIRs (std::vector<llvm::Value *> const &toMatch, unsigned pos,  MatchUseful const &MU, llvmMutationOp::MutantReplacors const &repl, DoReplaceUseful &DRU, ModuleUserInfos const &MI) {}
    
    void matchAndReplace (std::vector<llvm::Value *> const &toMatch, llvmMutationOp const &mutationOp, MutantsOfStmt &resultMuts, bool &isDeleted, ModuleUserInfos const &MI)
    {
        assert ((mutationOp.getNumReplacor() == 1 && isDeletion(mutationOp.getReplacor(0).getExpElemKey())) && "only Delete Stmt affect whole statement and match anything");
        doDeleteStmt (toMatch, mutationOp.getReplacor(0), resultMuts, isDeleted, MI);
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__AllStatements__
