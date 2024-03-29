#ifndef __MART_GENMU_operatorClasses__DerefPAdd__
#define __MART_GENMU_operatorClasses__DerefPAdd__

/**
 * -==== DerefPAdd.h
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

class DerefPAdd : public DerefPointerArith_Base {
protected:
  /**
   * \brief Implements from DerefPointerArith_Base
   */
  inline enum ExpElemKeys getCorrespondingAritPtrOp() { return mPADD; }

  /**
   * \brief Implements from DerefPointerArith_Base
   */
  inline bool dereferenceFirst() { return true; }

  inline bool isCommutative() { return true; }

public:
  bool matchIRs(MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp,
                unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) {
    // Make sure that the gep do not have struct elem access (only array index)
    if (llvm::GetElementPtrInst *gep =
            llvm::dyn_cast<llvm::GetElementPtrInst>(toMatch.getIRAt(pos)))
      if (gep->getNumIndices() != 1)
        return false;
    DerefPointerArith_Base::matchIRs(toMatch, mutationOp, pos, MU, MI);
    return true;
  }

  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());

    llvm::Value *deref = builder.CreateAlignedLoad(
#if (LLVM_VERSION_MAJOR >= 10)
        oprd1_addrOprd->getType()->getPointerElementType(),
        oprd1_addrOprd,
        llvm::MaybeAlign(MI.getDataLayout().getPointerTypeSize(oprd1_addrOprd->getType())));
#else
        oprd1_addrOprd,
        MI.getDataLayout().getPointerTypeSize(oprd1_addrOprd->getType()));
#endif
    if (!llvm::dyn_cast<llvm::Constant>(deref))
      replacement.push_back(deref);

    llvm::Value *ret =
        MI.getUserMaps()
            ->getMatcherObject(getCorrespondingAritPtrOp())
            ->createReplacement(deref, oprd2_intValOprd, replacement, MI);

    return ret;
  }
}; // class DerefPAdd

} // namespace mart

#endif //__MART_GENMU_operatorClasses__DerefPAdd__
