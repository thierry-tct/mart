#ifndef __MART_GENMU_operatorClasses__IntegerAbs__
#define __MART_GENMU_operatorClasses__IntegerAbs__

/**
 * -==== IntegerAbs.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief    Class for replacing a numeric integer expresion with absolute value
 */

#include "../ReplaceOnly_Base.h"

namespace mart {

class IntegerAbs : public ReplaceOnly_Base {
  /**
   * \bref Inplements virtual from @see GenericMuOpBase
   */
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());
    llvm::Value *minusVal = builder.CreateSub(
        llvm::ConstantInt::get(oprd1_addrOprd->getType(), 0), oprd1_addrOprd);
    llvm::Value *cmp = builder.CreateICmp(
        llvm::CmpInst::ICMP_SGE, oprd1_addrOprd,
        llvm::ConstantInt::get(oprd1_addrOprd->getType(), 0));
    llvm::Value *abs = builder.CreateSelect(cmp, oprd1_addrOprd, minusVal);
    if (!llvm::dyn_cast<llvm::Constant>(minusVal))
      replacement.push_back(minusVal);
    if (!llvm::dyn_cast<llvm::Constant>(cmp))
      replacement.push_back(cmp);
    if (!llvm::dyn_cast<llvm::Constant>(abs))
      replacement.push_back(abs);
    return abs;
  }
}; // class IntegerAbs

} // namespace mart

#endif //__MART_GENMU_operatorClasses__IntegerAbs__
