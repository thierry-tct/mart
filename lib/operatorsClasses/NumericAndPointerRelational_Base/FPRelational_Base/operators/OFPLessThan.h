#ifndef __KLEE_SEMU_GENMU_operatorClasses__OFPLessThan__
#define __KLEE_SEMU_GENMU_operatorClasses__OFPLessThan__

/**
 * -==== OFPLessThan.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class for all OFP LT relational mutation operator.
 * \details   This abstract class define is extended from the Generic base class. 
 */
 
#include "../FPRelational_Base.h"

class OFPLessThan: public FPRelational_Base
{
  protected:
    /**
     * \brief Implements from NumericAndPointerRelational_Base
     */
    inline llvm::CmpInst::Predicate getMyPredicate ()
    {
        return llvm::CmpInst::FCMP_OLT;
    }
    
    /**
     * \brief Implements from FPRelational_Base
     */
    inline bool isUnordedRel() 
    {
        return false;
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__OFPLessThan__
