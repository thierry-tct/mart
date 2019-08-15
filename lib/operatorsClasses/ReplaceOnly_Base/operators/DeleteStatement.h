#ifndef __MART_GENMU_operatorClasses__DeleteStatement__
#define __MART_GENMU_operatorClasses__DeleteStatement__

/**
 * -==== DeleteStatement.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief    Class for replacing by deletion. Actually inexistant. Please use
 * the method @see doDeleteStmt() method from GenericMutOpBase class instead
 */

#include "../ReplaceOnly_Base.h"

namespace mart {

class DeleteStatement : public ReplaceOnly_Base {
  /**
   * \bref Inplements virtual from @see GenericMuOpBase
   */
  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::errs() << "Error: the class DeleteStatement is unusable. Please use "
                    "the method doDeleteStmt() method from GenericMutOpBase "
                    "class instead.\n";
    assert(false);
  }
}; // class DeleteStatement

} // namespace mart

#endif //__MART_GENMU_operatorClasses__DeleteStatement__
