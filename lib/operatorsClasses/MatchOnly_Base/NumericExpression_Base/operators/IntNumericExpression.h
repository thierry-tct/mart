#ifndef __KLEE_SEMU_GENMU_operatorClasses__IntNumericExpression__
#define __KLEE_SEMU_GENMU_operatorClasses__IntNumericExpression__

/**
 * -==== IntNumericExpression.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Matching and replacement mutation operator for Integer Expressions.
 * \details   This abstract class define is extended from the @see NumericExpression_Base class. 
 */
 
#include "../NumericExpression_Base.h"

class IntNumericExpression: public NumericExpression_Base
{
  protected:
    /**
     * \brief Implement from @see NumericExpression_Base: Match int
     */
    inline bool exprMatched(llvm::Value *val)
    {  
        return val->getType()->isIntegerTy();
    }
    
    /**
     * \brief Implement from @see NumericExpression_Base: Match FP
     */
    inline enum ExpElemKeys getCorrespConstMatcherOp()
    {
        return mANYICONST;
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__IntNumericExpression__
