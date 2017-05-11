#ifndef __KLEE_SEMU_GENMU_operatorClasses__PointerGreaterThan__
#define __KLEE_SEMU_GENMU_operatorClasses__PointerGreaterThan__

/**
 * -==== PointerGreaterThan.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class for all Pointer GT relational mutation operator.
 * \details   This abstract class define is extended from the Generic base class. 
 */
 
#include "../PointerRelational_Base.h"

class PointerGreaterThan: public PointerRelational_Base
{
  protected:
    /**
     * \brief Implements from NumericAndPointerRelational_Base
     */
    inline llvm::CmpInst::Predicate getMyPredicate ()
    {
        return llvm::CmpInst::ICMP_UGT;
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__PointerGreaterThan__
