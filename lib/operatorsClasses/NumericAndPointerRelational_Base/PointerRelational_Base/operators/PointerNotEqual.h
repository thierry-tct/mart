#ifndef __MART_GENMU_operatorClasses__PointerNotEqual__
#define __MART_GENMU_operatorClasses__PointerNotEqual__

/**
 * -==== PointerNotEqual.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all Pointer NE relational mutation
 * operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../PointerRelational_Base.h"

namespace mart {

class PointerNotEqual : public PointerRelational_Base {
protected:
  /**
   * \brief Implements from NumericAndPointerRelational_Base
   */
  inline llvm::CmpInst::Predicate getMyPredicate() {
    return llvm::CmpInst::ICMP_NE;
  }
}; // class PointerNotEqual

} // namespace mart

#endif //__MART_GENMU_operatorClasses__PointerNotEqual__
