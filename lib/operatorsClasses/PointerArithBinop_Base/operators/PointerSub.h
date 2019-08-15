#ifndef __MART_GENMU_operatorClasses__PointerSub__
#define __MART_GENMU_operatorClasses__PointerSub__

/**
 * -==== PointerSub.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     class for mutation operator that match and replace pointer add
 * operation (pointer + integer).
 */

#include "../PointerArithBinop_Base.h"

namespace mart {

class PointerSub : public PointerArithBinop_Base {
protected:
  /**
   * \brief Implements from PointerArithBinop_Base
   */
  inline bool checkIntPartConst(llvm::ConstantInt *constIndxVal) {
    return constIndxVal->isNegative();
  }

  /**
   * \brief Implements from PointerArithBinop_Base
   */
  inline bool checkIntPartExp(llvm::Instruction *tmpI) {
    return (tmpI->getOpcode() == llvm::Instruction::Sub ||
            tmpI->getOpcode() == llvm::Instruction::FSub);
  }

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());
    llvm::Value *valtmp = builder.CreateNeg(oprd2_intValOprd);
    if (!llvm::dyn_cast<llvm::Constant>(valtmp))
      replacement.push_back(valtmp);
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    llvm::Value *addsub_gep = builder.CreateInBoundsGEP(oprd1_addrOprd, valtmp);
#else
    llvm::Value *addsub_gep =
        builder.CreateInBoundsGEP(nullptr, oprd1_addrOprd, valtmp);
#endif
    if (!llvm::dyn_cast<llvm::Constant>(addsub_gep))
      replacement.push_back(addsub_gep);
    return addsub_gep;
  }
}; // class PointerSub

} // namespace mart

#endif //__MART_GENMU_operatorClasses__PointerSub__
