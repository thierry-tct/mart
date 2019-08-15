#ifndef __MART_GENMU_operatorClasses__NumericVariable_Base__
#define __MART_GENMU_operatorClasses__NumericVariable_Base__

/**
 * -==== NumericVariable_Base.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class that match numeric variable and
 * replace (but do not create replacement).
 */

#include "../MatchOnly_Base.h"

namespace mart {

class NumericVariable_Base : public MatchOnly_Base {
protected:
  /**
   * \brief This method is to be implemented by each numeric variable
   * matching-mutating operator
   * \detail takes a value and check wheter it matches with the type that the
   * mutation operator matcher can handle.
   * @param val is the value to check
   * @return if the value matches, otherwise, return false.
   */
  inline virtual bool varMatched(llvm::Value *val) = 0;
  /*{  //Match both int and FP
      if (val->getType()->isFloatingPointTy() || val->getType()->isIntegerTy())
          return true;
      return false;
  }*/

public:
  bool matchIRs(MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp,
                unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) {
    llvm::Value *val = toMatch.getIRAt(pos);
    if (llvm::isa<llvm::LoadInst>(val) && varMatched(val)) {
      MatchUseful *ptr_mu = MU.getNew();
      ptr_mu->appendHLOprdsSource(pos);
      ptr_mu->appendRelevantIRPos(pos);
      ptr_mu->setHLReturningIRPos(pos);
    }
    return (MU.first() != MU.end());
  }

  void prepareCloneIRs(MatchStmtIR const &toMatch, unsigned pos,
                       MatchUseful const &MU,
                       llvmMutationOp::MutantReplacors const &repl,
                       DoReplaceUseful &DRU, ModuleUserInfos const &MI) {
    DRU.toMatchMutant.setToCloneStmtIROf(toMatch, MI);
    llvm::Value *oprdptr[] = {nullptr, nullptr};
    for (int i = 0; i < repl.getOprdIndexList().size(); i++) {
      if (!(oprdptr[i] = createIfConst(
                MU.getHLOperandSource(0, DRU.toMatchMutant)->getType(),
                repl.getOprdIndexList()[i]))) {
        oprdptr[i] = MU.getHLOperandSource(0, DRU.toMatchMutant);
      }
    }

    DRU.appendHLOprds(oprdptr[0]);
    DRU.appendHLOprds(oprdptr[1]);
    DRU.setOrigRelevantIRPos(MU.getRelevantIRPos());
    DRU.setHLReturningIRPos(MU.getHLReturningIRPos());
  }
}; // class NumericVariable_Base

} // namespace mart

#endif //__MART_GENMU_operatorClasses__NumericVariable_Base__
