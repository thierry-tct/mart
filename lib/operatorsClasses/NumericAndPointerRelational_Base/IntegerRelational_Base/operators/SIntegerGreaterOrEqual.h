#ifndef __KLEE_SEMU_GENMU_operatorClasses__SIntegerGreaterOrEqual__
#define __KLEE_SEMU_GENMU_operatorClasses__SIntegerGreaterOrEqual__

/**
 * -==== SIntegerGreaterOrEqual.h
 *
 *                LLGenMu LLVM Mutation Tool
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class for all SInteger GE relational mutation operator.
 * \details   This abstract class define is extended from the Generic base class. 
 */
 
#include "../IntegerRelational_Base.h"

class SIntegerGreaterOrEqual: public IntegerRelational_Base
{
  protected:
    /**
     * \brief Implements from NumericAndPointerRelational_Base
     */
    inline llvm::CmpInst::Predicate getMyPredicate ()
    {
        return llvm::CmpInst::ICMP_SGE;
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__SIntegerGreaterOrEqual__
