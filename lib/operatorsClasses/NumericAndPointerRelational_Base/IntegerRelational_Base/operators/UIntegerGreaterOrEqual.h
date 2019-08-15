#ifndef __MART_GENMU_operatorClasses__UIntegerGreaterOrEqual__
#define __MART_GENMU_operatorClasses__UIntegerGreaterOrEqual__

/**
 * -==== UIntegerGreaterOrEqual.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all UInteger GE relational
 * mutation operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../IntegerRelational_Base.h"

namespace mart {

class UIntegerGreaterOrEqual : public IntegerRelational_Base {
protected:
  /**
   * \brief Implements from NumericAndPointerRelational_Base
   */
  inline llvm::CmpInst::Predicate getMyPredicate() {
    return llvm::CmpInst::ICMP_UGE;
  }
}; // class UIntegerGreaterOrEqual

} // namespace mart

#endif //__MART_GENMU_operatorClasses__UIntegerGreaterOrEqual__
