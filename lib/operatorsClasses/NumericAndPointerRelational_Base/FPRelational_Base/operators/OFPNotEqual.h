#ifndef __MART_GENMU_operatorClasses__OFPNotEqual__
#define __MART_GENMU_operatorClasses__OFPNotEqual__

/**
 * -==== OFPNotEqual.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all OFP NE relational mutation
 * operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../FPRelational_Base.h"

namespace mart {

class OFPNotEqual : public FPRelational_Base {
protected:
  /**
   * \brief Implements from NumericAndPointerRelational_Base
   */
  inline llvm::CmpInst::Predicate getMyPredicate() {
    return llvm::CmpInst::FCMP_ONE;
  }

  /**
   * \brief Implements from FPRelational_Base
   */
  inline bool isUnordedRel() { return false; }
}; // class OFPNotEqual

} // namespace mart

#endif //__MART_GENMU_operatorClasses__OFPNotEqual__
