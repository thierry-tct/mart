#ifndef __KLEE_SEMU_GENMU_operatorClasses__IntegerNotEqual__
#define __KLEE_SEMU_GENMU_operatorClasses__IntegerNotEqual__

/**
 * -==== IntegerNotEqual.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class for all Integer NE relational mutation operator.
 * \details   This abstract class define is extended from the Generic base class. 
 */
 
#include "../IntegerRelational_Base.h"

class IntegerNotEqual: public IntegerRelational_Base
{
  protected:
    /**
     * \brief Implements from NumericAndPointerRelational_Base
     */
    inline llvm::CmpInst::Predicate getMyPredicate ()
    {
        return llvm::CmpInst::ICMP_NE;
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__IntegerNotEqual__
