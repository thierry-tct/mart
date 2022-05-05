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
                    "the method doTrapStmt() from GenericMutOpBase "
                    "class instead.\n";
    assert(false);
    // Suppress build warnings
    (void)oprd1_addrOprd;
    (void)oprd2_intValOprd;
    (void)replacement;
    (void)MI;
    return nullptr;
  }
}; // class TrapStatement

} // namespace mart

#endif //__MART_GENMU_operatorClasses__TrapStatement__
