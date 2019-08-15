#ifndef __MART_GENMU_operatorClasses__Logical_Base__
#define __MART_GENMU_operatorClasses__Logical_Base__

/**
 * -==== Logical_Base.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all logical mutation operators.
 * \details   This abstract class define is extended from the Generic base
 * class.
 * \todo      Rework the design to have 'and' and 'or' separated and make use of
 * matchIRs and prepareClone
 * \todo      Add support for a&&b --> b&&a, as well as a||b --> b||a, a&&b -->
 * b||a ...
 * \todo      Add support for a&&b --> !a, a&&b --> !b ...
 */

#include <queue>
#include <set>

#include "../GenericMuOpBase.h"

namespace mart {

class Logical_Base : public GenericMuOpBase {
public:
  bool matchIRs(MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp,
                unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) {
    llvm::errs() << "Unsuported yet: 'matchIRs' mathod of Logical should not "
                    "be called \n";
    assert(false);
  }

  void prepareCloneIRs(MatchStmtIR const &toMatch, unsigned pos,
                       MatchUseful const &MU,
                       llvmMutationOp::MutantReplacors const &repl,
                       DoReplaceUseful &DRU, ModuleUserInfos const &MI) {
    llvm::errs() << "Unsuported yet: 'prepareCloneIRs' mathod of Logical "
                    "should not be called \n";
    assert(false);
  }

  void matchAndReplace(MatchStmtIR const &toMatch,
                       llvmMutationOp const &mutationOp,
                       MutantsOfStmt &resultMuts,
                       WholeStmtMutationOnce &iswholestmtmutated,
                       ModuleUserInfos const &MI) {
    MutantsOfStmt::MutantStmtIR toMatchMutant;
    int pos = -1;
    for (auto *val : toMatch.getIRList()) {
      pos++;

      /// MATCH
      if (llvm::BranchInst *br = llvm::dyn_cast<llvm::BranchInst>(val)) {
        if (br->isUnconditional())
          continue;

        /// No need to checktype because logical is always on boolean
        // if (!(checkCPTypeInIR (mutationOp.getCPType(0), br->getCondition()))
        //    continue;

        assert(br->getNumSuccessors() == 2 &&
               "conditional break should have 2 successors");

        llvm::BasicBlock *thenBB = nullptr;
        llvm::BasicBlock *elseBB = nullptr;
        llvm::BasicBlock *shortCircuitBB = nullptr;
        int shortCircuitSID = -1;
        int thenSID = -1;
        int elseSID = -1;
        if (mutationOp.getMatchOp() == mAND) {
          shortCircuitBB = br->getSuccessor(0);
          shortCircuitSID = 0;
          elseBB = br->getSuccessor(1);
          elseSID = 1;
        } else if (mutationOp.getMatchOp() == mOR) {
          shortCircuitBB = br->getSuccessor(1);
          shortCircuitSID = 1;
          thenBB = br->getSuccessor(0);
          thenSID = 0;
        } else {
          assert(false && "Unreachable: unexpected Instr (expect mAND or mOR)");
        }

        std::vector<llvm::BranchInst *> brs;
        std::queue<llvm::BasicBlock *> bbtosee;
        std::set<llvm::BasicBlock *> seenbb;

        bbtosee.push(shortCircuitBB);
        seenbb.insert(shortCircuitBB);
        while (!bbtosee.empty()) {
          auto *curBB = bbtosee.front();
          bbtosee.pop();
          for (auto &popInstr : *curBB) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            for (llvm::Value::use_iterator ui = popInstr.use_begin(),
                                           ue = popInstr.use_end();
                 ui != ue; ++ui) {
              auto &U = ui.getUse();
#else
            for (auto &U : popInstr.uses()) {
#endif
              llvm::Instruction *uInst =
                  llvm::dyn_cast<llvm::Instruction>(U.getUser());
              assert(uInst &&
                     "Wrong user in module (user is not of type Instruction)");
              if (seenbb.count(uInst->getParent()) == 0) {
                seenbb.insert(uInst->getParent());
                bbtosee.push(uInst->getParent());
              }
              if (auto *buInst = llvm::dyn_cast<llvm::BranchInst>(uInst)) {
                brs.push_back(buInst);
              }
            }
          }
        }
        llvm::BranchInst *searchedBr = nullptr;
        for (auto *brF : brs) {
          if (brF->isUnconditional()) {
            if (seenbb.count(brF->getSuccessor(0)) == 0) {
              searchedBr = nullptr;
              break;
            }
          } else {
            if (seenbb.count(brF->getSuccessor(0)) == 0 &&
                seenbb.count(brF->getSuccessor(1)) == 0) {
              if (searchedBr) {
                searchedBr = nullptr;
                break;
              } else
                searchedBr = brF;
            } else if (seenbb.count(brF->getSuccessor(0)) == 0 ||
                       seenbb.count(brF->getSuccessor(1)) == 0) {
              searchedBr = nullptr;
              break;
            }
          }
        }

        if (!searchedBr)
          continue;

        /// No need to checktype because logical is always on boolean
        // if (!(checkCPTypeInIR (mutationOp.getCPType(1),
        // searchedBr->getCondition()))
        //    continue;

        if (mutationOp.getMatchOp() == mAND) {
          if (elseBB == searchedBr->getSuccessor(1))
            thenBB = searchedBr->getSuccessor(0);
          // else if (elseBB == searchedBr->getSuccessor(0))
          //    thenBB = searchedBr->getSuccessor(1);
          else
            continue;
        } else if (mutationOp.getMatchOp() == mOR) {
          if (thenBB == searchedBr->getSuccessor(0))
            elseBB = searchedBr->getSuccessor(1);
          // else if (thenBB == searchedBr->getSuccessor(1))
          //    elseBB = searchedBr->getSuccessor(0);
          else
            continue;
        } else {
          assert(false && "Unreachable: unexpected Instr (expect mAND or mOR)");
        }

        for (auto &repl : mutationOp.getMutantReplacorsList()) {
          toMatchMutant.clear();
          if (checkWholeStmtAndMutate(toMatch, repl, resultMuts,
                                      iswholestmtmutated, MI)) {
            ; // Do nothing, already mutated
          } else {
            toMatchMutant.setToCloneStmtIROf(toMatch, MI);
            if (repl.getExpElemKey() == mAND || repl.getExpElemKey() == mOR ||
                repl.getExpElemKey() == mKEEP_ONE_OPRD ||
                repl.getExpElemKey() == mCONST_VALUE_OF) { // directly replace
              // NO CALL TO doReplacement HERE
              llvm::BranchInst *clonedBr =
                  llvm::dyn_cast<llvm::BranchInst>(toMatchMutant.getIRAt(pos));
              switch (repl.getExpElemKey()) {
              case mAND: {
                assert(mutationOp.getMatchOp() == mOR &&
                       "only 'or' can change to 'and' here");
                clonedBr->setSuccessor(shortCircuitSID, elseBB);
                clonedBr->setSuccessor(thenSID, shortCircuitBB);
                break;
              }
              case mOR: {
                assert(mutationOp.getMatchOp() == mAND &&
                       "only 'and' can change to 'or' here");
                clonedBr->setSuccessor(shortCircuitSID, thenBB);
                clonedBr->setSuccessor(elseSID, shortCircuitBB);
                break;
              }
              case mKEEP_ONE_OPRD: {
                if (mutationOp.getMatchOp() == mAND) {
                  if (repl.getOprdIndexList()[0] == 0)
                    clonedBr->setSuccessor(shortCircuitSID, thenBB);
                  else if (repl.getOprdIndexList()[0] == 1)
                    clonedBr->setSuccessor(elseSID, shortCircuitBB);
                  else
                    assert(false && "Invalid arg for mKEEP_ONE_OPRD when AND");
                } else if (mutationOp.getMatchOp() == mOR) {
                  if (repl.getOprdIndexList()[0] == 0)
                    clonedBr->setSuccessor(shortCircuitSID, elseBB);
                  else if (repl.getOprdIndexList()[0] == 1)
                    clonedBr->setSuccessor(thenSID, shortCircuitBB);
                  else
                    assert(false && "Invalid arg for mKEEP_ONE_OPRD when OR");
                } else
                  assert(false &&
                         "Unreachable -- Or and And only supported here");
                break;
              }
              case mCONST_VALUE_OF: {
                bool const_bool_val = (std::stod(
                                            llvmMutationOp::getConstValueStr(
                                            repl.getOprdIndexList()[0])
                                        ) != 0);
                if (const_bool_val) {
                  clonedBr->setSuccessor(shortCircuitSID, thenBB);
                  if (mutationOp.getMatchOp() == mAND)
                    clonedBr->setSuccessor(elseSID, thenBB);
                  else
                    assert(mutationOp.getMatchOp() == mOR && 
                                            "Invalid arg for CONST_VALUE_OF");
                } else {
                  clonedBr->setSuccessor(shortCircuitSID, elseBB);
                  if (mutationOp.getMatchOp() == mOR)
                    clonedBr->setSuccessor(thenSID, elseBB);
                  else
                    assert(mutationOp.getMatchOp() == mAND && 
                                            "Invalid arg for CONST_VALUE_OF");
                }
                break;
              }
              default:
                assert(false && "Unreachable");
              }
              std::vector<unsigned> relevantPosInToMatch(
                  {pos}); // Only pos because is was the only added
              resultMuts.add(/*toMatch, */ toMatchMutant, repl,
                             relevantPosInToMatch);
              toMatchMutant.clear();
            } else {
              llvm::errs() << "repl.getExpElemKey() is " 
                           << repl.getExpElemKey() << "\n";
              assert(false && "Unsuported yet");
            }
          }
        }
      }
    }
  }
};
}

#endif //__MART_GENMU_operatorClasses__Logical_Base__
