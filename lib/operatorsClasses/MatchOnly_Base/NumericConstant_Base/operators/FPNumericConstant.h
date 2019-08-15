#ifndef __MART_GENMU_operatorClasses__FPNumericConstant__
#define __MART_GENMU_operatorClasses__FPNumericConstant__

/**
 * -==== FPNumericConstant.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Matching and replacement mutation operator for Floating Point
 * constants.
 * \details   This abstract class define is extended from the @see
 * NumericConstant_Base class.
 */

#include "../NumericConstant_Base.h"

namespace mart {

class FPNumericConstant : public NumericConstant_Base {
protected:
  /**
   * \brief Implement from @see NumericConstant_Base: Match FP
   */
  inline llvm::Constant *constMatched(llvm::Value *val) {
    return llvm::dyn_cast<llvm::ConstantFP>(val);
  }

  /**
   * \brief Implement from @see NumericConstant_Base
   */
  inline bool constAndEquals(llvm::Value *c1, llvm::Value *c2) {
    assert(c1->getType() == c2->getType() && "Type Mismatch (FP)!");
    if (llvm::dyn_cast<llvm::ConstantFP>(c1)->isExactlyValue(
            llvm::dyn_cast<llvm::ConstantFP>(c2)->getValueAPF()))
      return true;
    return false;
  }
}; // class FPNumericConstant

} // namespace mart

#endif //__MART_GENMU_operatorClasses__FPNumericConstant__
