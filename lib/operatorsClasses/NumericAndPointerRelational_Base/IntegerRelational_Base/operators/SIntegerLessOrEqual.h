#ifndef __MART_GENMU_operatorClasses__SIntegerLessOrEqual__
#define __MART_GENMU_operatorClasses__SIntegerLessOrEqual__

/**
 * -==== SIntegerLessOrEqual.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all SInteger LE relational
 * mutation operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../IntegerRelational_Base.h"

namespace mart {

class SIntegerLessOrEqual : public IntegerRelational_Base {
protected:
  /**
   * \brief Implements from NumericAndPointerRelational_Base
   */
  inline llvm::CmpInst::Predicate getMyPredicate() {
    return llvm::CmpInst::ICMP_SLE;
  }
}; // class SIntegerLessOrEqual

} // namespace mart

#endif //__MART_GENMU_operatorClasses__SIntegerLessOrEqual__
