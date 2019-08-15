#ifndef __MART_GENMU_operatorClasses__PointerLessThan__
#define __MART_GENMU_operatorClasses__PointerLessThan__

/**
 * -==== PointerLessThan.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all Pointer LT relational mutation
 * operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../PointerRelational_Base.h"

namespace mart {

class PointerLessThan : public PointerRelational_Base {
protected:
  /**
   * \brief Implements from NumericAndPointerRelational_Base
   */
  inline llvm::CmpInst::Predicate getMyPredicate() {
    return llvm::CmpInst::ICMP_ULT;
  }
}; // class PointerLessThan

} // namespace mart

#endif //__MART_GENMU_operatorClasses__PointerLessThan__
