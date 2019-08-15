#ifndef __MART_GENMU_operatorClasses__FPAbs__
#define __MART_GENMU_operatorClasses__FPAbs__

/**
 * -==== FPAbs.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief    Class for replacing a numeric Floating Point expresion with
 * absolute value
 */

#include "../ReplaceOnly_Base.h"

namespace mart {

class FPAbs : public ReplaceOnly_Base {
  /**
   * \bref Inplements virtual from @see GenericMuOpBase
   */
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());
    llvm::Value *fminusVal = builder.CreateFSub(
        llvm::ConstantFP::get(oprd1_addrOprd->getType(), 0.0), oprd1_addrOprd);
    llvm::Value *fcmp = builder.CreateFCmp(
        llvm::CmpInst::FCMP_UGE, oprd1_addrOprd,
        llvm::ConstantFP::get(oprd1_addrOprd->getType(), 0.0));
    llvm::Value *fabs = builder.CreateSelect(fcmp, oprd1_addrOprd, fminusVal);
    if (!llvm::dyn_cast<llvm::Constant>(fminusVal))
      replacement.push_back(fminusVal);
    if (!llvm::dyn_cast<llvm::Constant>(fcmp))
      replacement.push_back(fcmp);
    if (!llvm::dyn_cast<llvm::Constant>(fabs))
      replacement.push_back(fabs);
    return fabs;
  }
}; // class FPAbs

} // namespace mart

#endif //__MART_GENMU_operatorClasses__FPAbs__
