
#ifndef __KLEE_SEMU_GENMU_typesops__
#define __KLEE_SEMU_GENMU_typesops__

#include <sstream>

#include "llvm/IR/Value.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/DenseMap.h"

#include "llvm/Support/raw_ostream.h"

char * G_MAIN_FUNCTION_NAME = "main";

enum codeParts {cpEXPR=0, cpCONSTNUM, cpVAR, cpADDRESS, cpPOINTER};

//enum typeOP {Arithetical, Relational, Logical, Bitwise, Assignement, Misc, Call, DelStmt};
//enum modeOP {Unary, Binary, None};	//None for DelStmt
enum ExpElemKeys {mALLSTMT=0, mALLFEXPR, mALLIEXPR, mANYFVAR, mANYIVAR, mANYFCONST, mANYICONST, mDELSTMT, mKEEP_ONE_OPRD, mCONST_VALUE_OF, //special, Delete stmt
            mIASSIGN, mFASSIGN, mADD, mFADD, mPADD, mSUB, mFSUB, mPSUB, mMUL, mFMUL, mSDIV, mUDIV, mFDIV, mSMOD, mUMOD, mFMOD,  //Arithmetic Binary
            mNEG, mFNEG, mLEFTINC, mFLEFTINC, mRIGHTINC, mFRIGHTINC, mLEFTDEC, mFLEFTDEC, mRIGHTDEC, mFRIGHTDEC, mABS, mFABS,    //Arithmetic Unary
            mBITAND, mBITOR, mBITXOR, mBITSHIFTLEFT, mABITSHIFTRIGHT, mLBITSHIFTRIGHT,    //Bitwise Binary
            mBITNOT,                                             //Bitwise Unary
            mFCMP_FALSE, mFCMP_OEQ, mFCMP_OGT, mFCMP_OGE, mFCMP_OLT, mFCMP_OLE, mFCMP_ONE, mFCMP_ORD, mFCMP_UNO, mFCMP_UEQ, mFCMP_UGT, mFCMP_UGE, mFCMP_ULT, mFCMP_ULE, mFCMP_UNE, mFCMP_TRUE, //Relational FP
            mICMP_EQ, mICMP_NE, mICMP_UGT, mICMP_UGE, mICMP_ULT, mICMP_ULE, mICMP_SGT, mICMP_SGE, mICMP_SLT, mICMP_SLE,   //Relational Int or ptr
            mAND, mOR,                              //Logical Binary
            mNOT,                                    //Logical Unary
            
            mCALL, mNEWCALLEE,              // called function
            mRETURN_BREAK_CONTINUE,         // delete them by replacing unconditional 'br' target. for the final return with argument, set it to 0
            /**** ADD HERE ****/
            
            
            /******************/
            mFORBIDEN_TYPE,   //Says that we cannot use an op for the type (float or int)
            enumExpElemKeysSIZE     //Number of elements in this enum
          };

class llvmMutationOp;

void matchANYFEXPR (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchANYIEXPR (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchANYFVAR (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchANYIVAR (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchANYFCONST (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchANYICONST (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchASSIGN (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchADD (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchFADD (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchPADD (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchSUB (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchFSUB (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchPSUB (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchMUL (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchFMUL (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchSDIV (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchUDIV (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchFDIV (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchSMOD (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchUMOD (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchFMOD (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchBITAND (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchBITOR (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchBITXOR (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchBITSHIFTLEFT (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchABITSHIFTRIGHT (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchLBITSHIFTRIGHT (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchALLNEGS (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchINC_DEC (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);
void matchRELATIONALS (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);

void matchAND_OR (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);

void matchCALL (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);

void matchRETURN_BREAK_CONTINUE (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts);

/**** ADD HERE ****/

class UserMaps
{
public:   
    using MatcherFuncType = void (*)(std::vector<llvm::Value *>&, llvmMutationOp &, std::vector<std::vector<llvm::Value *>>&);
private:
    llvm::DenseMap<unsigned/*enum ExpElemKeys*/, MatcherFuncType> mapOperationMatcher;
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    std::map<std::string, std::vector<enum ExpElemKeys>> mapConfnameOperations;
#else
    llvm::StringMap<std::vector<enum ExpElemKeys>> mapConfnameOperations;
#endif
    
    void addOpMatchFuncPair (enum ExpElemKeys op, MatcherFuncType func)
    {
        const std::pair<enum ExpElemKeys, MatcherFuncType> val(op, func);
        if (! mapOperationMatcher.insert(val).second)
        {   
            llvm::errs() << "ERROR: Mutation operation matcher appears multiple times (op = " << op << ").\n";
            assert (false && "");
        }
    }
    
    void addConfNameOpPair (llvm::StringRef cname, std::vector<enum ExpElemKeys> ops)
    {
        const std::pair<std::string, std::vector<enum ExpElemKeys>> val(cname.upper(), ops);
        if (! mapConfnameOperations.insert(val).second)
        {   
            llvm::errs() << "ERROR: config operation name appears multiple times (" << cname.upper() << ").\n";
            assert (false && "");
        }
        
        /*auto val_iter = mapConfnameOperations.find(cname.upper());
        if (val_iter == mapConfnameOperations.end())
        {
            llvm::errs() << "ERROR: XXXXXXXXXXXXXXXXXXXXX: '" <<"')!!\n";
            assert (false && "");
        }*/
    }
public:
    inline bool isConstValOPRD(llvm::StringRef oprd)
    {
        if (oprd.startswith_lower("@") || oprd.startswith_lower("C") || oprd.startswith_lower("V") || oprd.startswith_lower("A") || oprd.startswith_lower("P"))
            return true;
        return false;
    }
    
    void validateNonConstValOPRD(llvm::StringRef oprd, unsigned lno)
    {
        if (! isConstValOPRD(oprd))
        {   
            llvm::errs() << "ERROR: Invalid operand in config file: line " << lno << ".\n";
            assert (false && "");
        }
    }
    
    static enum codeParts getCodePartType (llvm::StringRef oprd)
    {
        if (oprd.startswith_lower("@"))
            return cpEXPR;
        else if (oprd.startswith_lower("C"))
            return cpCONSTNUM;
        else if (oprd.startswith_lower("V"))
            return cpVAR;
        else if (oprd.startswith_lower("A"))
            return cpADDRESS;
        else if (oprd.startswith_lower("P"))
            return cpPOINTER;
        else
        {
            llvm::errs() << "Error: Invalid codepart: '" << oprd.str() << "'.\n";
            assert (false && "");
        }
    }
    
    bool isDeleteStmtConfName(llvm::StringRef s) 
    {
        return s.equals_lower("delstmt");
    }
    
    MatcherFuncType getMatcherFunc(enum ExpElemKeys opKey)
    {
        auto val_iter = mapOperationMatcher.find(opKey);
        if (val_iter == mapOperationMatcher.end())
        {
            llvm::errs() << "ERROR: Invalid matchop (" << opKey << "); not present in the mutants config matches.\n";
            assert (false && "");
        }
        
        return (val_iter->second);
    }
    std::vector<enum ExpElemKeys> * getExpElemKeys(llvm::StringRef operation, std::string &confexp, unsigned confline)
    {
        auto val_iter = mapConfnameOperations.find(operation.upper());
        if (val_iter == mapConfnameOperations.end())
        {
            llvm::errs() << "ERROR: Invalid operation in configuration: '" << operation.upper() << "' at config Line " <<confline<<" ('" <<confexp<<"')!!\n";
            assert (false && "");
        }
        
        return &(val_iter->second);
    }
    
    UserMaps()
    {
        mapOperationMatcher.clear();
        mapConfnameOperations.clear();
        
        //#Init mapConfnameOperations: addConfNameOpPair (<Name in conf>, {FPOrdered, FPUnordered, IntSigned, IntUnsigned})
        //#Init mapOperationMatcher
        
        addConfNameOpPair ("STMT", {mALLSTMT, mALLSTMT, mALLSTMT, mALLSTMT});
        //Match anything Have no match function (not needed, always matched)
        
        addConfNameOpPair ("@", {mALLFEXPR, mALLFEXPR, mALLIEXPR, mALLIEXPR});
        addOpMatchFuncPair (mALLFEXPR, matchANYFEXPR);
        addOpMatchFuncPair (mALLIEXPR, matchANYIEXPR);    //TODO: do this one as the const one
        
        addConfNameOpPair ("V", {mANYFVAR, mANYFVAR,/**/ mANYIVAR, mANYIVAR});
        addOpMatchFuncPair (mANYFVAR, matchANYFVAR);
        addOpMatchFuncPair (mANYIVAR, matchANYIVAR);
        
        addConfNameOpPair ("C", {mANYFCONST, mANYFCONST,/**/ mANYICONST, mANYICONST});
        addOpMatchFuncPair (mANYFCONST, matchANYFCONST);
        addOpMatchFuncPair (mANYICONST, matchANYICONST);
        
        addConfNameOpPair ("CONSTVAL", {mCONST_VALUE_OF, mCONST_VALUE_OF, mCONST_VALUE_OF, mCONST_VALUE_OF});
        //CONSTVAL Have no match function (cannot be a match -- something we want to mutate)
        
        addConfNameOpPair ("DELSTMT", {mDELSTMT, mDELSTMT, mDELSTMT, mDELSTMT});
        //DELSTMT Have no match function (cannot be a match -- something we want to mutate)
        
        addConfNameOpPair ("OPERAND", {mKEEP_ONE_OPRD, mKEEP_ONE_OPRD, mKEEP_ONE_OPRD, mKEEP_ONE_OPRD});
        //OPERAND Have no match function (cannot be a match -- something we want to mutate)
        
        addConfNameOpPair ("ASSIGN", {mFASSIGN, mFASSIGN, mIASSIGN, mIASSIGN});
        addOpMatchFuncPair (mFASSIGN, matchASSIGN);
        addOpMatchFuncPair (mIASSIGN, matchASSIGN);
        
        addConfNameOpPair ("ADD", {mFADD, mFADD,/**/ mADD, mADD});
        addOpMatchFuncPair (mFADD, matchFADD);
        addOpMatchFuncPair (mADD, matchADD);
        
        addConfNameOpPair ("PADD", {mPADD});        //Pointer
        addOpMatchFuncPair (mPADD, matchPADD);
        
        addConfNameOpPair ("SUB", {mFSUB, mFSUB, mSUB, mSUB});
        addOpMatchFuncPair (mFSUB, matchFSUB);
        addOpMatchFuncPair (mSUB, matchSUB);
        
        addConfNameOpPair ("PSUB", {mPSUB});        //Pointer
        addOpMatchFuncPair (mPSUB, matchPSUB);
        
        addConfNameOpPair ("MUL", {mFMUL, mFMUL, mMUL, mMUL});
        addOpMatchFuncPair (mFMUL, matchFMUL);
        addOpMatchFuncPair (mMUL, matchMUL);
        
        addConfNameOpPair ("DIV", {mFDIV, mFDIV, mSDIV, mUDIV});
        addOpMatchFuncPair (mFDIV, matchFDIV);
        addOpMatchFuncPair (mSDIV, matchSDIV);
        addOpMatchFuncPair (mUDIV, matchUDIV);
        
        addConfNameOpPair ("MOD", {mFMOD, mFMOD, mSMOD, mUMOD});
        addOpMatchFuncPair (mFMOD, matchFMOD);
        addOpMatchFuncPair (mSMOD, matchSMOD);
        addOpMatchFuncPair (mUMOD, matchUMOD);
        
        addConfNameOpPair ("BITAND", {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mBITAND, mBITAND});
        addOpMatchFuncPair (mBITAND, matchBITAND);
        
        addConfNameOpPair ("BITOR", {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mBITOR, mBITOR});
        addOpMatchFuncPair (mBITOR, matchBITOR);
        
        addConfNameOpPair ("BITXOR", {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mBITXOR, mBITXOR});
        addOpMatchFuncPair (mBITXOR, matchBITXOR);
        
        addConfNameOpPair ("BITSHL", {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mBITSHIFTLEFT, mBITSHIFTLEFT});
        addOpMatchFuncPair (mBITSHIFTLEFT, matchBITSHIFTLEFT);
        
        addConfNameOpPair ("BITSHR", {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mABITSHIFTRIGHT, mLBITSHIFTRIGHT});
        addOpMatchFuncPair (mABITSHIFTRIGHT, matchABITSHIFTRIGHT);
        addOpMatchFuncPair (mLBITSHIFTRIGHT, matchLBITSHIFTRIGHT);
        
        addConfNameOpPair ("BITNOT", {mFORBIDEN_TYPE, mFORBIDEN_TYPE, mBITNOT, mBITNOT});
        addOpMatchFuncPair (mBITNOT, matchALLNEGS);
        
        addConfNameOpPair ("NEG", {mFNEG, mFNEG, mNEG, mNEG});
        addOpMatchFuncPair (mFNEG, matchALLNEGS);
        addOpMatchFuncPair (mNEG, matchALLNEGS);
        
        addConfNameOpPair ("ABS", {mFABS, mFABS, mABS, mABS});  //ABS only replaced (not matched)
        
        addConfNameOpPair ("LEFTINC", {mFLEFTINC, mFLEFTINC, mLEFTINC, mLEFTINC});
        addOpMatchFuncPair (mFLEFTINC, matchINC_DEC);
        addOpMatchFuncPair (mLEFTINC, matchINC_DEC);
        
        addConfNameOpPair ("RIGHTINC", {mFRIGHTINC, mFRIGHTINC, mRIGHTINC, mRIGHTINC});
        addOpMatchFuncPair (mFRIGHTINC, matchINC_DEC);
        addOpMatchFuncPair (mRIGHTINC, matchINC_DEC);
        
        addConfNameOpPair ("LEFTDEC", {mFLEFTDEC, mFLEFTDEC, mLEFTDEC, mLEFTDEC});
        addOpMatchFuncPair (mFLEFTDEC, matchINC_DEC);
        addOpMatchFuncPair (mLEFTDEC, matchINC_DEC);
        
        addConfNameOpPair ("RIGHTDEC", {mFRIGHTDEC, mFRIGHTDEC, mRIGHTDEC, mRIGHTDEC});
        addOpMatchFuncPair (mFRIGHTDEC, matchINC_DEC);
        addOpMatchFuncPair (mRIGHTDEC, matchINC_DEC);
        
        addConfNameOpPair ("EQ", {mFCMP_OEQ, mFCMP_UEQ, mICMP_EQ, mICMP_EQ});
        addOpMatchFuncPair (mFCMP_OEQ, matchRELATIONALS);
        addOpMatchFuncPair (mFCMP_UEQ, matchRELATIONALS);
        addOpMatchFuncPair (mICMP_EQ, matchRELATIONALS);
        
        addConfNameOpPair ("NEQ", {mFCMP_ONE, mFCMP_UNE, mICMP_NE, mICMP_NE});
        addOpMatchFuncPair (mFCMP_ONE, matchRELATIONALS);
        addOpMatchFuncPair (mFCMP_UNE, matchRELATIONALS);
        addOpMatchFuncPair (mICMP_NE, matchRELATIONALS);
        
        addConfNameOpPair ("GT", {mFCMP_OGT, mFCMP_UGT, mICMP_SGT, mICMP_UGT});
        addOpMatchFuncPair (mFCMP_OGT, matchRELATIONALS);
        addOpMatchFuncPair (mFCMP_UGT, matchRELATIONALS);
        addOpMatchFuncPair (mICMP_SGT, matchRELATIONALS);
        addOpMatchFuncPair (mICMP_UGT, matchRELATIONALS);
        
        addConfNameOpPair ("GE", {mFCMP_OGE, mFCMP_UGE, mICMP_SGE, mICMP_UGE});
        addOpMatchFuncPair (mFCMP_OGE, matchRELATIONALS);
        addOpMatchFuncPair (mFCMP_UGE, matchRELATIONALS);
        addOpMatchFuncPair (mICMP_SGE, matchRELATIONALS);
        addOpMatchFuncPair (mICMP_UGE, matchRELATIONALS);
        
        addConfNameOpPair ("LT", {mFCMP_OLT, mFCMP_ULT, mICMP_SLT, mICMP_ULT});
        addOpMatchFuncPair (mFCMP_OLT, matchRELATIONALS);
        addOpMatchFuncPair (mFCMP_ULT, matchRELATIONALS);
        addOpMatchFuncPair (mICMP_SLT, matchRELATIONALS);
        addOpMatchFuncPair (mICMP_ULT, matchRELATIONALS);
        
        addConfNameOpPair ("LE", {mFCMP_OLE, mFCMP_ULE, mICMP_SLE, mICMP_ULE});
        addOpMatchFuncPair (mFCMP_OLE, matchRELATIONALS);
        addOpMatchFuncPair (mFCMP_ULE, matchRELATIONALS);
        addOpMatchFuncPair (mICMP_SLE, matchRELATIONALS);
        addOpMatchFuncPair (mICMP_ULE, matchRELATIONALS);
        
        addConfNameOpPair ("AND", {mAND, mAND, mAND, mAND});
        addOpMatchFuncPair (mAND, matchAND_OR);
        
        addConfNameOpPair ("OR", {mOR, mOR, mOR, mOR});
        addOpMatchFuncPair (mOR, matchAND_OR);
        
        // Called function replacement: The two functions should have the same prototype
        addConfNameOpPair ("CALL", {mCALL, mCALL, mCALL, mCALL});
        addOpMatchFuncPair (mCALL, matchCALL);
        
        // define what function should be changed to what when CALL is matched: 1st=matched callee; 2nd,3rd...=replacing callees
        addConfNameOpPair ("NEWCALLEE", {mNEWCALLEE, mNEWCALLEE, mNEWCALLEE, mNEWCALLEE});
        
        // to delete the return, break and continue (actually replace the target od unconditional br, or replace the return value of ret by 0). Should be replcaed only by delstmt
        addConfNameOpPair ("RETURN_BREAK_CONTINUE", {mRETURN_BREAK_CONTINUE,  mRETURN_BREAK_CONTINUE,  mRETURN_BREAK_CONTINUE,  mRETURN_BREAK_CONTINUE});
        
        /**** ADD HERE ****/
        
    }
};


// Each statement is a string where elements are separated by spaces
class llvmMutationOp
{

public:
    enum ExpElemKeys matchOp;
    std::vector<std::pair<enum ExpElemKeys, std::vector<unsigned>>> mutantReplacorsList;
    
    std::vector<std::string> mutOpNameList; 
    
    std::vector<enum codeParts> oprdCPType;

    llvmMutationOp(){};
    ~llvmMutationOp (){};
    void setMatchOp (enum ExpElemKeys m, std::vector<std::string> &oprdsStr, unsigned fromPos=0)
    {
        matchOp = m;
        for (std::vector<std::string>::iterator it=oprdsStr.begin()+fromPos, ie=oprdsStr.end(); it != ie; ++it)
            oprdCPType.push_back(UserMaps::getCodePartType(*it));
    }
    void addReplacor (const enum ExpElemKeys repop, std::vector<unsigned> &oprdpos, std::string name)
    {
        mutantReplacorsList.push_back(std::pair<enum ExpElemKeys, std::vector<unsigned>>(repop, oprdpos));
        mutOpNameList.push_back(name);
    }
    inline unsigned getNumReplacor()
    {
        return mutantReplacorsList.size();
    }
    enum codeParts getCPType (unsigned pos)
    {
        return oprdCPType.at(pos);
    }
    
    //These static fields are used to manage constant values
    static std::map<unsigned, std::string> posConstValueMap;
    static const unsigned maxOprdNum = 1000;
    static unsigned insertConstValue (std::string val, bool numeric=true)
    {
        //check that the string represent a Number (Int(radix 10 or 16) or FP(normal or exponential))
        if (numeric && ! isNumeric(llvm::StringRef(val).lower().c_str(), 10))   // 10 only for now
        {
            //return a value <= maxOprdNum
            return 0;
        }
        
        static unsigned pos = maxOprdNum;
        pos ++;
        
        for (std::map<unsigned, std::string>::iterator it = posConstValueMap.begin(), 
                ie = posConstValueMap.end(); it != ie; ++it)
        {
            if (! val.compare(it->second))
                return it->first;
        }
        if (! posConstValueMap.insert(std::pair<unsigned, std::string>(pos, val)).second)
            assert (false && "Error: inserting an already existing key in map posConstValueMap");
        return pos;
    }
    static std::string &getConstValueStr (unsigned pos) 
    {
        //Fails if pos is not in map
        return posConstValueMap.at(pos);
    }
    static void destroyPosConstValueMap()
    {
        posConstValueMap.clear();
    }
    
    static bool isNumeric( const char* pszInput, int nNumberBase )
    {
	    std::istringstream iss( pszInput );

	    if ( nNumberBase == 10 )
	    {
		    double dTestSink;
		    iss >> dTestSink;
	    }
	    else if ( nNumberBase == 8 || nNumberBase == 16 )
	    {
		    int nTestSink;
		    iss >> ( ( nNumberBase == 8 ) ? std::oct : std::hex ) >> nTestSink;
	    }
	    else
	        return false;

	    // was any input successfully consumed/converted?
	    if ( ! iss )
	        return false;

	    // was all the input successfully consumed/converted?
	    return ( iss.rdbuf()->in_avail() == 0 );
    }
    
    std::string toString()
    {
        std::string ret(std::to_string(matchOp) + " --> ");
        for (auto &v: mutantReplacorsList)
        {
            ret+=std::to_string(v.first)+"(";
            for (unsigned pos: v.second)
            {
                ret+="@"+std::to_string(pos)+",";
            }
            if (ret.back() == ',')
                ret.pop_back();
            ret+=");";
        }
        ret+="\n";
        return ret; 
    }
};

std::map<unsigned, std::string> llvmMutationOp::posConstValueMap;

#endif  //__KLEE_SEMU_GENMU_typesops__

