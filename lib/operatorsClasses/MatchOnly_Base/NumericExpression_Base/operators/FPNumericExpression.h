#ifndef __KLEE_SEMU_GENMU_operatorClasses__FPNumericExpression__
#define __KLEE_SEMU_GENMU_operatorClasses__FPNumericExpression__

/**
 * -==== FPNumericExpression.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Matching and replacement mutation operator for Floating Point Expressions.
 * \details   This abstract class define is extended from the @see NumericExpression_Base class. 
 */
 
#include "../NumericExpression_Base.h"

class FPNumericExpression: public NumericExpression_Base
{
  protected:
    /**
     * \brief Implement from @see NumericExpression_Base: Match FP
     */
    inline bool exprMatched(llvm::Value *val)
    {
        return val->getType()->isFloatingPointTy();
    }
    
    /**
     * \brief Implement from @see NumericExpression_Base: Match FP
     */
    inline enum ExpElemKeys getCorrespConstMatcherOp()
    {
        return mANYFCONST;
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__FPNumericExpression__
