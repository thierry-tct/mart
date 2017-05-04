#ifndef __KLEE_SEMU_GENMU_operatorClasses__IntNumericVariable__
#define __KLEE_SEMU_GENMU_operatorClasses__IntNumericVariable__

/**
 * -==== IntNumericVariable.h
 *
 *                LLGenMu LLVM Mutation Tool
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Matching and replacement mutation operator for Integer variables.
 * \details   This abstract class define is extended from the @see NumericVariable_Base class. 
 */
 
#include "../NumericVariable_Base.h"

class IntNumericVariable: public NumericVariable_Base
{
  protected:
    /**
     * \brief Implement from @see NumericVariable_Base: Match int
     */
    inline bool varMatched(llvm::Value *val)
    {  
        return val->getType()->isIntegerTy();
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__IntNumericVariable__
