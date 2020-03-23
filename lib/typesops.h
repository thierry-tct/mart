/**
 * -==== typesops.h
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Define the classes and types used by mutation process
 */

#ifndef __MART_GENMU_typesops__
#define __MART_GENMU_typesops__

#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "llvm/IR/InstrTypes.h" //TerminatorInst
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"

#include "llvm/IR/LLVMContext.h" //context

#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/DataLayout.h" //datalayout

#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
#include "llvm/DebugInfo.h" //DIScope
#else
#include "llvm/IR/DebugInfoMetadata.h" //DIScope
#endif

#include "usermaps.h"

// https://github.com/anhero/JsonBox
#include "third-parties/JsonBox/include/JsonBox.h"

namespace mart {

class UtilsFunctions {
  /**
   * \brief Print src location into the string stream. @note: better use
   * getSrcLoc() to access src loc info of an instruction
   */
  static void printLoc(llvm::DebugLoc const &Loc,
                       llvm::raw_string_ostream &ross) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    if (!Loc.isUnknown()) {
      llvm::DIScope Scope(Loc.getScope(llvm::getGlobalContext()));
      ross << Scope.getFilename();
      ross << ':' << Loc.getLine();
      if (Loc.getCol() != 0)
        ross << ':' << Loc.getCol();

      llvm::MDNode *inlineMN = Loc.getInlinedAt(llvm::getGlobalContext());
      if (inlineMN) {
        llvm::DebugLoc InlinedAtDL =
            llvm::DebugLoc::getFromDILocation(inlineMN);
        if (!InlinedAtDL.isUnknown()) {
          ross << " @[ ";
          printLoc(InlinedAtDL, ross);
          ross << " ]";
        }
      }
    }
#else
    if (Loc) {
      auto *Scope = llvm::cast<llvm::DIScope>(Loc.getScope());
      ross << Scope->getFilename();
      ross << ':' << Loc.getLine();
      if (Loc.getCol() != 0)
        ross << ':' << Loc.getCol();

      if (llvm::DebugLoc InlinedAtDL = Loc.getInlinedAt()) {
        ross << " @[ ";
        printLoc(InlinedAtDL, ross);
        ross << " ]";
      }
    }
#endif
  }

public:
  /**
   * \bief get the src location of the instruction and return it as a string, if
   * do not exist, return the empty string
   */
  static std::string getSrcLoc(llvm::Instruction const *inst) {
    const llvm::DebugLoc &Loc = inst->getDebugLoc();
    std::string tstr;
    llvm::raw_string_ostream ross(tstr);
    printLoc(Loc, ross);
    return ross.str();
  }
}; // class UtilsFunctions

/**
 * \bref This class define the information about the current modules, that can
 * be useful for the mutation operator
 */
struct ModuleUserInfos {
private:
  // \bref Current Module
  llvm::Module *curModule;

  // \bref Data Layout of the current module (useful to get information like
  // pointer size...)
  llvm::DataLayout *DL;

  // \bref The current context
  llvm::LLVMContext *curContext;

  // \bref The
  UserMaps *usermaps;

public:
  const char *G_MAIN_FUNCTION_NAME = "main";

  ModuleUserInfos(llvm::Module *module, UserMaps *umaps) {
    DL = nullptr;
    setModule(module);
    setUserMaps(umaps);
  }
  ~ModuleUserInfos() { delete DL; }

  inline void setModule(llvm::Module *module) {
    curModule = module;
    if (DL) {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
      // here 'module->getDataLayout()' returns a string
      DL->init(module->getDataLayout());
#else
      DL->init(module);
#endif
    } else {
      DL = new llvm::DataLayout(curModule);
    }
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 9)
    curContext = &(llvm::getGlobalContext());
#else
    if (curModule != nullptr) {
        // was created in readIR
        curContext = &(curModule->getContext());
    } else {
        static llvm::LLVMContext getGlobalContext;
        curContext = &getGlobalContext;
    }
#endif
  }
  inline void setUserMaps(UserMaps *umaps) { usermaps = umaps; }

  inline llvm::Module *getModule() const { return curModule; }
  inline llvm::DataLayout const &getDataLayout() const { return *DL; }
  inline llvm::LLVMContext &getContext() const { return *curContext; }
  inline UserMaps *getUserMaps() const { return usermaps; }
}; // struct ModuleUserInfos

// Each statement is a string where elements are separated by spaces
class llvmMutationOp {
public:
  struct MutantReplacors {
  private:
    enum ExpElemKeys expElemKey;
    std::vector<unsigned> oprdIndexes;
    std::string mutOpName;

  public:
    MutantReplacors(enum ExpElemKeys e, std::vector<unsigned> o, std::string mn)
        : expElemKey(e), oprdIndexes(o), mutOpName(mn) {}
    inline enum ExpElemKeys getExpElemKey() const {
      return expElemKey;
    } // first
    inline const std::vector<unsigned> &getOprdIndexList() const {
      return oprdIndexes;
    } // second
    inline const std::string &getMutOpName() const { return mutOpName; }
    inline const std::string *getMutOpNamePtr() const { return &mutOpName; }
  };

private:
  enum ExpElemKeys matchOp;
  std::vector<MutantReplacors> mutantReplacorsList;
  std::vector<enum codeParts> oprdCPType;

public:
  llvmMutationOp(){};
  ~llvmMutationOp(){};
  void setMatchOp(enum ExpElemKeys m, std::vector<std::string> const &oprdsStr,
                  unsigned fromPos = 0) {
    matchOp = m;
    for (std::vector<std::string>::const_iterator
             it = oprdsStr.begin() + fromPos,
             ie = oprdsStr.end();
         it != ie; ++it)
      oprdCPType.push_back(UserMaps::getCodePartType(*it));
  }
  inline void setMatchOp_CP(enum ExpElemKeys m,
                            std::vector<enum codeParts> const &oprdsCP) {
    matchOp = m;
    for (enum codeParts cp : oprdsCP)
      oprdCPType.push_back(cp);
  }
  inline void addReplacor(const enum ExpElemKeys repop,
                          std::vector<unsigned> const &oprdpos,
                          std::string const &name) {
    mutantReplacorsList.push_back(MutantReplacors(repop, oprdpos, name));
  }
  inline std::vector<MutantReplacors> const &getMutantReplacorsList() const {
    return mutantReplacorsList;
  }
  inline const MutantReplacors &getReplacor(unsigned ind) const {
    return mutantReplacorsList.at(ind);
  }
  inline unsigned getNumReplacor() const { return mutantReplacorsList.size(); }
  inline enum ExpElemKeys getMatchOp() const { return matchOp; }
  inline enum codeParts getCPType(unsigned pos) const {
    return oprdCPType.at(pos);
  }
  inline const std::vector<enum codeParts> &getListCPType() const {
    return oprdCPType;
  }

  ////////////////                          //////////////////
  /// These static fields are used to manage constant values /
  ////////////////////////////////////////////////////////////
private:
  static std::map<unsigned, std::string> posConstValueMap;
  static const unsigned maxOprdNum = 1000;

public:
  /// Check whether the index (pos) represent the index in the ConstValue Map of
  /// a user specified constant or an operand index.
  inline static bool isSpecifiedConstIndex(unsigned pos) {
    return (pos > maxOprdNum);
  }

  /**
   * \brief This method
   */
  static unsigned insertConstValue(std::string val, bool numeric = true) {
    // check that the string represent a Number (Int(radix 10 or 16) or
    // FP(normal or exponential))
    if (numeric &&
        !isNumeric(llvm::StringRef(val).lower().c_str(),
                   10)) /// radix 10 only for now. \todo implement others (8,
                        /// 16) @see ifConstCreate
    {
      // return a value <= maxOprdNum
      return 0;
    }

    static unsigned curPos = maxOprdNum; // static, keep value of previous call
    curPos++;

    for (std::map<unsigned, std::string>::iterator
             it = posConstValueMap.begin(),
             ie = posConstValueMap.end();
         it != ie; ++it) {
      if (!val.compare(it->second))
        return it->first;
    }
    if (!posConstValueMap.insert(std::pair<unsigned, std::string>(curPos, val))
             .second)
      assert(
          false &&
          "Error: inserting an already existing key in map posConstValueMap");
    return curPos;
  }
  static std::string &getConstValueStr(unsigned pos) {
    // Fails if pos is not in map
    return posConstValueMap.at(pos);
  }
  static void destroyPosConstValueMap() { posConstValueMap.clear(); }

  static bool isNumeric(const char *pszInput, int nNumberBase) {
    std::istringstream iss(pszInput);

    if (nNumberBase == 10) {
      double dTestSink;
      iss >> dTestSink;
    } else if (nNumberBase == 8 || nNumberBase == 16) {
      int nTestSink;
      iss >> ((nNumberBase == 8) ? std::oct : std::hex) >> nTestSink;
    } else
      return false;

    // was any input successfully consumed/converted?
    if (!iss)
      return false;

    // was all the input successfully consumed/converted?
    return (iss.rdbuf()->in_avail() == 0);
  }

  std::string toString() {
    std::string ret(std::to_string(matchOp) + " --> ");
    for (auto &v : mutantReplacorsList) {
      ret += std::to_string(v.getExpElemKey()) + "(";
      for (unsigned pos : v.getOprdIndexList()) {
        ret += "@" + std::to_string(pos) + ",";
      }
      if (ret.back() == ',')
        ret.pop_back();
      ret += ");";
    }
    ret += "\n";
    return ret;
  }
}; // class llvmMutationOp

// std::map<unsigned, std::string> llvmMutationOp::posConstValueMap;     //To
// avoid linking error (double def), this is moved into usermaps.cpp

/**
 * \brief Define the scope of the mutation (Which source file's code to mutate,
 * which function to mutate, should we mutate function call parameters? array
 * indexing?)
 */
class MutationScope {
private:
  static const bool matchOnlySrcFilePathBasename = true;
  bool mutateAllFuncs;
  std::unordered_set<llvm::Function *> funcsToMutate;
  bool initialized;

public:
  MutationScope() : initialized(false) {}

  std::string getBasename(std::string const &subjStr) {
    auto sep = subjStr.rfind('/');
    if (sep == std::string::npos)
      sep = subjStr.rfind('\\');
    if (sep == std::string::npos)
      sep = 0;
    else
      sep++;
    return subjStr.substr(sep);
  }

  void Initialize(llvm::Module &module, std::string inJsonFilename = "") {
    initialized = true;

    // No scope data was given, use everything
    if (inJsonFilename.empty()) {
      mutateAllFuncs = true;
      funcsToMutate.clear();
      return;
    }

    std::unordered_set<std::string> specSrcFiles;
    std::unordered_set<std::string> specFuncs;

    std::unordered_set<std::string> seenSrcs;

    JsonBox::Value inScope;
    inScope.loadFromFile(inJsonFilename);
    if (!inScope.isNull()) {
      assert(inScope.isObject() &&
             "The JSON file data of mutation scope must be a JSON object");

      // If "Source-Files" is not absent (select some)
      if (!inScope["Source-Files"].isNull()) {
        assert(inScope["Source-Files"].isArray() &&
               "The list of Source-File to mutate, if present, must be a JSON "
               "array of string");
        JsonBox::Array const &srcList = inScope["Source-Files"].getArray();
        if (srcList.size() > 0) // If there are some srcs specified
        {
          for (auto &val : srcList) {
            assert(val.isString() && "An element of the JSON array "
                                     "Source-Files is not a string. source "
                                     "file name must be string.");

            std::string tmp;
            if (matchOnlySrcFilePathBasename)
              tmp.assign(getBasename(val.getString()));
            else
              tmp.assign(val.getString());

            if (!specSrcFiles.insert(tmp).second) {
              llvm::errs() << "Failed to insert the source " << tmp
                           << " into set of srcs. Probably specified twice.";
              assert(false);
            }
          }
        }
      }
      if (!inScope["Functions"].isNull()) {
        // Check if no function is specified, mutate all (from the selected
        // sources), else only mutate those specified
        assert(inScope["Functions"].isArray() && "The list of Functions to "
                                                 "mutate, if present, must be "
                                                 "a JSON array of string");
        JsonBox::Array const &funcList = inScope["Functions"].getArray();
        if (funcList.size() > 0) // If there are some functions specified
        {
          for (auto &val : funcList) {
            assert(val.isString() && "An element of the JSON array Functions "
                                     "is not a string. Functions name must be "
                                     "string.");
            if (!specFuncs.insert(val.getString()).second) {
              llvm::errs() << "Failed to insert the source " << val.getString()
                           << " into set of Funcs. Probably specified twice.";
              assert(false);
            }
          }
        }
      }

      /*if (! inScope[""].isNull())
      {

      }*/

      // When no source is specified, no need to check existance of dbg info
      bool hasDbgIfSrc = (specSrcFiles.empty());

      for (auto &Func : module) {
        if (Func.isDeclaration())
          continue;

        bool canMutThisFunc = true;
        if (!specSrcFiles.empty()) {
          std::string srcOfF;
          for (auto &BB : Func) {
            for (auto &Inst : BB) {
              /// Get Src level LOC
              std::string tmpSrc = UtilsFunctions::getSrcLoc(&Inst);
              if (!tmpSrc.empty()) {
                srcOfF.assign(tmpSrc);
                assert(srcOfF.length() > 0 && "LOC in source is empty string");
                break;
              }
            }
            if (srcOfF.length() > 0) {
              hasDbgIfSrc = true;
              std::size_t found = srcOfF.find(":");
              assert(found != std::string::npos && found > 0 &&
                     "Problem with the src loc info, didn't find the ':'.");

              std::string tmp;
              if (matchOnlySrcFilePathBasename)
                tmp.assign(getBasename(srcOfF.substr(0, found)));
              else
                tmp.assign(srcOfF.substr(0, found));

              if (specSrcFiles.count(tmp) == 0) {
                canMutThisFunc = false;
              } else {
                seenSrcs.insert(tmp);
              }
              break;
            }
          }
        }
        if (!specFuncs.empty()) {
          if (specFuncs.count(Func.getName())) {
            if (!canMutThisFunc) {
              llvm::errs() << "Error in input file: Function selected to be "
                              "mutated but corresponding source file not "
                              "selected: "
                           << Func.getName() << "\n";
              assert(false);
            }
            funcsToMutate.insert(&Func);
          }
        } else if (canMutThisFunc) {
          funcsToMutate.insert(&Func);
        }
      }
      assert(hasDbgIfSrc && "No debug information in the module, but mutation "
                            "source file scope given");
      if (!specFuncs.empty()) {
        assert(specFuncs.size() == funcsToMutate.size() &&
               "Error: Either some specified function do not exist or Bug (non "
               "specified function is added).");
        for (auto *f : funcsToMutate) {
          if (!specFuncs.count(f->getName())) {
            assert(specFuncs.count(f->getName()) &&
                   "a function specified is not in function to mutate. please "
                   "report a bug");
          }
        }
      }
      if (seenSrcs.size() != specSrcFiles.size()) {
        llvm::errs() << "Specified Srcs: ";
        for (auto &s : specSrcFiles)
          llvm::errs() << " " << s;
        llvm::errs() << "\nSeen Srcs: ";
        for (auto &s : seenSrcs)
          llvm::errs() << " " << s;
        llvm::errs() << "\n";
        assert(seenSrcs.size() == specSrcFiles.size() &&
               "Some specified sources file are not found in the module.");
      }
    }
    mutateAllFuncs = funcsToMutate.empty();
  }

  /**
   * \brief This method return true if the function passed is in the scope (to
   * be mutated). false otherwise
   */
  inline bool functionInMutationScope(llvm::Function *F) {
    assert(isInitialized() &&
           "checking function in uninitialized scope object");
    if (mutateAllFuncs)
      return true;
    else
      return (funcsToMutate.count(F) > 0);
  }

  /**
   * \brief Check wheter initialized
   */
  inline bool isInitialized() { return initialized; }
}; // class MutationScope

/**
 *  \brief This class define ano original statement
 */
struct MatchStmtIR {
  std::vector<llvm::Value *> toMatchIRs;

  // must be ordered ( /*<BB startPos in IR vector, BB>*/)
  std::vector<std::pair<unsigned, llvm::BasicBlock *>> bbStartPosToOrigBB;

  // Alway go together with toMatchIRs (same modifications - push, pop,...)
  std::vector<unsigned> posIRsInOrigFunc;

  friend void setToCloneStmtIROf(MatchStmtIR const &toMatch);

  void clear() {
    toMatchIRs.clear();
    bbStartPosToOrigBB.clear();
    posIRsInOrigFunc.clear();
  }
  void dumpIRs() const {
    llvm::errs() << "\n";
    for (auto *x : toMatchIRs)
      x->dump();
  }
  inline int getNumBB() const { return bbStartPosToOrigBB.size(); }
  inline llvm::BasicBlock *getBBAt(int i) const {
    return bbStartPosToOrigBB[i].second;
  }
  inline unsigned getTotNumIRs() const { return toMatchIRs.size(); }
  inline const std::vector<llvm::Value *> &getIRList() const {
    return toMatchIRs;
  }
  inline llvm::Value *getIRAt(int ind) const { return toMatchIRs[ind]; }
  void getFirstAndLastIR(llvm::BasicBlock *selBB, llvm::Instruction *&firstIR,
                         llvm::Instruction *&lastIR) const {
    auto it = bbStartPosToOrigBB.begin(), ie = bbStartPosToOrigBB.end();
    for (; it != ie; ++it) {
      if (it->second == selBB)
        break;
    }
    assert(it != ie && "basic block absent");
    firstIR = llvm::dyn_cast<llvm::Instruction>(toMatchIRs.at(it->first));
    ++it;
    lastIR = llvm::dyn_cast<llvm::Instruction>(
        toMatchIRs.at((it != ie) ? it->first - 1 : toMatchIRs.size() - 1));
  }
  bool wholeStmtHasTerminators() const {
    int instpos = toMatchIRs.size() - 1;
    int bbind = bbStartPosToOrigBB.size() - 1;
    do {
      if (auto *term =
              llvm::dyn_cast<llvm::TerminatorInst>(toMatchIRs[instpos])) {
        if (term->getNumSuccessors() == 0) // case for return and unreachable...
          return true;
        for (auto i = 0; i < term->getNumSuccessors(); i++) {
          llvm::BasicBlock *bb = term->getSuccessor(i);
          bool notfound = true;
          for (auto &pit : bbStartPosToOrigBB)
            if (bb == pit.second)
              notfound = false;
          if (notfound)
            return true;
        }
      }
      // Index of last instruction of basic block,
      // before the one at position bbind
      instpos = bbStartPosToOrigBB[bbind--].first - 1;
    } while (bbind >= 0);
    return false;
  }

  /**
   * \brief Finds the position of an IR instruction in a list of IRs through
   * sequential search from a starting position
   * @param irinst value searched
   * @param pos is the starting point of the search
   * @param firstCheckBefore is the flag to decide whether we first search
   * before (true) @param pos or after (false)
   * @return the position of the searched IR instruction
   */
  int depPosofPos(llvm::Value const *irinst, int pos,
                  bool firstCheckBefore = true) const {
    int findpos;
    if (firstCheckBefore) {
      findpos = pos - 1;
      for (; findpos >= 0; findpos--)
        if (toMatchIRs[findpos] == irinst)
          break;
      if (findpos < 0) {
        for (findpos = pos + 1; findpos < toMatchIRs.size(); findpos++)
          if (toMatchIRs[findpos] == irinst)
            break;
        if (!(toMatchIRs.size() > findpos)) {
          for (auto *tmpins : toMatchIRs)
            llvm::dyn_cast<llvm::Instruction>(tmpins)->dump();
          llvm::errs() << "Pos = " << pos << "\n";
          /*toMatchIRs.size() > findpos*/
          assert(false && "Impossible error (before)");
        }
      }
    } else {
      findpos = pos + 1;
      for (; findpos < toMatchIRs.size(); findpos++)
        if (toMatchIRs[findpos] == irinst)
          break;
      if (findpos >= toMatchIRs.size()) {
        for (findpos = pos - 1; findpos >= 0; findpos--)
          if (toMatchIRs[findpos] == irinst)
            break;
        if (!(findpos >= 0)) {
          for (auto *tmpins : toMatchIRs)
            llvm::dyn_cast<llvm::Instruction>(tmpins)->dump();
          llvm::errs() << "Pos = " << pos << "\n";
          assert(false /*findpos>=0*/ && "Impossible error (after)");
        }
      }
    }
    return findpos;
  }
}; //~ struct MatchStmtIR

typedef unsigned MutantIDType;

struct WholeStmtMutationOnce {
private:
  bool deleted;
  bool trapped;

public:
  WholeStmtMutationOnce() : deleted(false), trapped(false) {}
  inline bool isDeleted() const { return deleted; }
  inline bool isTrapped() const { return trapped; }
  inline void setDeleted() { deleted = true; }
  inline void setTrapped() { trapped = true; }
}; // struct WholeStmtMutationOnc

/**
 * \brief This class represent the list of mutants of a statement, generated by
 * mutation op, these are not yet attached to the module. Once attached, update
 * @see MutantList
 */
struct MutantsOfStmt {
  /// This struct represent the toMatchMutant (Irs BB for the mutants and the
  /// corresponding BB in original)
  struct MutantStmtIR {
    // We use Vector for muBB because an IR in orig can give a mutant
    // having many BBs in mutant (ex: a&b --> a&&b). this No need for ordered
    std::unordered_map<llvm::BasicBlock *, std::vector<llvm::BasicBlock *>>
        origBBToMutBB;

    std::vector<llvm::Value *>
        toMatchIRsMutClone; // Help for fast element access by position

    // delete the basic block of the mutant (mutant is equivalent)
    void deleteContainedMutant() {
      for (auto it : origBBToMutBB)
        for (auto *bb : it.second)
          delete bb;
      clear();
    }
    inline void clear() {
      origBBToMutBB.clear();
      toMatchIRsMutClone.clear();
    }
    inline bool empty() {
      return origBBToMutBB.empty() && toMatchIRsMutClone.empty();
    }
    void dumpIRs() const {
      llvm::errs() << "\n";
      for (auto *x : toMatchIRsMutClone)
        x->dump();
    }
    inline void add(llvm::BasicBlock *orig,
                    std::vector<llvm::BasicBlock *> mut) {
      assert(origBBToMutBB.count(orig) == 0 && "orig already inserted");
      origBBToMutBB[orig] = mut;
    }
    inline std::vector<llvm::BasicBlock *> &getMut(llvm::BasicBlock *orig) {
      return origBBToMutBB[orig];
    }
    inline llvm::Value *getIRAt(int ind /*0,1,...*/) const {
      return toMatchIRsMutClone.at(ind);
    }

    /**
     * \brief insert an IR into this mutantstmt, THIS MUTANT MUST HAVE BEEN
     * CLONED FOM THE ORIGINAL, not set with setToEmptyOrFixedStmtOf()..
     */
    inline void insertIRAt(
        unsigned ind,
        llvm::Value *irVal) // TODO: fix for the case of empty middle block
    {
      llvm::Instruction *instAtInd = nullptr;
      if (toMatchIRsMutClone.size() > ind) {
        instAtInd = llvm::dyn_cast<llvm::Instruction>(toMatchIRsMutClone[ind]);
        // XXX: Make Sure that the middle basic block is never empty (according
        // to 'toMatchIRsMutClone'). Else can't know where to insert. Only
        // delete stmt can have it empty;
        llvm::dyn_cast<llvm::Instruction>(irVal)->insertBefore(instAtInd);
      } else { // last position of toMatchIRsMutClone, which cannot be empty
               // since it is cloned from original
        assert(
            origBBToMutBB.size() >= 1 &&
            "must have at least one BB here (since it must have been cloned)");
        llvm::BasicBlock *bb;
        if (toMatchIRsMutClone.empty())
          bb = origBBToMutBB.begin()->second.front();
        else
          bb = llvm::dyn_cast<llvm::Instruction>(toMatchIRsMutClone.back())
                   ->getParent(); // origBBToMutBB.begin()->second.front();

        bb->getInstList().push_back(llvm::dyn_cast<llvm::Instruction>(irVal));
      }

      toMatchIRsMutClone.insert(toMatchIRsMutClone.begin() + ind, irVal);
    }
    /**
     * \brief insert IRs into this mutantstmt, THIS MUTANT MUST HAVE BEEN CLONED
     * FOM THE ORIGINAL not set with setToEmptyOrFixedStmtOf()..
     * // TODO: fix for the case of empty middle block
     */
    inline void insertIRAt(unsigned ind,
                           std::vector<llvm::Value *> const &irVals) {
      llvm::Instruction *instAtInd = nullptr;
      if (toMatchIRsMutClone.size() > ind) {
        instAtInd = llvm::dyn_cast<llvm::Instruction>(toMatchIRsMutClone[ind]);
        // XXX: Make Sure that the middle basic block is never empty (according
        // to 'toMatchIRsMutClone'). Else can't know where to insert. Only
        // delete stmt can have it empty;
        for (auto *inst : irVals)
          llvm::dyn_cast<llvm::Instruction>(inst)->insertBefore(instAtInd);
      } else { // last position of toMatchIRsMutClone, which cannot be empty
               // since it is cloned from original
        assert(origBBToMutBB.size() >= 1 && "must have at least one BB here");
        llvm::BasicBlock *bb;
        if (toMatchIRsMutClone.empty())
          bb = origBBToMutBB.begin()->second.front();
        else
          bb = llvm::dyn_cast<llvm::Instruction>(toMatchIRsMutClone.back())
                   ->getParent(); // origBBToMutBB.begin()->second.front();

        for (auto *inst : irVals)
          bb->getInstList().push_back(llvm::dyn_cast<llvm::Instruction>(inst));
      }

      toMatchIRsMutClone.insert(toMatchIRsMutClone.begin() + ind,
                                irVals.begin(), irVals.end());
    }
    inline void eraseIRAt(unsigned ind) {
      if (!llvm::isa<llvm::Constant>(toMatchIRsMutClone[ind])) {
        if (!toMatchIRsMutClone[ind]->use_empty()) {
          llvm::errs() << "\nError: Deleting IR while its use is not empty. "
                          "Please report Bug (MART)!\n\n";
          llvm::dyn_cast<llvm::Instruction>(toMatchIRsMutClone[ind])->dump();
          assert(false);
        }
        llvm::dyn_cast<llvm::Instruction>(toMatchIRsMutClone[ind])
            ->eraseFromParent();
      }
      toMatchIRsMutClone.erase(toMatchIRsMutClone.begin() + ind);
    }
    void setToEmptyOrFixedStmtOf(
        MatchStmtIR const &toMatch, ModuleUserInfos const &MI,
        llvm::SmallVector<llvm::Instruction *, 1> *fixedinsts = nullptr) {
      assert(empty() && "already have some data, first clear it");
      for (auto i = 0; i < toMatch.getNumBB(); i++) {
        llvm::BasicBlock *bb = llvm::BasicBlock::Create(MI.getContext());
        origBBToMutBB[toMatch.getBBAt(i)] =
            std::vector<llvm::BasicBlock *>({bb});
        if (fixedinsts) {
          for (auto *finst : *fixedinsts) {
            bb->getInstList().push_back(finst);
            toMatchIRsMutClone.push_back(finst);
          }
        }
      }
    }

    /**
     * \brief Clone a statement into a new one. The structural relation betwenn
     * IRs in original is kept
     * \detail Assumes that the IRs instructions in stmtIR have same order as
     * the initial original IR code
     * @param toMatch is the list of IRs representing the statement to clone.
     */
    void setToCloneStmtIROf(MatchStmtIR const &toMatch,
                            ModuleUserInfos const &MI) {
      assert(empty() &&
             "already have some data, first clear it before cloning into");
      llvm::SmallDenseMap<llvm::Value *, llvm::Value *> pointerMap;
      for (llvm::Value *I : toMatch.toMatchIRs) {
        // clone instruction
        llvm::Value *newI = llvm::dyn_cast<llvm::Instruction>(I)->clone();

        // set name
        // if (I->hasName())
        //    newI->setName((I->getName()).str()+"_Mut0");

        if (!pointerMap.insert(std::pair<llvm::Value *, llvm::Value *>(I, newI))
                 .second) {
          assert(false && "Error (Mutation::getOriginalStmtBB): inserting an "
                          "element already in the map\n");
        }
      };
      for (llvm::Value *I : toMatch.toMatchIRs) {
        for (unsigned opos = 0;
             opos < llvm::dyn_cast<llvm::User>(I)->getNumOperands(); opos++) {
          auto oprd = llvm::dyn_cast<llvm::User>(I)->getOperand(opos);
          if (llvm::isa<llvm::Instruction>(oprd)) {
            if (auto newoprd = pointerMap.lookup(
                    oprd)) // TODO:Double check the use of lookup for this map
            {
              llvm::dyn_cast<llvm::User>(pointerMap.lookup(I))
                  ->setOperand(opos, newoprd); // TODO:Double check the use of
                                               // lookup for this map
            } else if (!llvm::isa<llvm::AllocaInst>(oprd)) {
              bool fail = false;
              switch (opos) {
              /*case 0:
                  {
                      if (llvm::isa<llvm::StoreInst>(I))
                          fail = true;
                      break;
                  }
              case 1:
                  {
                      if (llvm::isa<llvm::LoadInst>(I))
                          fail = true;
                      break;
                  }*/
              default:
                fail = true;
              }

              if (fail) {
                llvm::errs() << "Error (Mutation::getOriginalStmtBB): lookup "
                                "an element not in the map -- ";
                llvm::dyn_cast<llvm::Instruction>(I)->dump();
                oprd->dump();
                assert(false && "");
              }
            }
          }
        }
        // TODO:Double check the use of lookup for this map
        toMatchIRsMutClone.push_back(
            llvm::dyn_cast<llvm::Instruction>(pointerMap.lookup(I)));
      }

      // Now fix the basic block successors and create mutant BBs
      /// Create basic blocks
      for (auto &spbbP : toMatch.bbStartPosToOrigBB)
        origBBToMutBB[spbbP.second] = std::vector<llvm::BasicBlock *>(
            {llvm::BasicBlock::Create(MI.getContext())});

      int instpos = toMatchIRsMutClone.size() - 1;
      int bbind = toMatch.bbStartPosToOrigBB.size() - 1;
      do {
        if (auto *term = llvm::dyn_cast<llvm::TerminatorInst>(
                toMatchIRsMutClone[instpos])) {
          for (auto i = 0; i < term->getNumSuccessors(); i++) {
            llvm::BasicBlock *bb = term->getSuccessor(i);
            if (origBBToMutBB.count(bb) > 0) {
              term->setSuccessor(i, origBBToMutBB[bb].front());
            }
          }
        }

        // Insert the instructions
        llvm::BasicBlock *workBB =
            origBBToMutBB[toMatch.bbStartPosToOrigBB[bbind].second].front();
        for (auto ipos = toMatch.bbStartPosToOrigBB[bbind].first;
             ipos <= instpos; ipos++) {
          workBB->getInstList().push_back(
              llvm::dyn_cast<llvm::Instruction>(toMatchIRsMutClone[ipos]));
        }
        instpos = toMatch.bbStartPosToOrigBB[bbind--].first -
                  1; // Index of last instruction of basic block before that at
                     // position bbind
      } while (bbind >= 0);

      /*// Fix Phi Nodes with non-constant incoming values (those with constant
      incoming values are handled with proxy BBs). @note: PHI Node are always
      the first appearing instruction of their contained Basic Block
      for (auto ind=0; ind<toMatch.bbStartPosToOrigBB.size(); ind++)
      {
          llvm::PHINode *phiCand;
          auto ii = toMatch.bbStartPosToOrigBB[ind].first;  //first inst of this
      stmt in the BB at ind
          auto ilast = (ind+1 == toMatch.bbStartPosToOrigBB.size())?
      toMatchIRsMutClone.size(): toMatch.bbStartPosToOrigBB[ind+1].first;
          while (ii<ilast && (phiCand =
      llvm::dyn_cast<llvm::PHINode>(toMatchIRsMutClone[ii])))
          {
              for (auto pind=0; pind < phiCand->getNumIncomingValues(); ++pind)
              {
                  if (!
      llvm::isa<llvm::Constant>(phiCand->getIncomingValue(pind)))
                  {
                      llvm::BasicBlock *bb = phiCand->getIncomingBlock(pind);
                      assert (origBBToMutBB.count(bb) > 0 && "Error: Incoming
      Basic Block of PHI Node for non-constant value is not in toMatch (but must
      be)!");
                      phiCand->setIncomingBlock(pind,
      origBBToMutBB[bb].front());
                  }
              }
              ii++;
          }
      }*/

    } //~setToCloneStmtIROf

    /**
     * \brief Check that load and stores type are okay
     */
    bool checkLoadAndStoreTypes() {
      for (auto *xx : toMatchIRsMutClone) {
        if (llvm::LoadInst *yy = llvm::dyn_cast<llvm::LoadInst>(xx)) {
          if (llvm::cast<llvm::PointerType>(yy->getPointerOperand()->getType())
                  ->getElementType() != yy->getType()) {
            llvm::errs() << "ERROR: LOAD value type does not match pointer "
                            "pointee type\n";
            // for (auto *zz: toMatchIRsMutClone) zz->dump();
            yy->dump();
            // assert (false && "ERROR: LOAD value type does not match pointer
            // pointee type");
            return false;
          }
        }
        if (llvm::StoreInst *yy = llvm::dyn_cast<llvm::StoreInst>(xx)) {
          if (llvm::cast<llvm::PointerType>(yy->getPointerOperand()->getType())
                  ->getElementType() != yy->getValueOperand()->getType()) {
            llvm::errs() << "ERROR: STORE value type does not match pointer "
                            "pointee type\n";
            yy->dump();
            // assert (false && "ERROR: STORE value type does not match pointer
            // pointee type");
            return false;
          }
        }
      }
      return true;
    }
  }; // struct MutantStmtIR

  struct RawMutantStmt {
    MutantStmtIR mutantStmtIR;

    // Mutant type
    std::string typeName;

    // Location infos
    /// The statement mutation operation object sets this to the position in
    /// the original's (toMatch) vector (can use @seeDoReplaceUseful 's
    /// relevantIRPos)
    std::vector<unsigned> irRelevantPos;

    // Mutant id
    unsigned id;

    RawMutantStmt(MutantStmtIR const &toMatchMutant,
                  llvmMutationOp::MutantReplacors const &repl,
                  std::vector<unsigned> const &relevantPos) {
      mutantStmtIR = toMatchMutant;
      typeName = repl.getMutOpName();
      irRelevantPos = relevantPos;
      id = 0; // initialize to invalid mutant id value. will be modified during
              // merging mutant into Module
    }
  }; // struct RawMutantStmt

  std::vector<RawMutantStmt> results;

  /**
   * \brief This method add a new mutant statement.
   * \detail It computes the corresponding Weak mutation using difference
   * between @param toMatch and @param toMatchMutant
   * @param toMatch is the original Stmt (list of IRs)
   * @param toMatchMutant is the mutant Stmt (list of IRs)
   * @param repl is the mutation replacer that was used to tranform @param
   * toMatch and obtain @param toMatchMutant  (list of IRs)
   * @param relevantPos is the list of indices in @param toMatch of the IRs
   * modified by the mutation (relevant to mutation)
   */
  inline void add(MutantStmtIR const &toMatchMutant,
                  llvmMutationOp::MutantReplacors const &repl,
                  std::vector<unsigned> const &relevantPos) {
    results.emplace_back(toMatchMutant, repl, relevantPos);
  }

  inline void clear() { results.clear(); }

  inline unsigned getNumMuts() { return results.size(); }

  inline MutantStmtIR &getMutantStmtIR(unsigned index) {
    return results[index].mutantStmtIR;
  }
  inline const std::string &getTypeName(unsigned index) {
    return results[index].typeName;
  }
  inline const std::vector<unsigned> &getIRRelevantPos(unsigned index) {
    return results[index].irRelevantPos;
  }
  inline void setMutID(unsigned index, unsigned id) {
    assert(results[index].id == 0 && "setting ID twice");
    results[index].id = id;
  }
  inline unsigned getMutID(unsigned index) {
    assert(results[index].id != 0 && "getting invalid ID");
    return results[index].id;
  }
  inline bool isEmpty() { return (getNumMuts() == 0); }
}; //~struct MutantsOfStmt

/**
 * \brief This class represent a finest src level statement (all connected user-
 * uses on the registers level)
 */
struct StatementSearch {

#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
  std::unordered_set<llvm::Value *> visited; // No need for ordered
#else
  llvm::SmallPtrSet<llvm::Value *, 5> visited; // No need for ordered
#endif
  // Used to maintain same order of instruction as originally when extracting
  // source statement
  int stmtIRcount;

  MatchStmtIR matchStmtIR;

  /// \brief will contain the mutants BBs generated for this stmt
  MutantsOfStmt mutantStmt_list;

  /// \brief Help to check for otomicity of the IRs for this stm in a basic
  /// block. using @checkAtomicity()
  llvm::BasicBlock *atomicityInBB;

  llvm::BasicBlock *lastBB;

  void countInc() { stmtIRcount++; }
  void countDec() { stmtIRcount--; }
  void checkCountLogic() {
    if (stmtIRcount <= 0) {
      if (stmtIRcount < 0)
        llvm::errs() << "Error (Mutation::doMutate): 'stmtIRcount' should "
                        "never be less than 0; Possibly bug in the porgram.\n";
      else
        llvm::errs() << "Error (Mutation::doMutate): Instruction appearing "
                        "multiple times.\n";
      assert(false);
    }
  }

  inline bool isCompleted() {
    return (stmtIRcount == 0 && !matchStmtIR.toMatchIRs.empty());
  }

  /// return true if this statement search is on a block different than
  /// the block where it was created
  inline bool isOnNewBasicBlock(llvm::BasicBlock *curBB) {
    // atomicityInBB will never be NULL if it is new BB and the stmt
    // has parts on a previous one (see switchFromTo())
    // return true if: atomicityInBB is not null and BB differs
    return (atomicityInBB != nullptr && atomicityInBB != curBB);
  }

  void checkAtomicityInBB(llvm::BasicBlock *curBB) {
    if (atomicityInBB == curBB) {
      curBB->dump();
      llvm::errs() << "Error (Mutation::doMutate): Problem with IR - "
                      "statements are not atomic ("
                   << stmtIRcount
                   << "). Maybe mem2reg was applied to input module...\n";
      for (auto *rr : matchStmtIR.toMatchIRs)          // DEBUG
        llvm::dyn_cast<llvm::Instruction>(rr)->dump(); // DEBUG
      assert(false);
    }
  }

  StatementSearch() { clear(); }
  void clear() {
    matchStmtIR.clear();
    visited.clear();
    stmtIRcount = 0;
    atomicityInBB = lastBB = nullptr;
  }

  bool isVisited(llvm::Value *v) { return (visited.count(v) > 0); }
  bool visit(llvm::Value *v) {
    return visited.insert(v).second;
  } // if not yet visited, insert into 'visited' set and return true. else
  // return false.

  void appendIRToStmt(llvm::BasicBlock *bb, llvm::Value *ir,
                      unsigned posInFunc) {
    matchStmtIR.toMatchIRs.push_back(ir);
    matchStmtIR.posIRsInOrigFunc.push_back(posInFunc);
    if (lastBB != bb) {
      // assert ()
      // -1 bellow because we just appendend and inst and it should be its pos
      matchStmtIR.bbStartPosToOrigBB.emplace_back(
          matchStmtIR.toMatchIRs.size() - 1, bb);
      lastBB = bb;
    }
  }

  /// When switchingto a statement for search, make sure it is atomic
  static StatementSearch *switchFromTo(llvm::BasicBlock *curBB,
                                       StatementSearch *stmtFrom,
                                       StatementSearch *stmtTo) {
    if (stmtFrom)
      stmtFrom->atomicityInBB = curBB;
    assert(!stmtTo->isCompleted() &&
           "Switching to a completed (searching) statement. Should not");
    return stmtTo;
  }
}; //~ struct StatementSearch

/**
 * \brief This class define the final list of all mutant and their informations.
 * @Note: This is increased after each statement mutation and modifed (reduced)
 * during TCE equivalent mutant removal
 * Ensure that the mutant in the vector at position 'i' has ID 'i+1' (mutant
 * with ID 'j' is found at position 'j-1'). Note that positions start from 0.
 */
struct MutantInfoList {
  struct MutantInfo {
    // Mutant id
    MutantIDType id;

    // Mutant type
    std::string typeName;

    // Location infos
    std::string locFuncName;
    std::vector<unsigned> irLeveLocInFunc;
    std::string srcLevelLoc;

    MutantInfo(MutantIDType mutant_id, std::string const &type,
               std::string const &funcName, std::vector<unsigned> const &irPos,
               std::string const &srcLoc)
        : id(mutant_id), typeName(type), locFuncName(funcName),
          irLeveLocInFunc(irPos), srcLevelLoc(srcLoc) {}

    MutantInfo(MutantIDType mid, std::vector<llvm::Value *> const &toMatch,
               std::string const &mName, std::vector<unsigned> const &relpos,
               llvm::Function *curFunc, std::vector<unsigned> const &absPos) {
      id = mid;
      typeName = mName;
      locFuncName = curFunc->getName();
      for (auto ind : relpos)
        irLeveLocInFunc.push_back(absPos[ind]);

      /// Get SRC level LOC
      for (auto *val : toMatch) {
        if (const llvm::Instruction *I =
                llvm::dyn_cast<llvm::Instruction>(val)) {
          std::string tmpSrc = UtilsFunctions::getSrcLoc(I);
          if (!tmpSrc.empty()) {
            if (!(srcLevelLoc.empty() ||
                  srcLevelLoc.substr(0, srcLevelLoc.rfind(":") - 1) ==
                      tmpSrc.substr(0, tmpSrc.rfind(":") - 1)))
            // rfind to remove the column info
            {
              /*llvm::errs() << srcLevelLoc << " --- " << tmpSrc <<"\n\n";
              I->dump();
              llvm::errs() <<"toMatch: ---\n";
              for (auto *iival: toMatch)
                  iival->dump();
              assert (false && "A stmt spawn more than 1 src level line of
              code");*/

              // sometimes a function call may spawn multiple lines and the
              // params are computed first but are on next lines. The last would
              // be the function call line
              srcLevelLoc.assign(tmpSrc);
            }
            if (srcLevelLoc.empty()) {
              srcLevelLoc.assign(tmpSrc);
              // break;
            }
          }
        }
      }
    }
  }; // struct MutantInfo

  struct EquivalentDuplicateMutantInfo: MutantInfo{
    // ID of the mutant which mutant is dupliacte. if 0, it means it is equivalent mutant.
    MutantIDType duplicateOfID;

    EquivalentDuplicateMutantInfo(MutantInfo const &mi, MutantIDType newid, MutantIDType dupOfId): MutantInfo(newid, mi.typeName, mi.locFuncName, mi.irLeveLocInFunc, mi.srcLevelLoc), duplicateOfID(dupOfId) {}
  };

private:
  // Ordered list of mutants infos (IDs start from 1, the id 0 is the original program)
  // after TCE, ths contains only the TCE non equivalent, non duplicate mutants
  std::vector<MutantInfo> mutants;

  // Set of all mutants IDs for quich check
  // after TCE, ths contains only the TCE non equivalent, non duplicate mutants
  std::unordered_set<MutantIDType> containedMutsIDs;

  // Ordered list of equivalent and duplicate mutants infos (IDs start from 1, the id 0 is the original program)
  // Before TCE, this is empty
  // After TCE, ths contains only the TCE equivalent and duplicate mutants
  std::vector<EquivalentDuplicateMutantInfo> equivalent_duplicate_mutants;

  void internalAdd(MutantIDType mutant_id, std::string const &type,
                   std::string const &funcName,
                   std::vector<unsigned> const &irPos,
                   std::string const &srcLoc) {
    if (wasAdded(mutant_id)) {
      llvm::errs() << "Error: Mutant with ID " << mutant_id
                   << "internally added several time (reading from JSON)\n";
      assert(false);
    }
    mutants.emplace_back(mutant_id, type, funcName, irPos, srcLoc);
    containedMutsIDs.insert(mutant_id);
  }

  /**
   * \brief Check whether a mutant is already added
   */
  bool wasAdded(MutantIDType mid) const {
    return (containedMutsIDs.count(mid) > 0);
  }

  void setMutantSourceLoc(MutantIDType mutant_id, std::string loc) {
    mutants[mutant_id - 1].srcLevelLoc = loc;
  }

  /**
   * \brief Mutants existing on the IR inserted during compilation or IR preprocessing
   */
  void fixMissingSrcLocs() {
    //Since mutant ID follow the stmt order, search nearest after and before,
    // within the same function and take the closest 
    auto nMuts = getMutantsNumber();
    for (auto mid = 1; mid <= nMuts; ++mid)
      if (getMutantSourceLoc(mid).length() == 0) {
        auto &func = getMutantFunction(mid);
        MutantIDType b_mid = mid-1;
        MutantIDType a_mid = mid+1;
        while (b_mid > 0 && getMutantFunction(b_mid) == func && getMutantSourceLoc(b_mid).length() == 0)
          b_mid--;
        while (a_mid <= nMuts && getMutantFunction(a_mid) == func && getMutantSourceLoc(a_mid).length() == 0)
          a_mid++;
        if (b_mid > 0 and a_mid <= nMuts) {
          // take the one after XXX (more likely to be good)
          setMutantSourceLoc(mid, getMutantSourceLoc(b_mid));
        } else {
          if (b_mid > 0)
            setMutantSourceLoc(mid, getMutantSourceLoc(b_mid));
          if (a_mid <= nMuts)
            setMutantSourceLoc(mid, getMutantSourceLoc(a_mid));
          // If none, it means that no stmt has DBG info
        }
      }
  }

public:
  /**
   * \brief This method add a new mutant's info.
   * \detail It computes the corresponding location using @param relpos and
   * @param curFunc
   * @param toMatch is the original Stmt (list of IRs). Needed to obtain
   * metadata for src level location
   * @param mid is the id of the mutant
   * @param name is the name type of this mutant (as given by the user)
   * @param relpos is the list of indices in toMatch of the mutants IRs
   * @param curFunc is the function containing @param toMatch
   * @param toMatchIRPosInFunc is the list of positions of each IR in toMatch in
   * the function @param curFunc
   */
  void add(MutantIDType mid, std::vector<llvm::Value *> const &toMatch,
           std::string const &mName, std::vector<unsigned> const &relpos,
           llvm::Function *curFunc,
           std::vector<unsigned> const &toMatchIRPosInFunc) {
    if (wasAdded(mid))
      return;
    mutants.emplace_back(mid, toMatch, mName, relpos, curFunc,
                         toMatchIRPosInFunc);
    containedMutsIDs.insert(mid);
  }

  /**
   *  \brief remove the TCE's equivalent and duplicate mutants
   */
  void
  postTCEUpdate(std::map<MutantIDType, std::vector<MutantIDType>> const &nonduplicateIDMap, std::map<MutantIDType, MutantIDType> const &duplicate2nondupMap) {
    std::vector<unsigned> posToDel;
    unsigned pos = 0;
    for (auto &mut : mutants) {
      if (nonduplicateIDMap.count(mut.id) == 0)
        posToDel.push_back(pos);
      else {
        assert(nonduplicateIDMap.at(mut.id).size() == 1 &&
               "(CHECK) The number of element in second vector must be one "
               "here (containing the new mutant ID)");
        // Change mutant ID of non equivalent mutant to new ID
        mut.id = nonduplicateIDMap.at(mut.id).back();
      }
      pos++;
    }
    std::sort(posToDel.begin(), posToDel.end());
    MutantIDType newEqDupId = getMutantsNumber() + 1 - posToDel.size();
    for (auto it = posToDel.rbegin(), ie = posToDel.rend(); it != ie; ++it) {
      assert(mutants.at(*it).id == (1 + (*it)) && "Problem with the order of "
                                                  "mutants, or should delete "
                                                  "last element first");

      // store Duplicate/Equivalent mutant into dup/eq list
      MutantIDType nondup_oldid = duplicate2nondupMap.at(1 + (*it));
      if (nondup_oldid == 0)  // Equivalent mutant, put original ID as the mutant which it is duplicate of
        equivalent_duplicate_mutants.emplace_back(*(mutants.begin() + (*it)), newEqDupId++, 0);
      else // Duplicate of a mutant
        equivalent_duplicate_mutants.emplace_back(*(mutants.begin() + (*it)), newEqDupId++, nonduplicateIDMap.at(nondup_oldid).back());

      // Erase the duplicate/equivalent mutant from non dup/eq set/list
      containedMutsIDs.erase(1 + (*it));
      mutants.erase(mutants.begin() + (*it));
    }
  }

  MutantIDType getMutantsNumber() const { return mutants.size(); }
  const std::string &getMutantTypeName(MutantIDType mutant_id) const {
    return mutants[mutant_id - 1].typeName;
  }
  const std::string &getMutantFunction(MutantIDType mutant_id) const {
    return mutants[mutant_id - 1].locFuncName;
  }
  const std::vector<unsigned> &
  getMutantIrPosInFunction(MutantIDType mutant_id) const {
    return mutants[mutant_id - 1].irLeveLocInFunc;
  }
  const std::string &getMutantSourceLoc(MutantIDType mutant_id) const {
    return mutants[mutant_id - 1].srcLevelLoc;
  }

  void printToStdout() const {
    llvm::errs() << "\n~~~~~~~~~ MUTANTS INFOS ~~~~~~~~~\n\nID, Name, SRC "
                    "Location, Function Name\n-------------------\n";
    for (auto &info : mutants) {
      llvm::errs() << info.id << "," << info.typeName << "," << info.srcLevelLoc
                   << "," << info.locFuncName << "\n";
    }
    // JsonBox::Object outJSON;
    // getJson (outJSON);
    // outJSON.writeToStream(std::cout, true, true);
  }

  void printToJsonFile(std::string filename, std::string eqdupfilename) const {
    JsonBox::Object outJSON;
    getJson(outJSON);
    JsonBox::Value vout(outJSON);
    vout.writeToFile(filename, true, false);

    outJSON.clear();
    getEqDupJson(outJSON);
    JsonBox::Value eqdupvout(outJSON);
    eqdupvout.writeToFile(eqdupfilename, true, false);
  }

  void getJson(JsonBox::Object &outJ) const {
    for (auto &info : mutants) {
      std::string mid(std::to_string(info.id));
      // add a mutant field
      outJ[mid] = JsonBox::Object();

      // Fill in the mutant infos
      outJ[mid]["Type"] = JsonBox::Value(info.typeName);
      outJ[mid]["FuncName"] = JsonBox::Value(info.locFuncName);
      JsonBox::Array tmparr;
      for (auto pos : info.irLeveLocInFunc)
        tmparr.push_back(JsonBox::Value((int)pos));
      outJ[mid]["IRPosInFunc"] = tmparr;
      outJ[mid]["SrcLoc"] = JsonBox::Value(info.srcLevelLoc);
    }
  }

  void getEqDupJson(JsonBox::Object &outJ) const {
    for (auto &eqdupinfo : equivalent_duplicate_mutants) {
      std::string mid(std::to_string(eqdupinfo.id));
      // add a mutant field
      outJ[mid] = JsonBox::Object();

      // Fill in the mutant eqdupinfos
      outJ[mid]["Type"] = JsonBox::Value(eqdupinfo.typeName);
      outJ[mid]["FuncName"] = JsonBox::Value(eqdupinfo.locFuncName);
      JsonBox::Array tmparr;
      for (auto pos : eqdupinfo.irLeveLocInFunc)
        tmparr.push_back(JsonBox::Value((int)pos));
      outJ[mid]["IRPosInFunc"] = tmparr;
      outJ[mid]["SrcLoc"] = JsonBox::Value(eqdupinfo.srcLevelLoc);

      outJ[mid]["EquivalenDuplicateOf"] = JsonBox::Value(std::to_string(eqdupinfo.duplicateOfID));
    }
  }

  void loadFromJsonFile(std::string filename, bool fix_missing_srclocs=false) {
    JsonBox::Value value_in;
    value_in.loadFromFile(filename);
    assert(value_in.isObject() &&
           "The JSON file data of mutants info must be a JSON object");
    JsonBox::Object object_in = value_in.getObject();
    MutantIDType num_of_mutants = object_in.size();
    for (MutantIDType mutant_id = 1; mutant_id <= num_of_mutants; ++mutant_id) {
      std::string mutant_id_string(std::to_string(mutant_id));
      if (object_in.count(mutant_id_string) ==
          0) { // The mutants ID must go from 1 continuously upto num_mutants
        llvm::errs()
            << "Error: mutant " << mutant_id_string
            << " not present in mutant info (Total number of mutants is "
            << num_of_mutants << ")\n\n";
        assert(false);
      }
      assert(object_in[mutant_id_string].isObject() &&
             "Mutant Info must be JSON Object");
      JsonBox::Object mutant_info = object_in[mutant_id_string].getObject();

      JsonBox::Value &type_val = mutant_info["Type"];
      assert(type_val.isString() &&
             "Mutant operator Type must be string in JSON");
      std::string type = type_val.getString();

      JsonBox::Value &funcname_val = mutant_info["FuncName"];
      assert(funcname_val.isString() && "Function Name must be string in JSON");
      std::string funcName = funcname_val.getString();

      JsonBox::Value &irpos_val = mutant_info["IRPosInFunc"];
      assert(irpos_val.isArray() && "IR pos must be array in JSON");
      std::vector<unsigned> irPos;
      for (JsonBox::Value pos_val : irpos_val.getArray()) {
        assert(pos_val.isInteger() && "Pos of IR must be Integer");
        int pos = pos_val.getInteger();
        assert(pos >= 0 && "Pos if IR must be positive");
        irPos.push_back((unsigned)pos);
      }

      JsonBox::Value srcloc_val = mutant_info["SrcLoc"];
      assert(srcloc_val.isString() &&
             "Source Level Loc must be string in JSON");
      std::string srcLoc = srcloc_val.getString();

      internalAdd(mutant_id, type, funcName, irPos, srcLoc);
    }
    if (fix_missing_srclocs)
      fixMissingSrcLocs();
  }
}; // struct MutantInfoList

} // namespace mart

#endif //__MART_GENMU_typesops__
