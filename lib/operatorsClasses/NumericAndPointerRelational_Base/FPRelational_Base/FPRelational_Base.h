#ifndef __MART_GENMU_operatorClasses__FPRelational_Base__
#define __MART_GENMU_operatorClasses__FPRelational_Base__

/**
 * -==== FPRelational_Base.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all Floating Point relational
 * mutation operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../NumericAndPointerRelational_Base.h"

namespace mart {

class FPRelational_Base : public NumericAndPointerRelational_Base {
protected:
  /**
   * \brief Implements from NumericAndPointerRelational_Base
   */
  inline llvm::Constant *getZero(llvm::Value *val) {
    return llvm::ConstantFP::get(val->getType(), 0);
  }

  /**
   * \brief Implements from NumericAndPointerRelational_Base
   */
  inline llvm::CmpInst::Predicate getNeqPred() {
    return (isUnordedRel()) ? llvm::CmpInst::FCMP_UNE : llvm::CmpInst::FCMP_ONE;
  }

  /**
   * \brief Thell whether a Floating this floating point comparison is unorded
   * or not
   * @return true if the comparison (relational op) is unorded, else return
   * false
   */
  virtual inline bool isUnordedRel() = 0;

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());
    llvm::Value *fcmp =
        builder.CreateFCmp(getMyPredicate(), oprd1_addrOprd, oprd2_intValOprd);
    if (!llvm::dyn_cast<llvm::Constant>(fcmp))
      replacement.push_back(fcmp);
    fcmp = builder.CreateSIToFP(
        fcmp, oprd1_addrOprd
                  ->getType()); // operands are FP, make the result FP as well
    if (!llvm::dyn_cast<llvm::Constant>(fcmp))
      replacement.push_back(fcmp);
    return fcmp;
  }
}; // class FPRelational_Base

} // namespace mart

#endif //__MART_GENMU_operatorClasses__FPRelational_Base__
