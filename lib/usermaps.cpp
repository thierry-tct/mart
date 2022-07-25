/**
 * -==== usermaps.cpp
 *
 *                MART Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Define the class UserMaps which is used to map operation name (as
 * string) with the corresponding opcode (expElemKeys).
 *                it also map each opcode to the corresponding mutation op's C++
 * class object
 */

#include "usermaps.h"
#include "typesops.h"

using namespace mart;

// belong to typesops.h (but due to link error -double definition- I put it
// here)
std::map<unsigned, std::string> llvmMutationOp::posConstValueMap;

// contain headers for mutation operators
#include "AUTOGEN_mutation_op_headers.h.inc"

#if defined(CREATE_OBJ) || defined(DESTROY_OBJ)
#error "The macros CREATE_OBJ(C) and DESTROY_OBJ(C) are already defined.."
#endif

#define CREATE_OBJ(C) new C
#define DESTROY_OBJ(C) delete C

void UserMaps::addOpMatchObjectPair(enum ExpElemKeys op,
                                    GenericMuOpBase *muOpObj) {
  const std::pair<enum ExpElemKeys, GenericMuOpBase *> val(op, muOpObj);
  if (!mapOperationMatcher.insert(val).second) {
    llvm::errs()
        << "ERROR: Mutation operation matcher appears multiple times (op = "
        << op << ").\n";
    assert(false && "");
  }
}

void UserMaps::addConfNameOpPair(llvm::StringRef cname,
                                 std::vector<enum ExpElemKeys> ops) {
  const std::pair<std::string, std::vector<enum ExpElemKeys>> val(cname.upper(),
                                                                  ops);
  if (!mapConfnameOperations.insert(val).second) {
    llvm::errs() << "ERROR: config operation name appears multiple times ("
                 << cname.upper() << ").\n";
    assert(false && "");
  }

  /*auto val_iter = mapConfnameOperations.find(cname.upper());
  if (val_iter == mapConfnameOperations.end())
  {
      llvm::errs() << "ERROR: XXXXXXXXXXXXXXXXXXXXX: '" <<"')!!\n";
      assert (false && "");
  }*/
}

bool UserMaps::isConstValOPRD(llvm::StringRef oprd) {
#if (LLVM_VERSION_MAJOR >= 13)
  if (oprd.startswith_insensitive("@") || oprd.startswith_insensitive("C") ||
      oprd.startswith_insensitive("V") || oprd.startswith_insensitive("A") ||
      oprd.startswith_insensitive("P"))
#else
  if (oprd.startswith_lower("@") || oprd.startswith_lower("C") ||
      oprd.startswith_lower("V") || oprd.startswith_lower("A") ||
      oprd.startswith_lower("P"))
#endif
    return true;
  return false;
}

void UserMaps::validateNonConstValOPRD(llvm::StringRef oprd, unsigned lno) {
  if (!isConstValOPRD(oprd)) {
    llvm::errs() << "ERROR: Invalid operand in config file: line " << lno
                 << ".\n";
    assert(false && "");
  }
}

/*static*/ enum codeParts UserMaps::getCodePartType(llvm::StringRef oprd) {
#if (LLVM_VERSION_MAJOR >= 13)
  if (oprd.startswith_insensitive("@"))
#else
  if (oprd.startswith_lower("@"))
#endif
    return cpEXPR;
#if (LLVM_VERSION_MAJOR >= 13)
  else if (oprd.startswith_insensitive("C"))
#else
  else if (oprd.startswith_lower("C"))
#endif
    return cpCONSTNUM;
#if (LLVM_VERSION_MAJOR >= 13)
  else if (oprd.startswith_insensitive("V"))
#else
  else if (oprd.startswith_lower("V"))
#endif
    return cpVAR;
#if (LLVM_VERSION_MAJOR >= 13)
  else if (oprd.startswith_insensitive("A"))
#else
  else if (oprd.startswith_lower("A"))
#endif
    return cpADDRESS;
#if (LLVM_VERSION_MAJOR >= 13)
  else if (oprd.startswith_insensitive("P"))
#else
  else if (oprd.startswith_lower("P"))
#endif
    return cpPOINTER;
  else {
    llvm::errs() << "Error: Invalid codepart: '" << oprd.str() << "'.\n";
    assert(false && "");
  }
}

bool UserMaps::isWholeStmtMutationConfName(llvm::StringRef s) {
#if (LLVM_VERSION_MAJOR >= 13)
  return s.equals_insensitive("delstmt") || s.equals_insensitive("trapstmt");
#else
  return s.equals_lower("delstmt") || s.equals_lower("trapstmt");
#endif
}

/*static*/ bool UserMaps::containsDeleteStmtConfName(llvm::StringRef s) {
  return (s.lower().rfind("delstmt") != std::string::npos);
}

GenericMuOpBase *UserMaps::getMatcherObject(enum ExpElemKeys opKey) {
  auto val_iter = mapOperationMatcher.find(opKey);
  if (val_iter == mapOperationMatcher.end()) {
    llvm::errs() << "ERROR: Invalid matchop (" << opKey
                 << "); not present in the mutants config matches.\n";
    assert(false && "");
  }

  return (val_iter->second);
}

std::vector<enum ExpElemKeys> *
UserMaps::getExpElemKeys(llvm::StringRef operation, std::string &confexp,
                         unsigned confline) {
  auto val_iter = mapConfnameOperations.find(operation.upper());
  if (val_iter == mapConfnameOperations.end()) {
    llvm::errs() << "ERROR: Invalid operation in configuration: '"
                 << operation.upper() << "' at config Line " << confline
                 << " ('" << confexp << "')!!\n";
    assert(false && "");
  }

  return &(val_iter->second);
}

void UserMaps::clearAll() {
  mapConfnameOperations.clear();
  for (auto val_iter = mapOperationMatcher.begin(),
            momend = mapOperationMatcher.end();
       val_iter != momend; ++val_iter) {
    DESTROY_OBJ(val_iter->second);
  }
  mapOperationMatcher.clear();
}

UserMaps::~UserMaps() { clearAll(); }

UserMaps::UserMaps() {
  clearAll();

  //#Init mapConfnameOperations: addConfNameOpPair (<Name in conf>, {FPOrdered,
  // FPUnordered, IntSigned, IntUnsigned})
  //#Init mapOperationMatcher

  addConfNameOpPair("STMT", {mALLSTMT, mALLSTMT, mALLSTMT, mALLSTMT});
  addOpMatchObjectPair(mALLSTMT, CREATE_OBJ(AllStatements)); // Matcher
  // Match anything Have no match function (not needed, always matched)

  // XXX On frontend: equivalent to match var and match const
  addConfNameOpPair("@", {mALLFEXPR, mALLFEXPR, mALLIEXPR, mALLIEXPR});
  addOpMatchObjectPair(mALLFEXPR, CREATE_OBJ(FPNumericExpression));  // Matcher
  addOpMatchObjectPair(mALLIEXPR, CREATE_OBJ(IntNumericExpression)); // Matcher

  addConfNameOpPair("V", {mANYFVAR, mANYFVAR, /**/ mANYIVAR, mANYIVAR});
  addOpMatchObjectPair(mANYFVAR, CREATE_OBJ(FPNumericVariable));  // Matcher
  addOpMatchObjectPair(mANYIVAR, CREATE_OBJ(IntNumericVariable)); // Matcher

  addConfNameOpPair("C", {mANYFCONST, mANYFCONST, /**/ mANYICONST, mANYICONST});
  addOpMatchObjectPair(mANYFCONST, CREATE_OBJ(FPNumericConstant));  // Matcher
  addOpMatchObjectPair(mANYICONST, CREATE_OBJ(IntNumericConstant)); // Matcher

  addConfNameOpPair("A", {mANYADDRESS, mANYADDRESS, mANYADDRESS, mANYADDRESS});
  addOpMatchObjectPair(mANYADDRESS,
                       CREATE_OBJ(MemoryAddressValue)); // Matcher     //Pointer

  addConfNameOpPair("P", {mANYPOINTER, mANYPOINTER, mANYPOINTER, mANYPOINTER});
  addOpMatchObjectPair(mANYPOINTER,
                       CREATE_OBJ(MemoryPointerVariable)); // Matcher //Pointer

  // CONSTVAL Have no match function (cannot be a match -- something we want to
  // mutate)
  // Same as just putting a constant value. EX: "ADD(@1,@2) ---> xx,
  // CONSTVAL(0)" equivalent to "ADD(@1,@2) ---> xx, 0"
  addConfNameOpPair("CONSTVAL", {mCONST_VALUE_OF, mCONST_VALUE_OF,
                                 mCONST_VALUE_OF, mCONST_VALUE_OF});
  addOpMatchObjectPair(mCONST_VALUE_OF,
                       CREATE_OBJ(ConstantValueOf)); // Replacer

  // DELSTMT Have no match function (cannot be a match -- something we want to
  // mutate)
  // XXX (Do not add to map because) it should not be used at all (use the
  // doDeleteStmt method)
  addConfNameOpPair("DELSTMT", {mDELSTMT, mDELSTMT, mDELSTMT, mDELSTMT});
  // We add it herebut should not be used, this is just to raise error upon use
  // of its replacing method. use doDeleteStmt instead
  addOpMatchObjectPair(mDELSTMT, CREATE_OBJ(DeleteStatement));

  // Put a trap just before a stmt (equivalent toe replacing the statement by a
  // trap)
  addConfNameOpPair("TRAPSTMT", {mTRAPSTMT, mTRAPSTMT, mTRAPSTMT, mTRAPSTMT});
  addOpMatchObjectPair(mTRAPSTMT, CREATE_OBJ(TrapStatement));

  // OPERAND Have no match function (cannot be a match -- something we want to
  // mutate)
  addConfNameOpPair("OPERAND", {mKEEP_ONE_OPRD, mKEEP_ONE_OPRD, mKEEP_ONE_OPRD,
                                mKEEP_ONE_OPRD});
  addOpMatchObjectPair(mKEEP_ONE_OPRD, CREATE_OBJ(KeepOneOperand)); // Replacer

  addConfNameOpPair("ASSIGN", {mFASSIGN, mFASSIGN, mIASSIGN, mIASSIGN});
  addOpMatchObjectPair(mFASSIGN, CREATE_OBJ(FPAssign));
  addOpMatchObjectPair(mIASSIGN, CREATE_OBJ(IntegerAssign));

  addConfNameOpPair("ADD", {mFADD, mFADD, mADD, mADD});
  addOpMatchObjectPair(mFADD, CREATE_OBJ(FPAdd));
  addOpMatchObjectPair(mADD, CREATE_OBJ(IntegerAdd));

  addConfNameOpPair("SUB", {mFSUB, mFSUB, mSUB, mSUB});
  addOpMatchObjectPair(mFSUB, CREATE_OBJ(FPSub));
  addOpMatchObjectPair(mSUB, CREATE_OBJ(IntegerSub));

  addConfNameOpPair("PADD",
                    {mPADD, mPADD, mPADD, mPADD}); // Pointer (Address Increase)
  addOpMatchObjectPair(mPADD, CREATE_OBJ(PointerAdd));

  addConfNameOpPair("PSUB",
                    {mPSUB, mPSUB, mPSUB, mPSUB}); // Pointer (Address Decrease)
  addOpMatchObjectPair(mPSUB, CREATE_OBJ(PointerSub));

  addConfNameOpPair("MUL", {mFMUL, mFMUL, mMUL, mMUL});
  addOpMatchObjectPair(mFMUL, CREATE_OBJ(FPMul));
  addOpMatchObjectPair(mMUL, CREATE_OBJ(IntegerMul));

  addConfNameOpPair("DIV", {mFDIV, mFDIV, mSDIV, mUDIV});
  addOpMatchObjectPair(mFDIV, CREATE_OBJ(FPDiv));
  addOpMatchObjectPair(mSDIV, CREATE_OBJ(SignedIntegerDiv));
  addOpMatchObjectPair(mUDIV, CREATE_OBJ(UnsignedIntegerDiv));

  addConfNameOpPair("MOD", {mFMOD, mFMOD, mSMOD, mUMOD});
  addOpMatchObjectPair(mFMOD, CREATE_OBJ(FPMod));
  addOpMatchObjectPair(mSMOD, CREATE_OBJ(SignedIntegerMod));
  addOpMatchObjectPair(mUMOD, CREATE_OBJ(UnsignedIntegerMod));

  addConfNameOpPair("BITAND",
                    {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mBITAND, mBITAND});
  addOpMatchObjectPair(mBITAND, CREATE_OBJ(BitAnd));

  addConfNameOpPair("BITOR", {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mBITOR, mBITOR});
  addOpMatchObjectPair(mBITOR, CREATE_OBJ(BitOr));

  addConfNameOpPair("BITXOR",
                    {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mBITXOR, mBITXOR});
  addOpMatchObjectPair(mBITXOR, CREATE_OBJ(BitXor));

  addConfNameOpPair(
      "BITSHL", {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mBITSHIFTLEFT, mBITSHIFTLEFT});
  addOpMatchObjectPair(mBITSHIFTLEFT, CREATE_OBJ(BitShiftLeft));

  addConfNameOpPair("BITSHR", {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mABITSHIFTRIGHT,
                               mLBITSHIFTRIGHT});
  addOpMatchObjectPair(mABITSHIFTRIGHT, CREATE_OBJ(BitAShiftRight));
  addOpMatchObjectPair(mLBITSHIFTRIGHT, CREATE_OBJ(BitLShiftRight));

  addConfNameOpPair("BITNOT",
                    {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mBITNOT, mBITNOT});
  addOpMatchObjectPair(mBITNOT, CREATE_OBJ(BitNot));

  addConfNameOpPair("NEG", {mFNEG, mFNEG, mNEG, mNEG});
  addOpMatchObjectPair(mFNEG, CREATE_OBJ(FPMinus));
  addOpMatchObjectPair(mNEG, CREATE_OBJ(IntegerMinus));

  // ABS only replaced (not matched)
  addConfNameOpPair("ABS", {mFABS, mFABS, mABS, mABS});
  addOpMatchObjectPair(mFABS, CREATE_OBJ(FPAbs));     // Replacer
  addOpMatchObjectPair(mABS, CREATE_OBJ(IntegerAbs)); // Replacer

  addConfNameOpPair("LEFTINC", {mFLEFTINC, mFLEFTINC, mLEFTINC, mLEFTINC});
  addOpMatchObjectPair(mFLEFTINC, CREATE_OBJ(FPLeftInc));
  addOpMatchObjectPair(mLEFTINC, CREATE_OBJ(IntegerLeftInc));

  addConfNameOpPair("RIGHTINC", {mFRIGHTINC, mFRIGHTINC, mRIGHTINC, mRIGHTINC});
  addOpMatchObjectPair(mFRIGHTINC, CREATE_OBJ(FPRightInc));
  addOpMatchObjectPair(mRIGHTINC, CREATE_OBJ(IntegerRightInc));

  addConfNameOpPair("LEFTDEC", {mFLEFTDEC, mFLEFTDEC, mLEFTDEC, mLEFTDEC});
  addOpMatchObjectPair(mFLEFTDEC, CREATE_OBJ(FPLeftDec));
  addOpMatchObjectPair(mLEFTDEC, CREATE_OBJ(IntegerLeftDec));

  addConfNameOpPair("RIGHTDEC", {mFRIGHTDEC, mFRIGHTDEC, mRIGHTDEC, mRIGHTDEC});
  addOpMatchObjectPair(mFRIGHTDEC, CREATE_OBJ(FPRightDec));
  addOpMatchObjectPair(mRIGHTDEC, CREATE_OBJ(IntegerRightDec));

  // Pointer (Poniter Left Increment)
  addConfNameOpPair("PLEFTINC", {mPLEFTINC, mPLEFTINC, mPLEFTINC, mPLEFTINC});
  addOpMatchObjectPair(mPLEFTINC, CREATE_OBJ(PointerLeftInc));

  // Pointer (Poniter Right Increment)
  addConfNameOpPair("PRIGHTINC",
                    {mPRIGHTINC, mPRIGHTINC, mPRIGHTINC, mPRIGHTINC});
  addOpMatchObjectPair(mPRIGHTINC, CREATE_OBJ(PointerRightInc));

  // Pointer (Poniter Left Decrement)
  addConfNameOpPair("PLEFTDEC", {mPLEFTDEC, mPLEFTDEC, mPLEFTDEC, mPLEFTDEC});
  addOpMatchObjectPair(mPLEFTDEC, CREATE_OBJ(PointerLeftDec));

  // Pointer (Poniter Right Decrement)
  addConfNameOpPair("PRIGHTDEC",
                    {mPRIGHTDEC, mPRIGHTDEC, mPRIGHTDEC, mPRIGHTDEC});
  addOpMatchObjectPair(mPRIGHTDEC, CREATE_OBJ(PointerRightDec));

  addConfNameOpPair("EQ", {mFCMP_OEQ, mFCMP_UEQ, mICMP_EQ, mICMP_EQ});
  addOpMatchObjectPair(mFCMP_OEQ, CREATE_OBJ(OFPEqual));
  addOpMatchObjectPair(mFCMP_UEQ, CREATE_OBJ(UFPEqual));
  addOpMatchObjectPair(mICMP_EQ, CREATE_OBJ(IntegerEqual));

  addConfNameOpPair("NEQ", {mFCMP_ONE, mFCMP_UNE, mICMP_NE, mICMP_NE});
  addOpMatchObjectPair(mFCMP_ONE, CREATE_OBJ(OFPNotEqual));
  addOpMatchObjectPair(mFCMP_UNE, CREATE_OBJ(UFPNotEqual));
  addOpMatchObjectPair(mICMP_NE, CREATE_OBJ(IntegerNotEqual));

  addConfNameOpPair("GT", {mFCMP_OGT, mFCMP_UGT, mICMP_SGT, mICMP_UGT});
  addOpMatchObjectPair(mFCMP_OGT, CREATE_OBJ(OFPGreaterThan));
  addOpMatchObjectPair(mFCMP_UGT, CREATE_OBJ(UFPGreaterThan));
  addOpMatchObjectPair(mICMP_SGT, CREATE_OBJ(SIntegerGreaterThan));
  addOpMatchObjectPair(mICMP_UGT, CREATE_OBJ(UIntegerGreaterThan));

  addConfNameOpPair("GE", {mFCMP_OGE, mFCMP_UGE, mICMP_SGE, mICMP_UGE});
  addOpMatchObjectPair(mFCMP_OGE, CREATE_OBJ(OFPGreaterOrEqual));
  addOpMatchObjectPair(mFCMP_UGE, CREATE_OBJ(UFPGreaterOrEqual));
  addOpMatchObjectPair(mICMP_SGE, CREATE_OBJ(SIntegerGreaterOrEqual));
  addOpMatchObjectPair(mICMP_UGE, CREATE_OBJ(UIntegerGreaterOrEqual));

  addConfNameOpPair("LT", {mFCMP_OLT, mFCMP_ULT, mICMP_SLT, mICMP_ULT});
  addOpMatchObjectPair(mFCMP_OLT, CREATE_OBJ(OFPLessThan));
  addOpMatchObjectPair(mFCMP_ULT, CREATE_OBJ(UFPLessThan));
  addOpMatchObjectPair(mICMP_SLT, CREATE_OBJ(SIntegerLessThan));
  addOpMatchObjectPair(mICMP_ULT, CREATE_OBJ(UIntegerLessThan));

  addConfNameOpPair("LE", {mFCMP_OLE, mFCMP_ULE, mICMP_SLE, mICMP_ULE});
  addOpMatchObjectPair(mFCMP_OLE, CREATE_OBJ(OFPLessOrEqual));
  addOpMatchObjectPair(mFCMP_ULE, CREATE_OBJ(UFPLessOrEqual));
  addOpMatchObjectPair(mICMP_SLE, CREATE_OBJ(SIntegerLessOrEqual));
  addOpMatchObjectPair(mICMP_ULE, CREATE_OBJ(UIntegerLessOrEqual));

  // Pointer (Poniter Equal)
  addConfNameOpPair("PEQ", {mP_EQ, mP_EQ, mP_EQ, mP_EQ});
  // The difference with common EQ will be the operand types
  addOpMatchObjectPair(mP_EQ, CREATE_OBJ(PointerEqual));

  // Pointer (Poniter Not-Equal)
  addConfNameOpPair("PNEQ", {mP_NE, mP_NE, mP_NE, mP_NE});
  // The difference with common NEQ will be the operand types
  addOpMatchObjectPair(mP_NE, CREATE_OBJ(PointerNotEqual));

  // Pointer (Poniter Greater than)
  addConfNameOpPair("PGT", {mP_GT, mP_GT, mP_GT, mP_GT});
  // The difference with common GT will be the operand types
  addOpMatchObjectPair(mP_GT, CREATE_OBJ(PointerGreaterThan));

  // Pointer (Poniter Greater or Equal than)
  addConfNameOpPair("PGE", {mP_GE, mP_GE, mP_GE, mP_GE});
  // The difference with common GE will be the operand types
  addOpMatchObjectPair(mP_GE, CREATE_OBJ(PointerGreaterOrEqual));

  // Pointer (Poniter Less than)
  addConfNameOpPair("PLT", {mP_LT, mP_LT, mP_LT, mP_LT});
  // The difference with common LT will be the operand types
  addOpMatchObjectPair(mP_LT, CREATE_OBJ(PointerLessThan));

  // Pointer (Poniter Less or Equal than)
  addConfNameOpPair("PLE", {mP_LE, mP_LE, mP_LE, mP_LE});
  // The difference with common LE will be the operand types
  addOpMatchObjectPair(mP_LE, CREATE_OBJ(PointerLessOrEqual));

  addConfNameOpPair("AND", {mAND, mAND, mAND, mAND});
  addOpMatchObjectPair(mAND, CREATE_OBJ(LogicalAnd));

  addConfNameOpPair("OR", {mOR, mOR, mOR, mOR});
  addOpMatchObjectPair(mOR, CREATE_OBJ(LogicalOr));

  // Called function replacement: The two functions should have the same
  // prototype
  addConfNameOpPair("SWITCH", {mSWITCH, mSWITCH, mSWITCH, mSWITCH});
  addOpMatchObjectPair(mSWITCH, CREATE_OBJ(SwitchCases)); // Matcher

  // Replace only. This shuffle case destination (default as well), by swapping
  // destination Basic Blocks of 2 or more cases (number specified)
  // XXX (Do not add to map because) it should not be used at all (outside of
  // SwitchCases)
  addConfNameOpPair("SHUFFLECASESDESTS",
                    {mSHUFFLE_CASE_DESTS, mSHUFFLE_CASE_DESTS,
                     mSHUFFLE_CASE_DESTS, mSHUFFLE_CASE_DESTS});
  // We add it here but should not be used, this is just to raise error
  // upon use of its replacing methon. only Switch
  addOpMatchObjectPair(mSHUFFLE_CASE_DESTS,
                       CREATE_OBJ(ShuffleCaseDestinations));

  // Make one or more cases point to default basic block as destination
  // XXX (Do not add to map because) it should not be used at all (outside of
  // SwitchCases)
  addConfNameOpPair("REMOVECASES", {mREMOVE_CASES, mREMOVE_CASES, mREMOVE_CASES,
                                    mREMOVE_CASES});
  // We add it here but should not be used, this is just to
  // raise error upon use of its replacing methon. replacor only used by Switch
  addOpMatchObjectPair(mREMOVE_CASES, CREATE_OBJ(RemoveCases));

  // Called function replacement: The two functions should have the same
  // prototype
  addConfNameOpPair("CALL", {mCALL, mCALL, mCALL, mCALL});
  addOpMatchObjectPair(mCALL, CREATE_OBJ(FunctionCall)); // Matcher

  // define what function should be changed to what when CALL is matched:
  // 1st=matched callee; 2nd,3rd...=replacing callees
  // XXX (Do not add to map because) it should not be used at all (outside of
  // FunctionCall)
  addConfNameOpPair("NEWCALLEE",
                    {mNEWCALLEE, mNEWCALLEE, mNEWCALLEE, mNEWCALLEE});
  // We add it herebut should not be used, this is just to raise error
  // upon use of its replacing methon. this replacor is only used by Call
  addOpMatchObjectPair(mNEWCALLEE, CREATE_OBJ(NewCallee));

  // Shuffle function call arguments of the same type (swap them around)
  // XXX (Do not add to map because) it should not be used at all (outside of
  // FunctionCall)
  addConfNameOpPair("SHUFFLEARGS", {mSHUFFLE_ARGS, mSHUFFLE_ARGS, mSHUFFLE_ARGS,
                                    mSHUFFLE_ARGS});
  // We add it here but should not be used, this is just to raise error
  // upon use of its replacing methon. replacor only used by Call
  addOpMatchObjectPair(mSHUFFLE_ARGS, CREATE_OBJ(ShuffleArgs));

  // to delete the return, break and continue (actually replace the target BB of
  // unconditional br, or replace the return value of ret by 0). Should be
  // replcaced only by delstmt
  addConfNameOpPair("RETURN_BREAK_CONTINUE",
                    {mRETURN_BREAK_CONTINUE, mRETURN_BREAK_CONTINUE,
                     mRETURN_BREAK_CONTINUE, mRETURN_BREAK_CONTINUE});
  addOpMatchObjectPair(mRETURN_BREAK_CONTINUE,
                       CREATE_OBJ(ReturnBreakContinue)); // Matcher

  // DEREFS

  addConfNameOpPair("PADD_DEREF",
                    {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mPADD_DEREF_INT,
                     mPADD_DEREF_PTR}); // Pointer - Val
  addOpMatchObjectPair(mPADD_DEREF_INT, CREATE_OBJ(PAddDerefInt));
  addOpMatchObjectPair(mPADD_DEREF_PTR, CREATE_OBJ(PAddDerefPtr));

  addConfNameOpPair("PSUB_DEREF",
                    {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mPSUB_DEREF_INT,
                     mPSUB_DEREF_PTR}); // Pointer - Val
  addOpMatchObjectPair(mPSUB_DEREF_INT, CREATE_OBJ(PSubDerefInt));
  addOpMatchObjectPair(mPSUB_DEREF_PTR, CREATE_OBJ(PSubDerefPtr));

  addConfNameOpPair("PDEREF_ADD", {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mPDEREF_ADD,
                                   mPDEREF_PADD}); // Pointer - Val
  addOpMatchObjectPair(mPDEREF_ADD, CREATE_OBJ(DerefAdd));
  addOpMatchObjectPair(mPDEREF_PADD, CREATE_OBJ(DerefPAdd));

  addConfNameOpPair("PDEREF_SUB", {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mPDEREF_SUB,
                                   mPDEREF_PSUB}); // Pointer - Val
  addOpMatchObjectPair(mPDEREF_SUB, CREATE_OBJ(DerefSub));
  addOpMatchObjectPair(mPDEREF_PSUB, CREATE_OBJ(DerefPSub));

  addConfNameOpPair("PLEFTINC_DEREF",
                    {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mPLEFTINC_DEREF_INT,
                     mPLEFTINC_DEREF_PTR}); // Pointer - Val
  addOpMatchObjectPair(mPLEFTINC_DEREF_INT, CREATE_OBJ(PLeftIncDerefInt));
  addOpMatchObjectPair(mPLEFTINC_DEREF_PTR, CREATE_OBJ(PLeftIncDerefPtr));

  addConfNameOpPair("PRIGHTINC_DEREF",
                    {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mPRIGHTINC_DEREF_INT,
                     mPRIGHTINC_DEREF_PTR}); // Pointer - Val
  addOpMatchObjectPair(mPRIGHTINC_DEREF_INT, CREATE_OBJ(PRightIncDerefInt));
  addOpMatchObjectPair(mPRIGHTINC_DEREF_PTR, CREATE_OBJ(PRightIncDerefPtr));

  addConfNameOpPair("PLEFTDEC_DEREF",
                    {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mPLEFTDEC_DEREF_INT,
                     mPLEFTDEC_DEREF_PTR}); // Pointer - Val
  addOpMatchObjectPair(mPLEFTDEC_DEREF_INT, CREATE_OBJ(PLeftDecDerefInt));
  addOpMatchObjectPair(mPLEFTDEC_DEREF_PTR, CREATE_OBJ(PLeftDecDerefPtr));

  addConfNameOpPair("PRIGHTDEC_DEREF",
                    {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mPRIGHTDEC_DEREF_INT,
                     mPRIGHTDEC_DEREF_PTR}); // Pointer - Val
  addOpMatchObjectPair(mPRIGHTDEC_DEREF_INT, CREATE_OBJ(PRightDecDerefInt));
  addOpMatchObjectPair(mPRIGHTDEC_DEREF_PTR, CREATE_OBJ(PRightDecDerefPtr));

  addConfNameOpPair("PDEREF_LEFTINC",
                    {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mPDEREF_LEFTINC,
                     mPDEREF_PLEFTINC}); // Pointer - Val
  addOpMatchObjectPair(mPDEREF_LEFTINC, CREATE_OBJ(DerefLeftInc));
  addOpMatchObjectPair(mPDEREF_PLEFTINC, CREATE_OBJ(DerefPLeftInc));

  addConfNameOpPair("PDEREF_RIGHTINC",
                    {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mPDEREF_RIGHTINC,
                     mPDEREF_PRIGHTINC}); // Pointer - Val
  addOpMatchObjectPair(mPDEREF_RIGHTINC, CREATE_OBJ(DerefRightInc));
  addOpMatchObjectPair(mPDEREF_PRIGHTINC, CREATE_OBJ(DerefPRightInc));

  addConfNameOpPair("PDEREF_LEFTDEC",
                    {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mPDEREF_LEFTDEC,
                     mPDEREF_PLEFTDEC}); // Pointer - Val
  addOpMatchObjectPair(mPDEREF_LEFTDEC, CREATE_OBJ(DerefLeftDec));
  addOpMatchObjectPair(mPDEREF_PLEFTDEC, CREATE_OBJ(DerefPLeftDec));

  addConfNameOpPair("PDEREF_RIGHTDEC",
                    {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mPDEREF_RIGHTDEC,
                     mPDEREF_PRIGHTDEC}); // Pointer - Val
  addOpMatchObjectPair(mPDEREF_RIGHTDEC, CREATE_OBJ(DerefRightDec));
  addOpMatchObjectPair(mPDEREF_PRIGHTDEC, CREATE_OBJ(DerefPRightDec));

  /**** ADD HERE ****/
}
