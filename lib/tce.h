#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
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

class TCE {
public:
    void optimize(llvm::Module &module)
    {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
        llvm::PassManager fpm;
#else
        llvm::legacy::PassManager fpm;
#endif
        llvm::PassManagerBuilder pmbuilder;
        //fpm.add(llvm::createStripSymbolsPass(true));
        //fpm.add(llvm::createGlobalOptimizerPass());

        pmbuilder.OptLevel = 3;
        pmbuilder.SizeLevel = 2;
        //pmbuilder.Inliner = llvm::createFunctionInliningPass(3, 2); //This is for module pass
        pmbuilder.DisableUnitAtATime = false;
        pmbuilder.DisableUnrollLoops = false;
        pmbuilder.LoopVectorize = true;
        pmbuilder.SLPVectorize = true;
        pmbuilder.populateModulePassManager(fpm);
        fpm.run(module);
        /*for (auto &Func: module)
        {
            fpm.run(Func); 
        }*/
    }
    
    bool moduleDiff (llvm::Module *M1, llvm::Module *M2)
    {
        llvm::DiffConsumer Consumer;
        llvm::DifferenceEngine Engine(Consumer);
        
        Engine.diff(M1, M2);

        return Consumer.hadDifferences();
    }
}; 
