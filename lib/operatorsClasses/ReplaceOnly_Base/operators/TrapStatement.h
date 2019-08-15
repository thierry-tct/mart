#ifndef __MART_GENMU_operatorClasses__TrapStatement__
#define __MART_GENMU_operatorClasses__TrapStatement__

/**
 * -==== TrapStatement.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief    Class for replacing by a trap.  Actually inexistant. Please use the
 * method @see doTrapStmt() method from GenericMutOpBase class instead
 */

#include "../ReplaceOnly_Base.h"

namespace mart {

class TrapStatement : public ReplaceOnly_Base {
  /**
   * \bref Inplements virtual from @see GenericMuOpBase
   */
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::errs() << "Error: the class TrapStatement is unusable. Please use "
                    "the method doTrapStmt() method from GenericMutOpBase "
                    "class instead.\n";
    assert(false);
  }
}; // class TrapStatement

} // namespace mart

#endif //__MART_GENMU_operatorClasses__TrapStatement__
