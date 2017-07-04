#ifndef __KLEE_SEMU_GENMU_operatorClasses__DerefPSub__
#define __KLEE_SEMU_GENMU_operatorClasses__DerefPSub__

/**
 * -==== DerefPSub.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     class for mutation operator that match and replace pointer sub operation (pointer + integer).
 */
 
#include "../DerefPointerArith_Base.h"

class DerefPSub: public DerefPointerArith_Base
{
  protected:
    /**
     * \brief Implements from DerefPointerArith_Base
     */
    inline enum ExpElemKeys getCorrespondingAritPtrOp()
    {
        return mPSUB;
    }
    
    /**
     * \brief Implements from DerefPointerArith_Base
     */
    inline bool dereferenceFirst() 
    {
        return true;
    }
    
  public:
    llvm::Value * createReplacement (llvm::Value * oprd1_addrOprd, llvm::Value * oprd2_intValOprd, std::vector<llvm::Value *> &replacement, ModuleUserInfos const &MI)
    {
        llvm::IRBuilder<> builder(MI.getContext());
        
        llvm::Value *deref = builder.CreateAlignedLoad(oprd1_addrOprd, MI.getDataLayout().getPointerTypeSize(oprd1_addrOprd->getType())); 
        if (!llvm::dyn_cast<llvm::Constant>(deref))
            replacement.push_back(deref);
            
        llvm::Value *ret = MI.getUserMaps()->getMatcherObject(getCorrespondingAritPtrOp())->createReplacement(deref, oprd2_intValOprd, replacement, MI);
        
        return ret;
    }  
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__DerefPSub__
