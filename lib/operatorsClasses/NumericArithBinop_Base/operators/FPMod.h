#ifndef __KLEE_SEMU_GENMU_operatorClasses__FPMod__
#define __KLEE_SEMU_GENMU_operatorClasses__FPMod__

/**
 * -==== FPMod.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Matching and replacement mutation operator for Floating Point MOD.
 * \details   This abstract class define is extended from the @see NumericExpression_Base class. 
 */
 
#include "../NumericArithBinop_Base.h"

class FPMod: public NumericArithBinop_Base
{
  protected:
    /**
     * \brief Implement from @see NumericArithBinop_Base
     */
    inline unsigned getMyInstructionIROpCode()
    {  
        return llvm::Instruction::FRem;
    }
    
  public:
    llvm::Value * createReplacement (llvm::Value * oprd1_addrOprd, llvm::Value * oprd2_intValOprd, std::vector<llvm::Value *> &replacement, ModuleUserInfos const &MI)
    {
        llvm::IRBuilder<> builder(MI.getContext());
        llvm::Value *fmod = builder.CreateFRem(oprd1_addrOprd, oprd2_intValOprd);
        if (!llvm::dyn_cast<llvm::Constant>(fmod))
            replacement.push_back(fmod);
        return fmod;
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__FPMod__