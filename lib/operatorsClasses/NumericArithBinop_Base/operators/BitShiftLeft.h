#ifndef __MART_GENMU_operatorClasses__BitShiftLeft__
#define __MART_GENMU_operatorClasses__BitShiftLeft__

/**
 * -==== BitShiftLeft.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Matching and replacement mutation operator for BIT Shift Left.
 * \details   This abstract class define is extended from the @see
 * NumericExpression_Base class.
 */

#include "../NumericArithBinop_Base.h"

namespace mart {

class BitShiftLeft : public NumericArithBinop_Base {
protected:
  /**
   * \brief Implement from @see NumericArithBinop_Base
   */
  inline unsigned getMyInstructionIROpCode() { return llvm::Instruction::Shl; }

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());
    llvm::Value *shl = builder.CreateShl(oprd1_addrOprd, oprd2_intValOprd);
    if (!llvm::dyn_cast<llvm::Constant>(shl))
      replacement.push_back(shl);
    return shl;
  }
}; // class BitShiftLeft

} // namespace mart

#endif //__MART_GENMU_operatorClasses__BitShiftLeft__
