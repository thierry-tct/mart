#ifndef __MART_GENMU_operatorClasses__BitXor__
#define __MART_GENMU_operatorClasses__BitXor__

/**
 * -==== BitXor.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Matching and replacement mutation operator for BIT XOR.
 * \details   This abstract class define is extended from the @see
 * NumericExpression_Base class.
 */

#include "../NumericArithBinop_Base.h"

namespace mart {

class BitXor : public NumericArithBinop_Base {
protected:
  /**
   * \brief Implement from @see NumericArithBinop_Base
   */
  inline unsigned getMyInstructionIROpCode() { return llvm::Instruction::Xor; }

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());
    llvm::Value *bxor = builder.CreateXor(oprd1_addrOprd, oprd2_intValOprd);
    if (bxor != oprd1_addrOprd && bxor != oprd2_intValOprd)
      if (!llvm::dyn_cast<llvm::Constant>(bxor))
        replacement.push_back(bxor);
    return bxor;
  }
}; // class BitXor

} // namespace mart

#endif //__MART_GENMU_operatorClasses__BitXor__
