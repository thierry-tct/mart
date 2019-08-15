#ifndef __MART_GENMU_operatorClasses__FPRightInc__
#define __MART_GENMU_operatorClasses__FPRightInc__

/**
 * -==== FPRightInc.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all non-pointer arithmetic FP
 * right increment mutation operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../NumericArithIncDec_Base.h"

namespace mart {

class FPRightInc : public NumericArithIncDec_Base {
protected:
  /**
   * \brief Implement from @see NumericArithIncDec_Base
   */
  inline bool isMyAdd1Sub1(llvm::BinaryOperator *modif,
                           llvm::Constant *constpart, unsigned notloadat01) {
    if (modif->getOpcode() == llvm::Instruction::FAdd) {
      if (llvm::dyn_cast<llvm::ConstantFP>(constpart)->isExactlyValue(1.0))
        return true;
    } else {
      if (modif->getOpcode() == llvm::Instruction::FSub)
        if (llvm::dyn_cast<llvm::ConstantFP>(constpart)->isExactlyValue(-1.0))
          if (notloadat01 !=
              0) // Constants 1 or -1 should always be the right hand oprd
            return true;
    }
    return false;
  }

  /**
   * \brief Implement from @see NumericArithIncDec_Base
   */
  inline bool isLeft_notRight() { return false; }

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());

    // llvm::dyn_cast<llvm::Instruction>(oprd1_addrOprd)->dump();//DEBUG
    llvm::Value *rawVal = oprd1_addrOprd;
    std::vector<llvm::Value *> seenCasts;
    while (llvm::isa<llvm::CastInst>(oprd1_addrOprd)) {
      // assert (llvm::dyn_cast<llvm::User>(val)->getNumOperands() == 1);
      // if (llvm::isa<llvm::PtrToIntInst>(oprd1_addrOprd))
      //    break;
      seenCasts.insert(seenCasts.begin(), oprd1_addrOprd);
      oprd1_addrOprd =
          llvm::dyn_cast<llvm::User>(oprd1_addrOprd)->getOperand(0);
    }

    // Assuming that we check before that it was a variable with
    // 'checkCPTypeInIR'
    assert(
        (llvm::isa<llvm::LoadInst>(
            oprd1_addrOprd) /*|| llvm::isa<llvm::PtrToIntInst>(oprd1_addrOprd)*/) &&
        "Must be Load Instruction here (assign left hand oprd)");

    llvm::Value *changedVal;
    if (seenCasts.empty()) {
      changedVal = builder.CreateFAdd(
          oprd1_addrOprd,
          llvm::ConstantFP::get(oprd1_addrOprd->getType(), 1.0));
    } else {
      if (oprd1_addrOprd->getType()->isIntegerTy())
        changedVal = builder.CreateAdd(
            oprd1_addrOprd,
            llvm::ConstantInt::get(oprd1_addrOprd->getType(), 1));
      else if (oprd1_addrOprd->getType()->isFloatingPointTy())
        changedVal = builder.CreateFAdd(
            oprd1_addrOprd,
            llvm::ConstantFP::get(oprd1_addrOprd->getType(), 1.0));
      else
        assert("The type is neither integer nor floating point, can't INC");
    }

    if (!llvm::dyn_cast<llvm::Constant>(changedVal))
      replacement.push_back(changedVal);
    // llvm::dyn_cast<llvm::Instruction>(changedVal)->dump();//DEBUG
    llvm::Value *storeit =
        llvm::dyn_cast<llvm::User>(oprd1_addrOprd)
            ->getOperand(
                0); // get the address where the data comes from (for store)
    assert(changedVal->getType()->getPrimitiveSizeInBits() &&
           "Must be primitive here");
    storeit = builder.CreateAlignedStore(
        changedVal, storeit,
        MI.getDataLayout().getPrefTypeAlignment(changedVal->getType()));
    replacement.push_back(storeit);

    return rawVal;
  }
}; // class FPRightInc

} // namespace mart

#endif //__MART_GENMU_operatorClasses__FPRightInc__
