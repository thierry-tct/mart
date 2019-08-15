/**
 * -==== tce.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Define the class TCE which is used to call optimizer and code diff
 */

#ifndef __MART_GENMU_tce__
#define __MART_GENMU_tce__

#include <unordered_map>
#include <vector>

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
#include "llvm/PassManager.h"
#else
#include "llvm/IR/LegacyPassManager.h"
#endif
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
//#include "llvm/Transforms/IPO/InlinerPass.h"
#include "llvm/Transforms/IPO.h"

#include "llvm-diff/DifferenceEngine.h"

namespace mart {

class TCE {
public:
  void optimize(llvm::Module &module, unsigned optLevel = 0 /* 0,1,2,3 */) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    llvm::PassManager mpm;
#else
    llvm::legacy::PassManager mpm;
#endif
    llvm::PassManagerBuilder pmbuilder;
    // mpm.add(llvm::createStripSymbolsPass(true));
    // mpm.add(llvm::createGlobalOptimizerPass());

    pmbuilder.OptLevel = optLevel;
    pmbuilder.SizeLevel = 2;
    // pmbuilder.Inliner = llvm::createFunctionInliningPass(3, 2); //This is for
    // module pass
    pmbuilder.DisableUnitAtATime = false;
    pmbuilder.DisableUnrollLoops = false;
    pmbuilder.LoopVectorize = true;
    pmbuilder.SLPVectorize = true;
    pmbuilder.populateModulePassManager(mpm);
    mpm.run(module);
    /*for (auto &Func: module)
    {
        mpm.run(Func);
    }*/
  }

  void optimize(llvm::Function &func, unsigned optLevel = 0 /* 0,1,2,3 */) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    llvm::FunctionPassManager fpm(func.getParent());
#else
    llvm::legacy::FunctionPassManager fpm(func.getParent());
#endif
    llvm::PassManagerBuilder pmbuilder;
    // fpm.add(llvm::createStripSymbolsPass(true));
    // fpm.add(llvm::createGlobalOptimizerPass());

    pmbuilder.OptLevel = optLevel;
    pmbuilder.SizeLevel = 2;
    // pmbuilder.Inliner = llvm::createFunctionInliningPass(3, 2); //This is for
    // module pass
    pmbuilder.DisableUnitAtATime = false;
    pmbuilder.DisableUnrollLoops = false;
    pmbuilder.LoopVectorize = true;
    pmbuilder.SLPVectorize = true;
    pmbuilder.populateFunctionPassManager(fpm);
    fpm.doInitialization();
    fpm.run(func);
    fpm.doFinalization();
  }

  /**
   *  \brief check module difference
   * @return true if there was a difference between the two functions
   */
  bool moduleDiff(llvm::Module *M1, llvm::Module *M2) {
    llvm::DiffConsumer Consumer;
    llvm::DifferenceEngine Engine(Consumer);

    Engine.diff(M1, M2);

    return Consumer.hadDifferences();
  }

  /**
   *  \brief Check function difference
   * @return true if there was a difference between the two functions
   */
  bool functionDiff(llvm::Function *F1, llvm::Function *F2,
                    llvm::SmallVector<llvm::BasicBlock *, 2> *diffBBs) {
    llvm::DiffConsumer Consumer(diffBBs);
    llvm::DifferenceEngine Engine(Consumer);

    Engine.diff(F1, F2);

    return Consumer.hadDifferences();
  }

  /**
   * \brief Modifies 'diffFuncs2Muts' and 'mutatedFuncsOfMID'
   */
  void checkWithOrig(llvm::Module *clonedOrig, llvm::Module *clonedM,
                     std::vector<llvm::Function *> &mutatedFuncsOfMID) {
    mutatedFuncsOfMID.clear();
    if (clonedOrig->size() == clonedM->size()) {
      for (auto &func : *clonedOrig) {
        if (auto *mF = clonedM->getFunction(func.getName())) {
          if (functionDiff(&func, mF, nullptr)) // function differ, c
          {
            mutatedFuncsOfMID.push_back(&func);
          }
        } else
          assert(false &&
                 "TODO: case the mutation remove and add a function to module");
      }
    } else
      assert(false &&
             "TODO: case the mutation remove or add a function to module");
  }
}; // class TCE

} // namespace mart

#endif //__MART_GENMU_tce__
