#ifndef __MART_GENMU_operatorClasses__FunctionCall__
#define __MART_GENMU_operatorClasses__FunctionCall__

/**
 * -==== FunctionCall.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief   Generic abstract base class that match Specific function call
 * callee.
 * \todo    Extend to support calls to 'Invoke' and make use of the methods
 * 'matchIRs()' and 'prepareCloneIRs()'.
 */

#include "../MatchOnly_Base.h"

namespace mart {

class FunctionCall : public MatchOnly_Base {
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
      llvm::CallInst *call,
      unsigned shuffle_size /*Number of params involved in each shuffle*/,
      std::vector<std::vector<unsigned /*args position*/>> &results) {
    assert(shuffle_size >= 2 && "Cannot shuffle less than 2 elements");
    unsigned numArgs = call->getNumArgOperands();
    if (shuffle_size <= numArgs) {
      std::unordered_set<unsigned> visitedargs;
      for (unsigned i = 0; i < numArgs; ++i) {
        if (visitedargs.insert(i).second) {
          llvm::Type *i_type = call->getArgOperand(i)->getType();
          std::vector<unsigned> sametypeargs;
          sametypeargs.push_back(i);
          for (auto j = i + 1; j < numArgs; ++j)
            if (i_type == call->getArgOperand(j)->getType()) {
              sametypeargs.push_back(j);
              visitedargs.insert(j);
            }
          if (sametypeargs.size() >= shuffle_size) {
            std::vector<std::vector<unsigned>> combinations;
            getCombinations(sametypeargs, shuffle_size, 0,
                            std::vector<unsigned>(), combinations);
            for (auto &comb : combinations) {
              std::vector<unsigned> tmpv;
              for (unsigned k = 0; k < numArgs; ++k)
                tmpv.push_back(k);
              for (unsigned k = 0; k < sametypeargs.size(); ++k)
                tmpv[sametypeargs[k]] = comb[k];
              results.push_back(tmpv);
            }
          }
        }
      }
    }
  }

public:
  bool matchIRs(MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp,
                unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) {
    llvm::errs() << "Unsuported yet: 'matchIRs' mathod of FunctionCall should "
                    "not be called \n";
    assert(false);
  }

  void prepareCloneIRs(MatchStmtIR const &toMatch, unsigned pos,
                       MatchUseful const &MU,
                       llvmMutationOp::MutantReplacors const &repl,
                       DoReplaceUseful &DRU, ModuleUserInfos const &MI) {
    llvm::errs() << "Unsuported yet: 'prepareCloneIRs' mathod of FunctionCall "
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
      if (llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(val)) {
        llvm::Function *cf = call->getCalledFunction();

        // The case of call through function pointer: we can't directly get the
        // called function as this may be dynamically assigned.
        // XXX: for now just skip it.  TODO: check for the function alias if it
        // is same then fix it
        // if(!cf)
        //    continue;

        for (auto &repl : mutationOp.getMutantReplacorsList()) {
          if (checkWholeStmtAndMutate(toMatch, repl, resultMuts,
                                      iswholestmtmutated, MI)) {
            ; // Do nothing, already mutated
          } else if (repl.getExpElemKey() == mNEWCALLEE) {
            assert(llvmMutationOp::isSpecifiedConstIndex(
                repl.getOprdIndexList()
                    [0])); // repl.getOprdIndexList()[0,1,...] should be >
                           // llvmMutationOp::maxOprdNum
            if (cf &&
                cf->getName() == llvmMutationOp::getConstValueStr(
                                     repl.getOprdIndexList()
                                         [0])) //'repl.getOprdIndexList()[0]' is
                                               // it the function to match?
            {
              for (auto i = 1; i < repl.getOprdIndexList().size(); i++) {
                toMatchMutant.clear();
                toMatchMutant.setToCloneStmtIROf(toMatch, MI);
                llvm::Function *repFun = MI.getModule()->getFunction(
                    llvmMutationOp::getConstValueStr(
                        repl.getOprdIndexList()[i]));
                if (!repFun) {
                  llvm::errs()
                      << "\n# Error: Replacing function not found in module: "
                      << llvmMutationOp::getConstValueStr(
                             repl.getOprdIndexList()[i])
                      << "\n";
                  assert(false);
                }
                // Make sure that the two functions are compatible
                if (cf->getFunctionType() != repFun->getFunctionType()) {
                  llvm::errs() << "\n#Error: the function to be replaced and "
                                  "de replacing are of different types:"
                               << cf->getName() << " and " << repFun->getName()
                               << "\n";
                  assert(false);
                }
                llvm::dyn_cast<llvm::CallInst>(toMatchMutant.getIRAt(pos))
                    ->setCalledFunction(repFun);
                resultMuts.add(/*toMatch, */ toMatchMutant, repl,
                               std::vector<unsigned>({pos}));
              }
            }
          } else if (repl.getExpElemKey() == mSHUFFLE_ARGS) {
            std::vector<std::vector<unsigned>> combinations;
            assert(llvmMutationOp::isSpecifiedConstIndex(
                repl.getOprdIndexList()[0])); // the number of elements shuffled
                                              // each time
            unsigned shuffle_size = std::stol(
                llvmMutationOp::getConstValueStr(repl.getOprdIndexList()[0]));
            assert(shuffle_size >= 2 &&
                   "the specified shuffle size must be at least 2");
            generateCombinations(call, shuffle_size, combinations);
            unsigned argsNum = call->getNumArgOperands();
            std::vector<llvm::Value *> initialargsSequence;
            for (auto &argspos : combinations) {
              toMatchMutant.clear();
              toMatchMutant.setToCloneStmtIROf(toMatch, MI);
              llvm::CallInst *clonecall =
                  llvm::dyn_cast<llvm::CallInst>(toMatchMutant.getIRAt(pos));
              initialargsSequence.clear();
              for (unsigned i = 0; i < argsNum; ++i)
                initialargsSequence.push_back(clonecall->getArgOperand(i));
              for (unsigned i = 0; i < argsNum; ++i)
                clonecall->setArgOperand(i, initialargsSequence[argspos[i]]);

              resultMuts.add(/*toMatch, */ toMatchMutant, repl,
                             std::vector<unsigned>({pos}));
            }
          } else {
            assert(false && "Error: CALL should have only 'NEWCALLEE' or "
                            "'SHUFFLEARGS' as replacors");
          }
        }
      }
    }
  }
}; // class FunctionCall

} // namespace mart

#endif //__MART_GENMU_operatorClasses__FunctionCall__
