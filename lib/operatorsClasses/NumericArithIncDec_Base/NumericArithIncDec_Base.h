#ifndef __MART_GENMU_operatorClasses__NumericArithIncDec_Base__
#define __MART_GENMU_operatorClasses__NumericArithIncDec_Base__

/**
 * -==== NumericArithIncDec_Base.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all non-pointer arithmetic
 * increment/decrement mutation operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../GenericMuOpBase.h"

namespace mart {

class NumericArithIncDec_Base : public GenericMuOpBase {
protected:
  /**
   * \brief This method is to be implemented by each numeric Expression
   * matching-mutating operator
   * @return true if the parameters are matching the requirement for the class
   * (leftinc (x+1), rightinc(x-1)). else false
   * @param modif is the operation that add or substract 1
   * @param constpart is the constant representing either 1 or -1
   * @param notloadat01 is either 0 or 1 and says that the operand of modif at 0
   * or 1 is not the load instruction
   */
  inline virtual bool isMyAdd1Sub1(llvm::BinaryOperator *modif,
                                   llvm::Constant *constpart,
                                   unsigned notloadat01) = 0;

  /**
   * \brief This method is to be implemented by each numeric increment decrement
   * matching-mutating operator
   * @return true if the increment or decrement is 'left' (like ++i, --i).
   * otherwise false (like i++, i--)
   */
  inline virtual bool isLeft_notRight() = 0;

public:
  bool matchIRs(MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp,
                unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) {
    llvm::Value *val = toMatch.getIRAt(pos);
    if (llvm::StoreInst *store = llvm::dyn_cast<llvm::StoreInst>(val)) {
      llvm::Value *addr = store->getOperand(1);

      if (llvm::BinaryOperator *modif =
              llvm::dyn_cast<llvm::BinaryOperator>(store->getOperand(0))) {
        llvm::LoadInst *load;
        unsigned notloadat01;
        if (load = llvm::dyn_cast<llvm::LoadInst>(modif->getOperand(0)))
          notloadat01 = 1;
        else if (load = llvm::dyn_cast<llvm::LoadInst>(modif->getOperand(1)))
          notloadat01 = 0;
        else
          return false;

        if (load->getOperand(0) != addr)
          return false;

        if (!checkCPTypeInIR(mutationOp.getCPType(0), load))
          return false;

        llvm::Constant *constpart =
            llvm::dyn_cast<llvm::Constant>(modif->getOperand(notloadat01));
        if (!constpart)
          return false;

        if (!isMyAdd1Sub1(modif, constpart, notloadat01))
          return false;

        int returningIRPos;
        int loadpos = toMatch.depPosofPos(load, pos, true);
        int modifpos = toMatch.depPosofPos(modif, pos, true);
        assert((pos > loadpos && pos > modifpos) && "problem in IR order");

        // check wheter it is left or right inc-dec
        if (load->getNumUses() == 2 && modif->getNumUses() == 1) {
          if (!isLeft_notRight())
            returningIRPos = loadpos;
          else
            return false;
        } else if (load->getNumUses() == 1 && modif->getNumUses() == 2) {
          if (isLeft_notRight())
            returningIRPos = modifpos;
          else
            return false;
        } else if (load->getNumUses() == 1 &&
                   modif->getNumUses() ==
                       1) { // Here the increment-decrement do not return (this
                            // mutants will be duplicate for left and right)
          /// if (mutationOp.matchOp == mLEFTINC || mutationOp.matchOp ==
          /// mFLEFTINC || mutationOp.matchOp == mLEFTDEC || mutationOp.matchOp
          /// == mFLEFTDEC)
          returningIRPos = pos; // do not return
          /// else
          ///    continue;
        } else {
          return false;
        }

        MatchUseful *ptr_mu = MU.getNew();
        ptr_mu->appendHLOprdsSource(loadpos);
        ptr_mu->appendRelevantIRPos(modifpos);
        ptr_mu->appendRelevantIRPos(pos);
        ptr_mu->setHLReturningIRPos(returningIRPos);
      }
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
    DRU.posOfIRtoRemove.push_back(MU.getRelevantIRPosOf(1));
    DRU.appendHLOprds(oprdptr[0]);
    DRU.appendHLOprds(oprdptr[1]);
    DRU.setOrigRelevantIRPos(MU.getRelevantIRPos());
    DRU.setHLReturningIRPos(MU.getHLReturningIRPos());
  }
}; // class NumericArithIncDec_Base

} // namespace mart

#endif //__MART_GENMU_operatorClasses__NumericArithIncDec_Base__
