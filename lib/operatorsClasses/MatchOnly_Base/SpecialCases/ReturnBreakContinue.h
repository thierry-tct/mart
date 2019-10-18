#ifndef __MART_GENMU_operatorClasses__ReturnBreakContinue__
#define __MART_GENMU_operatorClasses__ReturnBreakContinue__

/**
 * -==== ReturnBreakContinue.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief   Generic abstract base class that match Return, Break and Continue
 * stmts.
 * \todo    Make use of the methods 'matchIRs()' and 'prepareCloneIRs()'.
 */

#include "../MatchOnly_Base.h"

namespace mart {

class ReturnBreakContinue : public MatchOnly_Base {
public:
  bool matchIRs(MatchStmtIR const &toMatch, llvmMutationOp const &mutationOp,
                unsigned pos, MatchUseful &MU, ModuleUserInfos const &MI) {
    llvm::errs() << "Unsuported yet: 'matchIRs' mathod of ReturnBreakContinue "
                    "should not be called \n";
    assert(false);
  }

  void prepareCloneIRs(MatchStmtIR const &toMatch, unsigned pos,
                       MatchUseful const &MU,
                       llvmMutationOp::MutantReplacors const &repl,
                       DoReplaceUseful &DRU, ModuleUserInfos const &MI) {
    llvm::errs() << "Unsuported yet: 'prepareCloneIRs' mathod of "
                    "ReturnBreakContinue should not be called \n";
    assert(false);
  }

  void matchAndReplace(MatchStmtIR const &toMatch,
                       llvmMutationOp const &mutationOp,
                       MutantsOfStmt &resultMuts,
                       WholeStmtMutationOnce &iswholestmtmutated,
                       ModuleUserInfos const &MI) {
    static llvm::Function *prevFunc = nullptr;
    static const llvm::Value *structRetUse = nullptr;
    // Here toMatch should have only one elem for 'br' and for ret, the last
    // elem should be 'ret'
    llvm::Instruction *retbr =
        llvm::dyn_cast<llvm::Instruction>(toMatch.getIRList().back());
    MutantsOfStmt::MutantStmtIR toMatchMutant;
    assert(mutationOp.getMutantReplacorsList().size() <= 2 &&
           "Return, Break and Continue can only be deleted or trapped");
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
    llvm::Function *curFunc = retbr->getParent()->getParent();
#else
    llvm::Function *curFunc = retbr->getFunction();
#endif
    if (prevFunc != curFunc) {
      assert(
          !structRetUse &&
          "Error, Maybe the return struct was not mutated for prev funtion!");
      prevFunc = curFunc;
      // Check whether the function returns a struct
      if (curFunc->getReturnType()->isVoidTy()) {
        for (llvm::Function::const_arg_iterator II = curFunc->arg_begin(),
                                                EE = curFunc->arg_end();
             II != EE; ++II) {
          const llvm::Argument *param = &*II;
          if (param->hasStructRetAttr()) {
            structRetUse = param;
            break;
          }
        }
      }
    }
    if (structRetUse) // 1: bitcast dests, 2: bitcast src, 3: copy src to dest
                      // (llvm.memcpy)
    {
      bool found = false;
      for (auto *ins : toMatch.getIRList())
        if (llvm::isa<llvm::BitCastInst>(ins) &&
            llvm::dyn_cast<llvm::User>(ins)->getOperand(0) == structRetUse)
          found = true;
      if (found) {
        structRetUse = nullptr;
        for (auto &repl : mutationOp.getMutantReplacorsList()) {
          // when delete, doDeleteStmt may call this function but there can't be
          // infinite recursion because here the stmt do not have terminator
          if (checkWholeStmtAndMutate(toMatch, repl, resultMuts,
                                      iswholestmtmutated, MI)) {
            ; // Do nothing, already mutated
          } else
            assert("The replacement for return break and continue should be "
                   "delete stmt");
        }
      }
    }

    if (auto *br = llvm::dyn_cast<llvm::BranchInst>(retbr)) {
      if (br->isConditional())
        return;
      assert(toMatch.getTotNumIRs() == 1 && "unconditional break should be the "
                                            "only instruction in the "
                                            "statement");
      llvm::BasicBlock *targetBB = br->getSuccessor(0);
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
      llvm::Function::iterator FI =
          br->getParent()->getNextNode(); // Iterator();
      if (FI != br->getParent()->getParent()->end()) {
#else
      llvm::Function::iterator FI = br->getParent()->getIterator();
      if (++FI != br->getFunction()->end()) {
#endif
        llvm::BasicBlock *followBB = &*FI;
        if (followBB != targetBB) {
          for (auto &repl : mutationOp.getMutantReplacorsList()) {
            if (isDeletion(repl.getExpElemKey())) {
              toMatchMutant.clear();
              toMatchMutant.setToCloneStmtIROf(toMatch, MI);
              llvm::dyn_cast<llvm::BranchInst>(
                  toMatchMutant.getIRAt(
                      0) /*last Inst(toMatch size here is 1)*/)
                  ->setSuccessor(0, followBB);
              resultMuts.add(
                  /*toMatch, */ toMatchMutant, repl,
                  std::vector<unsigned>(
                      {0}) /*toMatch size here is 1 (see assert above)*/);
              iswholestmtmutated.setDeleted();
            } else if (checkWholeStmtAndMutate(toMatch, repl, resultMuts,
                                               iswholestmtmutated, MI)) {
              ; // Do nothing, already mutated
            } else
              assert("The replacement for return break and continue should be "
                     "delete stmt");
          }
        }
      }
    } else if (auto *ret = llvm::dyn_cast<llvm::ReturnInst>(retbr)) {
      if (ret->getReturnValue() == nullptr) // void type
        return;

      llvm::Type *retType = curFunc->getReturnType();
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 9)
      llvm::IRBuilder<> builder(llvm::getGlobalContext());
#else
      static llvm::LLVMContext getGlobalContext;
      llvm::IRBuilder<> builder(getGlobalContext);
#endif
      // Case of main (delete lead to ret 0 - integer)
      if (curFunc->getName().equals(MI.G_MAIN_FUNCTION_NAME)) {
        assert(retType->isIntegerTy() &&
               "Main function must return an integer");
        for (auto &repl : mutationOp.getMutantReplacorsList()) {
          if (isDeletion(repl.getExpElemKey())) {
            // Skip this if the return value is already 0
            if (auto *rve =
                    llvm::dyn_cast<llvm::ConstantInt>(ret->getReturnValue()))
              if (rve->equalsInt(0))
                continue;
            toMatchMutant.clear(); // w do not clone here, just create new ret
            // llvm::ReturnInst *newret =
            // builder.CreateRet(llvm::ConstantInt::get(retType, 0));
            // toMatchMutant.push_back(newret);
            toMatchMutant.setToCloneStmtIROf(toMatch, MI);
            llvm::dyn_cast<llvm::ReturnInst>(
                toMatchMutant.getIRAt(toMatch.getTotNumIRs() - 1) /*last inst*/)
                ->setOperand(0, llvm::ConstantInt::get(retType, 0));
            std::vector<unsigned> relpos({toMatch.getTotNumIRs() - 1});
            /*std::vector<unsigned> relpos;
                        for (auto i=0; i<toMatch.getTotNumIRs();i++)
                            relpos.push_back(i);*/ // Uncomment this if the deletion
                                            // of the return value means delete
                                            // all that was computed there
                                            // (not: tmp=computation(); return
                                            // tmp; -- but instead: return
                                            // computation();)
            resultMuts.add(/*toMatch, */ toMatchMutant, repl, relpos);
            iswholestmtmutated.setDeleted();
          } else if (checkWholeStmtAndMutate(toMatch, repl, resultMuts,
                                             iswholestmtmutated, MI)) {
            ; // Do nothing, already mutated
          } else
            assert("The replacement for return break and continue should be "
                   "delete stmt");
        }
      } else if (retType->isIntOrIntVectorTy() || retType->isFPOrFPVectorTy() ||
                 retType->isPtrOrPtrVectorTy()) { // Case of primitive and
                                                  // pointer(delete lead to
                                                  // returning of varable value
                                                  // without reaching definition
                                                  // - uninitialized)
        for (auto &repl : mutationOp.getMutantReplacorsList()) {
          if (isDeletion(repl.getExpElemKey())) {
            toMatchMutant.clear();
            toMatchMutant.setToCloneStmtIROf(toMatch, MI);
            llvm::AllocaInst *alloca = builder.CreateAlloca(retType);
            alloca->setAlignment(
                MI.getDataLayout().getPrefTypeAlignment(retType));
            llvm::LoadInst *load = builder.CreateAlignedLoad(
                alloca, MI.getDataLayout().getPrefTypeAlignment(retType));
            // llvm::ReturnInst *newret = builder.CreateRet(load);
            unsigned initialRetPos = toMatch.getTotNumIRs() - 1;
            llvm::dyn_cast<llvm::ReturnInst>(
                toMatchMutant.getIRAt(initialRetPos) /*last inst*/)
                ->setOperand(0, load);
            toMatchMutant.insertIRAt(initialRetPos, load);
            toMatchMutant.insertIRAt(initialRetPos, alloca);
            std::vector<unsigned> relpos({initialRetPos});
            /*std::vector<unsigned> relpos;
                        for (auto i=0; i<toMatch.size();i++)
                            relpos.push_back(i);*/ // Uncomment this if the deletion
                                            // of the return value means delete
                                            // all that was computed there
                                            // (not: tmp=computation(); return
                                            // tmp; -- but instead: return
                                            // computation();)
            resultMuts.add(/*toMatch, */ toMatchMutant, repl, relpos);
            iswholestmtmutated.setDeleted();
          } else if (checkWholeStmtAndMutate(toMatch, repl, resultMuts,
                                             iswholestmtmutated, MI)) {
            ; // Do nothing, already mutated
          } else
            assert("The replacement for return val should be delete stmt");
        }
      }
    }
  }
}; // class ReturnBreakContinue

} // namespace mart

#endif //__MART_GENMU_operatorClasses__ReturnBreakContinue__
