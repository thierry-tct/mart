#ifndef __MART_GENMU_operatorClasses__PointerRelational_Base__
#define __MART_GENMU_operatorClasses__PointerRelational_Base__

/**
 * -==== PointerRelational_Base.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all Pointer relational mutation
 * operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../NumericAndPointerRelational_Base.h"

namespace mart {

class PointerRelational_Base : public NumericAndPointerRelational_Base {
protected:
  /**
   * \brief Implements from NumericAndPointerRelational_Base
   */
  inline llvm::Constant *getZero(llvm::Value *val) {
    return llvm::ConstantPointerNull::get(
        llvm::dyn_cast<llvm::PointerType>(val->getType()));
  }

  /**
   * \brief Implements from NumericAndPointerRelational_Base
   */
  inline llvm::CmpInst::Predicate getNeqPred() {
    llvm::errs() << "Error: Should never be called, because pointer relational "
                    "can only be replaced by another relational (which is "
                    "treated differently): " /*<< typeid(*this).name()*/
                 << "\n";
    assert(false);
  }

public:
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::errs() << "Error: Should never be called, because pointer relational "
                    "can only be replaced by another relational (Nothing else "
                    "can create it): " /*<< typeid(*this).name()*/
                 << "\n";
    assert(false);
  }
}; // class PointerRelational_Base

} // namespace mart

#endif //__MART_GENMU_operatorClasses__PointerRelational_Base__
