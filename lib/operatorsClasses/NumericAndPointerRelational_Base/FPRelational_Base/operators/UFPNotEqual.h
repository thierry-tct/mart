#ifndef __MART_GENMU_operatorClasses__UFPNotEqual__
#define __MART_GENMU_operatorClasses__UFPNotEqual__

/**
 * -==== UFPNotEqual.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all UFP NE relational mutation
 * operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../FPRelational_Base.h"

namespace mart {

class UFPNotEqual : public FPRelational_Base {
protected:
  /**
   * \brief Implements from NumericAndPointerRelational_Base
   */
  inline llvm::CmpInst::Predicate getMyPredicate() {
    return llvm::CmpInst::FCMP_UNE;
  }

  /**
   * \brief Implements from FPRelational_Base
   */
  inline bool isUnordedRel() { return true; }
}; // class UFPNotEqual

} // namespace mart

#endif //__MART_GENMU_operatorClasses__UFPNotEqual__
