#ifndef __MART_GENMU_operatorClasses__PLeftIncDerefInt__
#define __MART_GENMU_operatorClasses__PLeftIncDerefInt__

/**
 * -==== PLeftIncDerefInt.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     class for mutation operator that match and replace pointer add
 * operation (pointer + integer).
 */

#include "../DerefPointerArith_Base.h"

namespace mart {

class PLeftIncDerefInt : public DerefPointerArith_Base {
protected:
  /**
   * \brief Implements from DerefPointerArith_Base
   */
  inline enum ExpElemKeys getCorrespondingAritPtrOp() { return mPLEFTINC; }

  /**
   * \brief Implements from DerefPointerArith_Base
   */
  inline bool dereferenceFirst() { return false; }

  /**
   * \brief Implements from DerefPointerArith_Base
   */
  inline bool checkPtrContainedType(llvm::Value *pval) {
    if (auto *ptrTy = llvm::dyn_cast<llvm::PointerType>(pval->getType()))
      if (ptrTy->getElementType()->isIntegerTy())
        return true;
    return false;
  }

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());

    llvm::Value *ret = MI.getUserMaps()
                           ->getMatcherObject(getCorrespondingAritPtrOp())
                           ->createReplacement(oprd1_addrOprd, oprd2_intValOprd,
                                               replacement, MI);

    llvm::Value *deref = builder.CreateAlignedLoad(
        ret, MI.getDataLayout().getPointerTypeSize(ret->getType()));
    if (!llvm::dyn_cast<llvm::Constant>(deref))
      replacement.push_back(deref);

    return deref;
  }
}; // class PLeftIncDerefInt

} // namespace mart

#endif //__MART_GENMU_operatorClasses__PLeftIncDerefInt__
