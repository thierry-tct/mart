#ifndef __MART_GENMU_operatorClasses__SwitchCases__
#define __MART_GENMU_operatorClasses__SwitchCases__

/**
 * -==== SwitchCases.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief   Generic abstract base class that match Switch and make mutants
 * specific to switch statement.
 * \todo    Make use of the methods 'matchIRs()' and 'prepareCloneIRs()'.
 */

#include "../MatchOnly_Base.h"

namespace mart {

class SwitchCases : public MatchOnly_Base {
private:
  void permute(std::vector<unsigned> const &invect,
               std::vector<std::vector<unsigned>> &permutations) {
    assert(invect.size() == 2 && "For now only permutation of 2 elements "
                                 "(swap) is supported, TODO: implement for any "
                                 "number");
    permutations.emplace_back(std::vector<unsigned>({invect[1], invect[0]}));
  }

  // For the first call, select_size must be at least 2
  void getCombinations(std::vector<unsigned> const &invector,
                       unsigned select_size, unsigned startpos,
                       std::vector<unsigned> workVector,
                       std::vector<std::vector<unsigned>> &combinations) {
    unsigned fin = invector.size();
    assert(select_size <= fin &&
           "selecting more elements than there are in the input vector");
    // assert (select_size > 0 && "select_size must be at least 1");
    if (select_size == 0) {
      if (workVector.size() > 1) {
        std::vector<std::vector<unsigned>> permutations;
        permute(workVector, permutations);
        for (auto &v : permutations) {
          std::vector<unsigned> tmpv(invector);
          for (unsigned i = 0, ie = workVector.size(); i < ie; ++i)
            tmpv[workVector[i]] = invector[v[i]];
          combinations.push_back(tmpv);
        }
      }
    } else {
      if (startpos + select_size < fin)
        getCombinations(invector, select_size, startpos + 1, workVector,
                        combinations);
      fin -= select_size - 1;
      for (unsigned i = startpos; i < fin; ++i) {
        std::vector<unsigned> workVectorCopy(workVector);
        workVectorCopy.push_back(i);
        getCombinations(invector, select_size - 1, i + 1, workVectorCopy,
                        combinations);
      }
    }
  }

  // Ex: 1,2,3,4 with size 2 --> 2,1,3,4; 1,2,4,3;... but not 2,3,1,4 (since
  // more that 2 element are in different postions)
  void generateCombinations(
      llvm::SwitchInst *sw,
      unsigned shuffle_size /*Number of params involved in each shuffle*/,
      std::vector<std::vector<unsigned /*args position*/>> &results) {
    assert(shuffle_size >= 2 && "Cannot shuffle less than 2 elements");
    unsigned numSuccs = sw->getNumSuccessors();
    if (shuffle_size <= numSuccs) {
      assert(shuffle_size == 2 &&
             "shuffle size must be 2"); // Do onlywith 2 to avoid very large
                                        // number of mutants on programs with
                                        // very large number of cases
      std::vector<unsigned> succsindexes;
      for (unsigned i = 0; i < numSuccs; ++i)
        succsindexes.push_back(i);
      for (unsigned i = 0; i < numSuccs; ++i) {
        for (unsigned j = i + 1; j < numSuccs; ++j) {
          if (sw->getSuccessor(i) != sw->getSuccessor(j)) {
            results.push_back(succsindexes);
            std::vector<unsigned> &tmp = results.back();
            tmp[i] = j;
            tmp[j] = i;
          }
        }
      }

      /*getCombinations (succsindexes, shuffle_size, 0, std::vector<unsigned>(),
      results);

      // remove redundancy (when multiple cases have same successor and are
      swaped between each others
      for (long int pos = results.size()-1; pos >= 0; --pos)
      {
          auto &comb = results[pos];
          for (unsigned i=0; i < comb.size();++i)
          {
              if (comb[i] != succsindexes[i] && sw->getSuccessor(comb[i]) ==
      sw->getSuccessor(succsindexes[i]))
              {
                  results.erase(results.begin() + pos);
                  break;
              }
          }
      }*/
    }
  }

public:
  bool matchIRs(MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp,
                unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) {
    llvm::errs() << "Unsuported yet: 'matchIRs' mathod of SwitchCases should "
                    "not be called \n";
    assert(false);
  }

  void prepareCloneIRs(MatchStmtIR const &toMatch, unsigned pos,
                       MatchUseful const &MU,
                       llvmMutationOp::MutantReplacors const &repl,
                       DoReplaceUseful &DRU, ModuleUserInfos const &MI) {
    llvm::errs() << "Unsuported yet: 'prepareCloneIRs' mathod of SwitchCases "
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
      if (llvm::SwitchInst *sw = llvm::dyn_cast<llvm::SwitchInst>(val)) {
        for (auto &repl : mutationOp.getMutantReplacorsList()) {
          if (checkWholeStmtAndMutate(toMatch, repl, resultMuts,
                                      iswholestmtmutated, MI)) {
            ; // Do nothing, already mutated
          } else if (repl.getExpElemKey() == mREMOVE_CASES) {
            assert(llvmMutationOp::isSpecifiedConstIndex(
                repl.getOprdIndexList()[0])); // the number of elements shuffled
                                              // each time
            unsigned numRemCases = std::stol(
                llvmMutationOp::getConstValueStr(repl.getOprdIndexList()[0]));
            assert(
                numRemCases >= 1 &&
                "the specified number of cases to remove must be at least 1");
            assert(numRemCases == 1 && "For now we only support remove 1 case "
                                       "at the time. TODO: add support for "
                                       "more");

#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            for (auto casei = sw->case_begin(), caseE = sw->case_end();
                 casei != caseE; ++casei) {
// auto &casei = *caseIt;
#else
            for (auto &casei : sw->cases()) {
#endif
              if (casei.getCaseSuccessor() == sw->getDefaultDest())
                continue;

              llvm::ConstantInt *caseval = casei.getCaseValue();
              toMatchMutant.clear();
              toMatchMutant.setToCloneStmtIROf(toMatch, MI);
              llvm::SwitchInst *clonesw =
                  llvm::dyn_cast<llvm::SwitchInst>(toMatchMutant.getIRAt(pos));
#if (LLVM_VERSION_MAJOR <= 4)
              clonesw->findCaseValue(caseval).setSuccessor(
                  clonesw->getDefaultDest());
#else
              (*(clonesw->findCaseValue(caseval))).setSuccessor(
                  clonesw->getDefaultDest());
#endif

              resultMuts.add(/*toMatch, */ toMatchMutant, repl,
                             std::vector<unsigned>({pos}));
            }
          } else if (repl.getExpElemKey() == mSHUFFLE_CASE_DESTS) {
            std::vector<std::vector<unsigned>> combinations;
            assert(llvmMutationOp::isSpecifiedConstIndex(
                repl.getOprdIndexList()[0])); // the number of elements shuffled
                                              // each time
            unsigned shuffle_size = std::stol(
                llvmMutationOp::getConstValueStr(repl.getOprdIndexList()[0]));
            assert(shuffle_size >= 2 &&
                   "the specified shuffle size must be at least 2");
            generateCombinations(sw, shuffle_size, combinations);
            unsigned succNum = sw->getNumSuccessors();
            std::vector<llvm::BasicBlock *> initialcaseSequence;
            for (auto &bbspos : combinations) {
              toMatchMutant.clear();
              toMatchMutant.setToCloneStmtIROf(toMatch, MI);
              llvm::SwitchInst *clonesw =
                  llvm::dyn_cast<llvm::SwitchInst>(toMatchMutant.getIRAt(pos));
              initialcaseSequence.clear();
              for (unsigned i = 0; i < succNum; ++i)
                initialcaseSequence.push_back(clonesw->getSuccessor(i));
              for (unsigned i = 0; i < succNum; ++i)
                clonesw->setSuccessor(i, initialcaseSequence[bbspos[i]]);

              resultMuts.add(/*toMatch, */ toMatchMutant, repl,
                             std::vector<unsigned>({pos}));
            }
          } else {
            assert(false && "Error: SWITCH should have only 'REMOVECASE' and "
                            "'SHUFFLECASES' as replacors");
          }
        }
      }
    }
  }
}; // class SwitchCases

} // namespace mart

#endif //__MART_GENMU_operatorClasses__SwitchCases__
