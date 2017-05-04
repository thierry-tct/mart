#ifndef __KLEE_SEMU_GENMU_operatorClasses__NumericConstant_Base__
#define __KLEE_SEMU_GENMU_operatorClasses__NumericConstant_Base__

/**
 * -==== NumericConstant_Base.h
 *
 *                LLGenMu LLVM Mutation Tool
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class that match numeric constants and replace (but do not create replacement). 
 */
 
#include "../MatchOnly_Base.h"

class NumericConstant_Base: public MatchOnly_Base
{
  protected:
    /**
     * \brief This method is to be implemented by each numeric constant matching-mutating operator
     * \detail takes a vaule and check wheter it matches with the type that the mutation operator matcher can handle.
     * @param val is the value to check
     * @return the corresponding constant is the value matches, otherwise, return a null pointer.
     */
    inline virtual llvm::Constant * constMatched(llvm::Value *val) = 0;
    /*{  //Match both int and FP
        llvm::Constant *cval;
        if (cval = llvm::dyn_cast<llvm::ConstantInt>(val))
            return cval;
        if (cval = llvm::dyn_cast<llvm::ConstantFP>(val))
            return cval;
        return cval;
    }*/
    
  public:
    bool matchIRs (std::vector<llvm::Value *> const &toMatch, llvmMutationOp const &mutationOp, unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) 
    {
        llvm::Value *val = toMatch.at(pos);
        for (auto oprdvIt = 0; oprdvIt < llvm::dyn_cast<llvm::User>(val)->getNumOperands(); oprdvIt++) 
        {
            if (llvm::Constant *constval = constMatched(llvm::dyn_cast<llvm::User>(val)->getOperand(oprdvIt)))
            {
                MatchUseful *ptr_mu = MU.getNew();
                ptr_mu->appendHLOprdsSource(pos, oprdvIt);
                ptr_mu->appendRelevantIRPos(pos);
                ptr_mu->setHLReturnIntoIRPos(pos);
                ptr_mu->setHLReturnIntoOprdIndex(oprdvIt);
            }
        }
        return (MU.first() != MU.end());
    }
    
    void prepareCloneIRs (std::vector<llvm::Value *> const &toMatch, unsigned pos,  MatchUseful const &MU, llvmMutationOp::MutantReplacors const &repl, DoReplaceUseful &DRU, ModuleUserInfos const &MI)
    {
        cloneStmtIR (toMatch, DRU.toMatchClone);
        llvm::Value * oprdptr[]={nullptr, nullptr};
        for (int i=0; i < repl.getOprdIndexList().size(); i++)
        {
            if (repl.getOprdIndexList()[i] == 0)
            {
                oprdptr[i] = llvm::dyn_cast<llvm::User>(DRU.toMatchClone[MU.getHLReturnIntoIRPos()])->getOperand(MU.getHLReturnIntoOprdIndex());
            }
            else 
            {
                oprdptr[i] = createIfConst (MU.getHLOperandSource(0, DRU.toMatchClone)->getType(), repl.getOprdIndexList()[i]);  //it is a given constant value (ex: 2, 5, ...)
                assert (oprdptr[i] && "unreachable, invalid operand id");
            }
        }
        //llvm::errs() << constval->getType() << " " << oprdptr[0]->getType() << " " << oprdptr[1]->getType() << "\n";
        //llvm::dyn_cast<llvm::Instruction>(val)->dump();
        if (oprdptr[0] && oprdptr[1])   //binary operation case
            assert (oprdptr[0]->getType() == oprdptr[1]->getType() && "ERROR: Type mismatch!!");
            
        DRU.appendHLOprds(oprdptr[0]);
        DRU.appendHLOprds(oprdptr[1]);
        DRU.setOrigRelevantIRPos(MU.getRelevantIRPos());
        DRU.setHLReturnIntoIRPos(MU.getHLReturnIntoIRPos());
        DRU.setHLReturnIntoOprdIndex(MU.getHLReturnIntoOprdIndex());
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__NumericConstant_Base__
