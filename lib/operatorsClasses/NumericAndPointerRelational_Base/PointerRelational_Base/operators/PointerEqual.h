#ifndef __MART_GENMU_operatorClasses__PointerEqual__
#define __MART_GENMU_operatorClasses__PointerEqual__

/**
 * -==== PointerEqual.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all Pointer EQ relational mutation
 * operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../PointerRelational_Base.h"

namespace mart {

class PointerEqual : public PointerRelational_Base {
protected:
  /**
   * \brief Implements from NumericAndPointerRelational_Base
   */
  inline llvm::CmpInst::Predicate getMyPredicate() {
    return llvm::CmpInst::ICMP_EQ;
  }
}; // class PointerEqual

} // namespace mart

#endif //__MART_GENMU_operatorClasses__PointerEqual__
