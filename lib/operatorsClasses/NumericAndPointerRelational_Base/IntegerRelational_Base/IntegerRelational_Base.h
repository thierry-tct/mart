#ifndef __MART_GENMU_operatorClasses__IntegerRelational_Base__
#define __MART_GENMU_operatorClasses__IntegerRelational_Base__

/**
 * -==== IntegerRelational_Base.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all integer relational mutation
 * operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../NumericAndPointerRelational_Base.h"

namespace mart {

class IntegerRelational_Base : public NumericAndPointerRelational_Base {
protected:
  /**
   * \brief Implements from NumericAndPointerRelational_Base
   */
  inline llvm::Constant *getZero(llvm::Value *val) {
    return llvm::ConstantInt::get(val->getType(), 0);
    ;
  }

  /**
   * \brief Implements from NumericAndPointerRelational_Base
   */
  inline llvm::CmpInst::Predicate getNeqPred() {
    return llvm::CmpInst::ICMP_NE;
  }

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());
    llvm::Value *icmp =
        builder.CreateICmp(getMyPredicate(), oprd1_addrOprd, oprd2_intValOprd);
    if (!llvm::dyn_cast<llvm::Constant>(icmp))
      replacement.push_back(icmp);
    if (icmp->getType() != oprd1_addrOprd->getType()) {
      icmp = builder.CreateZExt(icmp, oprd1_addrOprd->getType());
      if (!llvm::dyn_cast<llvm::Constant>(icmp))
        replacement.push_back(icmp);
    }
    return icmp;
  }
}; // class IntegerRelational_Base

} // namespace mart

#endif //__MART_GENMU_operatorClasses__IntegerRelational_Base__
