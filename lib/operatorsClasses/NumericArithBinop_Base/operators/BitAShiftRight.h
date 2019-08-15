#ifndef __MART_GENMU_operatorClasses__BitAShiftRight__
#define __MART_GENMU_operatorClasses__BitAShiftRight__

/**
 * -==== BitAShiftRight.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Matching and replacement mutation operator for BIT Arithmetic
 * Shift Right.
 * \details   This abstract class define is extended from the @see
 * NumericExpression_Base class.
 */

#include "../NumericArithBinop_Base.h"

namespace mart {

class BitAShiftRight : public NumericArithBinop_Base {
protected:
  /**
   * \brief Implement from @see NumericArithBinop_Base
   */
  inline unsigned getMyInstructionIROpCode() { return llvm::Instruction::AShr; }

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());
    llvm::Value *ashr = builder.CreateAShr(oprd1_addrOprd, oprd2_intValOprd);
    if (!llvm::dyn_cast<llvm::Constant>(ashr))
      replacement.push_back(ashr);
    return ashr;
  }
}; // class BitAShiftRight

} // namespace mart

#endif //__MART_GENMU_operatorClasses__BitAShiftRight__
