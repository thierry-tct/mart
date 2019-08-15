#ifndef __MART_GENMU_operatorClasses__NumericAssign_Base__
#define __MART_GENMU_operatorClasses__NumericAssign_Base__

/**
 * -==== NumericAssign_Base.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all non-pointer assignement
 * mutation operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../GenericMuOpBase.h"

namespace mart {

class NumericAssign_Base : public GenericMuOpBase {
protected:
  /**
   * \brief This method is to be implemented by each numeric Assign
   * matching-mutating operator
   * @return the llvm op code of the instruction to match.
   */
  inline virtual bool valTypeMatched(llvm::Type *type) = 0;

public:
  virtual bool matchIRs(MatchStmtIR const &toMatch,
                        llvmMutationOp const &mutationOp, unsigned pos,
                        MatchUseful &MU, ModuleUserInfos const &MI) {
    llvm::Value *val = toMatch.getIRAt(pos);
    if (auto *store = llvm::dyn_cast<llvm::StoreInst>(val)) {
      if (valTypeMatched(store->getOperand(0)->getType()) &&
          checkCPTypeInIR(mutationOp.getCPType(1), store->getOperand(0))) {
        MatchUseful *ptr_mu = MU.getNew();

        // if (llvm::isa<llvm::Constant>(store->getOperand(1)) ||
        // llvm::isa<llvm::AllocaInst>(store->getOperand(1)) ||
        // llvm::isa<llvm::GlovalValue>(store->getOperand(1)))  //ptr oprd (oprd
        // 1)
        ptr_mu->appendHLOprdsSource(pos, 1);
        // else
        //    ptr_mu->appendHLOprdsSource(toMatch.depPosofPos(store->getOperand(1),
        //    pos, true));

        if (llvm::isa<llvm::Constant>(
                store->getOperand(0))) // val oprd (oprd 2)
        {
          ptr_mu->appendHLOprdsSource(pos, 0);
          ptr_mu->setHLReturningIRPos(pos);
        } else {
          if (llvm::isa<llvm::Argument>(store->getOperand(
                  0))) // Function argument, treat it as for constant
          {
            ptr_mu->appendHLOprdsSource(pos, 0);
            ptr_mu->setHLReturningIRPos(pos);
          } else {
            unsigned valncp =
                toMatch.depPosofPos(store->getOperand(0), pos, true);
            ptr_mu->appendHLOprdsSource(valncp);
            ptr_mu->setHLReturningIRPos(pos); // valncp);  //TODO: find a way to
                                              // handle a=b=c becomes a=b+.
                                              // heres the use a of b=c which is
                                              // use of c, become the use of
                                              // b+c. replace by valncp only
                                              // into users that follow??
          }
        }

        ptr_mu->appendRelevantIRPos(pos);
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
    unsigned storePos = MU.getRelevantIRPosOf(0);
    unsigned hlRetPos = MU.getHLReturningIRPos();
    assert(storePos >= hlRetPos && "storePos should be >= hlRetPos");
    llvm::Value *load = nullptr;
    for (int i = 0; i < repl.getOprdIndexList().size(); i++) {
      if (!(oprdptr[i] = createIfConst(
                MU.getHLOperandSource(1 /*1 to select val operand*/,
                                      DRU.toMatchMutant)
                    ->getType(),
                repl.getOprdIndexList()[i]))) {
        if (repl.getOprdIndexList()[i] == 0) // lvalue
        {
          llvm::IRBuilder<> builder(MI.getContext());
          load = builder.CreateAlignedLoad(
              MU.getHLOperandSource(0 /*1st HL Oprd*/, DRU.toMatchMutant),
              llvm::dyn_cast<llvm::StoreInst>(
                  DRU.toMatchMutant.getIRAt(storePos))
                  ->getAlignment());
          oprdptr[i] = load;
        } else // rvalue
          oprdptr[i] = MU.getHLOperandSource(repl.getOprdIndexList()[i] /*1*/,
                                             DRU.toMatchMutant);
      }
    }

    if (load) // if the lvalue was there, load have been created. We add now
              // because adding in the loop will modify toMatchMutant and bias
              // other OprdSources
    {
      DRU.toMatchMutant.insertIRAt(storePos, load);
      if (storePos == hlRetPos) // When a constant is the rvalue of assignment
        hlRetPos++;
      storePos++;
    }

    DRU.posOfIRtoRemove.push_back(storePos);
    DRU.appendHLOprds(oprdptr[0]);
    DRU.appendHLOprds(oprdptr[1]);
    DRU.setOrigRelevantIRPos(MU.getRelevantIRPos());
    DRU.setHLReturningIRPos(hlRetPos);
  }

  llvm::Value *createReplacement(llvm::Value *oprd1_addrOprd,
                                 llvm::Value *oprd2_intValOprd,
                                 std::vector<llvm::Value *> &replacement,
                                 ModuleUserInfos const &MI) {
    llvm::IRBuilder<> builder(MI.getContext());
    llvm::Value *valRet = oprd2_intValOprd;
    while (llvm::isa<llvm::CastInst>(oprd1_addrOprd)) {
      // assert (llvm::dyn_cast<llvm::User>(val)->getNumOperands() == 1);
      if (llvm::isa<llvm::PtrToIntInst>(oprd1_addrOprd))
        break;
      oprd2_intValOprd = reverseCast(
          llvm::dyn_cast<llvm::CastInst>(oprd1_addrOprd)->getOpcode(),
          oprd2_intValOprd,
          llvm::dyn_cast<llvm::User>(oprd1_addrOprd)->getOperand(0)->getType());
      if (!llvm::dyn_cast<llvm::Constant>(oprd2_intValOprd))
        replacement.push_back(oprd2_intValOprd);
      oprd1_addrOprd =
          llvm::dyn_cast<llvm::User>(oprd1_addrOprd)->getOperand(0);
    }

    // Assuming that we check before that it was a variable with
    // 'checkCPTypeInIR'
    assert((llvm::isa<llvm::LoadInst>(oprd1_addrOprd) ||
            llvm::isa<llvm::PtrToIntInst>(oprd1_addrOprd)) &&
           "Must be Load Instruction here (assign left hand oprd)");
    oprd1_addrOprd = llvm::dyn_cast<llvm::User>(oprd1_addrOprd)->getOperand(0);

    // store operand order is opposite of classical assignement (r->l)
    assert(oprd2_intValOprd->getType()->getPrimitiveSizeInBits() &&
           "The value to assign must be primitive here");
    llvm::Value *store = builder.CreateAlignedStore(
        oprd2_intValOprd, oprd1_addrOprd,
        MI.getDataLayout().getPrefTypeAlignment(oprd2_intValOprd->getType()));
    replacement.push_back(store);
    return valRet;
  }
}; // class NumericAssign_Base

} // namespace mart

#endif //__MART_GENMU_operatorClasses__NumericAssign_Base__
