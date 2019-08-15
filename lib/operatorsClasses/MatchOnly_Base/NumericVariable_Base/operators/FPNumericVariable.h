#ifndef __MART_GENMU_operatorClasses__FPNumericVariable__
#define __MART_GENMU_operatorClasses__FPNumericVariable__

/**
 * -==== FPNumericVariable.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Matching and replacement mutation operator for Floating Point
 * variables.
 * \details   This abstract class define is extended from the @see
 * NumericVariable_Base class.
 */

#include "../NumericVariable_Base.h"

namespace mart {

class FPNumericVariable : public NumericVariable_Base {
protected:
  /**
   * \brief Implement from @see NumericVariable_Base: Match FP
   */
  inline bool varMatched(llvm::Value *val) {
    return val->getType()->isFloatingPointTy();
  }
}; // class FPNumericVariable

} // namespace mart

#endif //__MART_GENMU_operatorClasses__FPNumericVariable__
