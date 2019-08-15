#ifndef __MART_GENMU_operatorClasses__SignedIntegerMod__
#define __MART_GENMU_operatorClasses__SignedIntegerMod__

/**
 * -==== SignedIntegerMod.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Matching and replacement mutation operator for Signed Integer MOD.
 * \details   This abstract class define is extended from the @see
 * NumericExpression_Base class.
 */

#include "../NumericArithBinop_Base.h"

namespace mart {

class SignedIntegerMod : public NumericArithBinop_Base {
protected:
  /**
   * \brief Implement from @see NumericArithBinop_Base
   */
  inline unsigned getMyInstructionIROpCode() { return llvm::Instruction::SRem; }

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());
    llvm::Value *smod = builder.CreateSRem(oprd1_addrOprd, oprd2_intValOprd);
    if (!llvm::dyn_cast<llvm::Constant>(smod))
      replacement.push_back(smod);
    return smod;
  }
}; // class SignedIntegerMod

} // namespace mart

#endif //__MART_GENMU_operatorClasses__SignedIntegerMod__
