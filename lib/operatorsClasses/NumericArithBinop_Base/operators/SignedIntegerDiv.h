#ifndef __MART_GENMU_operatorClasses__SignedIntegerDiv__
#define __MART_GENMU_operatorClasses__SignedIntegerDiv__

/**
 * -==== SignedIntegerDiv.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Matching and replacement mutation operator for Signed Integer DIV.
 * \details   This abstract class define is extended from the @see
 * NumericExpression_Base class.
 */

#include "../NumericArithBinop_Base.h"

namespace mart {

class SignedIntegerDiv : public NumericArithBinop_Base {
protected:
  /**
   * \brief Implement from @see NumericArithBinop_Base
   */
  inline unsigned getMyInstructionIROpCode() { return llvm::Instruction::SDiv; }

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());
    llvm::Value *sdiv = builder.CreateSDiv(oprd1_addrOprd, oprd2_intValOprd);
    if (!llvm::dyn_cast<llvm::Constant>(sdiv))
      replacement.push_back(sdiv);
    return sdiv;
  }
}; // class SignedIntegerDiv

} // namespace mart

#endif //__MART_GENMU_operatorClasses__SignedIntegerDiv__
