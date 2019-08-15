#ifndef __MART_GENMU_operatorClasses__ShuffleCaseDestinations__
#define __MART_GENMU_operatorClasses__ShuffleCaseDestinations__

/**
 * -==== ShuffleCaseDestinations.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief    Class for replacing shuffling switch's case and default destination
 * basic blocks. Actually inexistant. only the class Switchcases can implements
 * and use such replacer
 */

#include "../ReplaceOnly_Base.h"

namespace mart {

class ShuffleCaseDestinations : public ReplaceOnly_Base {
  /**
   * \bref Inplements virtual from @see GenericMuOpBase
   */
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::errs() << "Error: the class ShuffleCaseDestinations is unusable. "
                    "only SwitchCases can implements and use such replacer.\n";
    assert(false);
  }
}; // class ShuffleCaseDestinations

} // namespace mart

#endif //__MART_GENMU_operatorClasses__ShuffleCaseDestinations__
