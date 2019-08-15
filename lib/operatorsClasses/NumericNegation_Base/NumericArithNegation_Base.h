#ifndef __MART_GENMU_operatorClasses__NumericArithNegation_Base__
#define __MART_GENMU_operatorClasses__NumericArithNegation_Base__

/**
 * -==== NumericArithNegation_Base.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all non-pointer negation mutation
 * operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../GenericMuOpBase.h"

namespace mart {

class NumericArithNegation_Base : public GenericMuOpBase {
protected:
  /**
   * \brief This method is to be implemented by each numeric arithmetic negation
   * matching-mutating operator
   * \detail we want to know if the negation is matched and return the operand
   * index of the negated value in the binop
   * @return the operand number in the binary operation if it match with the
   * corresponding negation, else -1.
   */
  inline virtual int matchAndGetOprdID(llvm::BinaryOperator *propNeg) = 0;

public:
  bool matchIRs(MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp,
                unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) {
    llvm::Value *val = toMatch.getIRAt(pos);
    if (llvm::BinaryOperator *neg = llvm::dyn_cast<llvm::BinaryOperator>(val)) {
      int oprdId;
      oprdId = matchAndGetOprdID(neg);
      if (oprdId < 0)
        return false;

      if (!checkCPTypeInIR(mutationOp.getCPType(0), neg->getOperand(oprdId)))
        return false;

      MatchUseful *ptr_mu = MU.getNew();
      ptr_mu->appendHLOprdsSource(
          toMatch.depPosofPos(neg->getOperand(oprdId), pos, true));
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
        oprdptr[i] = MU.getHLOperandSource(repl.getOprdIndexList()[i],
                                           DRU.toMatchMutant);
      }
    }

    DRU.posOfIRtoRemove.push_back(MU.getRelevantIRPosOf(0));
    DRU.appendHLOprds(oprdptr[0]);
    DRU.appendHLOprds(oprdptr[1]);
    DRU.setOrigRelevantIRPos(MU.getRelevantIRPos());
    DRU.setHLReturningIRPos(MU.getHLReturningIRPos());
  }
}; // class NumericArithNegation_Base

} // namespace mart

#endif //__MART_GENMU_operatorClasses__NumericArithNegation_Base__
