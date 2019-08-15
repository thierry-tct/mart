#ifndef __MART_GENMU_operatorClasses__NumericConstant_Base__
#define __MART_GENMU_operatorClasses__NumericConstant_Base__

/**
 * -==== NumericConstant_Base.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class that match numeric constants and
 * replace (but do not create replacement).
 */

#include "../MatchOnly_Base.h"

namespace mart {

class NumericConstant_Base : public MatchOnly_Base {
protected:
  /**
   * \brief This method is to be implemented by each numeric constant
   * matching-mutating operator
   * \detail takes a vaule and check wheter it matches with the type that the
   * mutation operator matcher can handle.
   * @param val is the value to check
   * @return the corresponding constant is the value matches, otherwise, return
   * a null pointer.
   */
  inline virtual llvm::Constant *constMatched(llvm::Value *val) = 0;
  /*{  //Match both int and FP
      llvm::Constant *cval;
      if (cval = llvm::dyn_cast<llvm::ConstantInt>(val))
          return cval;
      if (cval = llvm::dyn_cast<llvm::ConstantFP>(val))
          return cval;
      return cval;
  }*/

public:
  bool matchIRs(MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp,
                unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) {
    llvm::Value *val = toMatch.getIRAt(pos);

    // Skip switch
    if (llvm::isa<llvm::SwitchInst>(val))
      return false;

    // llvm memory intrinsec: do no mutate alignement param. XXX
    if (auto *callinst = llvm::dyn_cast<llvm::CallInst>(val)) {
      if (llvm::Function *fun = callinst->getCalledFunction()) {
        auto fName = fun->getName();
        if (fName.startswith("llvm.mem") && fun->getReturnType()->isVoidTy() &&
            callinst->getNumArgOperands() == 5) // TODO: check llvm way of
                                                // detecting memory intrinsec
                                                // call, if exist
        {
          // only mutate, if possible, the 'len' and 'isvolatile' params
          int indxs[2] = {2, 4};
          for (auto i = 0; i < 2; i++) {
            if (llvm::Constant *constval =
                    constMatched(callinst->getArgOperand(indxs[i]))) {
              MatchUseful *ptr_mu = MU.getNew();
              ptr_mu->appendHLOprdsSource(pos, indxs[i]);
              ptr_mu->appendRelevantIRPos(pos);
              ptr_mu->setHLReturnIntoIRPos(pos);
              ptr_mu->setHLReturnIntoOprdIndex(indxs[i]);
            }
          }
        }
      }
    } else if (llvm::GetElementPtrInst *gep =
                   llvm::dyn_cast<llvm::GetElementPtrInst>(
                       val)) { // for gep, only mutate the poiter indexing
      int pindex;
      if (llvm::Value *tmpVal = checkIsPointerIndexingAndGet(gep, pindex)) {
        if (llvm::Constant *constval = constMatched(tmpVal)) {
          pindex++; // This because 'checkIsPointerIndexingAndGet' returns Gep
                    // index which is User index - 1
          MatchUseful *ptr_mu = MU.getNew();
          ptr_mu->appendHLOprdsSource(pos, pindex);
          ptr_mu->appendRelevantIRPos(pos);
          ptr_mu->setHLReturnIntoIRPos(pos);
          ptr_mu->setHLReturnIntoOprdIndex(pindex);
        }
      }
    } else {
      // for others mutate everything
      for (unsigned
               oprdvIt = 0,
               maxbound = llvm::dyn_cast<llvm::User>(val)->getNumOperands();
           oprdvIt < maxbound; oprdvIt++) {
        if (llvm::Constant *constval = constMatched(
                llvm::dyn_cast<llvm::User>(val)->getOperand(oprdvIt))) {
          MatchUseful *ptr_mu = MU.getNew();
          ptr_mu->appendHLOprdsSource(pos, oprdvIt);
          ptr_mu->appendRelevantIRPos(pos);
          ptr_mu->setHLReturnIntoIRPos(pos);
          ptr_mu->setHLReturnIntoOprdIndex(oprdvIt);
        }
      }
    }
    return (MU.first() != MU.end());
  }

  void prepareCloneIRs(MatchStmtIR const &toMatch, unsigned pos,
                       MatchUseful const &MU,
                       llvmMutationOp::MutantReplacors const &repl,
                       DoReplaceUseful &DRU, ModuleUserInfos const &MI) {
    DRU.toMatchMutant.setToCloneStmtIROf(toMatch, MI);
    llvm::Value *oprdptr[] = {nullptr, nullptr};
    for (int i = 0; i < repl.getOprdIndexList().size(); i++) {
      if (repl.getOprdIndexList()[i] == 0) {
        oprdptr[i] = llvm::dyn_cast<llvm::User>(
                         DRU.toMatchMutant.getIRAt(MU.getHLReturnIntoIRPos()))
                         ->getOperand(MU.getHLReturnIntoOprdIndex());
      } else {
        oprdptr[i] = createIfConst(
            MU.getHLOperandSource(0, DRU.toMatchMutant)->getType(),
            repl.getOprdIndexList()[i]); // it is a given constant value (ex: 2,
                                         // 5, ...)
        assert(oprdptr[i] && "unreachable, invalid operand id");
      }
    }
    // llvm::errs() << constval->getType() << " " << oprdptr[0]->getType() << "
    // " << oprdptr[1]->getType() << "\n";
    // llvm::dyn_cast<llvm::Instruction>(val)->dump();
    if (oprdptr[0] && oprdptr[1]) // binary operation case
      assert(oprdptr[0]->getType() == oprdptr[1]->getType() &&
             "ERROR: Type mismatch!!");

    DRU.appendHLOprds(oprdptr[0]);
    DRU.appendHLOprds(oprdptr[1]);
    DRU.setOrigRelevantIRPos(MU.getRelevantIRPos());
    DRU.setHLReturnIntoIRPos(MU.getHLReturnIntoIRPos());
    DRU.setHLReturnIntoOprdIndex(MU.getHLReturnIntoOprdIndex());
  }
}; // class NumericConstant_Base

} // namespace mart

#endif //__MART_GENMU_operatorClasses__NumericConstant_Base__
