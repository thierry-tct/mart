#ifndef __MART_GENMU_operatorClasses__IntegerAssign__
#define __MART_GENMU_operatorClasses__IntegerAssign__

/**
 * -==== IntegerAssign.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Matching and replacement mutation operator for Integer assignment.
 * \details   This abstract class define is extended from the @see
 * NumericExpression_Base class.
 */

#include "../NumericAssign_Base.h"

namespace mart {

class IntegerAssign : public NumericAssign_Base {
protected:
  /**
   * \brief Implement from @see NumericAssign_Base
   */
  inline bool valTypeMatched(llvm::Type *type) { return type->isIntegerTy(); }
}; // class IntegerAssign

} // namespace mart

#endif //__MART_GENMU_operatorClasses__IntegerAssign__
