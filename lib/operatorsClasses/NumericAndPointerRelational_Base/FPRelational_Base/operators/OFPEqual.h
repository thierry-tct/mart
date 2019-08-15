#ifndef __MART_GENMU_operatorClasses__OFPEqual__
#define __MART_GENMU_operatorClasses__OFPEqual__

/**
 * -==== OFPEqual.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all OFP EQ relational mutation
 * operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../FPRelational_Base.h"

namespace mart {

class OFPEqual : public FPRelational_Base {
protected:
  /**
   * \brief Implements from NumericAndPointerRelational_Base
   */
  inline llvm::CmpInst::Predicate getMyPredicate() {
    return llvm::CmpInst::FCMP_OEQ;
  }

  /**
   * \brief Implements from FPRelational_Base
   */
  inline bool isUnordedRel() { return false; }
}; // class OFPEqual

} // namespace mart

#endif //__MART_GENMU_operatorClasses__OFPEqual__
