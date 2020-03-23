/**
 * -==== mutation.cpp
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Implementation of Mutation class
 */

#include <fstream>
#include <queue>
#include <regex>
#include <set>
#include <sstream>
#include <stack>
#include <tuple>
#include <vector>

#include "ReadWriteIRObj.h"

#include "mutation.h"
#include "operatorsClasses/GenericMuOpBase.h"
#include "tce.h" //Trivial Compiler Equivalence
#include "typesops.h"
#include "usermaps.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Type.h"
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
// This is needed for 3rd param of SplitBlock
#include "llvm/Analysis/RegionInfo.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Linker.h"      //for Linker
#include "llvm/PassManager.h" //This is needed for 3rd param of SplitBlock
#else
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h" //for Linker
#endif
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"

#include "llvm/Transforms/Utils/Cloning.h" //for CloneModule

// llvm::GetSuccessorNumber and  llvm::GetSuccessorNumber
#include "llvm/Analysis/CFG.h"
// for llvm::DemotePHIToStack   used to remove PHI nodes after mutation
#include "llvm/Transforms/Utils/Local.h"

#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
// llvm::StripDebugInfo (llvm::Module &M)   //remove all debugging infos
#include "llvm/DebugInfo.h"
#else
// llvm::StripDebugInfo (llvm::Module &M)   //remove all debugging infos
// (llvm/IR/DebugInfoMetadata.h)
#include "llvm/IR/DebugInfo.h"
#endif

// llvm::DbgInfoIntrinsic, to check for llvm.dbg.declare and llvm.dbg.value
#include "llvm/IR/IntrinsicInst.h"

/*#ifdef ENABLE_OPENMP_PARALLEL
#include "omp.h"
#endif*/

using namespace mart;

#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
static bool oldVersionIsEHPad(llvm::Instruction const *Inst) {
  switch (Inst->getOpcode()) {
  // case llvm::Instruction::CatchSwitch:
  // case llvm::Instruction::CatchPad:
  // case llvm::Instruction::CleanupPad:
  case llvm::Instruction::LandingPad:
    return true;
  default:
    return false;
  }
}
#endif

static bool isWMSafe(llvm::Instruction *Inst, bool &canExcept) {
  static std::unordered_set<unsigned /*opcode*/> otherSafeOpCodes;
  static std::unordered_set<unsigned /*opcode*/> canExceptInsts;
  if (otherSafeOpCodes.empty()) {
    otherSafeOpCodes.insert(llvm::Instruction::ICmp);
    otherSafeOpCodes.insert(llvm::Instruction::FCmp);
    otherSafeOpCodes.insert(llvm::Instruction::PHI);
    otherSafeOpCodes.insert(llvm::Instruction::Select);
    otherSafeOpCodes.insert(llvm::Instruction::ExtractElement);
    otherSafeOpCodes.insert(llvm::Instruction::Select);
    otherSafeOpCodes.insert(llvm::Instruction::ExtractValue);
    otherSafeOpCodes.insert(llvm::Instruction::Load);
    otherSafeOpCodes.insert(llvm::Instruction::GetElementPtr);

    canExceptInsts.insert(llvm::Instruction::ExtractElement); // null pointer
    canExceptInsts.insert(llvm::Instruction::ExtractValue);   // null pointer
    canExceptInsts.insert(llvm::Instruction::Load);           // null pointer
    // canExceptInsts.insert(llvm::Instruction::GetElementPtr); //actually can't
    canExceptInsts.insert(llvm::Instruction::AddrSpaceCast); // null pointer
    canExceptInsts.insert(llvm::Instruction::UDiv);          // division by 0
    canExceptInsts.insert(llvm::Instruction::SDiv);          // division by 0
    canExceptInsts.insert(llvm::Instruction::URem);          // division by 0
    canExceptInsts.insert(llvm::Instruction::SRem);          // division by 0
  }

  if (Inst->isBinaryOp() || Inst->isCast() ||
      (otherSafeOpCodes.count(Inst->getOpcode()) > 0)) {
    canExcept = (canExceptInsts.count(Inst->getOpcode()) > 0);
    return true;
  }
  canExcept = true;
  return false;
}

static void
getWMUnsafeInstructions(llvm::BasicBlock *BB,
                        std::vector<llvm::Instruction *> &unsafeInsts) {
  bool canExcept;
  for (auto &Inst : *BB) {
    if (!isWMSafe(&Inst, canExcept)) {
      unsafeInsts.push_back(&Inst);
    } else if (canExcept) { // For now we consider can except as unsafe
      unsafeInsts.push_back(&Inst);
    }
  }
}

Mutation::Mutation(llvm::Module &module, std::string mutConfFile,
                   DumpMutFunc_t writeMutsF, std::string scopeJsonFile)
    : forKLEESEMu(true), funcForKLEESEMu(nullptr),
      writeMutantsCallback(writeMutsF), moduleInfo(&module, &usermaps) {
  // tranform the PHI Node with any non-constant incoming value with reg2mem
  preprocessVariablePhi(module);

  // set module
  currentInputModule = &module;
  // for now the input is transformed (mutated to become mutant)
  currentMetaMutantModule = currentInputModule;

  // get mutation config (operators)
  assert(getConfiguration(mutConfFile) &&
         "@Mutation(): getConfiguration(mutconfFile) Failed!");

  // Get scope info
  mutationScope.Initialize(module, scopeJsonFile);

  // initialize mutantIDSelectorName
  getanothermutantIDSelectorName();
  curMutantID = 0;

  // initialize postMutationPointFuncName
  getanotherPostMutantPointFuncName();
}

/**
 * \brief PREPROCESSING - Remove PHI Nodes, replacing by reg2mem, for every
 * function in module
 */
void Mutation::preprocessVariablePhi(llvm::Module &module) {
  // Replace the PHI node with memory, to avoid error with verify, as it don't
  // get the relation of mutant ID...
  for (auto &Func : module) {
    if (skipFunc(Func))
      continue;

    llvm::CastInst *AllocaInsertionPoint = nullptr;
    llvm::BasicBlock *BBEntry = &(Func.getEntryBlock());
    llvm::BasicBlock::iterator I = BBEntry->begin();
    while (llvm::isa<llvm::AllocaInst>(I)) {
      // needed because of array size of alloca demote to mem
      if (llvm::dyn_cast<llvm::AllocaInst>(I)->isArrayAllocation())
        break;
      ++I;
    }

    AllocaInsertionPoint = new llvm::BitCastInst(
        llvm::Constant::getNullValue(llvm::Type::getInt32Ty(Func.getContext())),
        llvm::Type::getInt32Ty(Func.getContext()), "my reg2mem alloca point",
        &*I);

    std::vector<llvm::PHINode *> phiNodes;
    for (auto &bb : Func)
      for (auto &instruct : bb)
        if (auto *phiN = llvm::dyn_cast<llvm::PHINode>(&instruct))
          phiNodes.push_back(phiN);
    for (auto it = phiNodes.rbegin(), ie = phiNodes.rend(); it != ie; ++it) {
      auto *phiN = *it;
      bool hasNonConstIncVal = true; /*false;
      for (unsigned pind=0, pe=phiN->getNumIncomingValues(); pind < pe; ++pind)
      {
          if (! llvm::isa<llvm::Constant>(phiN->getIncomingValue(pind)))
          {
              hasNonConstIncVal = true;
              break;
          }
      }*/
      if (hasNonConstIncVal) {
        /***if (! AllocaInsertionPoint)
        {
            llvm::BasicBlock * BBEntry = &(Func.getEntryBlock());
            llvm::BasicBlock::iterator I = BBEntry->begin();
            while (llvm::isa<llvm::AllocaInst>(I)) ++I;

            AllocaInsertionPoint = new llvm::BitCastInst(
               llvm::Constant::getNullValue(llvm::Type::getInt32Ty(Func.getContext())),
               llvm::Type::getInt32Ty(Func.getContext()), "my reg2mem alloca
        point", &*I);
        }****/
        auto *allocaPN = MYDemotePHIToStack(phiN, AllocaInsertionPoint);
        if (!allocaPN) {
          llvm::errs() << "warn: Failed to transform phi node (Maybe PHI Node "
                          "has no 'uses')\n";
          // assert(false);
        }
      }
    }

    // Now take care of the values used on different Blocks and alloca's array
    // size values: there is no more Phi Nodes
    std::vector<llvm::Instruction *> crossBBInsts;
    for (auto &bb : Func) {
      for (auto &instruct : bb) {
        if (auto *alloca = llvm::dyn_cast<llvm::AllocaInst>(&instruct)) {
          if (alloca->isArrayAllocation()) {
            llvm::Value *alloc_arr_size = alloca->getArraySize();
            if (llvm::isa<llvm::LoadInst>(alloc_arr_size)) // already good
              continue;

            llvm::AllocaInst *Slot = new llvm::AllocaInst(
                alloc_arr_size->getType(), 
#if (LLVM_VERSION_MAJOR >= 5)
                0, // Adress Space default value
#endif
                nullptr,
                alloc_arr_size->getName() + ".reg2mem", AllocaInsertionPoint);
            llvm::Instruction *loadinsertpt;
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
            loadinsertpt =
                llvm::isa<llvm::Constant>(alloc_arr_size)
                    ? alloca
                    : ++(llvm::BasicBlock::iterator(*(
                          llvm::dyn_cast<llvm::Instruction>(alloc_arr_size))));
#else
            loadinsertpt =
                llvm::isa<llvm::Constant>(alloc_arr_size)
                    ? alloca
                    : &*(++((llvm::dyn_cast<llvm::Instruction>(alloc_arr_size))
                                ->getIterator()));
#endif
            new llvm::StoreInst(alloc_arr_size, Slot, loadinsertpt);
            llvm::Value *V =
                new llvm::LoadInst(Slot, alloc_arr_size->getName() + ".reload",
                                   false /*VolatileLoads*/, alloca);
            alloca->setOperand(0, V); // set array size
          }

          continue;
        }

#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
        for (llvm::Value::use_iterator ui = instruct.use_begin(),
                                       ue = instruct.use_end();
             ui != ue; ++ui) {
          auto &Usr = ui.getUse();
#else
        for (auto &Usr : instruct.uses()) {
#endif
          llvm::Instruction *U =
              llvm::dyn_cast<llvm::Instruction>(Usr.getUser());
          if (U->getParent() != &bb) {
            crossBBInsts.push_back(&instruct);
            break;
          }
        }
      }
    }
    for (auto *Inst : crossBBInsts)
      MyDemoteRegToStack(*Inst, false /*volitile?*/, AllocaInsertionPoint);

    if (AllocaInsertionPoint)
      AllocaInsertionPoint->eraseFromParent();

    Mutation::checkFunctionValidity(
        Func, "ERROR: PreProcessing ERROR: Preprocessing Broke Function!");
  }
}

/// DemotePHIToStack - This function takes a virtual register computed by a PHI
/// node and replaces it with a slot in the stack frame allocated via alloca.
/// The PHI node is deleted. It returns the pointer to the alloca inserted.
/// @MART: edited from "llvm/lib/Transforms/Utils/DemoteRegToStack.cpp"
/// TODO: @todo Update this as LLVM evolve
llvm::AllocaInst *Mutation::MYDemotePHIToStack(llvm::PHINode *P,
                                               llvm::Instruction *AllocaPoint) {
  if (P->use_empty()) {
    P->eraseFromParent();
    return nullptr;
  }

  // Create a stack slot to hold the value.
  llvm::AllocaInst *Slot;
  if (AllocaPoint) {
    Slot = new llvm::AllocaInst(P->getType(), 
#if (LLVM_VERSION_MAJOR >= 5)
                                0, // Adress Space default value
#endif
                                nullptr,
                                P->getName() + ".reg2mem", AllocaPoint);
  } else {
    /*llvm::Function *F = P->getParent()->getParent();
    Slot = new llvm::AllocaInst(P->getType(), nullptr, P->getName() +
    ".reg2mem",
                          &F->getEntryBlock().front());*/
    assert(false && "must have non-null insertion point, which if after all "
                    "allocates of entry BB");
  }

  // Iterate over each operand inserting a store in each predecessor.
  for (unsigned i = 0, e = P->getNumIncomingValues(); i < e; ++i) {
    if (llvm::InvokeInst *II =
            llvm::dyn_cast<llvm::InvokeInst>(P->getIncomingValue(i))) {
      assert(II->getParent() != P->getIncomingBlock(i) &&
             "Invoke edge not supported yet");
      (void)II;
    }
    llvm::Instruction *storeInsertPoint = nullptr;
    llvm::Value *incomingValue = P->getIncomingValue(i);
    llvm::BasicBlock *incomingBlock = P->getIncomingBlock(i);
    // avoid atomicity problem of high level stmts
    if ((storeInsertPoint = llvm::dyn_cast<llvm::Instruction>(incomingValue)) &&
        (storeInsertPoint->getParent() == incomingBlock)) {
      storeInsertPoint = storeInsertPoint->getNextNode();
      assert(storeInsertPoint && "storeInsertPoint is null");
    } else // if (llvm::isa<llvm::Constant>(incomingValue) || incomingBlock !=
           // incomingValue->getParent())
    {
      if (AllocaPoint->getParent() == incomingBlock)
        storeInsertPoint = AllocaPoint;
      else
        storeInsertPoint = &*(incomingBlock->begin());
    }
    new llvm::StoreInst(incomingValue, Slot, storeInsertPoint);
    // P->getIncomingBlock(i)->getTerminator());
  }

// Insert a load in place of the PHI and replace all uses.
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
  llvm::BasicBlock::iterator InsertPt = llvm::BasicBlock::iterator(P);

  for (; llvm::isa<llvm::PHINode>(InsertPt) || oldVersionIsEHPad(InsertPt);
       ++InsertPt)
    /* empty */; // Don't insert before PHI nodes or landingpad instrs.
#else
  llvm::BasicBlock::iterator InsertPt = P->getIterator();

  for (; llvm::isa<llvm::PHINode>(InsertPt) || InsertPt->isEHPad(); ++InsertPt)
    /* empty */; // Don't insert before PHI nodes or landingpad instrs.
#endif

  llvm::Value *V =
      new llvm::LoadInst(Slot, P->getName() + ".reload", &*InsertPt);
  P->replaceAllUsesWith(V);

  // Delete PHI.
  P->eraseFromParent();
  return Slot;
}

/// DemoteRegToStack - This function takes a virtual register computed by an
/// Instruction and replaces it with a slot in the stack frame, allocated via
/// alloca.  This allows the CFG to be changed around without fear of
/// invalidating the SSA information for the value.  It returns the pointer to
/// the alloca inserted to create a stack slot for I.
/// @MART: edited from "llvm/lib/Transforms/Utils/DemoteRegToStack.cpp"
/// TODO: @todo Update this as LLVM evolve
llvm::AllocaInst *Mutation::MyDemoteRegToStack(llvm::Instruction &I,
                                               bool VolatileLoads,
                                               llvm::Instruction *AllocaPoint) {
  /*if (I.use_empty())
  {
      I.eraseFromParent();
      return nullptr;
  }*/

  // Change the users from different BB to read from stack.
  std::vector<llvm::Instruction *> crossBBUsers;
  llvm::BasicBlock *Ibb = I.getParent();
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
  for (llvm::Value::use_iterator ui = I.use_begin(), ue = I.use_end(); ui != ue;
       ++ui) {
    auto &Usr = ui.getUse();
#else
  for (auto &Usr : I.uses()) {
#endif
    llvm::Instruction *U = llvm::dyn_cast<llvm::Instruction>(Usr.getUser());
    if (U->getParent() != Ibb)
      crossBBUsers.push_back(U);
  }

  llvm::AllocaInst *Slot = nullptr;
  if (!crossBBUsers.empty()) {
    // Create a stack slot to hold the value.
    if (AllocaPoint) {
      Slot = new llvm::AllocaInst(I.getType(), 
#if (LLVM_VERSION_MAJOR >= 5)
                                  0, // Adress Space default value
#endif
                                  nullptr,
                                  I.getName() + ".reg2mem", AllocaPoint);
    } else {
      /*llvm::Function *F = I.getParent()->getParent();
      Slot = new llvm::AllocaInst(I.getType(), nullptr, I.getName() +
      ".reg2mem",
                            &F->getEntryBlock().front());*/
      assert(false && "must have non-null insertion point, which if after all "
                      "allocates of entry BB");
    }

    // We cannot demote invoke instructions to the stack if their normal edge
    // is critical. Therefore, split the critical edge and create a basic block
    // into which the store can be inserted.  TODO TODO: Check out whether this
    // is needed here (Invoke..)
    if (llvm::InvokeInst *II = llvm::dyn_cast<llvm::InvokeInst>(&I)) {
      if (!II->getNormalDest()->getSinglePredecessor()) {
        unsigned SuccNum =
            llvm::GetSuccessorNumber(II->getParent(), II->getNormalDest());
        assert(llvm::isCriticalEdge(II, SuccNum) &&
               "Expected a critical edge!");
        llvm::BasicBlock *BB = llvm::SplitCriticalEdge(II, SuccNum);
        assert(BB && "Unable to split critical edge.");
        (void)BB;
      }
    }

    for (auto *U : crossBBUsers) {
      // XXX: No need to care for PHI Nodes, since they are already all removed
      llvm::Value *V =
          new llvm::LoadInst(Slot, I.getName() + ".reload", VolatileLoads, U);
      U->replaceUsesOfWith(&I, V);
    }

    // Insert stores of the computed value into the stack slot. We have to be
    // careful if I is an invoke instruction, because we can't insert the store
    // AFTER the terminator instruction.
    llvm::BasicBlock::iterator InsertPt;
    if (!llvm::isa<llvm::TerminatorInst>(I)) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
      InsertPt = ++(llvm::BasicBlock::iterator(I));
      for (; llvm::isa<llvm::PHINode>(InsertPt) || oldVersionIsEHPad(InsertPt);
           ++InsertPt)
        /* empty */; // Don't insert before PHI nodes or landingpad instrs.
#else
      InsertPt = ++I.getIterator();
      for (; llvm::isa<llvm::PHINode>(InsertPt) || InsertPt->isEHPad();
           ++InsertPt)
        /* empty */; // Don't insert before PHI nodes or landingpad instrs.
#endif
    } else {
      llvm::InvokeInst &II = llvm::cast<llvm::InvokeInst>(I);
      InsertPt = II.getNormalDest()->getFirstInsertionPt();
    }

    new llvm::StoreInst(&I, Slot, &*InsertPt);
  }
  return Slot;
}

/**
 * \brief The function that mutation will skip
 */
inline bool Mutation::skipFunc(llvm::Function &Func) {
  assert(
      (funcForKLEESEMu == nullptr ||
       Func.getParent() == currentMetaMutantModule) &&
      "calling skipFunc on the wrong module. Must be called only in doMutate");

  // Skip Function with only Declaration (External function -- no definition)
  if (Func.isDeclaration())
    return true;

  if (forKLEESEMu && funcForKLEESEMu == &Func)
    return true;

  if (mutationScope.isInitialized() &&
      !mutationScope.functionInMutationScope(&Func))
    return true;

  return false;
}

bool Mutation::getConfiguration(std::string &mutConfFile) {
  // TODO
  std::vector<unsigned> reploprd;
  std::vector<llvmMutationOp> mutationOperations;
  std::vector<std::string> matchoprd;
  std::vector<enum ExpElemKeys> *correspKeysMatch;
  std::vector<enum ExpElemKeys> *correspKeysMutant;

  std::ifstream infile(mutConfFile);
  if (infile) {
    std::string linei;
    std::vector<std::string> matchop_oprd;
    unsigned confLineNum = 0;
    while (infile) {
      confLineNum++;
      std::getline(infile, linei);

      // remove comment (Everything after the '#' character)
      size_t commentInd = linei.find_first_of('#');
      if (commentInd != std::string::npos)
        linei.erase(commentInd, std::string::npos);

      // skip white(empty) line
      if (std::regex_replace(linei, std::regex("^\\s+|\\s+$"),
                             std::string("$1"))
              .length() == 0) {
        // llvm::errs() << "#"<<linei <<"#\n";
        continue;
      }

      std::regex rgx("\\s+\-\->\\s+"); // Matcher --> Replacors
      std::sregex_token_iterator iter(linei.begin(), linei.end(), rgx, -1);
      std::sregex_token_iterator end;
      unsigned short matchRepl = 0;
      for (; iter != end; ++iter) {
        if (matchRepl == 0) {
          std::string matchstr2(*iter);
          // operation (operand1,operand2,...)
          std::regex rgx2("\\s*,\\s*|\\s*\\(\\s*|\\s*\\)\\s*");
          std::sregex_token_iterator iter2(matchstr2.begin(), matchstr2.end(),
                                           rgx2, -1);
          std::sregex_token_iterator end2;

          matchoprd.clear();
          for (; iter2 != end2; ++iter2) {
            // Position 0 for operation, rest for operands
            matchoprd.push_back(std::regex_replace(std::string(*iter2),
                                                   std::regex("^\\s+|\\s+$"),
                                                   std::string("$1")));
          }

          // when we match @ or C or V or A or P, they are their own params
          if (matchoprd.size() == 1 &&
              usermaps.isConstValOPRD(matchoprd.back())) {
            // I am my own parameter
            matchoprd.push_back(matchoprd.back());
          }

          mutationOperations.clear();

          correspKeysMatch = usermaps.getExpElemKeys(
              matchoprd.front(), matchstr2,
              confLineNum); // floats then ints. EX: OF, UF, SI, UI
          for (unsigned i = 0; i < correspKeysMatch->size(); i++) {
            mutationOperations.push_back(llvmMutationOp());
            mutationOperations.back().setMatchOp(correspKeysMatch->at(i),
                                                 matchoprd, 1);
          }
        } else if (matchRepl == 1) {
          std::string matchstr3(*iter);
          std::regex rgx3("\\s*;\\s*"); // Replacors
          std::sregex_token_iterator iter3(matchstr3.begin(), matchstr3.end(),
                                           rgx3, -1);
          std::sregex_token_iterator end3;
          for (; iter3 != end3; ++iter3) // For each replacor
          {
            std::string tmprepl(std::regex_replace(std::string(*iter3),
                                                   std::regex("^\\s+|\\s+$"),
                                                   std::string("$1")));
            // MutName, operation(operand1,operand2,...)
            std::regex rgx4("\\s*,\\s*|\\s*\\(\\s*|\\s*\\)\\s*");
            std::sregex_token_iterator iter4(tmprepl.begin(), tmprepl.end(),
                                             rgx4, -1);
            std::sregex_token_iterator end4;
            std::string mutName(*iter4);
            ++iter4;
            if (!(iter4 != end4)) {
              llvm::errs() << "only Mutant name, no info at line "
                           << confLineNum << ", mutantion op name: " << mutName
                           << "\n";
              assert(iter4 != end4 && "only Mutant name, no info!");
            }
            std::string mutoperation(*iter4);

            // If replace with constant number, add it here
            auto contvaloprd = llvmMutationOp::insertConstValue(
                mutoperation, true); // numeric=true
            if (llvmMutationOp::isSpecifiedConstIndex(contvaloprd)) {
              reploprd.clear();
              reploprd.push_back(contvaloprd);
              for (auto &mutOper : mutationOperations)
                mutOper.addReplacor(mCONST_VALUE_OF, reploprd, mutName);
              ++iter4;
              assert(iter4 == end4 &&
                     "Expected no mutant operand for const value replacement!");
              continue;
            }

            ++iter4;
            if (!(iter4 != end4)) {
              if (!usermaps.isWholeStmtMutationConfName(mutoperation)) {
                llvm::errs() << "no mutant operands at line " << confLineNum
                             << ", replacor: " << mutoperation << "\n";
                assert(iter4 != end4 && "no mutant operands");
              }
            }

            reploprd.clear();

            for (; iter4 != end4; ++iter4) // for each operand
            {
              if ((*iter4).length() == 0) {
                if (!usermaps.isWholeStmtMutationConfName(mutoperation)) {
                  llvm::errs() << "missing operand at line " << confLineNum
                               << "\n";
                  assert(false && "");
                }
                continue;
              }

              bool found = false;
              unsigned pos4 = 0;

              // If constant number, directly add it
              if (llvmMutationOp::isSpecifiedConstIndex(
                      pos4 = llvmMutationOp::insertConstValue(
                          std::string(*iter4), true))) // numeric=true
              {
                reploprd.push_back(pos4);
                continue;
              }

              // If CALLED Function replacement
              if (mutationOperations.back().getMatchOp() == mCALL) {
                for (auto &obj : mutationOperations)
                  assert(obj.getMatchOp() == mCALL &&
                         "All 4 types should be mCALL here");
                pos4 = llvmMutationOp::insertConstValue(std::string(*iter4),
                                                        false);
                assert(llvmMutationOp::isSpecifiedConstIndex(pos4) &&
                       "Insertion should always work here");
                reploprd.push_back(pos4);
                continue;
              }

              // Make sure that the parameter is either anything or any variable
              // or any pointer or any constant
              usermaps.validateNonConstValOPRD(llvm::StringRef(*iter4),
                                               confLineNum);

              // search the operand position
              for (std::vector<std::string>::iterator it41 =
                       matchoprd.begin() + 1;
                   it41 != matchoprd.end(); ++it41) {
                if (llvm::StringRef(*it41).equals_lower(
                        llvm::StringRef(*iter4))) {
                  found = true;
                  break;
                }
                pos4++;
              }

              if (found) {
                reploprd.push_back(pos4); // map to Matcher operand
              } else {
                llvm::errs() << "Error in the replacor parameters (do not "
                                "match any of the Matcher's): '"
                             << tmprepl << "', confLine(" << confLineNum
                             << ")!\n";
                return false;
              }
            }

            correspKeysMutant = usermaps.getExpElemKeys(
                mutoperation, tmprepl,
                confLineNum); // floats then ints. EX: FDiv, SDiv, UDiv

            assert(correspKeysMutant->size() == mutationOperations.size() &&
                   "Incompatible types Mutation oprerator");

            unsigned iM = 0;
            while (iM < correspKeysMutant->size() &&
                   (isExpElemKeys_ForbidenType(
                        mutationOperations[iM].getMatchOp()) ||
                    isExpElemKeys_ForbidenType(correspKeysMutant->at(iM)))) {
              iM++;
            }
            if (iM >= correspKeysMutant->size())
              continue;

            mutationOperations[iM].addReplacor(correspKeysMutant->at(iM),
                                               reploprd, mutName);
            enum ExpElemKeys prevMu = correspKeysMutant->at(iM);
            enum ExpElemKeys prevMa = mutationOperations[iM].getMatchOp();
            for (iM = iM + 1; iM < correspKeysMutant->size(); iM++) {
              if (!isExpElemKeys_ForbidenType(
                      mutationOperations[iM].getMatchOp()) &&
                  !isExpElemKeys_ForbidenType(correspKeysMutant->at(iM))) {
                if (correspKeysMutant->at(iM) != prevMu ||
                    mutationOperations[iM].getMatchOp() != prevMa) {
                  mutationOperations[iM].addReplacor(correspKeysMutant->at(iM),
                                                     reploprd, mutName);
                  prevMu = correspKeysMutant->at(iM);
                  prevMa = mutationOperations[iM].getMatchOp();
                }
              }
            }
          }

          // Remove the matcher without replacors
          for (unsigned itlM = 0; itlM < mutationOperations.size();) {
            if (mutationOperations.at(itlM).getNumReplacor() < 1)
              mutationOperations.erase(mutationOperations.begin() + itlM);
            else
              ++itlM;
          }

          // Finished extraction for a line, Append to cofiguration
          for (auto &opsofmatch : mutationOperations) {
            configuration.mutators.push_back(opsofmatch);
          }
        } else { // should not be reached
          llvm::errs() << "Invalid Line: '" << linei << "'\n";
          return false; // assert (false && "Invalid Line!!!")
        }

        matchRepl++;
      }
    }
  } else {
    llvm::errs() << "Error while opening (or empty) mutant configuration '"
                 << mutConfFile << "'\n";
    return false;
  }

  /*//DEBUG
  for (llvmMutationOp oops: configuration.mutators)   //DEBUG
      llvm::errs() << oops.toString();    //DEBUG
  return false;   //DEBUG*/

  return true;
}

void Mutation::getanothermutantIDSelectorName() {
  static unsigned tempglob = 0;
  mutantIDSelectorName.assign("klee_semu_GenMu_Mutant_ID_Selector");
  if (tempglob > 0)
    mutantIDSelectorName.append(std::to_string(tempglob));
  tempglob++;

  mutantIDSelectorName_Func.assign(mutantIDSelectorName + "_Func");
}

void Mutation::getanotherPostMutantPointFuncName() {
  static unsigned tempglob = 0;
  postMutationPointFuncName.assign("klee_semu_GenMu_Post_Mutation_Point_Func");
  if (tempglob > 0)
    postMutationPointFuncName.append(std::to_string(tempglob));
  tempglob++;
}

// @Name: Mutation::getMutantsOfStmt
// This function takes a statement as a list of IR instruction, using the
// mutation model specified for this class, generate a list of all possible
// mutants
// of the statement
void Mutation::getMutantsOfStmt(MatchStmtIR const &stmtIR,
                                MutantsOfStmt &ret_mutants,
                                ModuleUserInfos const &moduleInfo) {
  assert((ret_mutants.getNumMuts() == 0) &&
         "Error (Mutation::getMutantsOfStmt): mutant list result vector is not "
         "empty!\n");

  WholeStmtMutationOnce iswholestmtmutated;

  for (llvmMutationOp &mutator : configuration.mutators) {
    // for (auto &mn: mutator.getMutantReplacorsList())    // DBG
    //    llvm::errs() << mn.getMutOpName() << "; ";      // DBG

    usermaps.getMatcherObject(mutator.getMatchOp())
        ->matchAndReplace(stmtIR, mutator, ret_mutants, iswholestmtmutated,
                          moduleInfo);

    // Check that load and stores type are okay
    for (MutantIDType i = 0, ie = ret_mutants.getNumMuts(); i < ie; i++) {
      if (!ret_mutants.getMutantStmtIR(i).checkLoadAndStoreTypes()) {
        llvm::errs() << "\nMutation: " << ret_mutants.getTypeName(i);
        // for (auto &mn: mutator.getMutantReplacorsList())
        //    llvm::errs() << mn.getMutOpName() << "; ";
        llvm::errs() << "\n\n";
        // for (auto *xx: stmtIR.getIRList()) xx->dump();
        assert(false);
      }
    }

    // Verify that no constant is considered as instruction in the mutant
    // (inserted in replacement vector)  TODO: Remove commented bellow
    /*# llvm::errs() << "\n@orig\n";   //DBG
    for (auto *dd: stmtIR)          //DBG
        dd->dump();                 //DBG*/
    /*for (auto ind = 0; ind < ret_mutants.getNumMuts(); ind++)
    {
        auto &mutInsVec = ret_mutants.getMutantStmtIR(ind);
        /*# llvm::errs() << "\n@Muts\n";    //DBG* /
        for (auto *mutIns: mutInsVec)
        {
            /*# mutIns->dump();     //DBG* /
            if(llvm::dyn_cast<llvm::Constant>(mutIns))
            {
                llvm::errs() << "\nError: A constant is considered as
    Instruction (inserted in 'replacement') for mutator (enum ExpElemKeys): " <<
    mutator.getMatchOp() << "\n\n";
                mutIns->dump();
                assert (false);
            }
        }
    }*/
    //}
    //}
  }
} //~Mutation::getMutantsOfStmt

llvm::Function *Mutation::createKSFunc(llvm::Module &module, bool bodyOnly,
                                        std::string ks_func_name) {
  llvm::Function *funcForKS = nullptr;
  if (!bodyOnly) {
    llvm::Constant *c = module.getOrInsertFunction(
        ks_func_name,
        llvm::Type::getVoidTy(moduleInfo.getContext()),
        llvm::Type::getInt32Ty(moduleInfo.getContext()),
        llvm::Type::getInt32Ty(moduleInfo.getContext())
#if (LLVM_VERSION_MAJOR < 5)
        , NULL
#endif
    );
    funcForKS = llvm::cast<llvm::Function>(c);
    if (!funcForKS) {
      llvm::errs() << "Failed to create function " << ks_func_name << "\n";
      assert(false);
    }
  } else {
    funcForKS = module.getFunction(ks_func_name);
    if (!funcForKS) {
      llvm::errs() << "Function" << ks_func_name << " is not existing\n";
      assert(false);
    }
  }
  llvm::BasicBlock *block =
      llvm::BasicBlock::Create(moduleInfo.getContext(), "entry", funcForKS);
  llvm::IRBuilder<> builder(block);
  llvm::Function::arg_iterator args = funcForKS->arg_begin();
  llvm::Value *x = llvm::dyn_cast<llvm::Value>(args++);
  x->setName("mutIdFrom");
  llvm::Value *y = llvm::dyn_cast<llvm::Value>(args++);
  y->setName("mutIdTo");
  // builder.CreateBinOp(llvm::Instruction::Mul, x, y, "tmp");*/
  builder.CreateRetVoid();

  return funcForKS;
}

llvm::Function *Mutation::createGlobalMutIDSelector_Func(llvm::Module &module,
                                                         bool bodyOnly) {
  return Mutation::createKSFunc(module, bodyOnly, mutantIDSelectorName_Func);
}

llvm::Function *Mutation::createPostMutationPointFunc(llvm::Module &module,
                                                         bool bodyOnly) {
  return Mutation::createKSFunc(module, bodyOnly, postMutationPointFuncName);
}

void Mutation::checkModuleValidity(llvm::Module &Mod, const char *errMsg) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
  if (verifyFuncModule && llvm::verifyModule(Mod, llvm::AbortProcessAction))
#else
  if (verifyFuncModule && llvm::verifyModule(Mod, &llvm::errs()))
#endif
  {
    llvm::errs() << errMsg << "\n";
    assert(false); // return false;
  }
}

void Mutation::checkFunctionValidity(llvm::Function &Func, const char *errMsg) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
  if (verifyFuncModule && llvm::verifyFunction(Func, llvm::AbortProcessAction))
#else
  if (verifyFuncModule && llvm::verifyFunction(Func, &llvm::errs()))
#endif
  {
    llvm::errs() << errMsg << "(" << Func.getName() << ")\n";
    assert(false); // return false;
  }
}

// @Name: doMutate
// This is the main method of the class Mutation, Call this to mutate a module
bool Mutation::doMutate() {
  llvm::Module &module = *currentMetaMutantModule;

  // Instert Mutant ID Selector Global Variable
  while (module.getNamedGlobal(mutantIDSelectorName)) {
    llvm::errs() << "The gobal variable '" << mutantIDSelectorName
                 << "' already present in code!\n";
    assert(
        false &&
        "ERROR: Module already mutated!"); // getanothermutantIDSelectorName();
  }
  module.getOrInsertGlobal(mutantIDSelectorName,
                           llvm::Type::getInt32Ty(moduleInfo.getContext()));
  llvm::GlobalVariable *mutantIDSelectorGlobal =
      module.getNamedGlobal(mutantIDSelectorName);
  // mutantIDSelectorGlobal->setLinkage(llvm::GlobalValue::CommonLinkage);
  // //commonlinkage require 0 as initial value
  mutantIDSelectorGlobal->setAlignment(4);
  mutantIDSelectorGlobal->setInitializer(llvm::ConstantInt::get(
      moduleInfo.getContext(), llvm::APInt(32, 0, false)));

  // XXX: Insert definition of the function whose call argument will tell
  // KS which mutants to fork
  if (forKLEESEMu) {
    funcForKLEESEMu = createGlobalMutIDSelector_Func(module);
  }

  // mutate
  struct SourceStmtsSearchList {
    std::vector<StatementSearch *> sourceOrderedStmts;
    llvm::BasicBlock *curBB = nullptr;
    StatementSearch *createNewElem(llvm::BasicBlock *bb) {
      if (bb != curBB) {
        // if (curBB != nullptr)
        sourceOrderedStmts.push_back(nullptr);
        curBB = bb;
      }
      sourceOrderedStmts.push_back(new StatementSearch);
      return sourceOrderedStmts.back();
    }
    void remove(StatementSearch *ss) {
      delete ss;
      bool found = false;
      for (long i = sourceOrderedStmts.size() - 1; i >= 0; i--)
        if (sourceOrderedStmts[i] == ss) {
          sourceOrderedStmts.erase(sourceOrderedStmts.begin() + i);
          found = true;
        }
      assert(found && "removing a statement not inserted");
    }
    void appendOrder(llvm::BasicBlock *bb, StatementSearch *ss) {
      if (bb != curBB) {
        assert(curBB != nullptr &&
               "calling append before calling createNewElem");
        sourceOrderedStmts.push_back(nullptr);
        curBB = bb;
      }
      sourceOrderedStmts.push_back(ss);
    }
    void doneSearch() { sourceOrderedStmts.push_back(nullptr); }
    void clear() {
      std::unordered_set<StatementSearch *> stmp(sourceOrderedStmts.begin(),
                                                 sourceOrderedStmts.end());
      for (auto *ss : stmp)
        if (ss)
          delete ss;
      sourceOrderedStmts.clear();
      curBB = nullptr;
    }
    std::vector<StatementSearch *> &getSourceOrderedStmts() {
      return sourceOrderedStmts;
    }
  } srcStmtsSearchList;

  /**
   * \brief This class create a proxy BB for each incoming BB of PHI nodes whose
   * corresponding value is a CONSTANT (independent on value computed on
   * previous BB)
   */
  struct ProxyForPHI {
    std::unordered_set<llvm::BasicBlock *> proxies;
    std::unordered_map<llvm::PHINode *,
                       std::unordered_map<llvm::BasicBlock * /*a Basic Block*/,
                                          llvm::BasicBlock * /*its Proxy*/>>
        phiBBProxy;
    llvm::Function *curFunc = nullptr;

    void clear(llvm::Function *f) {
      proxies.clear();
      phiBBProxy.clear();
      curFunc = f;
    }
    bool isProxy(llvm::BasicBlock *bb) { return (proxies.count(bb) > 0); }
    void getProxiesTerminators(llvm::PHINode *phi,
                               std::vector<llvm::Instruction *> &terms) {
      auto itt = phiBBProxy.find(phi);
      assert(itt != phiBBProxy.end() && "looking for missing phi node");
      for (auto inerIt : itt->second)
        terms.push_back(inerIt.second->getTerminator());
    }
    void handleBB(llvm::BasicBlock *bb, ModuleUserInfos const &MI) { // see
      // http://llvm.org/docs/doxygen/html/BasicBlock_8cpp_source.html#l00401
      llvm::TerminatorInst *TI = bb->getTerminator();
      if (!TI)
        return;
      for (auto i = 0; i < TI->getNumSuccessors(); i++) {
        llvm::BasicBlock *Succ = TI->getSuccessor(i);
        for (llvm::BasicBlock::iterator II = Succ->begin(), IE = Succ->end();
             II != IE; ++II) {
          llvm::PHINode *PN = llvm::dyn_cast<llvm::PHINode>(II);
          if (!PN)
            break;
          handlePhi(PN, MI);
        }
      }
    }
    void handlePhi(llvm::PHINode *phi, ModuleUserInfos const &MI) {
      static unsigned proxyBBNum = 0;
      llvm::BasicBlock *phiBB = nullptr;
      std::pair<
          std::unordered_map<llvm::PHINode *,
                             std::unordered_map<llvm::BasicBlock *,
                                                llvm::BasicBlock *>>::iterator,
          bool>
          ittmp;
      unsigned pind = 0;
      /*for (; pind < phi->getNumIncomingValues(); ++pind)
      {
          if (llvm::isa<llvm::Constant>(phi->getIncomingValue(pind)))
          {
              ittmp = phiBBProxy.emplace (phi,
      std::unordered_map<llvm::BasicBlock *, llvm::BasicBlock *>());
              if (ittmp.second==false)
                  return;
              phiBB = phi->getParent();
              break;
          }
      }*/
      ittmp = phiBBProxy.emplace(
          phi, std::unordered_map<llvm::BasicBlock *, llvm::BasicBlock *>());
      if (ittmp.second == false)
        return;
      phiBB = phi->getParent();

      for (; pind < phi->getNumIncomingValues(); ++pind) {
        /*if (! llvm::isa<llvm::Constant>(phi->getIncomingValue(pind)))
            continue;*/

        /// The incoming value is a constant
        llvm::BasicBlock *bb = phi->getIncomingBlock(pind);
        // create proxy
        llvm::BasicBlock *proxyBlock = llvm::BasicBlock::Create(
            MI.getContext(),
            std::string("MART.PHI_N_Proxy") + std::to_string(proxyBBNum++),
            curFunc, bb->getNextNode());
        // create unconditional branch to phiBB and
        // insert at the end of proxyBlock
        llvm::BranchInst::Create(phiBB, proxyBlock);
        if ((ittmp.first->second.emplace(bb, proxyBlock)).second == false) {
          proxyBlock->eraseFromParent();
        } else {
          proxies.insert(proxyBlock);

          phi->setIncomingBlock(pind, proxyBlock);
          llvm::TerminatorInst *TI = bb->getTerminator();
          bool found = false; // DEBUG
          for (auto i = 0; i < TI->getNumSuccessors(); i++) {
            if (phiBB == TI->getSuccessor(i)) {
              TI->setSuccessor(i, proxyBlock);
              found = true; // DEBUG
            }
          }
          assert(found && "bb must have phiBB as a successor"); // DEBUG
        }
      }
    }
  } phiProxy;

  // pos in sourceStmts of the statement spawning multiple BB. The actual
  // mutation happend only when this is empty at the end of a BB.
  std::set<StatementSearch *> remainMultiBBLiveStmts;
  unsigned mod_mutstmtcount = 0;

  // set to null after each stmt search completion
  StatementSearch *curLiveStmtSearch = nullptr;

  /******************************************************
   **** Search for high level statement (source level) **
   ******************************************************/
  for (auto &Func : module) {

    // Skip Function with only Declaration (External function -- no definition)
    if (skipFunc(Func))
      continue;

    phiProxy.clear(&Func);

    ///\brief This hel recording the IR's LOC: index in the function it belongs
    unsigned instructionPosInFunc = 0;

    ///\brief In case we have multiBB stmt, this say which is the first BB to
    /// start mutation from. this is equal to itBBlock below if only sigle BB
    /// stmts
    /// Set to null after each actual mutation take place
    llvm::BasicBlock *mutationStartingAtBB = nullptr;

    std::unordered_set<llvm::Instruction *> consecutiveSkippedInsts;

    for (auto itBBlock = Func.begin(), F_end = Func.end(); itBBlock != F_end;
         ++itBBlock) {
      /// Do not mutate the inserted proxy for PHI nodes
      if (phiProxy.isProxy(&*itBBlock))
        continue;

      /// set the Basic block from which the actual mutation should start
      if (!mutationStartingAtBB)
        mutationStartingAtBB = &*itBBlock;

      // make sure that in case this BB has phi node as successor, proxy BB will
      // be created and added.
      phiProxy.handleBB(&*itBBlock, moduleInfo);

      std::queue<llvm::Value *> curUses;

      for (auto &Instr : *itBBlock) {
        // This should always be before anything else in this loop
        instructionPosInFunc++;

        // If the instruction was mached as to be skipped (using an instruction)
        // that was skipped,  skip it (example: 'store' following 'stacksave')
        if (!consecutiveSkippedInsts.empty()) {
          if (consecutiveSkippedInsts.count(&Instr)) {
            consecutiveSkippedInsts.erase(&Instr);
            continue;
          } else {
            llvm::errs() << "Mart@Error: the instruction to delete does not "
                            "directly follow its precursor! (Atomic..)\n";
            Instr.dump();
            assert(false && "consecutiveSkippedInsts not empty");
          }
        }

// For Now do not mutate Exeption handling code, TODO later. TODO
// (http://llvm.org/docs/doxygen/html/Instruction_8h_source.html#l00393)
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
        if (oldVersionIsEHPad(&Instr))
#else
        if (Instr.isEHPad())
#endif
        {
          llvm::errs()
              << "(msg) Exception handling not mutated for now. TODO\n";
          continue;
        }

        // Do not mind Alloca when no size specified, if size specified, remove
        // anything related to size that was added before
        if (auto *alloca = llvm::dyn_cast<llvm::AllocaInst>(&Instr)) {
          if (alloca->isArrayAllocation()) {
            if (curLiveStmtSearch && curLiveStmtSearch->isVisited(&Instr)) {
              // do not mutate alloca
              srcStmtsSearchList.remove(curLiveStmtSearch);
              curLiveStmtSearch = nullptr;
            } else {
              Instr.getParent()->dump();
              assert(false && "Non atomic ??. Please Report bug (Mart)");
              // assert (llvm::isa<llvm::ConstantInt>(alloca->getArraySize()) &&
              // "Non Atomic??");
            }
            continue;
          } else {
            continue;
          }
        }

        // If PHI node and wasn't processed by proxy, add proxies
        if (auto *phiN = llvm::dyn_cast<llvm::PHINode>(&Instr))
          phiProxy.handlePhi(phiN, moduleInfo);

        // Skip llvm debugging functions void @llvm.dbg.declare and void
        // @llvm.dbg.value, and klee special function...
        if (auto *callinst = llvm::dyn_cast<llvm::CallInst>(&Instr)) {
          if (auto *intrinsic = llvm::dyn_cast<llvm::IntrinsicInst>(callinst)) {
            // llvm.dbg.declare  and llvm.dbg.value
            if (llvm::isa<llvm::DbgInfoIntrinsic>(intrinsic)) {
              if (curLiveStmtSearch &&
                  curLiveStmtSearch->isVisited(intrinsic)) {
                assert(false && "The debug statement should not have been in "
                                "visited (cause no dependency on others "
                                "stmts...)"); // DBG
                srcStmtsSearchList.remove(curLiveStmtSearch);
                curLiveStmtSearch = nullptr;
              }
              continue;
            } else if (intrinsic->getIntrinsicID() ==
                       llvm::Intrinsic::stackrestore) {
              // XXX For stacksave and restore, skip for now
              if (curLiveStmtSearch &&
                  curLiveStmtSearch->isVisited(intrinsic)) {
                srcStmtsSearchList.remove(curLiveStmtSearch);
                curLiveStmtSearch = nullptr;
              }
              continue;
            } else if (intrinsic->getIntrinsicID() ==
                       llvm::Intrinsic::stacksave) {
// XXX For stacksave and restore, skip for now
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
              auto *ss_store =
                  llvm::dyn_cast<llvm::StoreInst>(*(intrinsic->use_begin()));
#else
              auto *ss_store =
                  llvm::dyn_cast<llvm::StoreInst>(*(intrinsic->user_begin()));
#endif
              // If the Stroe do not directly follow, it will be caught above
              assert(ss_store && intrinsic->hasOneUse() &&
                     "unexpected stacksave use pattern");
              consecutiveSkippedInsts.insert(ss_store);
              continue;
            }
          } else if (llvm::Function *fun = callinst->getCalledFunction()) {
            // TODO: handle function alias (get called function)
            if (forKLEESEMu && fun->getName().equals("klee_make_symbolic") &&
                callinst->getNumArgOperands() == 3 &&
                fun->getReturnType()->isVoidTy()) {
              if (curLiveStmtSearch && curLiveStmtSearch->isVisited(&Instr)) {
                srcStmtsSearchList.remove(
                    curLiveStmtSearch); // do not mutate klee_make_symbolic
                curLiveStmtSearch = nullptr;
              }
              continue;
            }
          }
        }

        // In case this is not the begining of a stmt search (there are live
        // stmts)
        if (curLiveStmtSearch) {
          if (curLiveStmtSearch->isVisited(&Instr)) // is it visited?
          {
            curLiveStmtSearch->checkCountLogic();
            curLiveStmtSearch->appendIRToStmt(&*itBBlock, &Instr,
                                              instructionPosInFunc - 1);
            curLiveStmtSearch->countDec();

            continue;
          }
        }

        bool foundd = false;
        for (auto *remMStmt : remainMultiBBLiveStmts) {
          if (remMStmt->isVisited(&Instr)) {
            // Check that Statements are atomic (all IR of stmt1 before any IR
            // of stmt2, except Alloca - actually all allocas are located at the
            // beginning of the function)
            remMStmt->checkAtomicityInBB(&*itBBlock);

            curLiveStmtSearch = StatementSearch::switchFromTo(
                &*itBBlock, curLiveStmtSearch, remMStmt);
            srcStmtsSearchList.appendOrder(&*itBBlock, curLiveStmtSearch);
            remainMultiBBLiveStmts.erase(remMStmt);
            foundd = true;

            // process as for visited Inst, as above
            curLiveStmtSearch->checkCountLogic();
            curLiveStmtSearch->appendIRToStmt(&*itBBlock, &Instr,
                                              instructionPosInFunc - 1);
            curLiveStmtSearch->countDec();

            break;
          }
        }
        if (foundd) {
          continue;
        } else {
          if (curLiveStmtSearch && !curLiveStmtSearch->isCompleted()) {
            // TODO TODO: Implement the option (1) of Splitting when a
            // statement,
            // is non-atomic, at preprocessing (demoting reg to mem
            // on the atomicity breaking values).
            // XXX For now, we merge the non atomic statement with its
            // 'in between' statement (make a bit of harm to whole stmt mutation
            // like SDL, whle option 1 will make a bit of harm to mutation
            // involving several IRs).
            // this is done by creating new stmt search only when on new BB
            if (curLiveStmtSearch->isOnNewBasicBlock(&*itBBlock)) {
              remainMultiBBLiveStmts.insert(curLiveStmtSearch);
              curLiveStmtSearch = StatementSearch::switchFromTo(
                  &*itBBlock, curLiveStmtSearch,
                  srcStmtsSearchList.createNewElem(
                      &*itBBlock)); //(re)initialize
            }
          } else {
            curLiveStmtSearch = StatementSearch::switchFromTo(
                &*itBBlock, nullptr,
                srcStmtsSearchList.createNewElem(&*itBBlock)); //(re)initialize
          }
        }

        /* //Commented because the mutating function do no delete stmt with
        terminator instr (to avoid misformed while), but only delete for return
        break and continue in this case
        //make the final unconditional branch part of this statement (to avoid
        multihop empty branching)
        if (llvm::isa<llvm::BranchInst>(&Instr))
        {
            if (llvm::dyn_cast<llvm::BranchInst>(&Instr)->isUnconditional() &&
        !visited.empty())
            {
                curLiveStmtSearch->appendIRToStmt(&*itBBlock, &Instr,
        instructionPosInFunc - 1);
                continue;
            }
        }*/

        curLiveStmtSearch->appendIRToStmt(&*itBBlock, &Instr,
                                          instructionPosInFunc - 1);
        if (!curLiveStmtSearch->visit(&Instr)) {
          // Func.dump();
          llvm::errs() << "\nInstruction: ";
          Instr.dump();
          assert(false && "first time seing an instruction but present in "
                          "visited. report bug");
        }
        curUses.push(&Instr);
        while (!curUses.empty()) {
          llvm::Value *popInstr = curUses.front();
          curUses.pop();
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
          for (llvm::Value::use_iterator ui = popInstr->use_begin(),
                                         ue = popInstr->use_end();
               ui != ue; ++ui) {
            auto &U = ui.getUse();
#else
          for (auto &U : popInstr->uses()) {
#endif
            if (curLiveStmtSearch->visit(U.getUser())) // wasn't visited? insert
            {
              curUses.push(U.getUser());
              curLiveStmtSearch->countInc();
            }
          }
          // consider only operands when more than 1 (popInstr is a user or
          // operand or Load or Alloca)
          // if (llvm::dyn_cast<llvm::User>(popInstr)->getNumOperands() > 1)
          if (!(llvm::isa<llvm::AllocaInst>(popInstr))) {
            for (unsigned opos = 0;
                 opos < llvm::dyn_cast<llvm::User>(popInstr)->getNumOperands();
                 opos++) {
              auto oprd =
                  llvm::dyn_cast<llvm::User>(popInstr)->getOperand(opos);

              //@ Check that oprd is not Alloca (done already above 'if')..

              if (!oprd || llvm::isa<llvm::AllocaInst>(oprd))
                continue;

              if (llvm::dyn_cast<llvm::Instruction>(oprd) &&
                  curLiveStmtSearch->visit(oprd)) {
                curUses.push(oprd);
                curLiveStmtSearch->countInc();
              }
            }
          }
        }
        // curUses is empty here
      } // for (auto &Instr: *itBBlock)

      curLiveStmtSearch = nullptr;

      // Check if we can mutate now or not (seach completed all live stmts)
      if (!remainMultiBBLiveStmts.empty())
        continue;

      /***********************************************************
      // \brief Actual mutation **********************************
      /***********************************************************/

      /// \brief mutate all the basic blocks between 'mutationStartingAtBB' and
      /// '&*itBBlock'
      srcStmtsSearchList.doneSearch(); // append the last nullptr to order...
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
      auto changingBBIt = llvm::Function::iterator(mutationStartingAtBB);
      auto stopAtBBIt = llvm::Function::iterator(itBBlock);
#else
      auto changingBBIt = mutationStartingAtBB->getIterator();
      auto stopAtBBIt = itBBlock->getIterator();
#endif
      ++stopAtBBIt; // pass the current block
      llvm::BasicBlock *sstmtCurBB =
          nullptr; // The loop bellow will be executed at least once
      auto curSrcStmtIt = srcStmtsSearchList.getSourceOrderedStmts().begin();

      /// Get all the mutants
      for (auto *sstmt : srcStmtsSearchList.getSourceOrderedStmts()) {
        if (sstmt && sstmt->mutantStmt_list.isEmpty()) // not yet mutated
        {
          // Find all mutants and put into 'mutantStmt_list'
          getMutantsOfStmt(sstmt->matchStmtIR, sstmt->mutantStmt_list,
                           moduleInfo);

          // set the mutant IDs
          for (auto mind = 0; mind < sstmt->mutantStmt_list.getNumMuts();
               mind++) {
            sstmt->mutantStmt_list.setMutID(mind, ++curMutantID);
            // for(auto
            // &xx:sstmt->mutantStmt_list.getMutantStmtIR(mind).origBBToMutBB)
            //    for(auto *bb: xx.second)
            //        bb->dump();
          }
        }
      }

      // for each BB place in the muatnts
      for (; changingBBIt != stopAtBBIt; ++changingBBIt) {
        /// Do not mutate the inserted proxies for PHI nodes
        if (phiProxy.isProxy(&*changingBBIt))
          continue;

        sstmtCurBB = &*changingBBIt;

        for (++curSrcStmtIt /*the 1st is nullptr*/; *curSrcStmtIt != nullptr;
             ++curSrcStmtIt) // different BB stmts are delimited by nullptr
        {
          unsigned nMuts = (*curSrcStmtIt)->mutantStmt_list.getNumMuts();

          // Mutate only when mutable: at least one mutant (nMuts > 0)
          if (nMuts > 0) {
            llvm::Instruction *firstInst, *lastInst;
            (*curSrcStmtIt)
                ->matchStmtIR.getFirstAndLastIR(&*changingBBIt, firstInst,
                                                lastInst);

            /// If the firstInst (intended basic block plit point) isPHI Node,
            /// instead of splitting, directly add the mutant selection switch
            /// on the Proxy BB.
            bool usePhiProxy_NoSplitBB = false;
            if (llvm::isa<llvm::PHINode>(firstInst))
              usePhiProxy_NoSplitBB = true;

            llvm::BasicBlock *original = nullptr;
            std::vector<llvm::Instruction *> linkterminators;
            std::vector<llvm::SwitchInst *> sstmtMutants;

#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            llvm::PassManager PM;
            llvm::RegionInfo *tmp_pass = new llvm::RegionInfo();
            PM.add(tmp_pass); // tmp_pass must be created with 'new'
            if (!usePhiProxy_NoSplitBB) {
              original = llvm::SplitBlock(sstmtCurBB, firstInst, tmp_pass);
#else
            if (!usePhiProxy_NoSplitBB) {
              original = llvm::SplitBlock(sstmtCurBB, firstInst);
#endif
              original->setName(std::string("MART.original_Mut0.Stmt") +
                                std::to_string(mod_mutstmtcount));

              // this cannot be nullptr because the block just got splitted
              linkterminators.push_back(sstmtCurBB->getTerminator());
            } else {
              // PHI Node is always the first non PHI instruction of its BB
              original = sstmtCurBB;

              phiProxy.getProxiesTerminators(
                  llvm::dyn_cast<llvm::PHINode>(firstInst), linkterminators);
            }

            for (auto *lkt : linkterminators) {
              llvm::IRBuilder<> sbuilder(lkt);

              // XXX: Insert definition of the function whose call argument will
              // tell KS which mutants to fork (done elsewhere)
              if (forKLEESEMu) {
                std::vector<llvm::Value *> argsv;
                argsv.push_back(llvm::ConstantInt::get(
                    moduleInfo.getContext(),
                    llvm::APInt(
                        32, (uint64_t)(
                                (*curSrcStmtIt)->mutantStmt_list.getMutID(0)),
                        false)));
                argsv.push_back(llvm::ConstantInt::get(
                    moduleInfo.getContext(),
                    llvm::APInt(32, (uint64_t)((*curSrcStmtIt)
                                                   ->mutantStmt_list.getMutID(
                                                       nMuts - 1)),
                                false)));
                sbuilder.CreateCall(funcForKLEESEMu, argsv);
              }

              sstmtMutants.push_back(sbuilder.CreateSwitch(
                  sbuilder.CreateAlignedLoad(mutantIDSelectorGlobal, 4),
                  original, nMuts));

              // Remove old terminator link
              lkt->eraseFromParent();
            }

            // Separate Mutants(including original) BB from rest of instr
            // if we have another stmt after this in this BB
            if (!llvm::dyn_cast<llvm::Instruction>(lastInst)->isTerminator()) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
              llvm::BasicBlock *nextBB =
                  llvm::SplitBlock(original, lastInst->getNextNode(), tmp_pass);
#else
              llvm::BasicBlock *nextBB =
                  llvm::SplitBlock(original, lastInst->getNextNode());
#endif
              nextBB->setName(std::string("MART.BBafter.Stmt") +
                              std::to_string(mod_mutstmtcount));

              sstmtCurBB = nextBB;
            } else {
              // llvm::errs() << "Error (Mutation::doMutate): Basic Block '" <<
              // original->getName() << "' has no terminator!\n";
              // return false;
              sstmtCurBB = original;
            }

            // XXX: Insert mutant blocks here
            //@# MUTANTS (see ELSE bellow)
            for (auto ms_ind = 0;
                 ms_ind < (*curSrcStmtIt)->mutantStmt_list.getNumMuts();
                 ms_ind++) {
              auto &mut_stmt_ir =
                  (*curSrcStmtIt)->mutantStmt_list.getMutantStmtIR(ms_ind);
              std::string mutIDstr(std::to_string(
                  (*curSrcStmtIt)->mutantStmt_list.getMutID(ms_ind)));

              // Store mutant info
              mutantsInfos.add(
                  (*curSrcStmtIt)->mutantStmt_list.getMutID(ms_ind),
                  (*curSrcStmtIt)->matchStmtIR.toMatchIRs,
                  (*curSrcStmtIt)->mutantStmt_list.getTypeName(ms_ind),
                  (*curSrcStmtIt)->mutantStmt_list.getIRRelevantPos(ms_ind),
                  &Func, (*curSrcStmtIt)->matchStmtIR.posIRsInOrigFunc);

              // construct Basic Block and insert before original
              std::vector<llvm::BasicBlock *> &mutBlocks =
                  mut_stmt_ir.getMut(&*changingBBIt);

              // Add to mutant selection switch
              for (auto *swches : sstmtMutants)
                swches->addCase(
                    llvm::ConstantInt::get(
                        moduleInfo.getContext(),
                        llvm::APInt(32, (uint64_t)(*curSrcStmtIt)
                                            ->mutantStmt_list.getMutID(ms_ind),
                                    false)),
                    mutBlocks.front());

              for (auto *subBB : mutBlocks) {
                subBB->setName(std::string("MART.Mutant_preTCEMut") + mutIDstr);
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5) //-----------
                assert(!subBB->getParent() && "Already has a parent");

                if (original)
                  Func.getBasicBlockList().insert(
                      llvm::Function::iterator(original), subBB);
                else
                  Func.getBasicBlockList().push_back(subBB);
#else  //----------------
                subBB->insertInto(&Func, original);
#endif //-----------------
              }

              // if we have another stmt after this in this BB
              if (!llvm::dyn_cast<llvm::Instruction>(lastInst)
                       ->isTerminator()) {
                // clone original terminator
                llvm::Instruction *mutTerm = original->getTerminator()->clone();

                // set name
                if (original->getTerminator()->hasName())
                  mutTerm->setName(
                      (original->getTerminator()->getName()).str() + "_Mut" +
                      mutIDstr);

                // set as mutant terminator
                mutBlocks.back()->getInstList().push_back(mutTerm);
              }
            }

            /*//delete previous instructions
            auto rit = sstmt.rbegin();
            for (; rit!= sstmt.rend(); ++rit)
            {
                llvm::dyn_cast<llvm::Instruction>(*rit)->eraseFromParent();
            }*/

            // Help name the labels for mutants
            mod_mutstmtcount++;
          } //~ if(nMuts > 0)
        }
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
        // make 'changeBBIt' point to the last BB before the next
        // one to explore
        changingBBIt = llvm::Function::iterator(sstmtCurBB);
#else
        // make 'changeBBIt' foint to the last BB before the next
        // one to explore
        changingBBIt = sstmtCurBB->getIterator();
#endif
      } // Actual mutation for

// Get to the right block
/*while (&*itBBlock != sstmtCurBB)
{
    itBBlock ++;
}*/

// Do not use changingBBIt here because it is advanced
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
      // make 'changeBBIt' foint to the last BB before the next one to explore
      itBBlock = llvm::Function::iterator(sstmtCurBB);
#else
      // make 'changeBBIt' foint to the last BB before the next one to explore
      itBBlock = sstmtCurBB->getIterator();
#endif

      ///\brief Mutation over for the current set of BB, reinitialize
      ///'mutationStartingAtBB' for the coming ones
      mutationStartingAtBB = nullptr;

      srcStmtsSearchList.clear();

    } // for each BB in Function

    assert(remainMultiBBLiveStmts.empty() &&
           "Something wrong with function (missing IRs) or bug!");

    // repairDefinitionUseDomination(Func);  //TODO: when we want to directly
    // support cross BB use

    // Func.dump();

    Mutation::checkFunctionValidity(
        Func, "ERROR: Misformed Function After mutation!");
  } // for each Function in Module

  //@ Set the Initial Value of mutantIDSelectorGlobal to '<Highest Mutant ID> +
  // 1' (which is equivalent to selecting the original program)
  mutantIDSelectorGlobal->setInitializer(llvm::ConstantInt::get(
      moduleInfo.getContext(),
      llvm::APInt(32, (uint64_t)1 + curMutantID, false)));

  // module.dump();

  Mutation::checkModuleValidity(module,
                                "ERROR: Misformed Module after mutation!");

  return true;
} //~Mutation::doMutate

/**
 * \brief for the statements spawning multiple BB, make sure that the IR used in
 * other BB will not make verifier complain. We fix by adding a phi node at the
 * convergence point of mutants and original.
 * TODO: When we want do support cross BB use
 */
/*void Mutation::repairDefinitionUseDomination(llvm::Function &Func);
{
    llvm::SwitchInst *mutSelSw = nullptr;
    for (auto &BB: Func)
    {
        for (auto &Inst: BB)
        {
            if (mutSelSw = llvm::dyn_cast<llvm::SwitchInst>(&Inst))
            {

            }
        }
    }
}*/

/**
 * \brief obtain the Weak Mutant kill condition and insert it before the
 * instruction @param insertBeforeInst and return the result of the comparison
 * 'original' != 'mutant'
 */
void Mutation::getWMConditions(
    std::vector<llvm::Instruction *> &origUnsafes,
    std::vector<llvm::Instruction *> &mutUnsafes,
    std::vector<std::vector<llvm::Value *>> &conditions) {
  // Look for difference. For now we stop when finding the first unsafe
  // instruction
  if (mutUnsafes.size() == origUnsafes.size()) {
    bool surelyDiffer = false;
    llvm::IRBuilder<> tmpbuilder(moduleInfo.getContext());
    for (unsigned i = 0, ie = origUnsafes.size(); i < ie; ++i) {
      if (!origUnsafes[i]->isSameOperationAs(
              mutUnsafes[i], llvm::Instruction::CompareUsingScalarTypes)) {
        surelyDiffer = true;
        break;
      }
      std::vector<llvm::Value *> subconds;
      for (unsigned j = 0, je = origUnsafes[i]->getNumOperands(); j < je; ++j) {
        llvm::Value *o_oprd = origUnsafes[i]->getOperand(j);
        llvm::Value *m_oprd = mutUnsafes[i]->getOperand(j);

        // As long as oprds are equals keep checking
        if (o_oprd == m_oprd)
          continue;

        // If an operand is a BasicBlock (e.g.: switch, br), they  Surely differ
        if (llvm::isa<llvm::BasicBlock>(o_oprd) && o_oprd != m_oprd) {
          surelyDiffer = true;
          break;
        }

        llvm::Type *o_type = o_oprd->getType();
        llvm::Type *m_type = m_oprd->getType();

        if (!o_type->isSingleValueType() || o_type->isX86_MMXTy()) {
          if (o_oprd != m_oprd) {
            surelyDiffer = true;
            break;
          }
        } else {
          if (o_type->isIntOrIntVectorTy() || o_type->isPtrOrPtrVectorTy()) {
            if (o_type->isPointerTy() &&
                llvm::dyn_cast<llvm::PointerType>(o_type)->getElementType() !=
                    llvm::dyn_cast<llvm::PointerType>(m_type)
                        ->getElementType()) {
              surelyDiffer = true;
              break;
            }
            // create condition
            subconds.push_back(tmpbuilder.CreateICmpNE(o_oprd, m_oprd));
          } else if (o_type->isFPOrFPVectorTy()) {
            // create condition
            subconds.push_back(tmpbuilder.CreateFCmpUNE(o_oprd, m_oprd));
          } else {
            //origUnsafes[i]->dump(); mutUnsafes[i]->dump();
            //o_oprd->dump(); o_type->dump();
            //m_oprd->dump(); m_type->dump();
            assert(false && "Unreachable");
          }
        }
      }
      // make final condition (and of all subconditions)
      conditions.push_back(subconds);

      if (surelyDiffer) // anything added in subconds will be removed bellow
        break;
    }
    if (!surelyDiffer)
      return;
  }

  // condition here (original != Mutant)
  // return llvm::ConstantInt::getTrue(moduleInfo.getContext());
  for (auto &subcond : conditions)
    for (auto *cinst : subcond)
      if (llvm::isa<llvm::Instruction>(cinst)) {
        llvm::dyn_cast<llvm::User>(cinst)->dropAllReferences();
#if (LLVM_VERSION_MAJOR < 5)
        delete cinst;
#else
        cinst->deleteValue();
#endif
      }
  conditions.clear();
  conditions.push_back(std::vector<llvm::Value *>({llvm::ConstantInt::getTrue(
      moduleInfo.getContext())})); // true (there is difference - weakly killed)
}

/**
 * \brief transform non optimized meta-mutant module into weak mutation module.
 * @param cmodule is the meta mutant module. @note: it will be transformed into
 * WM module, so clone module before this call
 */
void Mutation::computeWeakMutation(std::unique_ptr<llvm::Module> &cmodule,
                                   std::unique_ptr<llvm::Module> &modWMLog) {
  llvm::errs() << "Computing weak mutation labels...\n"; ////DBG

  assert(!cmodule->getFunction(wmLogFuncName) && "Name clash for weak mutation "
                                                 "log function Name, please "
                                                 "change it from your program");
  assert(!cmodule->getFunction(wmFFlushFuncName) &&
         "Name clash for weak mutation FFlush function Name, please change it "
         "from you program");
  assert(!cmodule->getGlobalVariable(wmHighestMutantIDConst) &&
         "Name clash for weak mutation highest mutant ID Global Variable Name, "
         "please change it from you program");

/// Link cmodule with the corresponding driver module (actually only need c
/// module)
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
  llvm::Linker linker(cmodule.get());
  std::string ErrorMsg;
  if (linker.linkInModule(modWMLog.get(), &ErrorMsg)) {
    llvm::errs()
        << "Failed to link weak mutation module with log function module: "
        << ErrorMsg << "\n";
    assert(false);
  }
  modWMLog.reset(nullptr);
#else
  llvm::Linker linker(*cmodule);
  if (linker.linkInModule(std::move(modWMLog))) {
    assert(false &&
           "Failed to link weak mutation module with log function module");
  }
#endif

  // Strip useless debugging infos
  llvm::StripDebugInfo(*cmodule.get());

  llvm::Function *funcWMLog = cmodule->getFunction(wmLogFuncName);
  llvm::Function *funcWMFflush = cmodule->getFunction(wmFFlushFuncName);
  llvm::GlobalVariable *constWMHighestID =
      cmodule->getNamedGlobal(wmHighestMutantIDConst);
  assert(funcWMLog && "Weak Mutation Log Function absent in WM Module. Was it "
                      "linked properly?");
  assert(funcWMFflush && "Weak Mutation FFlush Function absent in WM Module. "
                         "Was it linked properly?");
  assert(constWMHighestID && "Weak Mutation WM Highest mutant ID absent in WM "
                             "Module. Was it linked properly?");
  constWMHighestID->setInitializer(llvm::ConstantInt::get(
      moduleInfo.getContext(),
      llvm::APInt(32, (uint64_t)getHighestMutantID(cmodule.get()), false)));
  constWMHighestID->setConstant(true);

  for (auto &Func : *cmodule) {
    if (&Func == funcWMLog || &Func == funcWMFflush)
      continue;
    for (auto &BB : Func) {
      std::vector<llvm::BasicBlock *> toBeRemovedBB;
      for (llvm::BasicBlock::iterator Iit = BB.begin(), Iie = BB.end();
           Iit != Iie;) {
        // we increment here so that 'eraseFromParent' bellow do not cause crash
        llvm::Instruction &Inst = *Iit++;
        if (auto *callI = llvm::dyn_cast<llvm::CallInst>(&Inst)) {
          // Weak mutation is not seen by KLEE, no need to keep KLEE's function.
          if (forKLEESEMu &&
              callI->getCalledFunction() ==
                  cmodule->getFunction(mutantIDSelectorName_Func)) {
            callI->eraseFromParent();
          }
        }
        if (auto *sw = llvm::dyn_cast<llvm::SwitchInst>(&Inst)) {
          if (auto *ld = llvm::dyn_cast<llvm::LoadInst>(sw->getCondition())) {
            // No case means that all the mutant at this position
            // were duplicate, no need to add label
            if (ld->getOperand(0) ==
                    cmodule->getNamedGlobal(mutantIDSelectorName) &&
                sw->getNumCases() > 0) {
              std::vector<llvm::ConstantInt *> cases;
              std::vector<llvm::Value *> argsv;
              auto *defaultBB = sw->getDefaultDest(); // original

              // Remove unused instructions (like dangling load created by
              // mutation) XXX
              llvm::SimplifyInstructionsInBlock(defaultBB);

              std::vector<llvm::Instruction *> origUnsafes;
              getWMUnsafeInstructions(defaultBB, origUnsafes);
              std::vector<llvm::IRBuilder<>> sbuilders;
              for (auto *II : origUnsafes)
                sbuilders.emplace_back(II);
              for (llvm::SwitchInst::CaseIt i = sw->case_begin(),
                                            e = sw->case_end();
                   i != e; ++i) {
#if (LLVM_VERSION_MAJOR <= 4)
                auto *mutIDConstInt = i.getCaseValue();
                
                /// Now create the call to weak mutation log func.
                auto *caseiBB = i.getCaseSuccessor(); // mutant
#else
                auto *mutIDConstInt = (*i).getCaseValue();
                
                /// Now create the call to weak mutation log func.
                auto *caseiBB = (*i).getCaseSuccessor(); // mutant
#endif
                // to be removed later
                cases.push_back(mutIDConstInt); 

                /// Since each case of switch correspond to a mutant and 
                /// each mutant has its own Basic Block, 
                /// there cannot be redundancy in toBeRemovedBB
                toBeRemovedBB.push_back(caseiBB);

                // Remove unused instructions (like dangling load created by
                // mutation) XXX
                llvm::SimplifyInstructionsInBlock(caseiBB);

                std::vector<llvm::Instruction *> mutUnsafes;
                getWMUnsafeInstructions(caseiBB, mutUnsafes);
                std::vector<std::vector<llvm::Value *>> condVals;
                getWMConditions(origUnsafes, mutUnsafes, condVals);

                assert(origUnsafes.size() >= condVals.size() &&
                       "at most 1 condition per unsafe inst");
                // assert (!condVals.empty() && "there must be at least on
                // condition");  //no condition mean that the mutant is
                // equivalent (maybe only safe instructions)

                /// Move the relevant code in the mutant for computing the 
                /// Conditions into the final WM part (original's code)
                for (unsigned ic = 0, ice = condVals.size(); ic < ice; ++ic) {
                  if (ic == 0) {
                    auto insert = &*(defaultBB->begin());
                    for (auto mutFirst = caseiBB->begin(),
                              mutEnd = caseiBB->end();
                         mutFirst != mutEnd;) {
                      auto minst = &*(mutFirst++);
                      if (minst == mutUnsafes[0])
                        break;
                      minst->moveBefore(insert);
                    }
                  } else {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
                    auto insert =
                        &*(++llvm::BasicBlock::iterator(origUnsafes[ic - 1]));
                    auto mutFirst =
                        ++llvm::BasicBlock::iterator(mutUnsafes[ic - 1]);
                    auto mutEnd = llvm::BasicBlock::iterator(mutUnsafes[ic]);
#else
                    auto insert = &*(++(origUnsafes[ic - 1]->getIterator()));
                    auto mutFirst = ++(mutUnsafes[ic - 1]->getIterator());
                    auto mutEnd = (mutUnsafes[ic]->getIterator());
#endif
                    for (; mutFirst != mutEnd;) {
                      auto minst = &*(mutFirst++);
                      if (minst == mutUnsafes[ic])
                        break;
                      minst->moveBefore(insert);
                    }
                  }

                  // Mutant is probably non equivalent to original at this
                  // unsafe
                  if (!condVals[ic].empty()) {
                    for (auto *condtmp : condVals[ic]) {
                      auto *subcondInst =
                          llvm::dyn_cast<llvm::Instruction>(condtmp);
                      if (subcondInst)
                        subcondInst->insertBefore(origUnsafes[ic]);
                      if (condVals[ic].front() != condtmp)
                        condVals[ic].front() = sbuilders[ic].CreateOr(
                            condVals[ic].front(), condtmp);
                    }
                    condVals[ic].front() = sbuilders[ic].CreateZExt(
                        condVals[ic].front(),
                        llvm::Type::getInt8Ty(
                            moduleInfo.getContext())); // convert i1 into i8
                    argsv.clear();
                    argsv.push_back(mutIDConstInt); // mutant ID
                    // weak kill condition
                    argsv.push_back(condVals[ic].front());
                    // call WM log func
                    sbuilders[ic].CreateCall(funcWMLog, argsv); 
                  }
                }

                // make sure that all copied inst that use mutant unsafe now use
                // orig unsafe (execution reach here with weakly alive only if
                // both are equivalent before)
                // XXX we put this here at the end to make sure that the
                // mutUnsafe uses, kept in condVals are all applied completely,
                // otherwise some won't be changed
                for (unsigned ic = 0, ice = condVals.size(); ic < ice; ++ic) {
                  if (mutUnsafes[ic]->getType() != origUnsafes[ic]->getType())
                    continue;
                  mutUnsafes[ic]->replaceAllUsesWith(origUnsafes[ic]);
                }

                // Drop all references of mut unsafes (after this , mutUnsafe
                // should not be used)
                for (auto *uns : mutUnsafes) {
                  uns->dropAllReferences();
                }
              }

              // Now call fflush
              argsv.clear();
              sbuilders.back().CreateCall(funcWMFflush, argsv);

              for (auto *caseval : cases) {
                llvm::SwitchInst::CaseIt cit = sw->findCaseValue(caseval);
                sw->removeCase(cit);
              }
            }
          }
        }
      }
      for (auto *bbrm : toBeRemovedBB)
        bbrm->eraseFromParent();
    }
  }
  if (forKLEESEMu) {
    llvm::Function *funcForKS = cmodule->getFunction(mutantIDSelectorName_Func);
    funcForKS->eraseFromParent();
  }
  llvm::GlobalVariable *mutantIDSelGlob =
      cmodule->getNamedGlobal(mutantIDSelectorName);
  // mutantIDSelGlob->setInitializer(llvm::ConstantInt::get(moduleInfo.getContext(),
  // llvm::APInt(32, (uint64_t)0, false)));    //Not needed because there is no
  // case anyway, only default
  if (mutantIDSelGlob)
    mutantIDSelGlob->setConstant(true); // only original...

  // verify WM module
  Mutation::checkModuleValidity(*cmodule, "ERROR: Misformed WM Module!");

  llvm::errs() << "Computing weak mutation done!\n"; ////DBG
} //~Mutation::computeWeakMutation

/*
 * Compute the module fo mutant coverage (the tests that covers the mutant 
 * (do not necessarily weakly or strongly kill)
 */
void Mutation::computeMutantCoverage(std::unique_ptr<llvm::Module> &metaModule, 
                                  std::unique_ptr<llvm::Module> &modCovLog) {
  llvm::errs() << "Computing mutation coverage labels...\n"; ////DBG

  assert (forKLEESEMu && "Coverage requires forKLEESEMu enable (it replaces it)");

  assert(!metaModule->getFunction(covLogFuncName) && "Name clash for mutation coverage"
                                                 "log function Name, please "
                                                 "change it from you program");
  assert(!metaModule->getGlobalVariable(wmHighestMutantIDConst) &&
         "Name clash for weak mutation highest mutant ID Global Variable Name, "
         "please change it from you program");

/// Link metaModule with the corresponding driver module (actually only need c
/// module)
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
  llvm::Linker linker(metaModule.get());
  std::string ErrorMsg;
  if (linker.linkInModule(modCovLog.get(), &ErrorMsg)) {
    llvm::errs()
        << "Failed to link mutation coverage module with log function module: "
        << ErrorMsg << "\n";
    assert(false);
  }
  modCovLog.reset(nullptr);
#else
  llvm::Linker linker(*metaModule);
  if (linker.linkInModule(std::move(modCovLog))) {
    assert(false &&
           "Failed to link weak mutation module with log function module");
  }
#endif

  // Strip useless debugging infos
  llvm::StripDebugInfo(*metaModule.get());

  llvm::Function *funcCovLog = metaModule->getFunction(covLogFuncName);
  llvm::GlobalVariable *constCovHighestID =
      metaModule->getNamedGlobal(wmHighestMutantIDConst);
  assert(funcCovLog && "Mutation coverage Log Function absent in Coverage Module. Was it "
                      "linked properly?");
  assert(constCovHighestID && "Mutation coverage coverage Highest mutant ID absent in"
                             "Coverage Module. Was it linked properly?");
  constCovHighestID->setInitializer(llvm::ConstantInt::get(
      moduleInfo.getContext(),
      llvm::APInt(32, (uint64_t)getHighestMutantID(metaModule.get()), false)));
  constCovHighestID->setConstant(true);

  llvm::Function *funcForKS = metaModule->getFunction(mutantIDSelectorName_Func);
  llvm::GlobalVariable *mutantIDSelGlob =
      metaModule->getNamedGlobal(mutantIDSelectorName);

  for (auto &Func : *metaModule) {
    if (&Func == funcCovLog)
      continue;
    
    // remove all mutants, keep oly original
    cleanFunctionToMut(Func, 0/*original mutantID*/, mutantIDSelGlob, funcForKS, false/*verify*/, false/*remove ks func calls*/);
  }

  // make KS function call cov log function
  funcForKS->deleteBody();
  llvm::BasicBlock *block =
      llvm::BasicBlock::Create(metaModule->getContext(), "entry", funcForKS);
  llvm::IRBuilder<> builder(block);
  llvm::Function::arg_iterator args = funcForKS->arg_begin();
  std::vector<llvm::Value *> argsv;
  argsv.push_back(llvm::dyn_cast<llvm::Value>(args++));
  argsv.push_back(llvm::dyn_cast<llvm::Value>(args++));
  builder.CreateCall(funcCovLog, argsv);
  builder.CreateRetVoid();
  
  if (mutantIDSelGlob)
    mutantIDSelGlob->setConstant(true); // only original...

  // verify WM module
  Mutation::checkModuleValidity(*metaModule, "ERROR: Misformed Mutant Coverage Module!");

  llvm::errs() << "Computing mutant-coverage done!\n"; ////DBG
}

/**
 * \brief If targetF is non nul, it should be the function corresponding to srcF
 * in Mod
 */
void Mutation::setModFuncToFunction(llvm::Module *Mod, llvm::Function *srcF,
                                    llvm::Function *targetF) {
  if (targetF == nullptr)
    targetF = Mod->getFunction(srcF->getName());
  else
    assert(targetF->getParent() == Mod &&
           "passed targetF which is not a function of module");
  assert(targetF && "the function Fun do not have an instance in module Mod");
  llvm::ValueToValueMapTy vmap;
  llvm::Function::arg_iterator destI = targetF->arg_begin();
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
  for (auto argIt = srcF->arg_begin(), argE = srcF->arg_end(); argIt != argE;
       ++argIt) {
    const llvm::Argument &aII = *argIt;
#else
  for (const llvm::Argument &aII : srcF->args()) {
#endif
    if (vmap.count(&aII) == 0)
      vmap[&aII] = &*destI++;
  }

  llvm::SmallVector<llvm::ReturnInst *, 8> Returns;
  targetF->dropAllReferences(); // deleteBody();
  llvm::CloneFunctionInto(targetF, srcF, vmap, true, Returns);
  // verify post clone into module
  // Mutation::checkModuleValidity(*Mod, "ERROR: Misformed module after
  // cloneFunctionInto!");
}

/**
 * \brief define the data structure that will be used by TCE and implements
 * their update when a new mutant is seen
 * TODO: add namespace around this
 */
struct DuplicateEquivalentProcessor {
  TCE tce;

  std::map<MutantIDType, std::vector<MutantIDType>> duplicateMap;

  std::map<MutantIDType, MutantIDType> dup2nondupMap;

  ///\brief mutant's function or module. that which will be used when
  /// serializing mutants to file.
  std::vector<llvm::Module *> mutModules;
  std::vector<llvm::Function *> mutFunctions;

  /// \brief the key of the map is the index (staring from 0) of the function in
  /// the original's optimized module, or -1 for any function not present in
  /// original optimized
  /// \brief the value is a vector of the ids of all the mutants (non dup) in
  /// duplicateMap for which the function is different than the orig's.
  std::unordered_map<llvm::Function *, std::vector<MutantIDType>> diffFuncs2Muts;

  /// \brief map to lookup the module(value) cleaned for each function (key)
  std::unordered_map<llvm::Function *, ReadWriteIRObj> inMemIRModBufByFunc;
  std::unordered_map<llvm::Function *, llvm::Module *> clonedModByFunc;

  /// \brief list of functions, according to meta-mutant module (current
  /// 'module').
  /// here we just need the function name, or the function to lookup module in
  /// 'clonedModByFunc' or 'inMemIRModBufByFunc'
  // nullptr mean more than 1 function mutated, or for the
  // original(0 function mutated)
  std::vector<llvm::Function *> funcMutByMutID;

  std::vector<llvm::SmallVector<llvm::BasicBlock *, 2>> diffBBWithOrig;

  bool isTCEFunctionMode;

  DuplicateEquivalentProcessor(MutantIDType highestMutID, bool is_tce_func_mode)
      : isTCEFunctionMode(is_tce_func_mode) {
    funcMutByMutID.resize(highestMutID + 1, nullptr);
    diffBBWithOrig.resize(highestMutID + 1);
  }

  /// Populate dup2nondupMap using the dupliate informations from duplicateMap
  void createEqDup2NonEqDupMap() {
    assert (dup2nondupMap.empty() && "Error: dup2nondupMap must be empty here!");
    for (auto &m_pair: duplicateMap) {
      for (MutantIDType dupId: m_pair.second) {
        assert (dup2nondupMap.count(dupId) == 0 && "Error: duplicate mutant of many non dupliate");
        dup2nondupMap[dupId] = m_pair.first;
      }
    }
  }

  void update(MutantIDType mutant_id, llvm::Module *clonedOrig,
              llvm::Module *clonedM) {
    std::vector<llvm::Function *> mutatedFuncsOfMID;
    if (isTCEFunctionMode) {
      if (tce.functionDiff(
              clonedOrig->getFunction(funcMutByMutID[mutant_id]->getName()),
              mutFunctions[mutant_id], &(diffBBWithOrig[mutant_id])))
        mutatedFuncsOfMID.push_back(
            clonedOrig->getFunction(funcMutByMutID[mutant_id]->getName()));

      // If there was difference with original process bellow
    } else {
      tce.checkWithOrig(clonedOrig, clonedM, mutatedFuncsOfMID);
    }

    if (mutatedFuncsOfMID.empty()) // equivalent with orig
    {
      duplicateMap.at(0).push_back(mutant_id);

      // delete its function to free memory space
      if (isTCEFunctionMode) {
        delete mutFunctions[mutant_id];
        mutFunctions[mutant_id] = nullptr;
      }
      // llvm::errs() << mutant_id << " is equivalent\n"; /////DBG
    } else {
      // Currently only support a mutant in a single funtion. TODO TODO: extent
      // to mutant cross function
      assert(mutatedFuncsOfMID.size() == 1 &&
             "//Currently only support a mutant in a single funtion. TODO "
             "TODO: extent to mutant cros function");

      bool hasEq = false;
      for (auto *mF : mutatedFuncsOfMID) {
        llvm::Function *subjFunc;
        if (isTCEFunctionMode)
          subjFunc = mutFunctions[mutant_id];
        else
          subjFunc = clonedM->getFunction(mF->getName());

        for (auto candID : diffFuncs2Muts.at(mF)) {
          llvm::Function *candFunc;
          if (isTCEFunctionMode) {
            // prune useless comparisons
            if (diffBBWithOrig[mutant_id].size() !=
                diffBBWithOrig[candID].size())
              continue;
            bool dbbdf = false;
            for (unsigned dbbInd = 0, dbbe = diffBBWithOrig[mutant_id].size();
                 dbbInd < dbbe; dbbInd++) {
              if (diffBBWithOrig[mutant_id][dbbInd] !=
                  diffBBWithOrig[candID][dbbInd]) {
                dbbdf = true;
                break;
              }
            }
            if (dbbdf)
              continue;

            candFunc = mutFunctions[candID];
          } else {
            candFunc = mutModules.at(candID)->getFunction(mF->getName());
          }

          if (!subjFunc) {
            if (candFunc)
              continue;

            hasEq = true;
            duplicateMap.at(candID).push_back(mutant_id);
            break;
          } else {
            if (!candFunc)
              continue;

            if (!tce.functionDiff(candFunc, subjFunc, nullptr)) {
              hasEq = true;
              duplicateMap.at(candID).push_back(mutant_id);
              break;
            }
          }
        }
      }
      if (!hasEq) {
        duplicateMap[mutant_id]; // insert id into the map
        for (auto *mF : mutatedFuncsOfMID)
          diffFuncs2Muts.at(mF).push_back(mutant_id);
      } else {
        // delete its function to free memory space
        if (isTCEFunctionMode) {
          delete mutFunctions[mutant_id];
          mutFunctions[mutant_id] = nullptr;
        }
        // llvm::errs() << mutant_id << " is duplicate\n"; /////DBG
      }
    }
  }
}; //~ struct DuplicateEquivalentProcessor

void Mutation::doTCE(std::unique_ptr<llvm::Module> &optMetaMu, std::unique_ptr<llvm::Module> &modWMLog, 
                    std::unique_ptr<llvm::Module> &modCovLog, bool writeMuts,
                    bool isTCEFunctionMode) {
  assert(currentMetaMutantModule && "Running TCE before mutation");
  llvm::Module &module = *currentMetaMutantModule;

  llvm::GlobalVariable *mutantIDSelGlob =
      module.getNamedGlobal(mutantIDSelectorName);
  assert(mutantIDSelGlob && "Unmutated module passed to TCE");

  MutantIDType highestMutID = getHighestMutantID(&module);

  DuplicateEquivalentProcessor dup_eq_processor(highestMutID,
                                                isTCEFunctionMode);

  ///\brief keep the original's module up to end of this function
  llvm::Module *clonedOrig = nullptr;

  /// make the new module that will have no metadata, to hopefully make
  /// mutantion TCE and write/ comilation faster
  std::unique_ptr<llvm::Module> subjModule(
      ReadWriteIRObj::cloneModuleAndRelease(&module));
  llvm::StripDebugInfo(*subjModule);

  if (isTCEFunctionMode) {
    dup_eq_processor.mutFunctions.clear();
    dup_eq_processor.mutFunctions.resize(highestMutID + 1, nullptr);
    llvm::errs() << "Cloning...\n"; //////DBG
    computeModuleBufsByFunc(*subjModule, nullptr,
                            &dup_eq_processor.clonedModByFunc,
                            dup_eq_processor.funcMutByMutID);

    clonedOrig = ReadWriteIRObj::cloneModuleAndRelease(
        dup_eq_processor.clonedModByFunc.at(
            dup_eq_processor.funcMutByMutID[0]));

    // The original
    assert(getMutant(*clonedOrig, 0, dup_eq_processor.funcMutByMutID[0],
                     'A' /*optimizeAllFunctions*/) &&
           "error: failed to get original");
  } else {
    dup_eq_processor.mutModules.clear();
    dup_eq_processor.mutModules.resize(highestMutID + 1, nullptr);
    llvm::errs() << "Cloning...\n"; //////DBG
    computeModuleBufsByFunc(*subjModule, &dup_eq_processor.inMemIRModBufByFunc,
                            nullptr, dup_eq_processor.funcMutByMutID);
    // Read all the mutantmodules possibly in parallel
    /*#ifdef ENABLE_OPENMP_PARALLEL
         omp_set_num_threads(std::min(atoi(std::getenv("OMP_NUM_THREADS")),
    std::max(omp_get_max_threads()/2, 1)));
         #pragma omp parallel for
    #endif*/
    for (MutantIDType id = 0; id <= highestMutID; id++) // id==0 is the original
    {
      dup_eq_processor.mutModules[id] =
          dup_eq_processor.inMemIRModBufByFunc
              .at(dup_eq_processor.funcMutByMutID[id])
              .readIR();
      if (id % 100 == 0)
        llvm::errs() << "Cloned up to " << id << ". ";
    }

    // The original
    clonedOrig = dup_eq_processor.mutModules[0];
    assert(getMutant(*clonedOrig, 0, dup_eq_processor.funcMutByMutID[0],
                     'M' /*optimizeModule*/) &&
           "error: failed to get original");
  }

  // The original cont.
  dup_eq_processor.duplicateMap[0]; // insert 0 into the map
  // initialize 'dup_eq_processor.diffFuncs2Muts'
  dup_eq_processor.diffFuncs2Muts[nullptr]; // for function not in original
  for (auto &origFunc : *clonedOrig)
    dup_eq_processor.diffFuncs2Muts[&origFunc];

  // The mutants

  /// \brief since the mutants of the same function have sequential ID, we use
  /// this to trac function change and do some operation only then...
  llvm::Function *curFunc_ForDebug = nullptr; ////DBG

  std::vector<bool> visitedEqDupMutants(highestMutID + 1, false);

  for (MutantIDType id = 1; id <= highestMutID; id++) // id==0 is the original
  {
    llvm::Module *clonedM = nullptr;

    // Currently only support a mutant in a single funtion. TODO TODO: extent to
    // mutant cros function
    assert(dup_eq_processor.funcMutByMutID[id] != nullptr &&
           "//Currently only support a mutant in a single funtion "
           "(dup_eq_processor.funcMutByMutID[id]). TODO TODO: extent to mutant "
           "cros function");

    if (curFunc_ForDebug != dup_eq_processor.funcMutByMutID[id]) {
      curFunc_ForDebug = dup_eq_processor.funcMutByMutID[id];
      llvm::errs() << "\nprocessing Func: " << curFunc_ForDebug->getName()
                   << ", Starting at mutant: " << id << "/" << highestMutID
                   << "\n\t";
    }

    // Check with original
    if (isTCEFunctionMode) {
      if (!visitedEqDupMutants[id]) {
        /// get 'mutFunctions' for all mutants in same function as 'id'. @Note:
        /// each mutant in only one funtion
        /// do this by using binary approach (divide an conquer) fo scalability
        /// (avoid cloning useles code)

        /// \brief get the module for each function (when cleaning all other
        /// funcs)
        std::stack<
            std::tuple<llvm::Function *, MutantIDType /*From*/, MutantIDType /*To*/>>
            workFStack;

        llvm::ValueToValueMapTy vmap;

        //TODO TODO: Dump previously found function's mutants here to save memory
        MutantIDType maxIDOfFunc = id;
        while (dup_eq_processor.funcMutByMutID[maxIDOfFunc] ==
                   dup_eq_processor.funcMutByMutID[id] &&
               maxIDOfFunc <= highestMutID)
          maxIDOfFunc++;
        maxIDOfFunc--;

        clonedM = dup_eq_processor.clonedModByFunc.at(
            dup_eq_processor.funcMutByMutID[id]);
        const std::string subjFunctionName =
            dup_eq_processor.funcMutByMutID[id]->getName();
        llvm::GlobalVariable *mutantIDSelGlobFF =
            clonedM->getNamedGlobal(mutantIDSelectorName);
        llvm::Function *mutantIDSelGlob_FuncFF =
            clonedM->getFunction(mutantIDSelectorName_Func);

        /// \brief in order to make optimization and function clone (with debug
        /// data), the function need to be in a module.
        /// This string is a name for a temporal function (global value) not yet
        /// in module, that will be used to temporally
        /// add the subject function for clone and optimization
        std::string temporaryFname(mutantIDSelectorName_Func +
                                   std::string("tmp"));
        unsigned uniq = 0;
        while (clonedM->getNamedValue(temporaryFname + std::to_string(uniq)))
          uniq++;
        temporaryFname += std::to_string(uniq);

        unsigned progressVerbose = 0;
        unsigned nextProgress = 5;
        unsigned progressVLandmark = (maxIDOfFunc - id) * nextProgress / 100;

        vmap.clear();
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 9)
        llvm::Function *cloneFuncTmp = llvm::CloneFunction(
                                clonedM->getFunction(subjFunctionName), vmap,
                                true /*moduleLevelChanges*/);
#else
        llvm::Function *cloneFuncTmp = llvm::CloneFunction(
                                clonedM->getFunction(subjFunctionName), vmap);
        // Here CloneFunction automatically add to module so remove 
        // FIXME: Make it better by chnaging code to have it adde here fine
        cloneFuncTmp->removeFromParent();
#endif
        workFStack.emplace(cloneFuncTmp, id, maxIDOfFunc);

        /// \brief Use binary approach(divide and conquer) to quickly obtain the
        /// module for each function. use DFS here to save memory (once seen
        /// process and delete duplicate)
        while (!workFStack.empty()) {
          auto &queueElem = workFStack.top();
          MutantIDType min = std::get<1>(queueElem), max = std::get<2>(queueElem);
          llvm::Function *cloneFuncL = std::get<0>(queueElem);
          workFStack.pop();

          if (min == max) {
            // add to module as temporary name
            cloneFuncL->setName(temporaryFname);
            clonedM->getFunctionList().push_back(cloneFuncL);

            // get final optimized function for mutant
            cleanFunctionToMut(*cloneFuncL, min, mutantIDSelGlobFF,
                               mutantIDSelGlob_FuncFF);
            dup_eq_processor.tce.optimize(*cloneFuncL,
                                          Mutation::funcModeOptLevel);
            dup_eq_processor.mutFunctions[min] = cloneFuncL;
            visitedEqDupMutants[min] = true; // visit

            // remove from module and set back original name
            cloneFuncL->removeFromParent();
            cloneFuncL->setName(subjFunctionName);

            // Process the mutant with TCE
            dup_eq_processor.update(min, clonedOrig, clonedM);

            // Progress  -- VERBOSE
            ++progressVerbose;
            if (progressVerbose == progressVLandmark) {
              llvm::errs() << nextProgress << "% ";
              nextProgress += 5;
              progressVLandmark = (maxIDOfFunc - id) * nextProgress / 100;
            }
          } else {
            MutantIDType mid = min + (max - min) / 2;

            // add to module as temporary name
            cloneFuncL->setName(temporaryFname);
            clonedM->getFunctionList().push_back(cloneFuncL);

            // Clone
            vmap.clear();
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 9)
            llvm::Function *cloneFuncR = llvm::CloneFunction(
                cloneFuncL, vmap, true /*moduleLevelChanges*/);
#else
            llvm::Function *cloneFuncR = llvm::CloneFunction(
                cloneFuncL, vmap);
            // Here CloneFunction automatically add to module so remove 
            // FIXME: Make it better by chnaging code to have it adde here fine
            cloneFuncR->removeFromParent();
#endif

            // remove from module and set back original name
            cloneFuncL->removeFromParent();
            cloneFuncL->setName(subjFunctionName);

            assert(
                (min <= mid && mid < max) &&
                "shlould reach here only if we have at least 2 ids uncleaned");

            // [min, mid]
            cleanFunctionSWmIDRange(*cloneFuncL, min, mid, mutantIDSelGlobFF,
                                    mutantIDSelGlob_FuncFF);
            // left side has been cleaned, now add remaining right side to be
            // processed next
            workFStack.emplace(cloneFuncL, mid + 1, max);

            // [mid+1, max]
            cleanFunctionSWmIDRange(*cloneFuncR, mid + 1, max,
                                    mutantIDSelGlobFF, mutantIDSelGlob_FuncFF);
            // right side have been cleaned now add the remaining left side to
            // be processed next
            workFStack.emplace(cloneFuncR, min, mid);
          }
        }

        clonedM = nullptr;

        // set the correct function for this mutant in the corresponding
        // function module
        // assert (getMutant (*clonedM, id, dup_eq_processor.funcMutByMutID[id],
        // 'F') && "error: failed to get mutant");

      } else ///~ for "if (!visitedEqDupMutants[id])"
      {
        // already processed during DFS
        continue;
      }
    } else ///~ for "if (isTCEFunctionMode)"
    {
      clonedM = dup_eq_processor.mutModules[id];
      assert(
          getMutant(*clonedM, id, dup_eq_processor.funcMutByMutID[id], 'M') &&
          "error: failed to get mutant");
      dup_eq_processor.update(id, clonedOrig, clonedM);
    }
  }

  llvm::errs() << "Done processing Funcs!\n"; ////DBG

  // Store some statistics about the mutants
  preTCENumMuts = highestMutID;
  postTCENumMuts = dup_eq_processor.duplicateMap.size() -
                   1; // -1 because the original is also in
  numEquivalentMuts = dup_eq_processor.duplicateMap.at(0).size();
  numDuplicateMuts = preTCENumMuts - postTCENumMuts - numEquivalentMuts;
  //

  /*for (auto &p :dup_eq_processor.duplicateMap)
  {
      llvm::errs() << p.first << " <--> {";
      for (auto eq: p.second)
          llvm::errs() << eq << " ";
      llvm::errs() << "}\n";
  }*/

  // create the equivalent duplicate 2 non eq/dup map. call this only once
  dup_eq_processor.createEqDup2NonEqDupMap();

  // re-assign the ids of mutants
  dup_eq_processor.duplicateMap.erase(0); // Remove original
  MutantIDType newmutIDs = 1;

  // The keys of dup_eq_processor.duplicateMap are (must be) sorted in
  // increasing order: helpful when enabled 'forKLEESEMu'
  for (auto &mm : dup_eq_processor.duplicateMap) {
    mm.second.clear();
    mm.second.push_back(newmutIDs++);
  }

  // update mutants infos
  mutantsInfos.postTCEUpdate(dup_eq_processor.duplicateMap, dup_eq_processor.dup2nondupMap);

  for (auto &Func : module) {
    for (auto &BB : Func) {
      std::vector<llvm::BasicBlock *> toBeRemovedBB;
      for (llvm::BasicBlock::iterator Iit = BB.begin(), Iie = BB.end();
           Iit != Iie;) {
        // we increment here so that 'eraseFromParent' bellow do not cause crash
        llvm::Instruction &Inst = *Iit++;
        if (auto *callI = llvm::dyn_cast<llvm::CallInst>(&Inst)) {
          if (forKLEESEMu &&
              callI->getCalledFunction() ==
                  module.getFunction(mutantIDSelectorName_Func)) {
            uint64_t fromMID =
                llvm::dyn_cast<llvm::ConstantInt>(callI->getArgOperand(0))
                    ->getZExtValue();
            uint64_t toMID =
                llvm::dyn_cast<llvm::ConstantInt>(callI->getArgOperand(1))
                    ->getZExtValue();
            unsigned newFromi = 0, newToi = 0;
            for (auto i = fromMID; i <= toMID; i++) {
              if (dup_eq_processor.duplicateMap.count(i) != 0) {
                newToi = i;
                if (newFromi == 0)
                  newFromi = i;
              }
            }
            if (newToi == 0) {
              callI->eraseFromParent();
            } else {
              callI->setArgOperand(
                  0,
                  llvm::ConstantInt::get(
                      moduleInfo.getContext(),
                      llvm::APInt(32, (uint64_t)(dup_eq_processor.duplicateMap
                                                     .at(newFromi)
                                                     .front()),
                                  false)));
              callI->setArgOperand(
                  1,
                  llvm::ConstantInt::get(
                      moduleInfo.getContext(),
                      llvm::APInt(
                          32,
                          (uint64_t)(
                              dup_eq_processor.duplicateMap.at(newToi).front()),
                          false)));
            }
          }
        }
        if (auto *sw = llvm::dyn_cast<llvm::SwitchInst>(&Inst)) {
          if (auto *ld = llvm::dyn_cast<llvm::LoadInst>(sw->getCondition())) {
            if (ld->getOperand(0) ==
                module.getNamedGlobal(mutantIDSelectorName)) {
              uint64_t fromMID = highestMutID;
              uint64_t toMID = 0;
              for (llvm::SwitchInst::CaseIt i = sw->case_begin(),
                                            e = sw->case_end();
                   i != e; ++i) {
#if (LLVM_VERSION_MAJOR <= 4)
                uint64_t curcase = i.getCaseValue()->getZExtValue();
#else
                uint64_t curcase = (*i).getCaseValue()->getZExtValue();
#endif
                if (curcase > toMID)
                  toMID = curcase;
                if (curcase < fromMID)
                  fromMID = curcase;
              }
              for (unsigned i = fromMID; i <= toMID; i++) {
                llvm::SwitchInst::CaseIt cit =
                    sw->findCaseValue(llvm::ConstantInt::get(
                        moduleInfo.getContext(),
                        llvm::APInt(32, (uint64_t)(i), false)));
                if (dup_eq_processor.duplicateMap.count(i) == 0) {
#if (LLVM_VERSION_MAJOR <= 4)
                  toBeRemovedBB.push_back(cit.getCaseSuccessor());
#else
                  toBeRemovedBB.push_back((*cit).getCaseSuccessor());
#endif
                  sw->removeCase(cit);
                } else {
                  auto new_mid = dup_eq_processor.duplicateMap.at(i).front();
#if (LLVM_VERSION_MAJOR <= 4)
                  cit.setValue(llvm::ConstantInt::get(
                      moduleInfo.getContext(),
                      llvm::APInt(32, (uint64_t)(new_mid), false)));
                  cit.getCaseSuccessor()->setName(
                      std::string("MART.Mutant_Mut") + std::to_string(new_mid));
#else
                  (*cit).setValue(llvm::ConstantInt::get(
                      moduleInfo.getContext(),
                      llvm::APInt(32, (uint64_t)(new_mid), false)));
                  (*cit).getCaseSuccessor()->setName(
                      std::string("MART.Mutant_Mut") + std::to_string(new_mid));
#endif
                }
              }
            }
          }
        }
      }
      for (auto *bbrm : toBeRemovedBB)
        bbrm->eraseFromParent();
    }
  }

  //@ Set the Initial Value of mutantIDSelectorGlobal to '<Highest Mutant ID> +
  // 1' (which is equivalent to original)
  // here 'dup_eq_processor.duplicateMap' contain only
  // remaining mutant (excluding original)
  highestMutID = dup_eq_processor.duplicateMap.size();
  mutantIDSelGlob = module.getNamedGlobal(mutantIDSelectorName);

  if (highestMutID == 0) { // No mutants
    mutantIDSelGlob->eraseFromParent();
  } else {
    mutantIDSelGlob->setInitializer(llvm::ConstantInt::get(
        moduleInfo.getContext(),
        llvm::APInt(32, (uint64_t)1 + highestMutID, false)));
  }

  /// Write mutants files and weak mutation file
  /// XXX After writing the, do not use dup_eq_processor.mutModules of
  /// dup_eq_processor.mutFunctions, snce they are modified by the write mutants
  /// callback
  if (writeMuts || modWMLog || modCovLog) {
    std::unique_ptr<llvm::Module> wmModule(nullptr);
    std::unique_ptr<llvm::Module> covModule(nullptr);

    // WM
    if (modWMLog) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
      wmModule.reset(llvm::CloneModule(&module));
#else
      wmModule = llvm::CloneModule(&module);
#endif
      computeWeakMutation(wmModule, modWMLog);
    }

    // Cov
    if (modCovLog) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
      covModule.reset(llvm::CloneModule(&module));
#else
      covModule = llvm::CloneModule(&module);
#endif
      computeMutantCoverage(covModule, modCovLog);
    }

    // XXX After writing the, do not use dup_eq_processor.mutModules of
    // dup_eq_processor.mutFunctions
    if (writeMuts) {
      if (isTCEFunctionMode) {
        assert(dup_eq_processor.mutModules.empty() &&
               "dup_eq_processor.mutModules must be empty here");
        unsigned tmpHMID =
            dup_eq_processor.mutFunctions.size() - 1; // initial highestMutID
        dup_eq_processor.mutModules.resize(tmpHMID + 1, nullptr);
        // set clonedOrig to have KS function as declaration, not definition
        if (forKLEESEMu) {
          llvm::Function *funcForKS = clonedOrig->getFunction(mutantIDSelectorName_Func);
          funcForKS->deleteBody();
        }
        dup_eq_processor.mutModules[0] = clonedOrig;
        for (unsigned tmpid = 1; tmpid <= tmpHMID;
             tmpid++) // id==0 is the original
        {
          dup_eq_processor.mutModules[tmpid] =
              dup_eq_processor.clonedModByFunc.at(
                  dup_eq_processor.funcMutByMutID[tmpid]);
        }
        assert(writeMutantsCallback(this, &dup_eq_processor.duplicateMap,
                                    &dup_eq_processor.mutModules,
                                    wmModule.get(), covModule.get(),
                                    &dup_eq_processor.mutFunctions) &&
               "Failed to dump mutants IRs");
        dup_eq_processor.mutModules.clear();
        dup_eq_processor.mutModules.resize(0);
      } else {
        assert(writeMutantsCallback(this, &dup_eq_processor.duplicateMap,
                                    &dup_eq_processor.mutModules,
                                    wmModule.get(), covModule.get(), nullptr) &&
               "Failed to dump mutants IRs");
      }
    } else {
      assert(writeMutantsCallback(this, nullptr, nullptr, wmModule.get(), covModule.get(),
                                  nullptr) &&
             "Failed to dump weak mutantion IR. (can be null)");
    }
  }

  optMetaMu.reset(ReadWriteIRObj::cloneModuleAndRelease(&module));
  dup_eq_processor.tce.optimize(*(optMetaMu.get()), modModeOptLevel);

  // XXX create the final version of the meta-mutant file
  if (forKLEESEMu) {
    // add the function to check mutant join (post mutation point instruction)
    Mutation::applyPostMutationPointForKSOnMetaModule(module);
    // Delete the body for optimization to apply 
    llvm::Function *funcForKS = module.getFunction(mutantIDSelectorName_Func);
    funcForKS->deleteBody();
    funcForKS = module.getFunction(postMutationPointFuncName);
    funcForKS->deleteBody();
    // dup_eq_processor.tce.optimize(module);

    // if There are mutants
    if (highestMutID > 0) {
      // Create a body for the function 'mutantIDSelectorName_Func'
      createGlobalMutIDSelector_Func(module, true);
      // Create a body for the function 'postMutationPointFuncName'
      createPostMutationPointFunc(module, true);
    }

    /// reduce consecutive range (of selector func) into one //TODO
  } else {
    // dup_eq_processor.tce.optimize(module);
  }

  // verify post TCE Meta-module
  Mutation::checkModuleValidity(module,
                                "ERROR: Misformed post-TCE Meta-Module!");

  // free dup_eq_processor.mutModules' cloned modules.
  if (isTCEFunctionMode) {
    for (auto *ff : dup_eq_processor.mutFunctions)
      if (ff)
        delete ff;
    delete clonedOrig;
    for (auto &mmIt : dup_eq_processor.clonedModByFunc)
      delete mmIt.second;
  } else {
    for (auto *mm : dup_eq_processor.mutModules)
      delete mm;
  }
}

/**
 * \brief transform non optimized meta-mutant module into weak mutation module.
 * @param cmodule is the meta mutant module. @note: it will be transformed into
 * WM module, so clone module before this call
 */
void Mutation::linkMetamoduleWithMutantSelection(
                            std::unique_ptr<llvm::Module> &optMetaMu,
                            std::unique_ptr<llvm::Module> &mutantSelectorMod) {

  assert(!optMetaMu->getFunction(metamutantSelectorFuncname) && 
                                                "Name clash for mutant selector"
                                                "function Name, please "
                                                "change it from your program");

  /// Link optMetaMu with the corresponding driver module (actually only need c
  /// module)
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
  llvm::Linker linker(optMetaMu.get());
  std::string ErrorMsg;
  if (linker.linkInModule(mutantSelectorMod.get(), &ErrorMsg)) {
    llvm::errs()
        << "Failed to link meta mutant module with mutant selector module"
        << ErrorMsg << "\n";
    assert(false);
  }
  mutantSelectorMod.reset(nullptr);
#else
  llvm::Linker linker(*optMetaMu);
  if (linker.linkInModule(std::move(mutantSelectorMod))) {
    assert(false &&
           "Failed to link meta mutant module with mutant selector module");
  }
#endif
}

/**
 * \brief Create the post mutation point function and insert as needed
 */
void Mutation::applyPostMutationPointForKSOnMetaModule(llvm::Module &module) {
  // First create the function into the module
  llvm::Function *funcForKS = Mutation::createPostMutationPointFunc(module, false);

  llvm::GlobalVariable *mutantIDSelGlob = module.getNamedGlobal(mutantIDSelectorName);
  
  // look for every mutation point and insert a call to the function
  std::map<llvm::BasicBlock*, std::vector<MutantIDType>> pointBB2mutantIDVect; 
  for (auto &func: module) {
    for (auto &BB: func) {
      for (auto &inst: BB) {
        if (auto *sw = llvm::dyn_cast<llvm::SwitchInst>(&inst)) {
          if (auto *ld = llvm::dyn_cast<llvm::LoadInst>(sw->getCondition())) {
            if (ld->getOperand(0) == mutantIDSelGlob) {
              //XXX is mutation point, now process
              if (sw->getNumCases() == 0)
                continue;

              pointBB2mutantIDVect.clear();
              
              // Original
              llvm::TerminatorInst* origBBterm_i = 
                            llvm::dyn_cast<llvm::TerminatorInst>(
                                        sw->getDefaultDest()->getTerminator());
              assert (origBBterm_i && "malformed original BB");
              for (auto sid = 0; sid < origBBterm_i->getNumSuccessors(); ++sid) {
                llvm::BasicBlock * pointBB = origBBterm_i->getSuccessor(sid);
                pointBB2mutantIDVect[pointBB].push_back(0);
              }
              
              // Mutants
              for (auto csit = sw->case_begin(), cse = sw->case_end();
                   csit != cse; ++csit) {
#if (LLVM_VERSION_MAJOR <= 4)
                llvm::TerminatorInst* mutBBterm_i = 
                            llvm::dyn_cast<llvm::TerminatorInst>(
                                        csit.getCaseSuccessor()->getTerminator());
                uint64_t curcaseuint = csit.getCaseValue()->getZExtValue();
#else
                llvm::TerminatorInst* mutBBterm_i = 
                            llvm::dyn_cast<llvm::TerminatorInst>(
                                        (*csit).getCaseSuccessor()->getTerminator());
                uint64_t curcaseuint = (*csit).getCaseValue()->getZExtValue();
#endif
                assert (mutBBterm_i && "malformed mutant BB");
                for (auto sid = 0; sid < mutBBterm_i->getNumSuccessors(); ++sid) {
                  llvm::BasicBlock * pointBB = mutBBterm_i->getSuccessor(sid);
                  pointBB2mutantIDVect[pointBB].push_back(curcaseuint);
                }
              }

              // add the function call instrumentations
              for (auto &pbb_it: pointBB2mutantIDVect) {
                llvm::Instruction * insert_pt = &*(pbb_it.first->getFirstInsertionPt());
                // Sort the value in increasing order
                std::sort(pbb_it.second.begin(), pbb_it.second.end());
                llvm::IRBuilder<> sbuilder(insert_pt);
                std::vector<llvm::Value *> argsv;
                MutantIDType fromID, toID;
                fromID = toID = pbb_it.second.front();
                for (auto mid : pbb_it.second) {
                  if (mid > toID+1) {
                    // Store
                    argsv.clear();
                    argsv.push_back(llvm::ConstantInt::get(
                        moduleInfo.getContext(),
                        llvm::APInt(32, (uint64_t)(fromID), false)));
                    argsv.push_back(llvm::ConstantInt::get(
                        moduleInfo.getContext(),
                        llvm::APInt(32, (uint64_t)(toID), false)));
                    sbuilder.CreateCall(funcForKS, argsv);
                    // reset fromID and toID
                    fromID = toID = mid;
                  } else {
                    toID = mid;
                  }
                }
                // Store last
                argsv.clear();
                argsv.push_back(llvm::ConstantInt::get(moduleInfo.getContext(),
                                    llvm::APInt(32, (uint64_t)(fromID), false)));
                argsv.push_back(llvm::ConstantInt::get(moduleInfo.getContext(),
                                      llvm::APInt(32, (uint64_t)(toID), false)));
                sbuilder.CreateCall(funcForKS, argsv);
              }
            }
          }
        }
      }
    }
  }
}

/**
 * \brief Clean switch of mutant for the muatnt ids in range from to
 */
void Mutation::cleanFunctionSWmIDRange(llvm::Function &Func,
                                       MutantIDType mIDFrom, MutantIDType mIDTo,
                                       llvm::GlobalVariable *mutantIDSelGlob,
                                       llvm::Function *mutantIDSelGlob_Func) {
  if (Func.isDeclaration())
    return;
  std::vector<llvm::BasicBlock *> toBeRemovedBB;
  std::vector<llvm::ConstantInt *> tmpCaseVals;
  for (auto &BB : Func) {
    toBeRemovedBB.clear();
    for (llvm::BasicBlock::iterator Iit = BB.begin(), Iie = BB.end();
         Iit != Iie;) {
      // we increment here so that 'eraseFromParent' bellow do not cause crash
      llvm::Instruction &Inst = *Iit++;
      if (auto *callI = llvm::dyn_cast<llvm::CallInst>(&Inst)) {
        if (forKLEESEMu && callI->getCalledFunction() == mutantIDSelGlob_Func) {
          // remove all calls to the heler function for KLEE integration
          callI->eraseFromParent();
        }
      }
      if (auto *sw = llvm::dyn_cast<llvm::SwitchInst>(&Inst)) {
        if (auto *ld = llvm::dyn_cast<llvm::LoadInst>(sw->getCondition())) {
          if (ld->getOperand(0) == mutantIDSelGlob) {
            tmpCaseVals.clear();
            for (auto csit = sw->case_begin(), cse = sw->case_end();
                 csit != cse; ++csit) {
#if (LLVM_VERSION_MAJOR <= 4)
              llvm::ConstantInt *curcase = csit.getCaseValue();
#else
              llvm::ConstantInt *curcase = (*csit).getCaseValue();
#endif
              uint64_t curcaseuint = curcase->getZExtValue();
              if (curcaseuint > mIDTo || curcaseuint < mIDFrom)
                continue;
              tmpCaseVals.push_back(curcase);
            }
            // in case all mutant at this swich should be removed
            if (tmpCaseVals.size() == sw->getNumCases()) {
              // Make all PHI nodes that referred to BB now refer to Pred as
              // their
              // source...
#if (LLVM_VERSION_MAJOR <= 4)
              llvm::BasicBlock *citSucc = sw->case_default().getCaseSuccessor();
#else
              llvm::BasicBlock *citSucc = (*(sw->case_default())).getCaseSuccessor();
#endif
              llvm::BasicBlock *swBB = sw->getParent();
              citSucc->replaceAllUsesWith(swBB);
              // Move all definitions in the successor to the predecessor...
              sw->eraseFromParent();
              ld->eraseFromParent();

              swBB->getInstList().splice(swBB->end(), citSucc->getInstList());
              toBeRemovedBB.push_back(citSucc);
            } else {
              for (auto csv : tmpCaseVals) {
                llvm::SwitchInst::CaseIt csit = sw->findCaseValue(csv);
#if (LLVM_VERSION_MAJOR <= 4)
                toBeRemovedBB.push_back(csit.getCaseSuccessor());
#else
                toBeRemovedBB.push_back((*csit).getCaseSuccessor());
#endif
                sw->removeCase(csit);
              }
            }
          }
        }
      }
    }
    for (auto *bbrm : toBeRemovedBB)
      bbrm->eraseFromParent();
  }
  // Check validity
  // Mutation::checkFunctionValidity(Func, "ERROR: Misformed Function after
  // cleanFunctionSWmIDRange!");
}

void Mutation::cleanFunctionToMut(llvm::Function &Func, MutantIDType mutantID,
                                  llvm::GlobalVariable *mutantIDSelGlob,
                                  llvm::Function *mutantIDSelGlob_Func,
                                  bool verifyIfEnabled, 
                                  bool removeSemuCalls) {
  if (Func.isDeclaration())
    return;
  for (auto &BB : Func) {
    std::vector<llvm::BasicBlock *> toBeRemovedBB;
    for (llvm::BasicBlock::iterator Iit = BB.begin(), Iie = BB.end();
         Iit != Iie;) {
      // we increment here so that 'eraseFromParent' bellow do not cause crash
      llvm::Instruction &Inst = *Iit++;
      if (auto *callI = llvm::dyn_cast<llvm::CallInst>(&Inst)) {
        if (removeSemuCalls && forKLEESEMu && callI->getCalledFunction() == mutantIDSelGlob_Func) {
          // remove all calls to the heler function for KLEE integration
          callI->eraseFromParent();
        }
      }
      if (auto *sw = llvm::dyn_cast<llvm::SwitchInst>(&Inst)) {
        if (auto *ld = llvm::dyn_cast<llvm::LoadInst>(sw->getCondition())) {
          if (ld->getOperand(0) == mutantIDSelGlob) {
            llvm::SwitchInst::CaseIt cit =
                sw->findCaseValue(llvm::ConstantInt::get(
                    moduleInfo.getContext(),
                    llvm::APInt(32, (uint64_t)(mutantID), false)));
            llvm::BasicBlock *citSucc = nullptr;
            llvm::BasicBlock *swBB = sw->getParent();
            if (cit == sw->case_default()) {
              // Not the mutants (mutants is same as orig here)
#if (LLVM_VERSION_MAJOR <= 4)
              citSucc = cit.getCaseSuccessor();
#else
              citSucc = (*cit).getCaseSuccessor();
#endif
              for (auto csit = sw->case_begin(), cse = sw->case_end();
                   csit != cse; ++csit)
#if (LLVM_VERSION_MAJOR <= 4)
                if (csit.getCaseSuccessor() != citSucc)
                  toBeRemovedBB.push_back(csit.getCaseSuccessor());
#else
                if ((*csit).getCaseSuccessor() != citSucc)
                  toBeRemovedBB.push_back((*csit).getCaseSuccessor());
#endif
            } else { // mutants
#if (LLVM_VERSION_MAJOR <= 4)
              citSucc = cit.getCaseSuccessor();
#else
              citSucc = (*cit).getCaseSuccessor();
#endif
              for (auto csit = sw->case_begin(), cse = sw->case_end();
                   csit != cse; ++csit)
#if (LLVM_VERSION_MAJOR <= 4)
                if (csit.getCaseSuccessor() != citSucc)
                  toBeRemovedBB.push_back(csit.getCaseSuccessor());
#else
                if ((*csit).getCaseSuccessor() != citSucc)
                  toBeRemovedBB.push_back((*csit).getCaseSuccessor());
#endif
              if (sw->getDefaultDest() != citSucc)
                toBeRemovedBB.push_back(sw->getDefaultDest());
            }

            // Make all PHI nodes that referred to BB now refer to Pred as their
            // source...
            citSucc->replaceAllUsesWith(swBB);
            // Move all definitions in the successor to the predecessor...
            sw->eraseFromParent();
            ld->eraseFromParent();

            swBB->getInstList().splice(swBB->end(), citSucc->getInstList());
            toBeRemovedBB.push_back(citSucc);

            // llvm::BranchInst::Create (citSucc, sw);
            // sw->eraseFromParent();
          }
        }
      }
    }
    for (auto *bbrm : toBeRemovedBB)
      bbrm->eraseFromParent();
  }
  // Check validity
  if (verifyIfEnabled)
    Mutation::checkFunctionValidity(
        Func, "ERROR: Misformed Function after cleanToMut!");
}

void Mutation::computeModuleBufsByFunc(
    llvm::Module &module,
    std::unordered_map<llvm::Function *, ReadWriteIRObj> *inMemIRModBufByFunc,
    std::unordered_map<llvm::Function *, llvm::Module *> *clonedModByFunc,
    std::vector<llvm::Function *> &funcMutByMutID) {
  if (inMemIRModBufByFunc == nullptr && clonedModByFunc == nullptr)
    assert(false && "Both 'inMemIRModBufByFunc' and 'clonedModByFunc' are "
                    "NULL, should choose one");
  if (inMemIRModBufByFunc != nullptr && clonedModByFunc != nullptr)
    assert(false && "Both 'inMemIRModBufByFunc' and 'clonedModByFunc' are NOT "
                    "NULL, should choose one");

  std::unordered_set<MutantIDType> mutHavingMoreThan1Func;
  llvm::GlobalVariable *mutantIDSelGlob =
      module.getNamedGlobal(mutantIDSelectorName);
  llvm::Function *mutantIDSelGlob_Func =
      module.getFunction(mutantIDSelectorName_Func);
  // llvm::errs() << "XXXCloning...\n";   //////DBG
  /// \brief First pass to get the functions mutated for each mutant (nullptr
  /// when more than one function mutated).
  for (auto &Func : module) {
    bool funcMutated = false;
    for (auto &BB : Func) {
      for (llvm::BasicBlock::iterator Iit = BB.begin(), Iie = BB.end();
           Iit != Iie;) {
        llvm::Instruction &Inst = *Iit++;
        if (auto *sw = llvm::dyn_cast<llvm::SwitchInst>(&Inst)) {
          if (auto *ld = llvm::dyn_cast<llvm::LoadInst>(sw->getCondition())) {
            if (ld->getOperand(0) == mutantIDSelGlob) {
              for (llvm::SwitchInst::CaseIt cit = sw->case_begin(),
                                            ce = sw->case_end();
                   cit != ce; ++cit) {
#if (LLVM_VERSION_MAJOR <= 4)
                MutantIDType mid = cit.getCaseValue()->getZExtValue();
#else
                MutantIDType mid = (*cit).getCaseValue()->getZExtValue();
#endif
                if (funcMutByMutID[mid] == nullptr) {
                  funcMutByMutID[mid] = &Func;
                  funcMutated = true;
                } else if (funcMutByMutID[mid] != &Func) {
                  mutHavingMoreThan1Func.insert(mid);
                }
              }
            }
          }
        }
      }
    }
    if (funcMutated) {
      if (inMemIRModBufByFunc != nullptr)
        (*inMemIRModBufByFunc)[&Func]; // insert Func to map
      else
        (*clonedModByFunc)[&Func] = nullptr;
    }
  }

  // set mutant taking more than one function's function to null
  for (auto mid : mutHavingMoreThan1Func)
    funcMutByMutID[mid] = nullptr;

  TCE tce;

  /// \brief get the module for each function (when cleaning all other funcs)
  std::stack<std::tuple<llvm::Module *, unsigned /*From*/, unsigned /*To*/>>
      workStack;
  std::vector<std::string> mutFuncList;
  if (clonedModByFunc != nullptr) {
    (*clonedModByFunc)[nullptr] =
        ReadWriteIRObj::cloneModuleAndRelease(&module);
    for (auto &funcmodIt : *clonedModByFunc)
      if (funcmodIt.first != nullptr)
        mutFuncList.push_back(funcmodIt.first->getName());
  } else {
    (*inMemIRModBufByFunc)[nullptr].setToModule(&module);
    for (auto &funcmodIt : *inMemIRModBufByFunc)
      if (funcmodIt.first != nullptr)
        mutFuncList.push_back(funcmodIt.first->getName());
  }
  if (!mutFuncList.empty())
    workStack.emplace(ReadWriteIRObj::cloneModuleAndRelease(&module), 0,
                      mutFuncList.size() - 1);

  /// \brief Use binary approach(divide and conquer) to quickly obtain the
  /// module for each function
  while (!workStack.empty()) {
    auto &stackElem = workStack.top();
    unsigned min = std::get<1>(stackElem), max = std::get<2>(stackElem);
    llvm::Module *cloneML = std::get<0>(stackElem);
    workStack.pop();

    if (min == max) {
      if (clonedModByFunc != nullptr) {
        (*clonedModByFunc)[module.getFunction(mutFuncList[min])] = cloneML;
      } else {
        (*inMemIRModBufByFunc)[module.getFunction(mutFuncList[min])]
            .setToModule(cloneML);
        delete cloneML;
      }
      continue;
    }

    unsigned mid = min + (max - min) / 2;

    llvm::Module *cloneMR = ReadWriteIRObj::cloneModuleAndRelease(cloneML);

    // llvm::errs() << "Func - " << funcPtr->getName() << "\n";   //////DBG
    assert((min <= mid && mid < max) &&
           "shlould reach here only if we have at least 2 functions uncleaned");

    llvm::GlobalVariable *mutantIDSelGlobMM =
        cloneML->getNamedGlobal(mutantIDSelectorName);
    llvm::Function *mutantIDSelGlob_FuncMM =
        cloneML->getFunction(mutantIDSelectorName_Func);
    for (auto fit = min; fit <= mid; fit++) {
      auto *Func = cloneML->getFunction(mutFuncList[fit]);
      cleanFunctionToMut(*Func, 0, mutantIDSelGlobMM, mutantIDSelGlob_FuncMM);
      tce.optimize(*Func, Mutation::funcModeOptLevel);
    }
    // left side has been cleaned, now add remaining right side to be processed
    // next
    workStack.emplace(cloneML, mid + 1, max);

    mutantIDSelGlobMM = cloneMR->getNamedGlobal(mutantIDSelectorName);
    mutantIDSelGlob_FuncMM = cloneMR->getFunction(mutantIDSelectorName_Func);
    for (auto fit = mid + 1; fit <= max; fit++) {
      auto *Func = cloneMR->getFunction(mutFuncList[fit]);
      cleanFunctionToMut(*Func, 0, mutantIDSelGlobMM, mutantIDSelGlob_FuncMM);
      tce.optimize(*Func, Mutation::funcModeOptLevel);
    }
    // right side have been cleaned now add the remaining left side to be
    // processed next
    workStack.emplace(cloneMR, min, mid);
  }
}

bool Mutation::getMutant(llvm::Module &module, unsigned mutantID,
                         llvm::Function *mutFunc, char optimizeModFuncNone) {
  unsigned highestMutID = getHighestMutantID(&module);
  if (mutantID > highestMutID)
    return false;

  llvm::GlobalVariable *mutantIDSelGlob =
      module.getNamedGlobal(mutantIDSelectorName);
  llvm::Function *mutantIDSelGlob_Func =
      module.getFunction(mutantIDSelectorName_Func);
  TCE tce;

  if (optimizeModFuncNone == 'F')
    assert(mutFunc && "optimize function but function is NULL");

  /// \brief remove every case of switches that are irrelevant, as well as all
  /// call of 'forKleeSeMU'
  /// XXX: This help to make optimization faster
  llvm::Function *mutFuncInThisModule = nullptr;
  llvm::Module::iterator modIter, modend;
  if (mutFunc) {
    mutFuncInThisModule = module.getFunction(mutFunc->getName());
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 8)
    modIter = llvm::Module::iterator(mutFuncInThisModule);
#else
    modIter = mutFuncInThisModule->getIterator();
#endif
    modend = modIter;
    ++modend;
  } else {
    modIter = module.begin();
    modend = module.end();
  }

  for (; modIter != modend; ++modIter) {
    if (modIter->isDeclaration())
      continue;
    llvm::Function &Func = *modIter;
    cleanFunctionToMut(Func, mutantID, mutantIDSelGlob, mutantIDSelGlob_Func);
  }

  // llvm::errs() << "Optimizing...\n";   /////DBG
  ///~

  if (optimizeModFuncNone == 'F') {
    tce.optimize(*mutFuncInThisModule, Mutation::funcModeOptLevel);
  } else if (optimizeModFuncNone == 'M') {
    mutantIDSelGlob->setInitializer(llvm::ConstantInt::get(
        moduleInfo.getContext(), llvm::APInt(32, (uint64_t)mutantID, false)));
    mutantIDSelGlob->setConstant(true);
    tce.optimize(module, Mutation::modModeOptLevel);
  } else if (optimizeModFuncNone == 'A') // optimize all the functions
  {
    for (auto &ffunc : module)
      if (!ffunc.isDeclaration())
        tce.optimize(ffunc, Mutation::funcModeOptLevel);
  } else {
    assert(
        optimizeModFuncNone == '0' &&
        "invalid optimization mode in getMutant: expect 'M', 'F', 'A' or '0'");
  }

  return true;
}

unsigned Mutation::getHighestMutantID(llvm::Module const *module) {
  if (!module)
    module = currentMetaMutantModule;
  llvm::GlobalVariable const *mutantIDSelectorGlobal =
      module->getNamedGlobal(mutantIDSelectorName);
  assert(mutantIDSelectorGlobal &&
         mutantIDSelectorGlobal->getInitializer()->getType()->isIntegerTy() &&
         "Unmutated module passed to TCE");
  return llvm::dyn_cast<llvm::ConstantInt>(
             mutantIDSelectorGlobal->getInitializer())
             ->getZExtValue() - 1;
}

void Mutation::loadMutantInfos(std::string filename) {
  mutantsInfos.loadFromJsonFile(filename);
}

void Mutation::dumpMutantInfos(std::string filename, std::string eqdup_filename) {
  mutantsInfos.printToJsonFile(filename, eqdup_filename);
}

std::string Mutation::getMutationStats() {
  std::string retstr;
  retstr += "\n# Number of Mutants:   PreTCE: " +
            std::to_string(preTCENumMuts) + ", PostTCE: " +
            std::to_string(postTCENumMuts) + ", ";
  retstr += "Equivalent: " + std::to_string(numEquivalentMuts) +
            ", Duplicates: " + std::to_string(numDuplicateMuts) + "\n\n";
  return retstr;
}

Mutation::~Mutation() {
  // mutantsInfos.printToStdout();

  llvm::errs() << getMutationStats();

  // Clear the constant map to avoid double free
  llvmMutationOp::destroyPosConstValueMap();
}
