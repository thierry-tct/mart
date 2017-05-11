#ifndef __KLEE_SEMU_GENMU_operatorClasses__SIntegerLessThan__
#define __KLEE_SEMU_GENMU_operatorClasses__SIntegerLessThan__

/**
 * -==== SIntegerLessThan.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class for all SInteger LT relational mutation operator.
 * \details   This abstract class define is extended from the Generic base class. 
 */
 
#include "../IntegerRelational_Base.h"

class SIntegerLessThan: public IntegerRelational_Base
{
  protected:
    /**
     * \brief Implements from NumericAndPointerRelational_Base
     */
    inline llvm::CmpInst::Predicate getMyPredicate ()
    {
        return llvm::CmpInst::ICMP_SLT;
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__SIntegerLessThan__
