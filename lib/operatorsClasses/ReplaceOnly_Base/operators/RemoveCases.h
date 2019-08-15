#ifndef __MART_GENMU_operatorClasses__RemoveCases__
#define __MART_GENMU_operatorClasses__RemoveCases__

/**
 * -==== RemoveCases.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief    Class for removing some switch's cases, thus making it them point
 * to default basic block. Actually inexistant. only the class SwitchCases can
 * implements and use such replacer
 */

#include "../ReplaceOnly_Base.h"

namespace mart {

class RemoveCases : public ReplaceOnly_Base {
  /**
   * \bref Inplements virtual from @see GenericMuOpBase
   */
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::errs() << "Error: the class RemoveCases is unusable. only "
                    "SwitchCases can implements and use such replacer.\n";
    assert(false);
  }
}; // class RemoveCases

} // namespace mart

#endif //__MART_GENMU_operatorClasses__RemoveCases__
