#ifndef __MART_GENMU_operatorClasses__NumericArithBinop_Base__
#define __MART_GENMU_operatorClasses__NumericArithBinop_Base__

/**
 * -==== NumericArithBinop_Base.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all non-pointer arithmetic binary
 * mutation operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../GenericMuOpBase.h"

namespace mart {

class NumericArithBinop_Base : public GenericMuOpBase {
protected:
  /**
   * \brief This method is to be implemented by each numeric arithmetic binary
   * operation matching-mutating operator
   * @return the llvm op code of the instruction to match.
   */
  inline virtual unsigned getMyInstructionIROpCode() = 0;

public:
  bool matchIRs(MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp,
                unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) {
    llvm::Value *val = toMatch.getIRAt(pos);
    if (auto *binop = llvm::dyn_cast<llvm::BinaryOperator>(val)) {
      if (binop->getOpcode() != getMyInstructionIROpCode())
        return false;

      for (auto oprdID = 0; oprdID < binop->getNumOperands(); oprdID++) {
        if (!checkCPTypeInIR(mutationOp.getCPType(oprdID),
                             binop->getOperand(oprdID)))
          return false;
      }

      MatchUseful *ptr_mu = MU.getNew();
      if (llvm::isa<llvm::Constant>(binop->getOperand(0))) // first oprd
        ptr_mu->appendHLOprdsSource(pos, 0);
      else
        ptr_mu->appendHLOprdsSource(
            toMatch.depPosofPos(binop->getOperand(0), pos, true));
      if (llvm::isa<llvm::Constant>(binop->getOperand(1))) // second oprd
        ptr_mu->appendHLOprdsSource(pos, 1);
      else
        ptr_mu->appendHLOprdsSource(
            toMatch.depPosofPos(binop->getOperand(1), pos, true));
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
                MU.getHLOperandSource(i, DRU.toMatchMutant)->getType(),
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

  /*llvm::Value * createReplacement (llvm::Value * oprd1_addrOprd, llvm::Value *
  oprd2_intValOprd, std::vector<llvm::Value *> &replacement, ModuleUserInfos
  const &MI)
  {
      llvm::IRBuilder<> builder(MI.getContext());
      llvm::Value *add = builder.CreateAdd(oprdptr1, oprdptr2);
      if (!llvm::dyn_cast<llvm::Constant>(add))
          replacement.push_back(add);
      return add;
  }*/
}; // class NumericArithBinop_Base

} // namespace mart

#endif //__MART_GENMU_operatorClasses__NumericArithBinop_Base__
