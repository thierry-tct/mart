#ifndef __MART_GENMU_operatorClasses__BitNot__
#define __MART_GENMU_operatorClasses__BitNot__

/**
 * -==== BitNot.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all non-pointer integer bitnot
 * negation mutation operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../NumericArithNegation_Base.h"

namespace mart {

class BitNot : public NumericArithNegation_Base {
protected:
  /**
   * \brief Implements from @see NumericArithNegation_Base
   */
  inline int matchAndGetOprdID(llvm::BinaryOperator *propNeg) {
    int oprdId = -1;
    if (propNeg->getOpcode() == llvm::Instruction::Xor) {
      if (llvm::ConstantInt *theConst =
              llvm::dyn_cast<llvm::ConstantInt>(propNeg->getOperand(0))) {
        if (theConst->equalsInt(-1))
          oprdId = 1;
        else if (llvm::isa<llvm::ConstantInt>(propNeg->getOperand(1)) &&
                 llvm::dyn_cast<llvm::ConstantInt>(propNeg->getOperand(1))
                     ->equalsInt(-1))
          oprdId = 0;
      } else {
        if (llvm::ConstantInt *theConst =
                llvm::dyn_cast<llvm::ConstantInt>(propNeg->getOperand(1)))
          if (theConst->equalsInt(-1))
            oprdId = 0;
      }
    }

    return oprdId;
  }

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());
    llvm::Value *bitnot = builder.CreateXor(
        oprd1_addrOprd,
        llvm::Constant::getAllOnesValue(
            oprd1_addrOprd
                ->getType())); // llvm::ConstantInt::get(oprd1_addrOprd->getType(),
                               // -1));
    if (!llvm::dyn_cast<llvm::Constant>(bitnot))
      replacement.push_back(bitnot);
    return bitnot;
  }
}; // class BitNot

} // namespace mart

#endif //__MART_GENMU_operatorClasses__BitNot__
