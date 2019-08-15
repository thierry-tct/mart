#ifndef __MART_GENMU_operatorClasses__PointerLeftInc__
#define __MART_GENMU_operatorClasses__PointerLeftInc__

/**
 * -==== PointerLeftInc.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     class for mutation operator that match and replace pointer left
 * increment operation.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../PointerIncDec_Base.h"

namespace mart {

class PointerLeftInc : public PointerIncDec_Base {
protected:
  /**
   * \brief Implements from PointerIncDec_Base
   */
  inline bool checkIntPartConst1(llvm::ConstantInt *constIndxVal) {
    return (constIndxVal->equalsInt(1));
  }

  /**
   * \brief Implements from PointerIncDec_Base
   */
  inline bool isLeft_notRight() { return true; }

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());

    // Assuming that we check before that it was a Pointer with
    // 'checkCPTypeInIR'
    assert(llvm::isa<llvm::LoadInst>(oprd1_addrOprd) &&
           "Must be Load Instruction here (address oprd)");

#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    llvm::Value *changedVal = builder.CreateInBoundsGEP(
        oprd1_addrOprd,
        llvm::ConstantInt::get(
            llvm::Type::getIntNTy(MI.getContext(),
                                  MI.getDataLayout().getPointerTypeSizeInBits(
                                      oprd1_addrOprd->getType())),
            1));
#else
    llvm::Value *changedVal = builder.CreateInBoundsGEP(
        nullptr, oprd1_addrOprd,
        llvm::ConstantInt::get(
            llvm::Type::getIntNTy(MI.getContext(),
                                  MI.getDataLayout().getPointerTypeSizeInBits(
                                      oprd1_addrOprd->getType())),
            1));
#endif
    if (!llvm::dyn_cast<llvm::Constant>(changedVal))
      replacement.push_back(changedVal);
    // llvm::dyn_cast<llvm::Instruction>(changedVal)->dump();//DEBUG
    llvm::Value *storeit = llvm::dyn_cast<llvm::LoadInst>(oprd1_addrOprd)
                               ->getPointerOperand(); // get the address where
                                                      // the data comes from
                                                      // (for store)
    // assert(changedVal->getType()->getPrimitiveSizeInBits() && "Must be
    // primitive here");
    storeit = builder.CreateAlignedStore(
        changedVal, storeit,
        MI.getDataLayout().getPointerTypeSize(
            storeit->getType())); // the val here is a pointer
    replacement.push_back(storeit);
    return changedVal;
  }
}; // class PointerLeftInc

} // namespace mart

#endif //__MART_GENMU_operatorClasses__PointerLeftInc__
