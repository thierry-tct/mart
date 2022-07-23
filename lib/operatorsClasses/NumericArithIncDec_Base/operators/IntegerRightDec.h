#ifndef __MART_GENMU_operatorClasses__IntegerRightDec__
#define __MART_GENMU_operatorClasses__IntegerRightDec__

/**
 * -==== IntegerRightDec.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all non-pointer arithmetic right
 * increment mutation operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../NumericArithIncDec_Base.h"

namespace mart {

class IntegerRightDec : public NumericArithIncDec_Base {
protected:
  /**
   * \brief Implement from @see NumericArithIncDec_Base
   */
  inline bool isMyAdd1Sub1(llvm::BinaryOperator *modif,
                           llvm::Constant *constpart, unsigned notloadat01) {
    if (modif->getOpcode() == llvm::Instruction::Add) {
      if (llvm::dyn_cast<llvm::ConstantInt>(constpart)->equalsInt(-1))
        return true;
    } else {
      if (modif->getOpcode() == llvm::Instruction::Sub)
        if (llvm::dyn_cast<llvm::ConstantInt>(constpart)->equalsInt(1))
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
    // Suppress build warnings
    (void)oprd2_intValOprd;
    // Computation
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
      changedVal = builder.CreateSub(
          oprd1_addrOprd, llvm::ConstantInt::get(oprd1_addrOprd->getType(), 1));
    } else {
      if (oprd1_addrOprd->getType()->isIntegerTy())
        changedVal = builder.CreateSub(
            oprd1_addrOprd,
            llvm::ConstantInt::get(oprd1_addrOprd->getType(), 1));
      else if (oprd1_addrOprd->getType()->isFloatingPointTy())
        changedVal = builder.CreateFSub(
            oprd1_addrOprd,
            llvm::ConstantFP::get(oprd1_addrOprd->getType(), 1.0));
      else
        assert("The type is neither integer nor floating point, can't DEC");
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
#if (LLVM_VERSION_MAJOR >= 10)
        llvm::MaybeAlign(MI.getDataLayout().getPrefTypeAlignment(changedVal->getType())));
#else
        MI.getDataLayout().getPrefTypeAlignment(changedVal->getType()));
#endif
    replacement.push_back(storeit);

    return rawVal;
  }
}; // class IntegerRightDec

} // namespace mart

#endif //__MART_GENMU_operatorClasses__IntegerRightDec__
