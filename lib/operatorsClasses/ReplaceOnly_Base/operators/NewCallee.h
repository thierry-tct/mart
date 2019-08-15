#ifndef __MART_GENMU_operatorClasses__NewCallee__
#define __MART_GENMU_operatorClasses__NewCallee__

/**
 * -==== NewCallee.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief    Class for replacing function call callee. Actually inexistant. only
 * the class FunctionCallCallee can implements and use such replacer
 */

#include "../ReplaceOnly_Base.h"

namespace mart {

class NewCallee : public ReplaceOnly_Base {
  /**
   * \bref Inplements virtual from @see GenericMuOpBase
   */
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::errs() << "Error: the class NewCallee is unusable. only "
                    "FunctionCallCallee can implements and use such "
                    "replacer.\n";
    assert(false);
  }
}; // class NewCallee

} // namespace mart

#endif //__MART_GENMU_operatorClasses__NewCallee__
