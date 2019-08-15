#ifndef __MART_GENMU_operatorClasses__IntegerMinus__
#define __MART_GENMU_operatorClasses__IntegerMinus__

/**
 * -==== IntegerMinus.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all non-pointer integer minus
 * negation mutation operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../NumericArithNegation_Base.h"

namespace mart {

class IntegerMinus : public NumericArithNegation_Base {
protected:
  /**
   * \brief Implements from @see NumericArithNegation_Base
   */
  inline int matchAndGetOprdID(llvm::BinaryOperator *propNeg) {
    int oprdId = -1;
    if (llvm::ConstantInt *theConst =
            llvm::dyn_cast<llvm::ConstantInt>(propNeg->getOperand(0)))
      if (propNeg->getOpcode() == llvm::Instruction::Sub)
        if (theConst->equalsInt(0))
          oprdId = 1;

    return oprdId;
  }

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());
    llvm::Value *neg = builder.CreateSub(
        llvm::ConstantInt::get(oprd1_addrOprd->getType(), 0), oprd1_addrOprd);
    if (!llvm::dyn_cast<llvm::Constant>(neg))
      replacement.push_back(neg);
    return neg;
  }
}; // class IntegerMinus

} // namespace mart

#endif //__MART_GENMU_operatorClasses__IntegerMinus__
