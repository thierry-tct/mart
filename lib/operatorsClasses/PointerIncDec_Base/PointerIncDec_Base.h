#ifndef __MART_GENMU_operatorClasses__PointerIncDec_Base__
#define __MART_GENMU_operatorClasses__PointerIncDec_Base__

/**
 * -==== PointerIncDec_Base.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all mutation operator that match
 * and replace binary increment/decrement operation.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../GenericMuOpBase.h"

namespace mart {

class PointerIncDec_Base : public GenericMuOpBase {
protected:
  /**
   * \brief This method is to be implemented by each numeric pointer arithmetic
   * ind/dec operation matching-mutating operator
   * @return true if the parameter matches, else false.
   */
  inline virtual bool checkIntPartConst1(llvm::ConstantInt *constIndxVal) = 0;

  /**
   * \brief This method is to be implemented by each Pointer increment decrement
   * matching-mutating operator
   * @return true if the increment or decrement is 'left' (like ++p, --p).
   * otherwise false (like p++, p--)
   */
  inline virtual bool isLeft_notRight() = 0;

public:
  bool matchIRs(MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp,
                unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) {
    llvm::Value *val = toMatch.getIRAt(pos);
    if (llvm::StoreInst *store = llvm::dyn_cast<llvm::StoreInst>(val)) {
      llvm::Value *addr =
          store->getOperand(1); // equivalent to getPointerOperand()

      if (llvm::GetElementPtrInst *modif =
              llvm::dyn_cast<llvm::GetElementPtrInst>(
                  store->getOperand(0))) // equivalent to getValueOperand()
      {
        // Should be only for pointer var and add/sub only on the pointer
        if (modif->getNumIndices() != 1)
          return false;
        llvm::LoadInst *load =
            llvm::dyn_cast<llvm::LoadInst>(modif->getPointerOperand());
        if (!load)
          return false;
        int indx;
        llvm::Value *indxVal = checkIsPointerIndexingAndGet(modif, indx);
        if (!indxVal || !llvm::isa<llvm::ConstantInt>(indxVal))
          return false;
        llvm::ConstantInt *constpart =
            llvm::dyn_cast<llvm::ConstantInt>(indxVal);
        if (!checkIntPartConst1(constpart))
          return false;

        // XXX: can actually remove this check, because it will always be a
        // pointer here
        if (!checkCPTypeInIR(mutationOp.getCPType(0),
                             modif->getPointerOperand()))
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
        } else if (load->getNumUses() == 1 && modif->getNumUses() == 1) {
          // Here the increment-decrement do not return (this
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
        ptr_mu->appendRelevantIRPos(modifpos); // 0
        ptr_mu->appendRelevantIRPos(pos);      // 1
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
    llvm::Value *ptroprd = nullptr, *valoprd = nullptr;
    if (repl.getOprdIndexList().size() == 2) // size is 2
    {
      ptroprd = MU.getHLOperandSource(repl.getOprdIndexList()[0],
                                      DRU.toMatchMutant); // load
      valoprd = createIfConst(
          (*(llvm::dyn_cast<llvm::GetElementPtrInst>(
                 DRU.toMatchMutant.getIRAt(MU.getRelevantIRPosOf(0)))
                 ->idx_begin()))
              ->getType(),
          repl.getOprdIndexList()[1]);
      assert(valoprd && "Problem here, must be hard coded constant int here");
    } else // size is 1
    {
      if (ptroprd = createIfConst(
              (*(llvm::dyn_cast<llvm::GetElementPtrInst>(
                     DRU.toMatchMutant.getIRAt(MU.getRelevantIRPosOf(0)))
                     ->idx_begin()))
                  ->getType(),
              repl.getOprdIndexList()[0])) { // The replacor should be
                                             // CONST_VALUE_OF
        llvm::IRBuilder<> builder(MI.getContext());
        ptroprd = builder.CreateIntToPtr(
            ptroprd,
            (DRU.toMatchMutant.getIRAt(MU.getHLReturningIRPos()))->getType());
        if (!llvm::isa<llvm::Constant>(ptroprd))
          DRU.toMatchMutant.insertIRAt(
              MU.getRelevantIRPosOf(1) + 1,
              ptroprd); // insert right after the instruction to remove
      } else            // pointer
      {
        ptroprd = MU.getHLOperandSource(
            repl.getOprdIndexList()[0],
            DRU.toMatchMutant); // load. repl.getOprdIndexList()[0] is equal to
                                // 0 here
      }
    }

    DRU.posOfIRtoRemove.push_back(MU.getRelevantIRPosOf(0));
    DRU.posOfIRtoRemove.push_back(MU.getRelevantIRPosOf(1));
    DRU.appendHLOprds(ptroprd);
    DRU.appendHLOprds(valoprd);
    DRU.setOrigRelevantIRPos(MU.getRelevantIRPos());
    DRU.setHLReturningIRPos(MU.getHLReturningIRPos());
  }
}; // class PointerIncDec_Base

} // namespace mart

#endif //__MART_GENMU_operatorClasses__PointerIncDec_Base__
