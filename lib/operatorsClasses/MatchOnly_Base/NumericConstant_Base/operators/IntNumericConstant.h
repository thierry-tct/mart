#ifndef __MART_GENMU_operatorClasses__IntNumericConstant__
#define __MART_GENMU_operatorClasses__IntNumericConstant__

/**
 * -==== IntNumericConstant.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Matching and replacement mutation operator for Integer constants.
 * \details   This abstract class define is extended from the @see
 * NumericConstant_Base class.
 */

#include "../NumericConstant_Base.h"

namespace mart {

class IntNumericConstant : public NumericConstant_Base {
protected:
  /**
   * \brief Implement from @see NumericConstant_Base: Match int
   */
  inline llvm::Constant *constMatched(llvm::Value *val) {
    return llvm::dyn_cast<llvm::ConstantInt>(val);
  }

  /**
   * \brief Implement from @see NumericConstant_Base
   */
  inline bool constAndEquals(llvm::Value *c1, llvm::Value *c2) {
    assert(c1->getType() == c2->getType() && "Type Mismatch (Int)!");
    auto *ic1 = llvm::dyn_cast<llvm::ConstantInt>(c1);
    auto *ic2 = llvm::dyn_cast<llvm::ConstantInt>(c2);
    if (ic1->getBitWidth() == ic2->getBitWidth()) {
      if (ic1->getValue() == ic2->getValue())
        return true;
    }
    return false;
  }
}; // class IntNumericConstant

} // namespace mart

#endif //__MART_GENMU_operatorClasses__IntNumericConstant__
