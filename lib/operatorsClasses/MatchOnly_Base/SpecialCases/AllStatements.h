#ifndef __MART_GENMU_operatorClasses__AllStatements__
#define __MART_GENMU_operatorClasses__AllStatements__

/**
 * -==== AllStatements.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class that match ANY statement.
 */

#include "../MatchOnly_Base.h"

namespace mart {

class AllStatements : public MatchOnly_Base {
public:
  bool matchIRs(MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp,
                unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) {
    return true;
  }

  void prepareCloneIRs(MatchStmtIR const &toMatch, unsigned pos,
                       MatchUseful const &MU,
                       llvmMutationOp::MutantReplacors const &repl,
                       DoReplaceUseful &DRU, ModuleUserInfos const &MI) {}

  void matchAndReplace(MatchStmtIR const &toMatch,
                       llvmMutationOp const &mutationOp,
                       MutantsOfStmt &resultMuts,
                       WholeStmtMutationOnce &iswholestmtmutated,
                       ModuleUserInfos const &MI) {
    for (unsigned i = 0; i < mutationOp.getNumReplacor(); ++i)
      assert(checkWholeStmtAndMutate(toMatch, mutationOp.getReplacor(i),
                                     resultMuts, iswholestmtmutated, MI) &&
             "only Delete Stmt and Trap affect whole statement and match "
             "anything");
  }
}; // class AllStatements

} // namespace mart

#endif //__MART_GENMU_operatorClasses__AllStatements__
