#ifndef __MART_GENMU_operatorClasses__DerefSub__
#define __MART_GENMU_operatorClasses__DerefSub__

/**
 * -==== DerefSub.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     class for mutation operator that match and replace pointer sub
 * operation (pointer + integer).
 */

#include "../DerefPointerArith_Base.h"

namespace mart {

class DerefSub : public DerefPointerArith_Base {
protected:
  /**
   * \brief Implements from DerefPointerArith_Base
   */
  inline enum ExpElemKeys getCorrespondingAritPtrOp() { return mSUB; }

  /**
   * \brief Implements from DerefPointerArith_Base
   */
  inline bool dereferenceFirst() { return true; }

  inline bool isCommutative() { return true; }

  /**
   * \brief Implements from DerefPointerArith_Base
   */
  inline virtual void getSubMatchMutationOp(llvmMutationOp const &mutationOp,
                                            llvmMutationOp &tmpMutationOp) {
    tmpMutationOp.setMatchOp_CP(
        getCorrespondingAritPtrOp(),
        std::vector<enum codeParts>({cpEXPR, mutationOp.getCPType(1)}));
  }

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());

    llvm::Value *deref = builder.CreateAlignedLoad(
        oprd1_addrOprd,
        MI.getDataLayout().getPointerTypeSize(oprd1_addrOprd->getType()));
    if (!llvm::dyn_cast<llvm::Constant>(deref))
      replacement.push_back(deref);

    auto ptrwidth =
        llvm::dyn_cast<llvm::IntegerType>(deref->getType())->getBitWidth();
    auto idxwidth =
        llvm::dyn_cast<llvm::IntegerType>(oprd2_intValOprd->getType())
            ->getBitWidth();
    if (ptrwidth != idxwidth) {
      oprd2_intValOprd = builder.CreateSExtOrTrunc(
          oprd2_intValOprd, deref->getType()); // Choose SExt over ZExt because
                                               // the index of gep is signed
      if (!llvm::dyn_cast<llvm::Constant>(oprd2_intValOprd))
        replacement.push_back(oprd2_intValOprd);
    }

    llvm::Value *ret =
        MI.getUserMaps()
            ->getMatcherObject(getCorrespondingAritPtrOp())
            ->createReplacement(deref, oprd2_intValOprd, replacement, MI);

    return ret;
  }
}; // class DerefSub

} // namespace mart

#endif //__MART_GENMU_operatorClasses__DerefSub__
