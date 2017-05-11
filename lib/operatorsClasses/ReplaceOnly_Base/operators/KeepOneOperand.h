#ifndef __KLEE_SEMU_GENMU_operatorClasses__KeepOneOperand__
#define __KLEE_SEMU_GENMU_operatorClasses__KeepOneOperand__

/**
 * -==== KeepOneOperand.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief    Class for replacing an expression with one of its operator (passed in the first operand of the method createReplacement())
 */
 
#include "../ReplaceOnly_Base.h"

class KeepOneOperand: public ReplaceOnly_Base
{
    /**
     * \bref Inplements virtual from @see GenericMuOpBase
     */
    llvm::Value * createReplacement (llvm::Value * oprd1_addrOprd, llvm::Value * oprd2_intValOprd, std::vector<llvm::Value *> &replacement, ModuleUserInfos const &MI)
    {
        return oprd1_addrOprd;
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__KeepOneOperand__
