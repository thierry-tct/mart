#ifndef __KLEE_SEMU_GENMU_operatorClasses__DerefPointerArithIncDec_Base__
#define __KLEE_SEMU_GENMU_operatorClasses__DerefPointerArithIncDec_Base__

/**
 * -==== DerefPointerArithIncDec_Base.h
 *
 *                MuLL Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details. 
 *  
 * \brief     Generic abstract base class for all mutation operator that match and replace a combination of arithmetic increment/decrement operation and pointer dereference.
 * \details   This abstract class define is extended from the Generic base class. 
 */
 
#include "GenericMuOpBase.h"

class DerefPointerArithIncDec_Base: public GenericMuOpBase
{
    protected:
    /**
     * \brief This method is to be implemented by each deref-arith matching-mutating operator
     * @return the constant key in user maps for the arithmetic numeric/pointer operation that is composing this operation together with deref
     */
    inline virtual enum ExpElemKeys getCorrespondingAritPtrOp() = 0;
    
    /**
     * \brief Say wheter the operation dereference first (ex: (*p)--, *(p)+9, ...)
     * @return true if the dereference first, else false.
     */
    inline virtual bool dereferenceFirst() = 0;
    
    /**
     *  \brief
     * @return the single use of the IR at position pos, and its position in 'ret_pos'. if do not have single use, return nullptr. 
     */
    inline llvm::Value * getSingleUsePos (MatchStmtIR const &toMatch, unsigned pos, unsigned &ret_pos)
    {
        auto * tmp = toMatch.getIRAt(pos);
        if (tmp->hasOneUse())
        {
            ret_pos = toMatch.depPosofPos(tmp->user_back(), pos, false);
            return tmp->user_back();
        }
        ret_pos = -1;  //max unsigned
        return nullptr;
    }
    
  public:
    bool matchIRs (MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp, unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) 
    {
        llvm::Value *val = toMatch.getIRAt(pos);
        if (MI.getUserMaps()->getMatcherObject(getCorrespondingAritPtrOp())->matchIRs(toMatch, mutationOp, pos, MU, MI))
        {
            assert (MU.next() == MU.end() && "Must have exactely one element here (deref first)");
            unsigned loadPos;
            if (dereferenceFirst())
            {
                llvm::SmallVector<std::pair<unsigned, unsigned>, 2> hloprd_reset_data;
                unsigned noprd = MU.getNumberOfHLOperands();
                assert (noprd <= 2 && "must be binop or incdec");
                for (unsigned hloprd_id = 0; hloprd_id < noprd ; ++hloprd_id)
                {
                    if (auto *load = llvm::dyn_cast<llvm::LoadInst>(MU.getHLOperandSource(hloprd_id, toMatch)))
                    {
                        auto *gep = llvm::dyn_cast<llvm::GetElementPtrInst>(load->getPointerOperand());
                        int indx;
                        if (gep && checkIsPointerIndexingAndGet(gep, indx))
                        {
                            //Modify MU, setting load pointer operand as HL perand instead of arith operands
                            hloprd_reset_data.emplace_back(hloprd_id, toMatch.depPosofPos(gep, pos, true));
                        }
                    }
                }
                if (hloprd_reset_data.empty())
                {
                    MU.clearAll();
                }
                else
                {
                    MatchUseful *ptr_mu = &MU;
                    for (auto i=hloprd_reset_data.size()-1; i>0; --i)   //if exactely 1 element, no need to getNew, since we can directly use the ptr_mu returned by arith matcher
                        ptr_mu = ptr_mu->getNew_CopyData();
                    unsigned i=0;
                    for (ptr_mu = MU.first(); ptr_mu != MU.end(); ptr_mu = ptr_mu->next(), ++i)
                    {
                        loadPos = hloprd_reset_data[i].second;
                        ptr_mu->resetHLOprdSourceAt(hloprd_reset_data[i].first, loadPos);
                        ptr_mu->appendRelevantIRPos(loadPos);
                    }
                }
            }
            else
            {
                if (llvm::dyn_cast_or_null<llvm::LoadInst>(getSingleUsePos(toMatch, MU.getHLReturningIRPos()/*Must have returningIR*/, loadPos)))
                {
                    //Modify MU, setting returning IR pos to loadPos
                    MU.setHLReturningIRPos(loadPos);
                    MU.appendRelevantIRPos(loadPos);
                }
                else
                {
                    MU.clearAll();
                }
            }
        }
        else
        {
            MU.clearAll();
        }
        
        return (MU.first() != MU.end());
    }
    
    void prepareCloneIRs (MatchStmtIR const &toMatch, unsigned pos,  MatchUseful const &MU, llvmMutationOp::MutantReplacors const &repl, DoReplaceUseful &DRU, ModuleUserInfos const &MI)
    {
    
        /*DRU.posOfIRtoRemove.push_back(newPos);
        DRU.appendHLOprds(ptroprd);
        DRU.appendHLOprds(valoprd);
        DRU.setOrigRelevantIRPos(MU.getRelevantIRPos());
        DRU.setHLReturningIRPos(newPos);*/
    }
};

#endif //__KLEE_SEMU_GENMU_operatorClasses__DerefPointerArithIncDec_Base__
