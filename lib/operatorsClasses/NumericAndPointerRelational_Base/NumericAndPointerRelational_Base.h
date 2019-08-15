#ifndef __MART_GENMU_operatorClasses__NumericAndPointerRelational_Base__
#define __MART_GENMU_operatorClasses__NumericAndPointerRelational_Base__

/**
 * -==== NumericAndPointerRelational_Base.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all non-pointer and pointer
 * relational mutation operator.
 * \details   This abstract class define is extended from the Generic base
 * class.
 */

#include "../GenericMuOpBase.h"

namespace mart {

class NumericAndPointerRelational_Base : public GenericMuOpBase {
protected:
  /**
   * \brief This method is to be implemented by each relational
   * matching-mutating operator
   * @return the predicate of CMP Inst for this rel operation (NE, EQ,..)
   */
  inline virtual llvm::CmpInst::Predicate getMyPredicate() = 0;

  /**
   * \brief This method is to be implemented by each relational
   * matching-mutating operator
   * @return the constant with value 0 of this type
   */
  inline virtual llvm::Constant *getZero(llvm::Value *val) = 0;

  /**
   * \brief This method is to be implemented by each relational
   * matching-mutating operator
   * @return predicate for not equal that match with the type of the passed
   * value
   */
  inline virtual llvm::CmpInst::Predicate getNeqPred() = 0;

public:
  bool matchIRs(MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp,
                unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) {
    llvm::Value *val = toMatch.getIRAt(pos);
    if (llvm::CmpInst *cmp = llvm::dyn_cast<llvm::CmpInst>(val)) {
      if (!(checkCPTypeInIR(mutationOp.getCPType(0), cmp->getOperand(0)) &&
            checkCPTypeInIR(mutationOp.getCPType(1), cmp->getOperand(1))))
        return false;

      if (getMyPredicate() != cmp->getPredicate())
        return false;

      MatchUseful *ptr_mu = MU.getNew();
      if (llvm::isa<llvm::Constant>(cmp->getOperand(0))) // first oprd
        ptr_mu->appendHLOprdsSource(pos, 0);
      else
        ptr_mu->appendHLOprdsSource(
            toMatch.depPosofPos(cmp->getOperand(0), pos, true));
      if (llvm::isa<llvm::Constant>(cmp->getOperand(1))) // second oprd
        ptr_mu->appendHLOprdsSource(pos, 1);
      else
        ptr_mu->appendHLOprdsSource(
            toMatch.depPosofPos(cmp->getOperand(1), pos, true));
      ptr_mu->appendRelevantIRPos(pos);
      ptr_mu->setHLReturningIRPos(pos);
    }
    return (MU.first() != MU.end());
  }

  void prepareCloneIRs(MatchStmtIR const &toMatch, unsigned pos,
                       MatchUseful const &MU,
                       llvmMutationOp::MutantReplacors const &repl,
                       DoReplaceUseful &DRU, ModuleUserInfos const &MI) {
    const std::map<enum ExpElemKeys, llvm::CmpInst::Predicate> *mrel_IRrel_Map =
        getPredRelMap();
    DRU.toMatchMutant.setToCloneStmtIROf(toMatch, MI);
    unsigned cmpPos = MU.getRelevantIRPosOf(0);
    std::map<enum ExpElemKeys, llvm::CmpInst::Predicate>::const_iterator
        map_it = mrel_IRrel_Map->find(repl.getExpElemKey());
    if (map_it != mrel_IRrel_Map->end()) { // directly replace the Predicate in
                                           // case the replacor is also a CMP
                                           // instruction
      // NO CALL TO doReplacement HERE
      llvm::dyn_cast<llvm::CmpInst>(DRU.toMatchMutant.getIRAt(cmpPos))
          ->setPredicate(map_it->second);
      llvm::Value *oprdptr[2];
      oprdptr[0] = llvm::dyn_cast<llvm::User>(DRU.toMatchMutant.getIRAt(cmpPos))
                       ->getOperand(0);
      oprdptr[1] = llvm::dyn_cast<llvm::User>(DRU.toMatchMutant.getIRAt(cmpPos))
                       ->getOperand(1);
      llvm::dyn_cast<llvm::User>(DRU.toMatchMutant.getIRAt(cmpPos))
          ->setOperand(0, oprdptr[repl.getOprdIndexList()[0]]);
      llvm::dyn_cast<llvm::User>(DRU.toMatchMutant.getIRAt(cmpPos))
          ->setOperand(1, oprdptr[repl.getOprdIndexList()[1]]);

      DRU.setOrigRelevantIRPos(MU.getRelevantIRPos());
      //\\\\resultMuts.push_back(DRU.toMatchMutant);
    } else {
      llvm::Value *oprdptr[] = {nullptr, nullptr};
      for (int i = 0; i < repl.getOprdIndexList().size(); i++) {
        if (!(oprdptr[i] = createIfConst(
                  MU.getHLOperandSource(i /*either 0 or 1*/, DRU.toMatchMutant)
                      ->getType(),
                  repl.getOprdIndexList()[i]))) {
          oprdptr[i] = MU.getHLOperandSource(repl.getOprdIndexList()[i],
                                             DRU.toMatchMutant);
        }
      }

      llvm::Constant *zero =
          getZero(MU.getHLOperandSource(1, DRU.toMatchMutant));
      llvm::CmpInst::Predicate neqPredHere = getNeqPred();
      DRU.appendHLOprds(oprdptr[0]);
      DRU.appendHLOprds(oprdptr[1]);
      DRU.setOrigRelevantIRPos(MU.getRelevantIRPos());
      DRU.setHLReturnIntoIRPos(cmpPos);
      DRU.setHLReturnIntoOprdIndex(0);

      llvm::dyn_cast<llvm::User>(DRU.toMatchMutant.getIRAt(cmpPos))
          ->setOperand(1, zero);
      llvm::dyn_cast<llvm::CmpInst>(DRU.toMatchMutant.getIRAt(cmpPos))
          ->setPredicate(neqPredHere);
      //\\\\doReplacement (toMatch, resultMuts, repl, DRU.toMatchMutant,
      // posOfIRtoRemove, oprdptr[0], oprdptr[1], dumb);
    }
  }

  void matchAndReplace(MatchStmtIR const &toMatch,
                       llvmMutationOp const &mutationOp,
                       MutantsOfStmt &resultMuts,
                       WholeStmtMutationOnce &iswholestmtmutated,
                       ModuleUserInfos const &MI) {
    MatchUseful mu;
    DoReplaceUseful dru;
    int pos = -1;
    for (auto *val : toMatch.getIRList()) {
      pos++;
      if (matchIRs(toMatch, mutationOp, pos, mu, MI)) {
        for (auto &repl : mutationOp.getMutantReplacorsList()) {
          if (checkWholeStmtAndMutate(toMatch, repl, resultMuts,
                                      iswholestmtmutated, MI)) {
            ; // Do nothing, already mutated
          } else {
            for (MatchUseful const *ptr_mu = mu.first(); ptr_mu != mu.end();
                 ptr_mu = ptr_mu->next()) {
              prepareCloneIRs(toMatch, pos, *ptr_mu, repl, dru, MI);
              try {
                dru.getOrigRelevantIRPos();
              } catch (std::exception &e) {
                llvm::errs() << "didn't set 'OrigRelevantIRPos': " << e.what();
              }

              if (dru.getHLReturnIntoIRPos() < 0) // the replacer is also
                                                  // reational: already mutated
                                                  // into toMatchMutant
                resultMuts.add(/*toMatch, */ dru.toMatchMutant, repl,
                               dru.getOrigRelevantIRPos());
              else
                doReplacement(toMatch, resultMuts, repl, dru.toMatchMutant,
                              dru.posOfIRtoRemove, dru.getHLOprdOrNull(0),
                              dru.getHLOprdOrNull(1), dru.getHLReturningIRPos(),
                              dru.getOrigRelevantIRPos(), MI,
                              dru.getHLReturnIntoIRPos(),
                              dru.getHLReturnIntoOprdIndex());

              // make sure to clear 'dru' for the next replcement
              dru.clearAll();
            }
          }
        }

        // make sure to clear both 'mu' for the next match
        mu.clearAll();
      }
    }
  }

protected:
  static std::map<enum ExpElemKeys, llvm::CmpInst::Predicate> *getPredRelMap() {
    static std::map<enum ExpElemKeys, llvm::CmpInst::Predicate> mrel_IRrel_Map;
    if (mrel_IRrel_Map.empty()) {
      typedef std::pair<enum ExpElemKeys, llvm::CmpInst::Predicate> RelPairType;
      mrel_IRrel_Map.insert(
          RelPairType(mFCMP_FALSE, llvm::CmpInst::FCMP_FALSE));
      mrel_IRrel_Map.insert(RelPairType(mFCMP_OEQ, llvm::CmpInst::FCMP_OEQ));
      mrel_IRrel_Map.insert(RelPairType(mFCMP_OGT, llvm::CmpInst::FCMP_OGT));
      mrel_IRrel_Map.insert(RelPairType(mFCMP_OGE, llvm::CmpInst::FCMP_OGE));
      mrel_IRrel_Map.insert(RelPairType(mFCMP_OLT, llvm::CmpInst::FCMP_OLT));
      mrel_IRrel_Map.insert(RelPairType(mFCMP_OLE, llvm::CmpInst::FCMP_OLE));
      mrel_IRrel_Map.insert(RelPairType(mFCMP_ONE, llvm::CmpInst::FCMP_ONE));
      mrel_IRrel_Map.insert(RelPairType(mFCMP_ORD, llvm::CmpInst::FCMP_ORD));
      mrel_IRrel_Map.insert(RelPairType(mFCMP_UNO, llvm::CmpInst::FCMP_UNO));
      mrel_IRrel_Map.insert(RelPairType(mFCMP_UEQ, llvm::CmpInst::FCMP_UEQ));
      mrel_IRrel_Map.insert(RelPairType(mFCMP_UGT, llvm::CmpInst::FCMP_UGT));
      mrel_IRrel_Map.insert(RelPairType(mFCMP_UGE, llvm::CmpInst::FCMP_UGE));
      mrel_IRrel_Map.insert(RelPairType(mFCMP_ULT, llvm::CmpInst::FCMP_ULT));
      mrel_IRrel_Map.insert(RelPairType(mFCMP_ULE, llvm::CmpInst::FCMP_ULE));
      mrel_IRrel_Map.insert(RelPairType(mFCMP_UNE, llvm::CmpInst::FCMP_UNE));
      mrel_IRrel_Map.insert(RelPairType(mFCMP_TRUE, llvm::CmpInst::FCMP_TRUE));
      mrel_IRrel_Map.insert(RelPairType(mICMP_EQ, llvm::CmpInst::ICMP_EQ));
      mrel_IRrel_Map.insert(RelPairType(mICMP_NE, llvm::CmpInst::ICMP_NE));
      mrel_IRrel_Map.insert(RelPairType(mICMP_UGT, llvm::CmpInst::ICMP_UGT));
      mrel_IRrel_Map.insert(RelPairType(mICMP_UGE, llvm::CmpInst::ICMP_UGE));
      mrel_IRrel_Map.insert(RelPairType(mICMP_ULT, llvm::CmpInst::ICMP_ULT));
      mrel_IRrel_Map.insert(RelPairType(mICMP_ULE, llvm::CmpInst::ICMP_ULE));
      mrel_IRrel_Map.insert(RelPairType(mICMP_SGT, llvm::CmpInst::ICMP_SGT));
      mrel_IRrel_Map.insert(RelPairType(mICMP_SGE, llvm::CmpInst::ICMP_SGE));
      mrel_IRrel_Map.insert(RelPairType(mICMP_SLT, llvm::CmpInst::ICMP_SLT));
      mrel_IRrel_Map.insert(RelPairType(mICMP_SLE, llvm::CmpInst::ICMP_SLE));

      mrel_IRrel_Map.insert(RelPairType(mP_EQ, llvm::CmpInst::ICMP_EQ));
      mrel_IRrel_Map.insert(RelPairType(mP_NE, llvm::CmpInst::ICMP_NE));
      mrel_IRrel_Map.insert(RelPairType(mP_GT, llvm::CmpInst::ICMP_UGT));
      mrel_IRrel_Map.insert(RelPairType(mP_GE, llvm::CmpInst::ICMP_UGE));
      mrel_IRrel_Map.insert(RelPairType(mP_LT, llvm::CmpInst::ICMP_ULT));
      mrel_IRrel_Map.insert(RelPairType(mP_LE, llvm::CmpInst::ICMP_ULE));

      assert(mrel_IRrel_Map.size() == 32 &&
             "Error Inserting some element into mrel_IRrel_Map");
    }

    return &mrel_IRrel_Map;
  }
}; // class NumericAndPointerRelational_Base

} // namespace mart

#endif //__MART_GENMU_operatorClasses__NumericAndPointerRelational_Base__
