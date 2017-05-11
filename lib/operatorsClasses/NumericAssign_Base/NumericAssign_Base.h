#ifndef __KLEE_SEMU_GENMU_operatorClasses__NumericAssign_Base__
#define __KLEE_SEMU_GENMU_operatorClasses__NumericAssign_Base__

/**
 * -==== NumericAssign_Base.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class for all non-pointer assignement mutation operator.
 * \details   This abstract class define is extended from the Generic base class. 
 */
 
#include "../GenericMuOpBase.h"

class NumericAssign_Base: public GenericMuOpBase
{
  protected:
    /**
     * \brief This method is to be implemented by each numeric Assign matching-mutating operator
     * @return the llvm op code of the instruction to match.
     */
    inline virtual bool valTypeMatched(llvm::Type * type) = 0;
    
  public:
    virtual bool matchIRs (std::vector<llvm::Value *> const &toMatch, llvmMutationOp const &mutationOp, unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) 
    {
        llvm::Value *val = toMatch.at(pos);
        if (auto *store = llvm::dyn_cast<llvm::StoreInst>(val))
        {
            if (valTypeMatched (store->getOperand(0)->getType()) && checkCPTypeInIR (mutationOp.getCPType(1), store->getOperand(0)))  
            {
                MatchUseful *ptr_mu = MU.getNew();
                
                //if (llvm::isa<llvm::Constant>(store->getOperand(1)) || llvm::isa<llvm::AllocaInst>(store->getOperand(1)) || llvm::isa<llvm::GlovalValue>(store->getOperand(1)))  //ptr oprd (oprd 1)
                ptr_mu->appendHLOprdsSource(pos, 1);
                //else
                //    ptr_mu->appendHLOprdsSource(depPosofPos(toMatch, store->getOperand(1), pos, true));
                
                if (llvm::isa<llvm::Constant>(store->getOperand(0)))    //val oprd (oprd 2)
                {
                    ptr_mu->appendHLOprdsSource(pos, 0);
                    ptr_mu->setHLReturningIRPos(pos);
                }
                else
                {
                    if (llvm::isa<llvm::Argument>(store->getOperand(0)))    //Function argument, treat it as for constant
                    {
                        ptr_mu->appendHLOprdsSource(pos, 0);
                        ptr_mu->setHLReturningIRPos(pos);
                    }
                    else
                    {
                        unsigned valncp = depPosofPos(toMatch, store->getOperand(0), pos, true);
                        ptr_mu->appendHLOprdsSource(valncp);
                        ptr_mu->setHLReturningIRPos(valncp);
                    }
                }
                    
                ptr_mu->appendRelevantIRPos(pos);
            }
        }
        return (MU.first() != MU.end());
    }
    
    void prepareCloneIRs (std::vector<llvm::Value *> const &toMatch, unsigned pos,  MatchUseful const &MU, llvmMutationOp::MutantReplacors const &repl, DoReplaceUseful &DRU, ModuleUserInfos const &MI)
    {
        cloneStmtIR (toMatch, DRU.toMatchClone);
        llvm::Value * oprdptr[]={nullptr, nullptr};
        unsigned storePos = MU.getRelevantIRPosOf(0);
        unsigned hlRetPos = MU.getHLReturningIRPos();
        assert (storePos >= hlRetPos && "storePos should be >= hlRetPos");
        llvm::Value *load = nullptr;
        for (int i=0; i < repl.getOprdIndexList().size(); i++)
        {
            if (!(oprdptr[i] = createIfConst (MU.getHLOperandSource(1/*1 to select val operand*/, DRU.toMatchClone)->getType(), repl.getOprdIndexList()[i])))   
            {
                if (repl.getOprdIndexList()[i] == 0)    // lvalue
                {
                    llvm::IRBuilder<> builder(MI.getContext());
                    load = builder.CreateAlignedLoad(MU.getHLOperandSource(0/*1st HL Oprd*/, DRU.toMatchClone), 
                                                                llvm::dyn_cast<llvm::StoreInst>(DRU.toMatchClone[storePos])->getAlignment());
                    oprdptr[i] = load;
                }
                else          //rvalue
                    oprdptr[i] = MU.getHLOperandSource(repl.getOprdIndexList()[i]/*1*/, DRU.toMatchClone);
            }
        }
        
        if (load)   //if the lvalue was there, load have been created. We add now because adding in the loop will modify toMatchClone and bias other OprdSources
        {
            DRU.toMatchClone.insert(DRU.toMatchClone.begin() + storePos, load);
            if (storePos == hlRetPos)   //When a constant is the rvalue of assignment
                hlRetPos++;
            storePos++;
        }
        
        DRU.posOfIRtoRemove.push_back(storePos);
        DRU.appendHLOprds(oprdptr[0]);
        DRU.appendHLOprds(oprdptr[1]);
        DRU.setOrigRelevantIRPos(MU.getRelevantIRPos());
        DRU.setHLReturningIRPos(hlRetPos);
    }
    
    llvm::Value * createReplacement (llvm::Value * oprd1_addrOprd, llvm::Value * oprd2_intValOprd, std::vector<llvm::Value *> &replacement, ModuleUserInfos const &MI)
    {
        llvm::IRBuilder<> builder(MI.getContext());
        llvm::Value *valRet = oprd2_intValOprd;
        while (llvm::isa<llvm::CastInst>(oprd1_addrOprd))
        {
            //assert (llvm::dyn_cast<llvm::User>(val)->getNumOperands() == 1);
            if (llvm::isa<llvm::PtrToIntInst>(oprd1_addrOprd))
                break;
            oprd2_intValOprd = reverseCast(llvm::dyn_cast<llvm::CastInst>(oprd1_addrOprd)->getOpcode(), oprd2_intValOprd, llvm::dyn_cast<llvm::User>(oprd1_addrOprd)->getOperand(0)->getType());
            if (!llvm::dyn_cast<llvm::Constant>(oprd2_intValOprd))
                replacement.push_back(oprd2_intValOprd);
            oprd1_addrOprd = llvm::dyn_cast<llvm::User>(oprd1_addrOprd)->getOperand(0);
        }
        
        //Assuming that we check before that it was a variable with 'checkCPTypeInIR'
        assert ((llvm::isa<llvm::LoadInst>(oprd1_addrOprd) || llvm::isa<llvm::PtrToIntInst>(oprd1_addrOprd)) && "Must be Load Instruction here (assign left hand oprd)");
        oprd1_addrOprd = llvm::dyn_cast<llvm::User>(oprd1_addrOprd)->getOperand(0);
        
        //store operand order is opposite of classical assignement (r->l)
        assert (oprd2_intValOprd->getType()->getPrimitiveSizeInBits() && "The value to assign must be primitive here");
        llvm::Value *store = builder.CreateAlignedStore(oprd2_intValOprd, oprd1_addrOprd, oprd2_intValOprd->getType()->getPrimitiveSizeInBits()/8);  //TODO: Maybe replace the alignement here with that from DataLayout
        replacement.push_back(store);
        return valRet;
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__NumericAssign_Base__
