#ifndef __MART_GENMU_operatorClasses__ReplaceOnly_Base__
#define __MART_GENMU_operatorClasses__ReplaceOnly_Base__

/**
 * -==== ReplaceOnly_Base.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for replacors (operation that are not
 * matched, but replace others - like Abs, delStmt).
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../GenericMuOpBase.h"

namespace mart {

class ReplaceOnly_Base : public GenericMuOpBase {
  /**
   * \bref Inplements virtual from @see GenericMuOpBase
   */
  bool matchIRs(MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp,
                unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) {
    llvm::errs() << "Error: Calling 'MatchIRs' method from a ReplaceOnly "
                    "objects of the class: " /*<< typeid(*this).name()*/
                 << "\n";
    assert(false);
  }

  /**
   * \bref Inplements virtual from @see GenericMuOpBase
   */
  void prepareCloneIRs(MatchStmtIR const &toMatch, unsigned pos,
                       MatchUseful const &MU,
                       llvmMutationOp::MutantReplacors const &repl,
                       DoReplaceUseful &DRU, ModuleUserInfos const &MI) {
    llvm::errs()
        << "Error: Calling 'prepareCloneIRs' method from a ReplaceOnly objects "
           "of the class: " /*<< typeid(*this).name()*/
        << "\n";
    assert(false);
  }

  /**
   * \bref Inplements virtual from @see GenericMuOpBase
   */
  void matchAndReplace(MatchStmtIR const &toMatch,
                       llvmMutationOp const &mutationOp,
                       MutantsOfStmt &resultMuts,
                       WholeStmtMutationOnce &iswholestmtmutated,
                       ModuleUserInfos const &MI) {
    llvm::errs()
        << "Error: Calling 'matchAndReplace' method from a ReplaceOnly objects "
           "of the class: " /*<< typeid(*this).name()*/
        << "\n";
    assert(false);
  }
}; // class ReplaceOnly_Base

} // namespace mart

#endif //__MART_GENMU_operatorClasses__ReplaceOnly_Base__
