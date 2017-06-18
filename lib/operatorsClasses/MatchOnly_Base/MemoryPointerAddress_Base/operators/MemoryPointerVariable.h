#ifndef __KLEE_SEMU_GENMU_operatorClasses__MemoryPointerVariable__
#define __KLEE_SEMU_GENMU_operatorClasses__MemoryPointerVariable__

/**
 * -==== MemoryPointerVariable.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Matching and replacement mutation operator for memory pointers (variable for address).
 * \details   This abstract class define is extended from the @see MemoryPointerAddress_Base class. 
 */
 
#include "../MemoryPointerAddress_Base.h"

class MemoryPointerVariable: public MemoryPointerAddress_Base
{
  protected:
    /**
     * \brief Implement from @see MemoryPointerAddress_Base: Match memory pointer
     */
    inline bool isMatched(llvm::Value *val)
    {
        return (llvm::isa<llvm::LoadInst>(val) && val->getType()->isPointerTy() && canGep(val));
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__MemoryPointerVariable__
