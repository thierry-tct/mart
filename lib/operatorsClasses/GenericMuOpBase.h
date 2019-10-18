#ifndef __MART_GENMU_operatorClasses__GenericMuOpBase__
#define __MART_GENMU_operatorClasses__GenericMuOpBase__

/**
 * -==== GenericMuOpBase.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Generic abstract base class for all mutation operator.
 * \details   This abstract class define the minimal interface to be defined by
 * all mutation operators.
 * \note      Avoid using static members in this class and its children classes
 */

#include <exception>
#include <map>
#include <typeinfo> //Useful for class name of an object
#include <vector>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/DataLayout.h"

#include "llvm/IR/Intrinsics.h" //llvm::Intrinsic::getDeclaration()

//#include "llvm/IR/InstrTypes.h"

#include "../typesops.h"
#include "../usermaps.h"

namespace mart {

/**
 * This structure - 'MatchUseful' - represent the data fields returned by the
 * method 'matchIRs' when match is successful.
 */
struct MatchUseful {
private:
  struct MatchUseful *additionalMU; // When there are many matches at a pos,
                                    // linklist then using this
  struct MatchUseful *curLast;

  /// \brief This vector is the list of pairs representing the IR used to make
  /// high level operands
  /// \brief the first element of the pair is the position (pos) in 'toMatch'.
  /// The second is a negative int if the value of the IR is considered
  /// \brief else, it represents the Operand index of the IR (toMatch[pos]).
  std::vector<std::pair<unsigned /*pos*/, int /*oprd index*/>>
      highLevelOprdsSources;
  // llvm::Value * highLevelReturningIR;
  int posReturningIR;
  std::vector<unsigned> relevantIRPos;
  // llvm::Value * returnIntoIR;
  int posReturnIntoIR;
  int retIntoOprdIndex; // For constant replacement

public:
  MatchUseful() : additionalMU(nullptr) { clearAll(); }
  ~MatchUseful() { clearAll(); }
  void clearAll() {
    highLevelOprdsSources.clear();
    relevantIRPos.clear();
    // highLevelReturningIR = nullptr;
    posReturningIR = -1;
    // returnIntoIR = nullptr;
    posReturnIntoIR = -1;
    retIntoOprdIndex = -1;
    if (additionalMU) {
      // additionalMU->clearAll(); //done in the destructor
      delete additionalMU;
      additionalMU = nullptr;
    }
    curLast = nullptr;
  }

  /// \brief expand
  inline struct MatchUseful *getNew() {
    if (!curLast) {
      curLast = this;
      return curLast;
    } else {
      curLast->additionalMU = new MatchUseful();
      return curLast->additionalMU;
    }
  }

  inline struct MatchUseful *getNew_CopyData() {
    struct MatchUseful *ret = getNew();
    if (ret != this) { // copy data
      ret->highLevelOprdsSources = highLevelOprdsSources;
      ret->relevantIRPos = relevantIRPos;
      ret->posReturningIR = posReturningIR;
      ret->posReturnIntoIR = posReturnIntoIR;
      ret->retIntoOprdIndex = retIntoOprdIndex;
    }
    return ret;
  }

  inline void appendHLOprdsSource(unsigned pos, int ir_oprdInd = -1) {
    highLevelOprdsSources.push_back(
        std::pair<unsigned /*pos*/, int /*oprd index*/>(pos, ir_oprdInd));
  }
  inline void resetHLOprdSourceAt(unsigned hlOprdID, unsigned pos,
                                  int ir_oprdInd = -1) {
    highLevelOprdsSources.at(hlOprdID).first = pos;
    highLevelOprdsSources.at(hlOprdID).second = ir_oprdInd;
  }
  inline llvm::Value *
  getHLOperandSource(unsigned hlOprdID,
                     MutantsOfStmt::MutantStmtIR &mutIRs) const {
    if (highLevelOprdsSources.at(hlOprdID).second < 0)
      return mutIRs.getIRAt(highLevelOprdsSources.at(hlOprdID).first);
    else
      return llvm::dyn_cast<llvm::User>(
                 mutIRs.getIRAt(highLevelOprdsSources.at(hlOprdID).first))
          ->getOperand(highLevelOprdsSources.at(hlOprdID).second);
  }
  inline llvm::Value const *getHLOperandSource(unsigned hlOprdID,
                                               MatchStmtIR const &matchIRs) {
    if (highLevelOprdsSources.at(hlOprdID).second < 0)
      return matchIRs.getIRAt(highLevelOprdsSources.at(hlOprdID).first);
    else
      return llvm::dyn_cast<llvm::User>(
                 matchIRs.getIRAt(highLevelOprdsSources.at(hlOprdID).first))
          ->getOperand(highLevelOprdsSources.at(hlOprdID).second);
  }
  inline int getHLOperandSourceIndexInIR(unsigned hlOprdID) const {
    return highLevelOprdsSources.at(hlOprdID).second;
  }
  inline int getHLOperandSourcePos(unsigned hlOprdID) const {
    return highLevelOprdsSources.at(hlOprdID).first;
  }
  inline unsigned getNumberOfHLOperands() const {
    return highLevelOprdsSources.size();
  }
  inline void appendRelevantIRPos(int pos) { relevantIRPos.push_back(pos); }
  inline int getRelevantIRPosOf(int posInList) const {
    return relevantIRPos.at(posInList);
  }
  inline const std::vector<unsigned> &getRelevantIRPos() const {
    return relevantIRPos;
  }

  inline void setHLReturningIRPos(int pos) { posReturningIR = pos; }
  inline int getHLReturningIRPos() const { return posReturningIR; }

  inline void setHLReturnIntoIRPos(int pos) { posReturnIntoIR = pos; }
  inline int getHLReturnIntoIRPos() const { return posReturnIntoIR; }

  inline void setHLReturnIntoOprdIndex(int pos) { retIntoOprdIndex = pos; }
  inline int getHLReturnIntoOprdIndex() const { return retIntoOprdIndex; }

  /// \brief Iterators methods
  inline MatchUseful const *first() const { return (curLast ? this : nullptr); }
  inline MatchUseful const *end() const { return nullptr; }
  inline MatchUseful const *next() const { return additionalMU; }

  inline MatchUseful *first() { return (curLast ? this : nullptr); }
  inline MatchUseful *end() { return nullptr; }
  inline MatchUseful *next() { return additionalMU; }
}; // struct MatchUseful

/**
 * This structure - 'DoReplaceUseful' - represent the data fields returned by
 * the
 * method 'prepareCloneIRs', and these are useful to call 'doReplacement'.
 */
struct DoReplaceUseful {
protected:
  // \brief The data for each field bellow is computed w.r.t toMatchMutant, and
  // not the original (toMatch)
  std::vector<llvm::Value *> highLevelOprds;
  // llvm::Value * highLevelReturningIR;
  int posHighLevelReturningIR;
  std::vector<unsigned> const *origRelevantIRPos;
  // llvm::Value * returnIntoIR;
  int posReturnIntoIR;
  int retIntoOprdIndex; // For constant replacement and some other that need
                        // dumb

public:
  MutantsOfStmt::MutantStmtIR toMatchMutant;
  std::vector<unsigned> posOfIRtoRemove;
  // enum replacementModes replaceMode;

  DoReplaceUseful() { clearAll(); }
  void clearAll() {
    highLevelOprds.clear();
    origRelevantIRPos = nullptr; // origRelevantIRPos.clear();
    // highLevelReturningIR = nullptr;
    posHighLevelReturningIR = -1;
    // returnIntoIR = nullptr;
    posReturnIntoIR = -1;
    retIntoOprdIndex = -1;

    toMatchMutant.clear();
    posOfIRtoRemove.clear();
    // replaceMode = rmVAL;
  }

  inline void appendHLOprds(llvm::Value *oprdV) {
    highLevelOprds.push_back(oprdV);
  }
  inline llvm::Value *getHLOprdOrNull(int ind) {
    return (ind < highLevelOprds.size() ? highLevelOprds.at(ind) : nullptr);
  }

  // inline void setHLReturningIR (llvm::Value *val) {highLevelReturningIR =
  // val;}
  // inline llvm::Value * getHLReturningIR () {return highLevelReturningIR;}

  // inline void appendOrigRelevantIRPos (int pos)
  // {origRelevantIRPos.push_back(pos);}
  inline void setOrigRelevantIRPos(std::vector<unsigned> const &posVect) {
    origRelevantIRPos = &posVect;
  }
  inline int getOrigRelevantIRPosOf(int posInList) {
    return origRelevantIRPos->at(posInList);
  }
  inline const std::vector<unsigned> &getOrigRelevantIRPos() {
    return *origRelevantIRPos;
  }

  inline void setHLReturningIRPos(int pos) { posHighLevelReturningIR = pos; }
  inline int getHLReturningIRPos() { return posHighLevelReturningIR; }

  // inline void setHLReturnIntoIR(llvm::Value *val) {returnIntoIR = val;}
  // inline llvm::Value * getHLReturnIntoIR () {return returnIntoIR;}

  inline void setHLReturnIntoIRPos(int pos) { posReturnIntoIR = pos; }
  inline int getHLReturnIntoIRPos() { return posReturnIntoIR; }

  inline void setHLReturnIntoOprdIndex(int pos) { retIntoOprdIndex = pos; }
  inline int getHLReturnIntoOprdIndex() { return retIntoOprdIndex; }
}; // struct DoReplaceUseful

class GenericMuOpBase {
public:
  virtual ~GenericMuOpBase() {}

  /**  A pure virtual member.
   * \brief Match the IR pattern that can be mutated by this operator, starting
   * from the IR at position 'pos'. For those that create only, report an error.
   * @return 'True' if the pattern is found, else return 'False'
   * @param toMatch is the statement in which we want to match the candidate
   * pattern to muatate.
   * @param pos is the position (starting from 0) of the IR instruction starting
   * point of the match temptative.
   * @param MU is the structure where the information about the matching are
   * store. It can be used later to prepare cloneIRs (@see prepareCloneIRs()).
   *               It can also be used when the matcher need to match a complex
   * pattern that require using different operations' matchers.
   * @param MI is the parameter containing the global information.
   */
  virtual bool matchIRs(MatchStmtIR const &toMatch,
                        llvmMutationOp const &mutationOp, unsigned pos,
                        MatchUseful &MU, ModuleUserInfos const &MI) = 0;

  /**  A pure virtual member.
   * \brief make the clones that will be mutated
   * @param toMatch is the statement in which we want to match the candidate
   * pattern to muatate.
   * @param pos is the position (starting from 0) of the IR instruction starting
   * point of the match temptative.
   * @param MU is the structure where the information about the matching were
   * store (@see matchIRs()).
   * @param DRU is the structure containing the information that will be needed
   * to call @see doReplacement(). The information can also be useful for
   * complex matches.
   * @param MI is the parameter containing the global information.
   */
  virtual void prepareCloneIRs(MatchStmtIR const &toMatch, unsigned pos,
                               MatchUseful const &MU,
                               llvmMutationOp::MutantReplacors const &repl,
                               DoReplaceUseful &DRU,
                               ModuleUserInfos const &MI) = 0;

  /**  A pure virtual member
   * \brief create and operation using the passed operands. For thos that match
   * only, report an error.
   * @param lh_addrOprdptr is the high level left hand operand for non-pointer
   * operation, and pointer (address) operand for operation on pointers.
   * @param rh_addrOprdptr is the high level right hand operand for non-pointer
   * operation, and non-pointer (non-address) operand for operation on pointer.
   * @param replacement is the list of IRs representing the created operation
   * @param MI is the parameter containing the global information.
   */
  virtual llvm::Value *
  createReplacement(llvm::Value *oprd1_addrOprd, llvm::Value *oprd2_intValOprd,
                    std::vector<llvm::Value *> &replacement,
                    ModuleUserInfos const &MI) = 0;

  /**  A virtual member.
   * \brief Match the IR pattern and mutate. This is entry-point method called
   during mutation. Reimplement this as needed.
   * \detail The method take a statement, a code pattern type to match (Ex: Add
   2 integers) as well as the muatant code pattern and generatemutated clones of
   the statement.
   * @param toMatch is the statement in which we want to match the candidate
   pattern to muatate.
   * @param mutationOp contains the pattern to match as well as the replacing
   ones.
   * @param resultMuts is the list where the generated mutants of the initial
   statement should be appended.
   * @param iswholestmtmutated is the flag that help to know wheter the
   statement has already been mutated (for whole stmt mutation,) by previous
   mutation op.
            Changed from false to true after deletion occurs.
   * @param MI is the parameter containing the global information.
   */
  virtual void matchAndReplace(MatchStmtIR const &toMatch,
                               llvmMutationOp const &mutationOp,
                               MutantsOfStmt &resultMuts,
                               WholeStmtMutationOnce &iswholestmtmutated,
                               ModuleUserInfos const &MI) {
    MatchUseful mu;
    DoReplaceUseful dru;
    int pos = -1;
    bool stmtDeleted = false;
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

  /**
   * \brief Thismethod is useful to help filter before hand the operation that
   * can never be matched in a stmt. overload this in the operator classes.
   * @return the list of instruction opcodes that are required to be present in
   * order to match for an operator. EX: for integer "i++," at least 'load',
   * 'add', 'store' must be present.
   * thust the method will return
   * std::vector<unsigned>({llvm::Instruction::Load, llvm::Instruction::Add,
   * llvm::Instruction::Store}). @Note: the order do not matter.
   */
  virtual std::vector<unsigned> getMinIRInstructionsToBeMatched() {
    return std::vector<unsigned>();
  }

protected:
  /**
   * \brief the method checkCPTypeInIR - "Check Code Part Type In IR" - validate
   * the seen value type match with the expected one.
   * \detail Use this when matching IR with code parts to mutate, to match each
   * oprd in IR with expected CPType
   * @param cpt is the code part type (ID) that we want validate with
   * @param val is the IR value the we want to validate
   * @return true if there is match, false otherwise.
   */
  bool checkCPTypeInIR(enum codeParts cpt, llvm::Value *val) {
    switch (cpt) {
    case cpEXPR: {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
      if (!(llvm::PointerType::isValidElementType(val->getType()) &&
            !val->getType()->isFunctionTy()))
#else
      if (!llvm::PointerType::isLoadableOrStorableType(val->getType()))
#endif
        return false;
      if (!(llvm::isa<llvm::CompositeType>(val->getType()) ||
            llvm::isa<llvm::FunctionType>(val->getType())))
        return true;
      return false;
    }
    case cpCONSTNUM: {
      if (llvm::isa<llvm::Constant>(val))
        if (llvm::isa<llvm::ConstantInt>(val) ||
            llvm::isa<llvm::ConstantFP>(
                val)) // || llvm::isa<llvm::ConstantExpr>(val))
          return true;
      return false;
    }
    case cpVAR: {
      while (llvm::isa<llvm::CastInst>(val)) {
        // assert (llvm::dyn_cast<llvm::User>(val)->getNumOperands() == 1);
        // if (llvm::isa<llvm::PtrToIntInst>(val))
        //    break;
        val = llvm::dyn_cast<llvm::User>(val)->getOperand(0);
      }
      if (llvm::isa<llvm::LoadInst>(
              val)) //|| llvm::isa<llvm::PtrToIntInst>(val)) // ||
                    // llvm::isa<llvm::ExtractValueInst>(val) ||
      // llvm::isa<llvm::ExtractElementInst>(val))
      {
        llvm::Type *valelty = llvm::dyn_cast<llvm::User>(val)
                                  ->getOperand(0)
                                  ->getType()
                                  ->getPointerElementType();
        if (!(llvm::isa<llvm::CompositeType>(valelty) ||
              llvm::isa<llvm::FunctionType>(valelty))) // XXX: Change this to
                                                       // add support for array
                                                       // and vector operation
                                                       // (besid float and int)
          return true;
      }
      return false;
    }
    case cpADDRESS: {
      if (llvm::isa<llvm::PointerType>(val->getType()) ||
          llvm::isa<llvm::AddrSpaceCastInst>(val) ||
          llvm::isa<llvm::IntToPtrInst>(
              val)) //|| llvm::isa<llvm::GetElementPtrInst>(val)
        return true;
      return false;
    }
    case cpPOINTER: {
      while (llvm::isa<llvm::CastInst>(val)) {
        // assert (llvm::dyn_cast<llvm::User>(val)->getNumOperands() == 1);
        val = llvm::dyn_cast<llvm::User>(val)->getOperand(0);
      }
      if (llvm::isa<llvm::LoadInst>(val) &&
          llvm::isa<llvm::PointerType>(val->getType()))
        return true;
      return false;
    }
    default: {
      llvm::errs() << "Unreachable, Invalid CPType: " << cpt << ".\n";
      assert(false && "");
    }
    }
  }

  /**
   * \brief This method obtain a constant numeric from a string contained in a
   * static structure of @see llvmMutationOp.
   * @param type is the type of the constant to return (integer - i64, i32... or
   * FloatingPoint...).
   * @param posConstValueMap_POS is the position of the string to convert into
   * constant. This assumes that the position is within bounds and the string
   * represent a constant. Otherwise Error.
   * @return the the constant obtained from the string.
   * \todo Add support for integer constant from string representing integer in
   * radix 16 and 8 (other than radix 10)
   */
  llvm::Constant *createIfConst(llvm::Type *type,
                                unsigned posConstValueMap_POS) {
    llvm::Constant *nc = nullptr;
    if (llvmMutationOp::isSpecifiedConstIndex(posConstValueMap_POS)) {
      if (type->isFloatingPointTy()) {
        nc = llvm::ConstantFP::get(
            type, llvmMutationOp::getConstValueStr(posConstValueMap_POS));
      } else if (type->isIntegerTy()) {
        unsigned bitwidth =
            llvm::dyn_cast<llvm::IntegerType>(type)->getBitWidth();
        unsigned required = llvm::APInt::getBitsNeeded(
            llvmMutationOp::getConstValueStr(posConstValueMap_POS),
            /*radix 10*/ 10);

        if (bitwidth >= required) {
          nc = llvm::ConstantInt::get(
              llvm::dyn_cast<llvm::IntegerType>(type),
              llvmMutationOp::getConstValueStr(posConstValueMap_POS),
              /*radix 10*/ 10);
        } else {
          llvm::APInt xx(required,
                         llvmMutationOp::getConstValueStr(posConstValueMap_POS),
                         /*radix 10*/ 10);
          nc = llvm::ConstantInt::get(type->getContext(), xx.trunc(bitwidth));
        }
      } else if (type->isPointerTy()) {
        if (llvmMutationOp::getConstValueStr(posConstValueMap_POS) != "0") {
          llvm::errs() << "The only constant that replace a memory address "
                          "must be 0 (NULL). But passed "
                       << llvmMutationOp::getConstValueStr(posConstValueMap_POS)
                       << "\n";
          assert(false);
        }
        nc = llvm::ConstantPointerNull::get(
            llvm::dyn_cast<llvm::PointerType>(type));
      } else {
        type->dump();
        assert(false && "unreachable!!");
      }
    }
    return nc;
  }

  /**
   * \brief The method applies the reverse of a "cast" instruction. Ex: passing
   * IntToPtr will return PtrToInt.
   * @param toRev is the llvm instruction code of the cast to reverse
   * @param subj is the value that should be cast with the reversed cast.
   * @param destTy is the destination subtype type within the general type
   * returned by the reverse cast.
   */
  llvm::Value *reverseCast(llvm::Instruction::CastOps toRev, llvm::Value *subj,
                           llvm::Type *destTy) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 9)
    llvm::IRBuilder<> builder(llvm::getGlobalContext());
#else
    static llvm::LLVMContext getGlobalContext;
    llvm::IRBuilder<> builder(getGlobalContext);
#endif
    switch (toRev) {
    case llvm::Instruction::Trunc:
      return builder.CreateZExt(subj, destTy);
    case llvm::Instruction::ZExt:
    case llvm::Instruction::SExt:
      return builder.CreateTrunc(subj, destTy);
    case llvm::Instruction::FPTrunc:
      return builder.CreateFPExt(subj, destTy);
    case llvm::Instruction::FPExt:
      return builder.CreateFPTrunc(subj, destTy);
    case llvm::Instruction::UIToFP:
      return builder.CreateFPToUI(subj, destTy);
    case llvm::Instruction::SIToFP:
      return builder.CreateFPToSI(subj, destTy);
    case llvm::Instruction::FPToUI:
      return builder.CreateUIToFP(subj, destTy);
    case llvm::Instruction::FPToSI:
      return builder.CreateSIToFP(subj, destTy);
    case llvm::Instruction::BitCast:
      return builder.CreateBitCast(subj, destTy);
    // case llvm::Instruction::AddrSpaceCast:
    // case llvm::Instruction::PtrToInt:
    // case llvm::Instruction::IntToPtr:
    default:
      assert(false && "Unreachable (reverseCast) !!!!");
    }
  }

  /**
   * \brief get the matcher/replacor code for the operation that can delete
   * statement which are block terminators (like return, break, continue)
   *
   */
  inline enum ExpElemKeys getTerminatorDeleterCode() {
    return mRETURN_BREAK_CONTINUE;
  }

  /**
   * \brief get the matcher/replacor code for the operation that can delete any
   * statement (like return, break, continue)
   */
  inline enum ExpElemKeys getGenericDeleterCode() { return mDELSTMT; }

  /**
   * \brief get the matcher/replacor code for the operation that can delete any
   * statement (like return, break, continue)
   */
  inline enum ExpElemKeys getGenericTrapInserterCode() { return mTRAPSTMT; }

  /**
   * \brief Check whether an operation is Del stmt, which deletes the whole
   * statement, or for return statement, set returned value to 0.
   * @param m is the operation to check
   * @return true if the operation is del stmt else false.
   */
  inline bool isDeletion(enum ExpElemKeys m) {
    return (m == getGenericDeleterCode());
  }

  /**
  * \brief Check whether an operation is Trap stmt, which replace the whole
  * statement by trap(basically inserting trap before) This make the program
  * abort when the stmt is reached.
  * @param m is the operation to check
  * @return true if the operation is del stmt else false.
  */
  inline bool isTrapInsertion(enum ExpElemKeys m) {
    return (m == getGenericTrapInserterCode());
  }

  /**
   * \brief Create the mutant of delete stmt, for the case where the statement
   is not a terminator (break, continue, return). For that, @see
   ReturnBreakContinue
   * @param toMatch is the original statement that have been matched (still
   attached to the original Module).
   * @param repl is the replacer whose name we will need. the name is the name
   of the current mutation operation (match-delete)
   * @param resultMuts is the list of all generated mutants of the original
   statement 'toMatch'. Note: non attached to original Module.
   * @param iswholestmtmutated is the flag that help to know wheter the
   statement has already been mutated, for each whole stmt mutation like
   deletestmt, trapstmt,...,  by previous mutation op.
             Changed from false to true after deletion occurs, or when
   undeletable.
   * @param MI is the parameter needed for the call to deletion for terminator
   stmt.
   */
  inline void doDeleteStmt(MatchStmtIR const &toMatch,
                           llvmMutationOp::MutantReplacors const &repl,
                           MutantsOfStmt &resultMuts,
                           WholeStmtMutationOnce &iswholestmtmutated,
                           ModuleUserInfos const &MI) {
    if (iswholestmtmutated.isDeleted())
      return;

    // In fact terminators are added during insertion into module of the mutants
    // only when the original stmt do not have terminator
    if (!toMatch.wholeStmtHasTerminators()) /// Avoid Deleting if statement's
                                            /// contition or the ret IR.
    {
      std::vector<unsigned> relpos;
      for (auto i = 0; i < toMatch.getTotNumIRs(); i++)
        relpos.push_back(i);
      MutantsOfStmt::MutantStmtIR toMatchMutant;
      toMatchMutant.setToEmptyOrFixedStmtOf(toMatch, MI);
      resultMuts.add(/*toMatch,  */ toMatchMutant, repl, relpos);
      // iswholestmtmutated.setDeleted();
    } else {
      enum ExpElemKeys termDelCode = getTerminatorDeleterCode();
      llvmMutationOp tmpMutationOp;
      tmpMutationOp.setMatchOp(termDelCode, std::vector<std::string>());
      tmpMutationOp.addReplacor(getGenericDeleterCode(),
                                std::vector<unsigned>(), repl.getMutOpName());
      // \brief Create a temporary mutation operator to match terminator and do
      // deletion...
      MI.getUserMaps()
          ->getMatcherObject(termDelCode)
          ->matchAndReplace(toMatch, tmpMutationOp, resultMuts,
                            iswholestmtmutated, MI);
    }

    // \brief In case it is a undeletable instruction (like if condition), set
    // is Deleted to true anyway.
    iswholestmtmutated.setDeleted();

    // assert (iswholestmtmutated.isDeleted() && "Problem with deletion. Must
    // succeed at this point!"); //commented because we may have conditional
    // terminator for if or switch conditions
  }

  /**
  * \brief Create the mutant of trap stmt, for the case where the statement
  * @param toMatch is the original statement that have been matched (still
  attached to the original Module).
  * @param repl is the replacer whose name we will need. the name is the name of
  the current mutation operation (match-delete)
  * @param resultMuts is the list of all generated mutants of the original
  statement 'toMatch'. Note: non attached to original Module.
  * @param iswholestmtmutated is the flag that help to know wheter the statement
  has already been mutated, for each whole stmt mutation like deletestmt,
  trapstmt,...,  by previous mutation op.
            Changed from false to true after deletion occurs, or when
  undeletable.
  * @param MI is the parameter needed for the call to deletion for terminator
  stmt.
  */
  inline void doTrapStmt(MatchStmtIR const &toMatch,
                         llvmMutationOp::MutantReplacors const &repl,
                         MutantsOfStmt &resultMuts,
                         WholeStmtMutationOnce &iswholestmtmutated,
                         ModuleUserInfos const &MI) {
    if (iswholestmtmutated.isTrapped())
      return;
    std::vector<unsigned> relpos;
    for (auto i = 0; i < toMatch.getTotNumIRs(); i++)
      relpos.push_back(i);
    MutantsOfStmt::MutantStmtIR toMatchMutant;
    llvm::SmallVector<llvm::Instruction *, 1> trapinst;
    llvm::Value *TrapFn =
        llvm::Intrinsic::getDeclaration(MI.getModule(), llvm::Intrinsic::trap);
    trapinst.push_back(llvm::CallInst::Create(TrapFn, "")); //->setDebugLoc(dl);

    // This will be the terminator for the mutant BB in case the current
    // original stmt has terminator
    // In fact terminators are added during insertion into module of the mutants
    // only when the original stmt do not have terminator
    if (toMatch.wholeStmtHasTerminators())
      trapinst.push_back(new llvm::UnreachableInst(MI.getContext()));

    toMatchMutant.setToEmptyOrFixedStmtOf(toMatch, MI, &trapinst);
    resultMuts.add(/*toMatch,  */ toMatchMutant, repl, relpos);
    iswholestmtmutated.setTrapped();
  }

  /**
   * \brief check whether the replacer is wholestmt mutation and return true or
   false. if it is, create the corresponding mutation
   * @param toMatch is the original statement that have been matched (still
   attached to the original Module).
   * @param repl is the replacer whose name we will need. the name is the name
   of the current mutation operation (match-delete)
   * @param resultMuts is the list of all generated mutants of the original
   statement 'toMatch'. Note: non attached to original Module.
   * @param iswholestmtmutated is the flag that help to know wheter the
   statement has already been mutated, for each whole stmt mutation like
   deletestmt, trapstmt,...,  by previous mutation op.
             Changed from false to true after deletion occurs, or when
   undeletable.
   * @param MI is the parameter needed for the call to deletion for terminator
   stmt.
   */
  inline bool checkWholeStmtAndMutate(
      MatchStmtIR const &toMatch, llvmMutationOp::MutantReplacors const &repl,
      MutantsOfStmt &resultMuts, WholeStmtMutationOnce &iswholestmtmutated,
      ModuleUserInfos const &MI) {
    if (isDeletion(repl.getExpElemKey()))
      doDeleteStmt(toMatch, repl, resultMuts, iswholestmtmutated, MI);
    else if (isTrapInsertion(repl.getExpElemKey()))
      doTrapStmt(toMatch, repl, resultMuts, iswholestmtmutated, MI);
    else
      return false;
    return true;
  }

  /**
   * \brief Check whether two values of the same type are constant and of same
   *values
   *@return true if the two values are constant and have same value, false
   *otherwise
   */
  inline virtual bool constAndEquals(llvm::Value *c1, llvm::Value *c2) {
    assert(c1->getType() == c2->getType() && "Type Mismatch!");
    if (llvm::isa<llvm::ConstantInt>(c1) && llvm::isa<llvm::ConstantInt>(c2)) {
      if (llvm::dyn_cast<llvm::ConstantInt>(c1)->equalsInt(
              llvm::dyn_cast<llvm::ConstantInt>(c2)->getZExtValue()))
        return true;
    } else if (llvm::isa<llvm::ConstantFP>(c1) &&
               llvm::isa<llvm::ConstantFP>(c2)) {
      if (llvm::dyn_cast<llvm::ConstantFP>(c1)->isExactlyValue(
              llvm::dyn_cast<llvm::ConstantFP>(c2)->getValueAPF()))
        return true;
    }

    return false;
  }

  /**
   * \brief
   * @return nullptr if the Gep do not represent a pointer indexing, else, it
   * return the value representing the index
   * When the pointer oprd do not comes from LoadInst, the 1st index of gep just
   * take out the address part (as alloca vars are actually addresses)
   * Also return the Gep index in 'index', which is (User operand - 1)
   */
  inline llvm::Value *
  checkIsPointerIndexingAndGet(llvm::GetElementPtrInst const *gep, int &index) {
    index = -1;
    if (gep->getNumIndices() < 1)
      return nullptr;
    llvm::Value const *ptrOprd = gep->getPointerOperand();
    llvm::Type const *ptrOprdElemType =
        llvm::dyn_cast<llvm::PointerType>(ptrOprd->getType())
            ->getPointerElementType();
    // if the pointer operand points to a non sequential type, the pointer must
    // come from load instruction, and the 1st idx of get will be the index
    // needed
    if (llvm::isa<llvm::PointerType>(ptrOprdElemType)) {
      assert(gep->getNumIndices() == 1 && "Gep Must have 1 index when the "
                                          "pointee type is a pointer (must "
                                          "load before doing any other Gep)");
      index = 0;
      return *(gep->idx_begin() + index);
    } else if (!llvm::isa<llvm::SequentialType>(ptrOprdElemType)) {
      if (llvm::isa<llvm::LoadInst>(ptrOprd)) {
        // return the first index (idx = 0)
        index = 0;
        return *(gep->idx_begin() + index);
      } else {
        return nullptr;
      }
    } else {
      // The pointer operand point to a sequential type, take idx 0 if
      // comes from load, else take idx 1
      if (llvm::isa<llvm::LoadInst>(ptrOprd)) {
        // return the first index (idx = 0)
        index = 0;
        return *(gep->idx_begin() + index);
      } else {
        // return the first index (idx = 1)
        if (gep->getNumIndices() > 1) {
          index = 1;
          return *(gep->idx_begin() + index);
        } else { 
          assert(gep->getNumIndices() == 1 &&
               "gep should have at least 1 index");
          // Only alloca of an array of array can have this gep
          // a
          if (llvm::isa<llvm::AllocaInst>(ptrOprd)) {
            index = 0;
            return *(gep->idx_begin() + index);
          } else {
            assert (false && "Must be Alloca here (array of array)");
          }
        }
      }
    }
  }

  /**
   * \brief Mutate the statement (list if LLVM IR Instructions) 'toMatchMutant'
   * a clone of the original statement 'toMatch'
   * then append the mutated statement (from 'toMatchMutant') into 'resultMuts'
   * \detail There are two modes: (1) specifying the IR whose value is replaced
   * by the created operation's value (here all its uses become the uses of the
   * created operation).
   *                                      Such IR is specified with the
   * parameter 'returningIR'. in this case, 'returnIntoIR' must be null and
   * retIntoOprdIndex < 0.
   *                              (2) Specifying the single IR where the value
   * of the created operation is used ('returnIntoIR').
   *                                      Here the corresponding operand index
   * is also specified ('retIntoOprdIndex'). Note that 'returningIR' must be
   * null in this case.
   * @param toMatch is the original statement that have been matched (still
   * attached to the original Module).
   * @param resultMuts is the list of all generated mutants of the original
   * statement 'toMatch'. Note: non attached to original Module.
   * @param repl is the mutation operation to apply in order to generate a
   * corresponding mutant. It contains the operation ID, the list of operands
   * indices and the name of the mutation.
   *              Example: {mSUB, [1,0], xx-yy}, means replace the matched
   * expression (having 2 operands) with SUB and switching the operands.
   * @param toMatchMutant is the semantic equivalent clone of the original
   * statement on which the muattion will take place.
   * @param posOfIRtoRemove is the list position (starting from 0) of the IR
   * instructions that should be removed during mutation. Note: the mutation may
   * add new ones.
   * @param oprd1_addrOprd is the high level left hand operand for non-pointer
   * operation, and pointer (address) operand for operation on pointers. This in
   * 'toMatchMutant'.
   * @param oprd2_intValOprd is the high level right hand operand for
   * non-pointer operation, and non-pointer (non-address) operand for operation
   * on pointer. This in 'toMatchMutant'.
   * @param returningIRPos is the position of the IR in toMatchMutant whose
   * value is the output of the part of the statement that is mutated. Note:
   * Thie can be null if nothing is returned.
   * @param relevantPosInToMatch is the list of IR's indices in @param toMatch
   * which are mutated (relevant to the change: Mutant Location as IR)
   * @param MI is the module information.
   * @param returnIntoIRPos when not negative is the pos of the IR in
   * toMatchMutant which is the single 'user' of the replacing operation. and
   * the operand index must be passed in retIntoOprd
   * @param retintoOprdIndex when returnintoIR is not null is the index of the
   * operand where the result of the created mutant computation must go
   */
  void doReplacement(MatchStmtIR const &toMatch, MutantsOfStmt &resultMuts,
                     llvmMutationOp::MutantReplacors const &repl,
                     MutantsOfStmt::MutantStmtIR &toMatchMutant,
                     std::vector<unsigned> &posOfIRtoRemove,
                     llvm::Value *oprd1_addrOprd, llvm::Value *oprd2_intValOprd,
                     int returningIRPos,
                     std::vector<unsigned> const &relevantPosInToMatch,
                     ModuleUserInfos const &MI, int returnIntoIRPos = -1,
                     int retIntoOprdIndex = -1) {
    std::vector<llvm::Value *> replacement;
    int replacementInsertPos = -1;

    llvm::Value *returningIR = nullptr;
    llvm::Value *returnIntoIR = nullptr;

    // assert ((repl.getExpElemKey() != mKEEP_ONE_OPRD ||
    // (repl.getOprdIndexList().size()==1 && repl.getOprdIndexList()[0] < 2)) &&
    // "Error in the replacor 'mKEEP_ONE_OPRD'");

    llvm::Value *createdRes =
        MI.getUserMaps()
            ->getMatcherObject(repl.getExpElemKey())
            ->createReplacement(oprd1_addrOprd, oprd2_intValOprd, replacement,
                                MI);

    //#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    // IRFlags non existant for these verisons
    //#else
    // Copy Flags if binop
    //        if (posOfIRtoRemove.size()==1 &&
    //        toMatchMutant[posOfIRtoRemove.front()] &&
    //        llvm::isa<llvm::BinaryOperator>(toMatchMutant[posOfIRtoRemove.front()]))
    //        {
    //            for (auto rinst: replacement)
    //            {
    //                if (auto * risbinop =
    //                llvm::dyn_cast<llvm::BinaryOperator>(rinst))
    //                    risbinop->copyIRFlags(toMatchMutant[posOfIRtoRemove.front()]);
    //            }
    //        }
    //#endif

    if (returningIRPos >= 0) {
      assert((returnIntoIRPos < 0 && retIntoOprdIndex < 0) &&
             "both returningIR and returnIntoIR mode are selected, should only "
             "be one!");

      replacementInsertPos =
          returningIRPos + 1; /// This is used in case posOfIRtoRemove is empty
      returningIR = toMatchMutant.getIRAt(returningIRPos);

      std::vector<std::pair<llvm::User *, unsigned>> affectedUnO;
// set uses of the matched IR to corresponding OPRD
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
      for (llvm::Value::use_iterator ui = returningIR->use_begin(),
                                     ue = returningIR->use_end();
           ui != ue; ++ui) {
        auto &U = ui.getUse();
        llvm::User *user = U.getUser();
        affectedUnO.push_back(
            std::pair<llvm::User *, unsigned>(user, ui.getOperandNo()));
// user->setOperand(ui.getOperandNo(), createdRes);
#else
      for (auto &U : llvm::dyn_cast<llvm::Instruction>(returningIR)->uses()) {
        llvm::User *user = U.getUser();
        affectedUnO.push_back(
            std::pair<llvm::User *, unsigned>(user, U.getOperandNo()));
// user->setOperand(U.getOperandNo(), createdRes);
#endif
        // Avoid infinite loop because of setoperand ...
        // if (llvm::dyn_cast<llvm::Instruction>(returningIR)->getNumUses() <=
        // 0)
        //    break;
      }
      for (auto &affected : affectedUnO)
        if (affected.first != createdRes &&
            std::find(replacement.begin(), replacement.end(), affected.first) ==
                replacement.end()) // avoid 'use loop': a->b->a
          affected.first->setOperand(affected.second, createdRes);
    } else {
      assert((returnIntoIRPos >= 0 && retIntoOprdIndex >= 0) &&
             "neither returningIR nor returnIntoIR mode are selected, or "
             "invalid oprd passed!");

      replacementInsertPos =
          returnIntoIRPos; /// This is used in case posOfIRtoRemove is empty.
                           /// XXX: careful with PHI Nodes (which must be first
                           /// of their BB)
      returnIntoIR = toMatchMutant.getIRAt(returnIntoIRPos);

      // Check, if createRes and the operand are contants, whether they are
      // equal and abort this mutant
      if (constAndEquals(llvm::dyn_cast<llvm::User>(returnIntoIR)
                             ->getOperand(retIntoOprdIndex),
                         createdRes)) {
        if (replacement.empty()) {
          toMatchMutant.deleteContainedMutant();
          return; // XXX: cancel this mutation because the resulting mutant is
                  // equivalent
        }
      }

      llvm::dyn_cast<llvm::User>(returnIntoIR)
          ->setOperand(retIntoOprdIndex, createdRes);
    }

    std::sort(posOfIRtoRemove.begin(), posOfIRtoRemove.end());
    for (int i = posOfIRtoRemove.size() - 1; i >= 0; i--) {
      toMatchMutant.eraseIRAt(posOfIRtoRemove[i]);
    }
    if (!replacement.empty()) {
      // there are element in posOfIRtoRemove and they are continuous (it waas
      // sorted above) then put at first
      if (!posOfIRtoRemove.empty()) {
        if (posOfIRtoRemove.back() - posOfIRtoRemove.front() + 1 ==
            posOfIRtoRemove.size()) {
          replacementInsertPos = posOfIRtoRemove[0];
        } else {
          unsigned cshift = 0;
          for (auto ttmp : posOfIRtoRemove) {
            if (ttmp >= replacementInsertPos)
              break;
            ++cshift;
          }
          replacementInsertPos -= cshift;
        }
      }
      toMatchMutant.insertIRAt(replacementInsertPos, replacement);
    }
    resultMuts.add(/*toMatch, */ toMatchMutant, repl, relevantPosInToMatch);

    toMatchMutant.clear();
  }

}; // class GenericMuOpBase

} // namespace mart

#endif //__MART_GENMU_operatorClasses__GenericMuOpBase__
