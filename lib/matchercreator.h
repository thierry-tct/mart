
#include <vector>
#include <map>

#include "llvm/IR/Value.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/DataLayout.h"

#include "typesops.h"

int depPosofPos (std::vector<llvm::Value *> &toMatch, llvm::Value *irinst, int pos);
void doReplacement (std::vector<llvm::Value *> &toMatch, std::vector<std::vector<llvm::Value *>> &resultMuts, std::pair<enum ExpElemKeys, std::vector<unsigned>> &repl, 
                    std::vector<llvm::Value *> &toMatchClone, std::vector<unsigned> &posOfIRtoRemove, llvm::Value *lhOprdptr, llvm::Value *rhOprdptr, llvm::Value *returningIR);
std::map<enum ExpElemKeys, llvm::CmpInst::Predicate> & getPredRelMap();

/* Assumes that the IRs instructions in stmtIR have same order as the initial original IR code*/
void cloneStmtIR (std::vector<llvm::Value *> &stmtIR, std::vector<llvm::Value *> &resultIR)
{
    llvm::SmallDenseMap<llvm::Value *, llvm::Value *> pointerMap;
    for (llvm::Value * I: stmtIR)
    { 
        //clone instruction
        llvm::Value * newI = llvm::dyn_cast<llvm::Instruction>(I)->clone();
        
        //set name
        //if (I->hasName())
        //    newI->setName((I->getName()).str()+"_Mut0");
        
        if (! pointerMap.insert(std::pair<llvm::Value *, llvm::Value *>(I, newI)).second)
        {
            assert(false && "Error (Mutation::getOriginalStmtBB): inserting an element already in the map\n");
        }
    };
    for (llvm::Value * I: stmtIR)
    {
        for(unsigned opos = 0; opos < llvm::dyn_cast<llvm::User>(I)->getNumOperands(); opos++)
        {
            auto oprd = llvm::dyn_cast<llvm::User>(I)->getOperand(opos);
            if (llvm::isa<llvm::Instruction>(oprd))
            {
                if (auto newoprd = pointerMap.lookup(oprd))    //TODO:Double check the use of lookup for this map
                {
                    llvm::dyn_cast<llvm::User>(pointerMap.lookup(I))->setOperand(opos, newoprd);  //TODO:Double check the use of lookup for this map
                }
                else
                {
                    bool fail = false;
                    switch (opos)
                    {
                        case 0:
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
                            }
                        default:
                            fail = true;
                    }
                    
                    if (fail)
                    {
                        llvm::errs() << "Error (Mutation::getOriginalStmtBB): lookup an element not in the map -- "; 
                        llvm::dyn_cast<llvm::Instruction>(I)->dump();
                        assert(false && "");
                    }
                }
            }
        }
        resultIR.push_back(llvm::dyn_cast<llvm::Instruction>(pointerMap.lookup(I)));   //TODO:Double check the use of lookup for this map
    }
}

// Use this when matching IR with case to mutate, to match each oprd in IR with expected CPType
bool checkCPTypeInIR (enum codeParts cpt, llvm::Value *val)
{
    switch (cpt)
    {
        case cpEXPR:
        {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            if (!(llvm::PointerType::isValidElementType(val->getType()) && !val->getType()->isFunctionTy()))
#else
            if (!llvm::PointerType::isLoadableorStorableType(val->getType()))
#endif
                return false;
            if (! (llvm::isa<llvm::CompositeType>(val->getType()) || llvm::isa<llvm::FunctionType>(val->getType())))
                return true;
            return false;
        }
        case cpCONSTNUM:
        {
            if (llvm::isa<llvm::Constant>(val))
                if (llvm::isa<llvm::ConstantInt>(val) || llvm::isa<llvm::ConstantFP>(val)) // || llvm::isa<llvm::ConstantExpr>(val))
                    return true;
            return false;
        }
        case cpVAR:
        {
            while (llvm::isa<llvm::CastInst>(val))
            {
                //assert (llvm::dyn_cast<llvm::User>(val)->getNumOperands() == 1);
                if (llvm::isa<llvm::PtrToIntInst>(val))
                    break;
                val = llvm::dyn_cast<llvm::User>(val)->getOperand(0);
            }
            if (llvm::isa<llvm::LoadInst>(val)) //|| llvm::isa<llvm::PtrToIntInst>(val)) // || llvm::isa<llvm::ExtractValueInst>(val) || llvm::isa<llvm::ExtractElementInst>(val))
            {
                llvm::Type *valelty = llvm::dyn_cast<llvm::User>(val)->getOperand(0)->getType()->getPointerElementType();
                if (! (llvm::isa<llvm::CompositeType>(valelty) || llvm::isa<llvm::FunctionType>(valelty)))      //XXX: Change this to add support for array and vector operation (besid float and int)
                    return true;
            }
            return false;
        }
        case cpADDRESS:
        {
            if (llvm::isa<llvm::GetElementPtrInst>(val) || llvm::isa<llvm::AddrSpaceCastInst>(val) || llvm::isa<llvm::IntToPtrInst>(val))
                return true;
            return false;
        }
        case cpPOINTER:
        {
            while (llvm::isa<llvm::CastInst>(val))
            {
                //assert (llvm::dyn_cast<llvm::User>(val)->getNumOperands() == 1);
                val = llvm::dyn_cast<llvm::User>(val)->getOperand(0);
            }
            if (llvm::isa<llvm::LoadInst>(val) && llvm::isa<llvm::PointerType>(val->getType()))
                return true;
            return false;
        }
        default:
        {
            llvm::errs() << "Unreachable, Invalid CPType: "<< cpt <<".\n";
            assert (false && "");
        }
    }
}

llvm::Value * reverseCast (llvm::Instruction::CastOps toRev, llvm::Value *subj, llvm::Type *destTy)
{
    llvm::IRBuilder<> builder(llvm::getGlobalContext());
    switch (toRev)
    {
        case llvm::Instruction::Trunc: 
            return builder.CreateZExt (subj, destTy);
        case llvm::Instruction::ZExt:
        case llvm::Instruction::SExt:
            return builder.CreateTrunc (subj, destTy);
        case llvm::Instruction::FPTrunc:
            return builder.CreateFPExt (subj, destTy);
        case llvm::Instruction::FPExt:
            return builder.CreateFPTrunc (subj, destTy);
        case llvm::Instruction::UIToFP:
            return builder.CreateFPToUI (subj, destTy);
        case llvm::Instruction::SIToFP:
            return builder.CreateFPToSI (subj, destTy);
        case llvm::Instruction::FPToUI:
            return builder.CreateUIToFP (subj, destTy);
        case llvm::Instruction::FPToSI:
            return builder.CreateSIToFP (subj, destTy);
        case llvm::Instruction::BitCast:
            return builder.CreateBitCast (subj, destTy);
        //case llvm::Instruction::AddrSpaceCast:
        //case llvm::Instruction::PtrToInt:
        //case llvm::Instruction::IntToPtr:
        default: assert(false && "Unreachable (reverseCast) !!!!");
        
    }
}

llvm::Value * allCreators(enum ExpElemKeys replfirst, llvm::Value * oprdptr1, llvm::Value * oprdptr2, std::vector<llvm::Value *> &replacement)
{
    llvm::IRBuilder<> builder(llvm::getGlobalContext());
    switch (replfirst)
    {
        case mIASSIGN:
        case mFASSIGN:
        {
            llvm::Value *valRet = oprdptr2;
            while (llvm::isa<llvm::CastInst>(oprdptr1))
            {
                //assert (llvm::dyn_cast<llvm::User>(val)->getNumOperands() == 1);
                if (llvm::isa<llvm::PtrToIntInst>(oprdptr1))
                    break;
                oprdptr2 = reverseCast(llvm::dyn_cast<llvm::CastInst>(oprdptr1)->getOpcode(), oprdptr2, llvm::dyn_cast<llvm::User>(oprdptr1)->getOperand(0)->getType());
                if (!llvm::dyn_cast<llvm::Constant>(oprdptr2))
                    replacement.push_back(oprdptr2);
                oprdptr1 = llvm::dyn_cast<llvm::User>(oprdptr1)->getOperand(0);
            }
            
            //Assuming that we check before that it was a variable with 'checkCPTypeInIR'
            assert ((llvm::isa<llvm::LoadInst>(oprdptr1) || llvm::isa<llvm::PtrToIntInst>(oprdptr1)) && "Must be Load Instruction here (assign left hand oprd)");
            oprdptr1 = llvm::dyn_cast<llvm::User>(oprdptr1)->getOperand(0);
            
            //store operand order is opposite of classical assignement (r->l)
            assert (oprdptr2->getType()->getPrimitiveSizeInBits() && "The value to assign must be primitive here");
            llvm::Value *store = builder.CreateAlignedStore(oprdptr2, oprdptr1, oprdptr2->getType()->getPrimitiveSizeInBits()/8);  //TODO: Maybe replace the alignement here with that from DataLayout
            replacement.push_back(store);
            return valRet;
        }
        case mKEEP_ONE_OPRD:
        {
            return oprdptr1;
        }
        case mCONST_VALUE_OF:
        {
            //The operand 'oprdptr1' here is already the constant, just return it
            return oprdptr1;
        }
        case mADD:
        {
            llvm::Value *add = builder.CreateAdd(oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(add))
                replacement.push_back(add);
            return add;
        }
        case mFADD:
        {
            llvm::Value *fadd = builder.CreateFAdd(oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(fadd))
                replacement.push_back(fadd);
            return fadd;
        }
        case mSUB:
        {
            llvm::Value *sub = builder.CreateSub(oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(sub))
                replacement.push_back(sub);
            return sub;
        }
        case mFSUB:
        {
            llvm::Value *fsub = builder.CreateFSub(oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(fsub))
                replacement.push_back(fsub);
            return fsub;
        }
        case mMUL:
        {
            llvm::Value *mul = builder.CreateMul(oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(mul))
                replacement.push_back(mul);
            return mul;
        }
        case mFMUL:
        {
            llvm::Value *fmul = builder.CreateFMul(oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(fmul))
                replacement.push_back(fmul);
            return fmul;
        }
        case mSDIV:
        {
            llvm::Value *sdiv = builder.CreateSDiv(oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(sdiv))
                replacement.push_back(sdiv);
            return sdiv;
        }
        case mUDIV:
        {
            llvm::Value *udiv = builder.CreateUDiv(oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(udiv))
                replacement.push_back(udiv);
            return udiv;
        }
        case mFDIV:
        {
            llvm::Value *fdiv = builder.CreateFDiv(oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(fdiv))
                replacement.push_back(fdiv);
            return fdiv;
        }
        case mSMOD:
        {
            llvm::Value *smod = builder.CreateSRem(oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(smod))
                replacement.push_back(smod);
            return smod;
        }
        case mUMOD:
        {
            llvm::Value *umod = builder.CreateURem(oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(umod))
                replacement.push_back(umod);
            return umod;
        }
        case mFMOD:
        {
            llvm::Value *fmod = builder.CreateFRem(oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(fmod))
                replacement.push_back(fmod);
            return fmod;
        }
        case mBITAND:
        {
            llvm::Value *band = builder.CreateAnd(oprdptr1, oprdptr2);
            if (band != oprdptr1 && band != oprdptr2)
                if (!llvm::dyn_cast<llvm::Constant>(band))
                    replacement.push_back(band);
            return band;
        }
        case mBITOR:
        {
            llvm::Value *bor = builder.CreateOr(oprdptr1, oprdptr2);
            if (bor != oprdptr1 && bor != oprdptr2)
                if (!llvm::dyn_cast<llvm::Constant>(bor))
                    replacement.push_back(bor);
            return bor;
        }
        case mBITXOR:
        {
            llvm::Value *bxor = builder.CreateXor(oprdptr1, oprdptr2);
            if (bxor != oprdptr1 && bxor != oprdptr2)
                if (!llvm::dyn_cast<llvm::Constant>(bxor))
                    replacement.push_back(bxor);
            return bxor;
        }
        case mBITSHIFTLEFT:
        {
            llvm::Value *shl = builder.CreateShl(oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(shl))
                replacement.push_back(shl);
            return shl;
        }
        case mABITSHIFTRIGHT:
        {
            llvm::Value *ashr = builder.CreateAShr(oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(ashr))
                replacement.push_back(ashr);
            return ashr;
        }
        case mLBITSHIFTRIGHT:
        {
            llvm::Value *lshr = builder.CreateLShr(oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(lshr))
                replacement.push_back(lshr);
            return lshr;
        }
        case mBITNOT:
        {
            llvm::Value *bitnot = builder.CreateXor(oprdptr1, llvm::Constant::getAllOnesValue(oprdptr1->getType())); //llvm::ConstantInt::get(oprdptr1->getType(), -1));
            if (!llvm::dyn_cast<llvm::Constant>(bitnot))
                replacement.push_back(bitnot);
            return bitnot;
        }
        case mNEG:
        {
            llvm::Value *neg = builder.CreateSub(llvm::ConstantInt::get(oprdptr1->getType(), 0), oprdptr1);
            if (!llvm::dyn_cast<llvm::Constant>(neg))
                replacement.push_back(neg);
            return neg;
        }
        case mFNEG:
        {
            llvm::Value *fneg = builder.CreateFSub(llvm::ConstantFP::get(oprdptr1->getType(), 0.0), oprdptr1);
            if (!llvm::dyn_cast<llvm::Constant>(fneg))
                replacement.push_back(fneg);
            return fneg;
        }
        case mABS:
        {
            llvm::Value *minusVal = builder.CreateSub(llvm::ConstantInt::get(oprdptr1->getType(), 0), oprdptr1);
            llvm::Value *cmp = builder.CreateICmp(llvm::CmpInst::ICMP_SGE, oprdptr1, llvm::ConstantInt::get(oprdptr1->getType(), 0));
            llvm::Value *abs = builder.CreateSelect(cmp, oprdptr1, minusVal);
            if (!llvm::dyn_cast<llvm::Constant>(minusVal))
                replacement.push_back(minusVal);
            if (!llvm::dyn_cast<llvm::Constant>(cmp))
                replacement.push_back(cmp);
            if (!llvm::dyn_cast<llvm::Constant>(abs))
                replacement.push_back(abs);
            return abs;
        }
        case mFABS:
        {
            llvm::Value *fminusVal = builder.CreateFSub(llvm::ConstantFP::get(oprdptr1->getType(), 0.0), oprdptr1);
            llvm::Value *fcmp = builder.CreateFCmp(llvm::CmpInst::FCMP_UGE, oprdptr1, llvm::ConstantFP::get(oprdptr1->getType(), 0.0));
            llvm::Value *fabs = builder.CreateSelect(fcmp, oprdptr1, fminusVal);
            if (!llvm::dyn_cast<llvm::Constant>(fminusVal))
                replacement.push_back(fminusVal);
            if (!llvm::dyn_cast<llvm::Constant>(fcmp))
                replacement.push_back(fcmp);
            if (!llvm::dyn_cast<llvm::Constant>(fabs))
                replacement.push_back(fabs);
            return fabs;
        }
        case mLEFTINC:
        case mFLEFTINC:
        case mRIGHTINC:
        case mFRIGHTINC:
        case mLEFTDEC:
        case mFLEFTDEC:
        case mRIGHTDEC:
        case mFRIGHTDEC:
        {
            bool leftside = false;
            if (replfirst == mLEFTINC || replfirst == mFLEFTINC || replfirst == mLEFTDEC || replfirst == mFLEFTDEC)
                leftside = true;
             //llvm::dyn_cast<llvm::Instruction>(oprdptr1)->dump();//DEBUG   
            llvm::Value *rawVal = oprdptr1;
            std::vector<llvm::Value *> seenCasts;
            while (llvm::isa<llvm::CastInst>(oprdptr1))
            {
                //assert (llvm::dyn_cast<llvm::User>(val)->getNumOperands() == 1);
                if (llvm::isa<llvm::PtrToIntInst>(oprdptr1))
                    break;
                seenCasts.insert(seenCasts.begin(), oprdptr1);
                oprdptr1 = llvm::dyn_cast<llvm::User>(oprdptr1)->getOperand(0);
            }
            
            //Assuming that we check before that it was a variable with 'checkCPTypeInIR'
            assert ((llvm::isa<llvm::LoadInst>(oprdptr1) /*|| llvm::isa<llvm::PtrToIntInst>(oprdptr1)*/) && "Must be Load Instruction here (assign left hand oprd)");
            
            llvm::Value *changedVal;
            if (seenCasts.empty())
            {
                if (replfirst == mLEFTINC || replfirst == mRIGHTINC)
                    changedVal = builder.CreateAdd(oprdptr1, llvm::ConstantInt::get(oprdptr1->getType(), 1));
                else if (replfirst == mFLEFTINC || replfirst == mFRIGHTINC)
                    changedVal = builder.CreateFAdd(oprdptr1, llvm::ConstantFP::get(oprdptr1->getType(), 1.0));
                else if (replfirst == mLEFTDEC || replfirst == mRIGHTDEC)
                    changedVal = builder.CreateSub(oprdptr1, llvm::ConstantInt::get(oprdptr1->getType(), 1));
                else if (replfirst == mFLEFTDEC || replfirst == mFRIGHTDEC)
                    changedVal = builder.CreateFSub(oprdptr1, llvm::ConstantFP::get(oprdptr1->getType(), 1.0));
            }
            else
            {
                if (replfirst == mLEFTINC || replfirst == mRIGHTINC || replfirst == mFLEFTINC || replfirst == mFRIGHTINC)
                {
                    if (oprdptr1->getType()->isIntegerTy())
                        changedVal = builder.CreateAdd(oprdptr1, llvm::ConstantInt::get(oprdptr1->getType(), 1));
                    else if (oprdptr1->getType()->isFloatingPointTy())
                        changedVal = builder.CreateFAdd(oprdptr1, llvm::ConstantFP::get(oprdptr1->getType(), 1.0));
                    else
                        assert ("The type is neither integer nor floating point, can't INC");
                }
                else if (replfirst == mLEFTDEC || replfirst == mRIGHTDEC || replfirst == mFLEFTDEC || replfirst == mFRIGHTDEC)
                {
                    if (oprdptr1->getType()->isIntegerTy())
                        changedVal = builder.CreateSub(oprdptr1, llvm::ConstantInt::get(oprdptr1->getType(), 1));
                    else if (oprdptr1->getType()->isFloatingPointTy())
                        changedVal = builder.CreateFSub(oprdptr1, llvm::ConstantFP::get(oprdptr1->getType(), 1.0));
                    else
                        assert ("The type is neither integer nor floating point, can't DEC");
                }
            }
            
            if (!llvm::dyn_cast<llvm::Constant>(changedVal))
                replacement.push_back(changedVal);
            //llvm::dyn_cast<llvm::Instruction>(changedVal)->dump();//DEBUG
            llvm::Value *storeit = llvm::dyn_cast<llvm::User>(oprdptr1)->getOperand(0);  //get the address where the data comes from (for store)
            assert(changedVal->getType()->getPrimitiveSizeInBits() && "Must be primitive here");
            storeit = builder.CreateAlignedStore(changedVal, storeit, changedVal->getType()->getPrimitiveSizeInBits()/8);       //the val here is primitive
            replacement.push_back(storeit);
            if (leftside)
            {
                if (!seenCasts.empty())
                {
                    std::vector<llvm::Value *> copySeenCasts;
                    cloneStmtIR (seenCasts, copySeenCasts);
                    llvm::dyn_cast<llvm::User>(copySeenCasts.front())->setOperand(0, changedVal);
                    replacement.insert(replacement.end(), copySeenCasts.begin(), copySeenCasts.end());
                    return replacement.back();
                }
                return changedVal;
            }
            else
            {
                return rawVal;
            }
        }
        case mFCMP_FALSE:
        case mFCMP_OEQ:
        case mFCMP_OGT:
        case mFCMP_OGE:
        case mFCMP_OLT:
        case mFCMP_OLE:
        case mFCMP_ONE:
        case mFCMP_ORD:
        case mFCMP_UNO:
        case mFCMP_UEQ:
        case mFCMP_UGT:
        case mFCMP_UGE:
        case mFCMP_ULT:
        case mFCMP_ULE:
        case mFCMP_UNE:
        case mFCMP_TRUE:
        {
            llvm::Value *fcmp = builder.CreateFCmp(getPredRelMap().at(replfirst), oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(fcmp))
                replacement.push_back(fcmp);
            fcmp = builder.CreateSIToFP (fcmp, oprdptr1->getType());
            if (!llvm::dyn_cast<llvm::Constant>(fcmp))
                replacement.push_back(fcmp);
            return fcmp;
        }
        case mICMP_EQ:
        case mICMP_NE:
        case mICMP_UGT:
        case mICMP_UGE:
        case mICMP_ULT:
        case mICMP_ULE:
        case mICMP_SGT:
        case mICMP_SGE:
        case mICMP_SLT:
        case mICMP_SLE:
        {
            //here oprdptr1 and 2 cannote be representing pointer, beacause only binary operation on 2 pointer is comparison
            // and comparison mutated to comparison is processed right after match
            llvm::Value *icmp = builder.CreateICmp(getPredRelMap().at(replfirst), oprdptr1, oprdptr2);
            if (!llvm::dyn_cast<llvm::Constant>(icmp))
                replacement.push_back(icmp);
            icmp = builder.CreateZExt (icmp, oprdptr1->getType());
            if (!llvm::dyn_cast<llvm::Constant>(icmp))
                replacement.push_back(icmp);
            return icmp;
        }
        
        /**** ADD HERE ****/
        
        //Insert new operators above this
        
        default:    //Unreachable
        {
            llvm::errs() << "Error: Undefine replacor code: " << replfirst << "\n";
            assert (false && "Invalid operation in 'allCreators'");
        }
    }
}

// The pointer is alway loaded (to get its address) then GEP
// array cannot be modified (array++ unallowed)
llvm::Value * allPCreators(enum ExpElemKeys replfirst, llvm::Value * addrOprd, llvm::Value * intValOprd, std::vector<llvm::Value *> &replacement, llvm::DataLayout &DL)
{
    llvm::IRBuilder<> builder(llvm::getGlobalContext());
    switch (replfirst)
    {
        case mKEEP_ONE_OPRD:
        {
            return oprdptr1;
        }
        case mCONST_VALUE_OF:
        {
            //The operand 'oprdptr1' here is already the constant, just return it
            return oprdptr1;
        }
        case mPADD:
        case mPSUB:
        {
            llvm::Value *valtmp = (replfirst==mPADD) ? intValOprd : builder.CreateNeg(intValOprd);
            if (!llvm::dyn_cast<llvm::Constant>(valtmp))
                replacement.push_back(valtmp);
            llvm::Value *addsub_gep = builder.CreateInBoundsGEP(nullptr, addrOprd, valtmp);
            if (!llvm::dyn_cast<llvm::Constant>(addsub_gep))
                replacement.push_back(addsub_gep);
            return addsub_gep;
        }        
        case mPLEFTINC:
        case mPRIGHTINC:
        case mPLEFTDEC:
        case mPRIGHTDEC:
        {
            //Assuming that we check before that it was a Pointer with 'checkCPTypeInIR'
            assert (llvm::isa<llvm::LoadInst>(addrOprd) && "Must be Load Instruction here (address oprd)");
            
            int inc_dec;
            if (replfirst == mPLEFTINC || replfirst == mPRIGHTINC)
                inc_dec = 1;
            else if (replfirst == mPLEFTDEC || replfirst == mPRIGHTDEC)
                inc_dec = -1;
            llvm::Value *changedVal = builder.CreateInBoundsGEP(nullptr, addrOprd, 
                                                                llvm::ConstantInt::get(llvm::Type::getIntNTy(llvm::getGlobalContext(), DL.getPointerTypeSizeInBits(addrOprd->getType())), inc_dec));
            
            if (!llvm::dyn_cast<llvm::Constant>(changedVal))
                replacement.push_back(changedVal);
            //llvm::dyn_cast<llvm::Instruction>(changedVal)->dump();//DEBUG
            llvm::Value *storeit = llvm::dyn_cast<llvm::LoadInst>(addrOprd)->getPointerOperand();  //get the address where the data comes from (for store)
            //assert(changedVal->getType()->getPrimitiveSizeInBits() && "Must be primitive here");
            storeit = builder.CreateAlignedStore(changedVal, storeit, DL.getPointerTypeSize(storeit.getType()));       //the val here is a pointer
            replacement.push_back(storeit);
            if (replfirst == mPLEFTINC || replfirst == mPLEFTDEC)
                return changedVal;
            else
                return addrOprd;
        }
    }
}

// Operation like (*p)++, *(p++), p[1], p[0]+1, ...
// array cannot be modified (array++ unallowed)
llvm::Value * allPDerefCreators(enum ExpElemKeys replfirst, llvm::Value * addrOprd, llvm::Value * valOprd, std::vector<llvm::Value *> &replacement, llvm::DataLayout &DL)
{
    llvm::IRBuilder<> builder(llvm::getGlobalContext());
    switch (replfirst)
    {
        case mPADD_DEREF:   // *(p + x) or p[x]
        case mPSUB_DEREF:   // *(p - x)
        {
            //assert (llvm::isa<llvm::LoadInst>(addrOprd) && "Must be Load Instruction here (address oprd): allPDerefCreators");
            assert (llvm::isa<llvm::IntegerType>(valOprd) && "The index for pointer deref must be integer");
            llvm::Value *pOP = allPCreators ((replfirst==mPADD_DEREF)?mPADD:mPSUB, addrOprd, valOprd, replacement, DL);
            llvm::Type *valType = pOP->getPointerElementType();    //get the type pointed by pOP
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            assert ((llvm::PointerType::isValidElementType(valType) && !valType->isFunctionTy()) && "Problem: this should not be called when the type is not loadable or storable(verifiy before)")
#else
            assert (llvm::PointerType::isLoadableorStorableType(valType) && "Problem: this should not be called when the type is not loadable or storable(verifiy before)");
#endif
            llvm::LoadInst *deref = builder.CreateAlignedLoad(pOP, DL.getTypeStoreSize(valType));
            replacement.push_back(deref);
            return deref;
        }     
        case mPDEREF_ADD:   // *(p) + x or p[0] + x
        case mPDEREF_SUB:   // *(p) - x or p[0] - x
        {
            //assert (llvm::isa<llvm::LoadInst>(addrOprd) && "Must be Load Instruction here (address oprd): allPDerefCreators");
            llvm::Type *valType = addrOprd->getPointerElementType();    //get the type pointed by addrOprd
            llvm::LoadInst *deref = builder.CreateAlignedLoad(addrOprd, DL.getTypeStoreSize(valType));
            replacement.push_back(deref);
            llvm::Value *addsubP;
            if (llvm::isa<llvm::PointerType>(deref))
            {
                assert (llvm::isa<llvm::IntegerType>(valOprd) && "The val(index) to add/sud to pointer must be integer");
                addsubP = allPCreators ((replfirst==mPDEREF_ADD)?mPADD:mPSUB, deref, valOprd, replacement, DL);
            }
            else
            {
                //deref has been loaded thus support arithmetic operation
                assert (checkCPTypeInIR(cpEXPR, valOprd) && "The value for arithmetic add or sub must be suporting arithmetic operation (cpExpr)");
                addsubP = allCreators ((replfirst==mPDEREF_ADD)?mADD:mSUB, deref, valOprd, replacement);
            }
            return addsubP;
        }   
        case mPLEFTINC_DEREF:   // *(++p)
        case mPRIGHTINC_DEREF:  // *(p++)
        case mPLEFTDEC_DEREF:   // *(--p)
        case mPRIGHTDEC_DEREF:  // *(p--)
        {
            enum ExpElemKeys tmpRepl;
            if (replfirst==mPLEFTINC_DEREF) 
                tmpRepl = mPLEFTINC;
            else if (replfirst==mPRIGHTINC_DEREF)
                tmpRepl = mPRIGHTINC;
            else if (replfirst==mPLEFTDEC_DEREF)
                tmpRepl = mPLEFTDEC;
            else
                tmpRepl = mPRIGHTDEC;
            assert (llvm::isa<llvm::LoadInst>(addrOprd) && "Must be Load Instruction here (address oprd), inc dec: allPDerefCreators");
            llvm::Value *pOP = allPCreators (tmpRepl, addrOprd, valOprd, replacement, DL);
            llvm::Type *valType = pOP->getPointerElementType();    //get the type pointed by pOP
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            assert ((llvm::PointerType::isValidElementType(valType) && !valType->isFunctionTy()) && "Problem: this should not be called when the type is not loadable or storable(verifiy before)");
#else
            assert (llvm::PointerType::isLoadableorStorableType(valType) && "Problem: this should not be called when the type is not loadable or storable(verifiy before)");
#endif
            llvm::LoadInst *deref = builder.CreateAlignedLoad(pOP, DL.getTypeStoreSize(valType));
            replacement.push_back(deref);
            return deref;
        }
        case mPDEREF_LEFTINC:   // ++(*p)
        case mPDEREF_RIGHTINC:  // (*p)++
        case mPDEREF_LEFTDEC:   // --(*p)
        case mPDEREF_RIGHTDEC:  // (*p)--
        {
            enum ExpElemKeys tmpRepl;
            //assert (llvm::isa<llvm::LoadInst>(addrOprd) && "Must be Load Instruction here (address oprd): allPDerefCreators");
            llvm::Type *valType = addrOprd->getPointerElementType();    //get the type pointed by addrOprd
            llvm::LoadInst *deref = builder.CreateAlignedLoad(addrOprd, DL.getTypeStoreSize(valType));
            replacement.push_back(deref);
            llvm::Value *incdecP;
            if (llvm::isa<llvm::PointerType>(deref))
            {
                if (replfirst==mPDEREF_LEFTINC) 
                    tmpRepl = mPLEFTINC;
                else if (replfirst==mPDEREF_RIGHTINC)
                    tmpRepl = mPRIGHTINC;
                else if (replfirst==mPDEREF_LEFTDEC)
                    tmpRepl = mPLEFTDEC;
                else
                    tmpRepl = mPRIGHTDEC;
                incdecP = allPCreators (tmpRepl, deref, valOprd, replacement, DL);
            }
            else
            {
                //deref has been loaded thus is a variable that can be inc/dec
                if (replfirst==mPDEREF_LEFTINC) 
                    tmpRepl = mLEFTINC;
                else if (replfirst==mPDEREF_RIGHTINC)
                    tmpRepl = mRIGHTINC;
                else if (replfirst==mPDEREF_LEFTDEC)
                    tmpRepl = mLEFTDEC;
                else
                    tmpRepl = mRIGHTDEC;
                incdecP = allCreators (tmpRepl, deref, valOprd, replacement);
            }
            return incdecP;
        }
    }
}

llvm::Constant *constCreator (llvm::Type *type, unsigned &posConstValueMap_POS, llvm::Constant *toCompare=nullptr)
{
    llvm::Constant *nc = nullptr;
    if (type->isFloatingPointTy())
    {
        nc = llvm::ConstantFP::get (type, llvmMutationOp::getConstValueStr(posConstValueMap_POS));
        
        if (toCompare)
        {
            assert (toCompare->getType() == type && "Type Mismatch!");
            //do not mutate when the replaced value is same (equivalent)
            if (llvm::dyn_cast<llvm::ConstantFP>(nc)->isExactlyValue(llvm::dyn_cast<llvm::ConstantFP>(toCompare)->getValueAPF()))
            {
                //delete nc;
                nc = nullptr;
            }
        }
        
    }
    else if (type->isIntegerTy())
    {
        nc = llvm::ConstantInt::get (llvm::dyn_cast<llvm::IntegerType>(type), llvmMutationOp::getConstValueStr(posConstValueMap_POS), /*radix 10*/10);
        
        if (toCompare)
        {
            assert (toCompare->getType() == type && "Type Mismatch!");
            //do not mutate when the replaced value is same (equivalent)
            if (llvm::dyn_cast<llvm::ConstantInt>(nc)->equalsInt(llvm::dyn_cast<llvm::ConstantInt>(toCompare)->getZExtValue()))
            {
                //delete nc;
                nc = nullptr;
            }
        }
    }
    else
    {
        assert (false && "unreachable!!");
    }
    return nc;
}


//Match constant 'C'
//@toMatch: the statement from where to match
//@replacors: list of replacement (<op, list of index of params of fmul>)
void matchANYCONST (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts, bool isInt) 
{
    int pos = -1;
    bool stmtDeleted = false;
    for (auto *val:toMatch)
    {
        pos++;
        for (auto oprdvIt = 0; oprdvIt < llvm::dyn_cast<llvm::User>(val)->getNumOperands(); oprdvIt++) 
        {
            llvm::Constant * constval = nullptr;
            if ((!isInt && llvm::isa<llvm::ConstantFP>(llvm::dyn_cast<llvm::User>(val)->getOperand(oprdvIt))) 
                || (isInt && llvm::isa<llvm::ConstantInt>(llvm::dyn_cast<llvm::User>(val)->getOperand(oprdvIt))))
            {
                constval = llvm::dyn_cast<llvm::Constant>(llvm::dyn_cast<llvm::User>(val)->getOperand(oprdvIt));
            }
                
            if (constval)
            {
                std::vector<llvm::Value *> toMatchClone;
                std::vector<llvm::Value *> replacement;
                for (auto &repl: mutationOp.mutantReplacorsList)
                {
                    if (repl.first == mDELSTMT)
                    {
                        if (!stmtDeleted)
                        {
                            resultMuts.push_back(std::vector<llvm::Value *>());
                            stmtDeleted = true;
                        }
                        continue;
                    }
                    
                    if (repl.first == mCONST_VALUE_OF)
                    {
                        assert (repl.second.size() == 1 && "Expected exactly one constant");
                        
                        llvm::Constant *nc = nullptr;
                        if(! (nc = constCreator (constval->getType(), repl.second[0], constval)))
                            continue;                        
                        
                        toMatchClone.clear();
                        cloneStmtIR (toMatch, toMatchClone);
                        
                        //llvm::Value* todel = llvm::dyn_cast<llvm::User>(toMatchClone[pos])->getOperand(oprdvIt);
                        llvm::dyn_cast<llvm::User>(toMatchClone[pos])->setOperand(oprdvIt, nc);
                        //delete todel;
                        
                        resultMuts.push_back(toMatchClone);
                        
                        continue;
                    }
                    
                    assert (repl.second.size() >= 1 && repl.second.size() <= 2 && "Must be unary or binary operation here!");
                    
                    toMatchClone.clear();
                    cloneStmtIR (toMatch, toMatchClone);
                    replacement.clear();
                    llvm::Value * oprdptr[]={nullptr, nullptr};
                    //bool constvalused = false;
                    for (int i=0; i < repl.second.size(); i++)
                    {
                        if (repl.second[i] == 0)
                        {
                            oprdptr[i] = llvm::dyn_cast<llvm::User>(toMatchClone[pos])->getOperand(oprdvIt);
                            //constvalused = true;
                        }
                        else if (repl.second[i] > llvmMutationOp::maxOprdNum)   //it is a given constant value (ex: 2, 5, ...)
                        {
                            oprdptr[i] = constCreator (constval->getType(), repl.second[i]);
                        }
                        else
                        {
                            assert (false && "unreachable, invalid operand id");
                        }
                    }
                    
                    //llvm::errs() << constval->getType() << " " << oprdptr[0]->getType() << " " << oprdptr[1]->getType() << "\n";
                    //llvm::dyn_cast<llvm::Instruction>(val)->dump();
                    if (oprdptr[0] && oprdptr[1])   //binary operation case
                        assert (oprdptr[0]->getType() == oprdptr[1]->getType() && "ERROR: Type mismatch!!");
                    llvm::Constant * createdRes = llvm::dyn_cast<llvm::Constant>(allCreators(repl.first, oprdptr[0], oprdptr[1], replacement));
                    
                    //llvm::Value* todel = llvm::dyn_cast<llvm::User>(toMatchClone[pos])->getOperand(oprdvIt);
                        
                    llvm::dyn_cast<llvm::User>(toMatchClone[pos])->setOperand(oprdvIt, createdRes);
                    
                    //if (!constvalused)
                    //    delete todel;
                        
                    resultMuts.push_back(toMatchClone); 
                    
                    continue;
                }
            }
        }
    }
}

//Match rvalue Expression '@'  -- any llvm register
//@toMatch: the statement from where to match
//@replacors: list of replacement (<op, list of index of params of fmul>)
void matchANYEXPR (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts, bool isInt) 
{
    int pos = -1;
    bool stmtDeleted = false;
    for (auto *val:toMatch)
    {
        pos++;
        
        if ((val->getType()->isFloatingPointTy() && !isInt) || (val->getType()->isIntegerTy() && isInt))
        {
            //Case of match any var
            if (mutationOp.matchOp == mANYIVAR || mutationOp.matchOp == mANYFVAR)
            {
                if (! llvm::isa<llvm::LoadInst>(val))
                    continue;
            }
            
            ///REPLACE    
            std::vector<llvm::Value *> toMatchClone;
            std::vector<unsigned> posOfIRtoRemove({pos+1});
            for (auto &repl: mutationOp.mutantReplacorsList)
            {
                toMatchClone.clear();
                if (repl.first == mDELSTMT)
                {
                    if (!stmtDeleted)
                    {
                        doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, nullptr, nullptr, nullptr);
                        stmtDeleted = true;
                    }
                }
                else
                {
                    cloneStmtIR (toMatch, toMatchClone);
                    llvm::Value *returning = toMatchClone[pos];
                    llvm::Value *oprdptr[2] = {nullptr, nullptr};
                    for (int i=0; i < repl.second.size(); i++)
                    {
                        if (repl.second[i] > llvmMutationOp::maxOprdNum)
                        {
                            oprdptr[i] = constCreator (toMatchClone[pos]->getType(), repl.second[i]);
                        }
                        else
                        {
                            oprdptr[i] = toMatchClone[pos];       //cloned 'val'
                        }
                    }
                    toMatchClone.insert(toMatchClone.begin() + pos + 1, nullptr);
                    doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, oprdptr[0], oprdptr[1], returning);
                }
            }
        }
    }
}

void matchANYFCONST (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchANYCONST (toMatch, mutationOp, resultMuts, false);
}

void matchANYICONST (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchANYCONST (toMatch, mutationOp, resultMuts, true);
}

void matchANYFVAR (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchANYEXPR (toMatch, mutationOp, resultMuts, false);  //there mach only load
}

void matchANYIVAR (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchANYEXPR (toMatch, mutationOp, resultMuts, true);   //there mach only load
}

void matchANYFEXPR (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)
{
    matchANYEXPR (toMatch, mutationOp, resultMuts, false);
}
void matchANYIEXPR (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)
{
    matchANYEXPR (toMatch, mutationOp, resultMuts, true);
}

//@toMatch: the statement from where to match
//@replacors: list of replacement (<op, list of index of params of add>)
void matchArithBinOp (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts, unsigned opCode) 
{   
    int pos = -1;
    bool stmtDeleted = false;
    for (auto *val:toMatch)
    {
        pos++;
        
        ///MATCH
        //Assume that val has to be of Instruction type
        if (llvm::dyn_cast<llvm::Instruction>(val)->getOpcode() != opCode)
            continue;
                
        if (llvm::isa<llvm::BinaryOperator>(val) || llvm::dyn_cast<llvm::StoreInst>(val))
        {
            llvm::Instruction *binOrStore = llvm::dyn_cast<llvm::Instruction>(val);
            
            bool opCPMismatch = false;
            
            // Do not match second oprd for store -- no need (address)
            if (llvm::dyn_cast<llvm::StoreInst>(val))
            {
                if ((mutationOp.matchOp == mIASSIGN && !binOrStore->getOperand(0)->getType()->isIntegerTy())
                    || (mutationOp.matchOp == mFASSIGN && !binOrStore->getOperand(0)->getType()->isFPOrFPVectorTy())
                    || (! checkCPTypeInIR (mutationOp.getCPType(1), binOrStore->getOperand(0))))
                {
                    opCPMismatch = true;
                }
            }
            else
            {
                for (auto oprdID=0; oprdID < binOrStore->getNumOperands(); oprdID++)
                {
                    if (! checkCPTypeInIR (mutationOp.getCPType(oprdID), binOrStore->getOperand(oprdID)))
                    {
                        opCPMismatch = true;
                        break;
                    }
                }
            }
            if (opCPMismatch)
                continue;
            
            ///REPLACE    
            std::vector<llvm::Value *> toMatchClone;
            std::vector<llvm::Value *> replacement;
            for (auto &repl: mutationOp.mutantReplacorsList)
            {
                if (repl.first == mDELSTMT)
                {
                    if (!stmtDeleted)
                    {
                        resultMuts.push_back(std::vector<llvm::Value *>());
                        stmtDeleted = true;
                    }
                    continue;
                }
                else
                {
                    auto posInClone = pos;
                    toMatchClone.clear();
                    cloneStmtIR (toMatch, toMatchClone);
                    replacement.clear();
                    llvm::Value * oprdptr[]={nullptr, nullptr};
                    
                    assert ((repl.first != mKEEP_ONE_OPRD || (repl.second.size()==1 && repl.second[0] < 2)) && "Error in the replacor 'mKEEP_ONE_OPRD'");
                    
                    if (mutationOp.matchOp == mIASSIGN || mutationOp.matchOp == mFASSIGN)
                    {
                        //left hand assignement is right hand for load and vice-versa
                        for (int i=0; i < repl.second.size(); i++)
                        {
                            if (repl.second[i] > llvmMutationOp::maxOprdNum)
                            {
                                oprdptr[i] = constCreator (llvm::dyn_cast<llvm::User>(toMatchClone[posInClone])->getOperand(0)->getType(), repl.second[i]);
                            }
                            else if (repl.second[i] == 0)    //left hand assignement is right hand for store and vice-versa
                            {
                                llvm::IRBuilder<> builder(llvm::getGlobalContext());
                                llvm::Value *load = builder.CreateAlignedLoad(llvm::dyn_cast<llvm::User>(toMatchClone[posInClone])->getOperand(1), 
                                                                            llvm::dyn_cast<llvm::StoreInst>(toMatchClone[posInClone])->getAlignment());
                                toMatchClone.insert(toMatchClone.begin() + posInClone, load);
                                posInClone++;
                                oprdptr[i] = load;
                            }
                            else        //repl.second[i] is 1 here
                                oprdptr[i] = llvm::dyn_cast<llvm::User>(toMatchClone[posInClone])->getOperand(1 - repl.second[i]);
                        }
                    }
                    else
                    {
                        for (int i=0; i < repl.second.size(); i++)
                        {
                            if (repl.second[i] > llvmMutationOp::maxOprdNum)
                            {
                                oprdptr[i] = constCreator (llvm::dyn_cast<llvm::User>(toMatchClone[posInClone])->getOperand(0)->getType(), repl.second[i]);  //The two operands of binOp have same type
                            }
                            else
                            {
                                oprdptr[i] = llvm::dyn_cast<llvm::User>(toMatchClone[posInClone])->getOperand(repl.second[i]);
                            }
                        }
                    }
                    
                    llvm::Value * createdRes = allCreators(repl.first, oprdptr[0], oprdptr[1], replacement);
                    
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                        //IRFlags non existant for these verisons
#else
                    //Copy Flags if binop
                    for (auto rinst: replacement)
                    {
                        if (auto * risbinop = llvm::dyn_cast<llvm::BinaryOperator>(rinst))
                            risbinop->copyIRFlags(toMatchClone[posInClone]);
                    }
#endif
                    std::vector<std::pair<llvm::User *, unsigned>> affectedUnO;
                    //set uses of the matched IR to corresponding OPRD
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                    for (llvm::Value::use_iterator ui=toMatchClone[posInClone]->use_begin(), ue=toMatchClone[posInClone]->use_end(); ui!=ue; ++ui)
                    {
                        auto &U = ui.getUse();
                        llvm::User *user = U.getUser();
                        affectedUnO.push_back(std::pair<llvm::User *, unsigned>(user,ui.getOperandNo()));
                        //user->setOperand(ui.getOperandNo(), createdRes);
#else
                    for (auto &U: llvm::dyn_cast<llvm::Instruction>(toMatchClone[posInClone])->uses())
                    {
                        llvm::User *user = U.getUser();
                        affectedUnO.push_back(std::pair<llvm::User *, unsigned>(user,U.getOperandNo()));
                        //user->setOperand(U.getOperandNo(), createdRes);
#endif
                        //Avoid infinite loop because of setoperand ...
                        //if (llvm::dyn_cast<llvm::Instruction>(toMatchClone[posInClone])->getNumUses() <= 0) 
                        //    break;
                    }
                    for(auto &affected: affectedUnO)
                        affected.first->setOperand(affected.second, createdRes);
                        
                    delete toMatchClone[posInClone];
                    toMatchClone.erase(toMatchClone.begin() + posInClone);
                    if (! replacement.empty())
                        toMatchClone.insert(toMatchClone.begin() + posInClone, replacement.begin(), replacement.end());
                    resultMuts.push_back(toMatchClone); 
                }
            }
        }
    }
}

//TODO Assign operation here
void matchASSIGN (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::Store);
}

//@toMatch: the statement from where to match
//@replacors: list of replacement (<op, list of index of params of add>)
void matchADD (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::Add);
}

//@toMatch: the statement from where to match
//@replacors: list of replacement (<op, list of index of params of fadd>)
void matchFADD (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::FAdd);
}

//@toMatch: the statement from where to match
//@replacors: list of replacement (<op, list of index of params of sub>)
void matchSUB (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::Sub);
}

//@toMatch: the statement from where to match
//@replacors: list of replacement (<op, list of index of params of fsub>)
void matchFSUB (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::FSub);
}

// Return nullptr if the gep do not represent a pointer indexing, else, it return the value representing the index
// The when the pointer oprd do not comes from LoadInst, the 1st index of get just take out the address part (as alloca vars are actually addresses)
inline llvm::Value* checkIsPointerIndexingAndGet (llvm::GetElementPtrInst * gep, int &index)
{
    index = -1;
    if (gep->getNumIndices() < 1)
        return nullptr;
    llvm::Instruction *ptrOprd = gep->getPointerOperand();
    //if the pointer operand points to a non sequential type, the pointer must come from load instruction, and the 1st idx of get will be the index needed
    if (! llvm::isa<llvm::SequentialType>(ptrOprd->getPointerElementType()))
    {
        if (llvm::isa<llvm::LoadInst>(ptrOprd))
        {
            // return the first index (idx = 0)
            index = 0;
            return *(gep->idx_begin() + index);
        }
        else
        {
            return nullptr;
        }
    }
    else    // The pointer operand point to a sequential type, take idx 0 if comes from load, else take idx 1
    {
        if (llvm::isa<llvm::LoadInst>(ptrOprd))
        {
            // return the first index (idx = 0)
            index = 0;
            return *(gep->idx_begin() + index);
        }
        else
        {
            // return the first index (idx = 1)
            assert (gep->getNumIndices() > 1 && "gep should have more than 1 index here");
            index = 1;
            return *(gep->idx_begin() + index);
        }
    }
}

// The pointer operand is at the left side and the integer at the right side
void matchPADD_SUB (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    int pos = -1;
    bool stmtDeleted = false;
    for (auto *val:toMatch)
    {
        pos++;
        
        ///MATCH
        if (auto *gep = llvm::dyn_cast<llvm::GetElementPtrInst>(val))
        {
            // check the pointer displacement index (0)'s value
            int indx;
            llvm::Value *indxVal = checkIsPointerIndexingAndGet(gep, indx);
            if (! indxVal)
                continue;
            if (llvm::ConstantInt * constIndxVal = llvm::dyn_cast<llvm::ConstantInt>(indxVal))
            {
                if (constIndxVal->isZero())
                    continue;
                else if(constIndxVal->isNegative()) //Match PSUB
                {
                    if (mutationOp.matchOp != mPSUB)
                        continue
                }
                else    //Match PADD
                {
                    if (mutationOp.matchOp != mPADD)
                        continue
                }
            }
            else    //it is a non constant
            {   
                llvm::Instruction *tmpI = llvm::dyn_cast<llvm::Instruction>(indxVal);
                while (llvm::isa<llvm::CastInst>(tmpI))
                    tmpI = llvm::dyn_cast<llvm::User>(tmpI)->getOperand(0)
                if (llvm::isa<llvm::binaryOperator>(tmpI) && (tmpI->getOpcode() == llvm::Instruction::Sub || tmpI->getOpcode() == llvm::Instruction::FSub)) //match PSUB
                {
                    if (mutationOp.matchOp != mPSUB)
                        continue
                }
                else    //match PADD
                {
                    if (mutationOp.matchOp != mPADD)
                        continue
                }
            }
            
            // Make sure the pointer is the right type  TODO: case where PADD (p,c), PADD(c,p), PADD(p,@)
            if (! checkCPTypeInIR (mutationOp.getCPType(0), gep->getpointerOperand()))
                continue;
                    
            int newPos = pos;
            if (indx > 0)
                newPos++;
            
            std::vector<unsigned> posOfIRtoRemove({newPos});
            for (auto &repl: mutationOp.mutantReplacorsList)
            {
                toMatchClone.clear();
                if (repl.first == mDELSTMT)
                {
                    doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, nullptr, nullptr, nullptr);
                }
                else
                {
                    assert ((repl.first != mKEEP_ONE_OPRD || (repl.second.size()==1 && repl.second[0] < 2)) && "Error in the replacor 'mKEEP_ONE_OPRD'");
                    
                    cloneStmtIR (toMatch, toMatchClone);
                    std::vector<llvm::Value> extraIdx;
                    llvm::GetElementPointer * preGep=nullptr, postGep=nullptr;
                    llvm::IRBuilder<> builder(llvm::getGlobalContext());
                    if (indx > 0)
                    {
                        llvm::GetElementPointer * curGI = llvm::dyn_cast<llvm::GetElementPointerInst>(toMatchClone[pos]);
                        extraIdx.clear();
                        for (auto i=0; i<indx;i++)
                            extraIdx.push_back(*(curGI->idx_begin() + i));
                        llvm::GetElementPointer * preGep = builder.CreateInBoundsGEP(nullptr, curGI->getPointerOperand(), extraIdx);
                    }
                    if (indx < gep->getNumIndices()-1)
                    {
                        llvm::GetElementPointer * curGI = llvm::dyn_cast<llvm::GetElementPointerInst>(toMatchClone[pos]);
                        extraIdx.clear();
                        for (auto i=indx+1; i<gep->getNumIndices();i++)
                            extraIdx.push_back(*(curGI->idx_begin() + i));
                        llvm::GetElementPointer * postGep = builder.CreateInBoundsGEP(nullptr, curGI, extraIdx);
                        std::vector<std::pair<llvm::User *, unsigned>> affectedUnO;
                        //set uses of the matched IR to corresponding OPRD
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                        for (llvm::Value::use_iterator ui=curGI->use_begin(), ue=curGI->use_end(); ui!=ue; ++ui)
                        {
                            auto &U = ui.getUse();
                            llvm::User *user = U.getUser();
                            affectedUnO.push_back(std::pair<llvm::User *, unsigned>(user,ui.getOperandNo()));
#else
                        for (auto &U: llvm::dyn_cast<llvm::Instruction>(curGI)->uses())
                        {
                            llvm::User *user = U.getUser();
                            affectedUnO.push_back(std::pair<llvm::User *, unsigned>(user,U.getOperandNo()));
#endif
                        }
                        for(auto &affected: affectedUnO)
                            if (affected.first != postGep)// && std::find(replacement.begin(), replacement.end(), affected.first) == replacement.end())     //avoid 'use loop': a->b->a
                                affected.first->setOperand(affected.second, postGep);
                    }
                    if (postGep)
                        toMatchClone.insert(toMatchClone.begin() + pos + 1, postGep);
                    if (preGep)
                        toMatchClone.insert(toMatchClone.begin() + pos, preGep);
                    llvm::Value * ptroprd = nullptr, *valoprd = nullptr;
                    if (repl.second.size() == 2)
                    {
                        if (preGep)
                            ptroprd = preGep;
                        else
                            ptroprd = llvm::dyn_cast<llvm::GetElementPointer>(toMatchClone[newPos])->getPointerOperand();
                        if (repl.second[1] > llvmMutationOp::maxOprdNum)
                            valoprd = constCreator (indxVal->getType(), repl.second[1]);
                        else
                            valoprd = indxVal;
                    }
                    else    //size is 1
                    {
                        if (repl.second[0] > llvmMutationOp::maxOprdNum)     
                        {   //The replacor should be CONST_VALUE_OF
                            ptroprd = constCreator (indxVal->getType(), repl.second[0]);
                            ptroprd = builder.CreateIntToPtr(ptroprd, preGep? preGep->getType(): llvm::dyn_cast<llvm::GetElementPointer>(toMatchClone[newPos])->getPointerOperand()->getType());
                            if (! llvm::isa<llvm::Constant>(ptroprd))
                                toMatchClone.insert(toMatchClone.begin() + newPos + 1, ptroprd);    //insert right after the instruction to remove
                        }
                        else if (repl.second[0] == 1)   //non pointer
                        {   //The replacor should be either KEEP_ONE_OPRD
                            ptroprd = builder.CreateIntToPtr(indxVal, preGep? preGep->getType(): llvm::dyn_cast<llvm::GetElementPointer>(toMatchClone[newPos])->getPointerOperand()->getType());
                            if (! llvm::isa<llvm::Constant>(ptroprd))
                                toMatchClone.insert(toMatchClone.begin() + newPos + 1, ptroprd);    //insert right after the instruction to remove
                        }
                        else // pointer
                        {
                            if (preGep)
                                ptroprd = preGep;
                            else
                                ptroprd = llvm::dyn_cast<llvm::GetElementPointer>(toMatchClone[newPos])->getPointerOperand();
                        }
                    }
                    doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, ptroprd, valoprd, toMatchClone[newPos], rmPTR);
                }
            }
        }
    }
}

//@ Pointer adding a value x: (p + x)
void matchPADD (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchPADD_SUB (toMatch, mutationOp, resultMuts);
}

//@ Pointer removing a value x: (p - x)
void matchPSUB (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchPADD_SUB (toMatch, mutationOp, resultMuts);
}

//@toMatch: the statement from where to match
//@replacors: list of replacement (<op, list of index of params of mul>)
void matchMUL (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::Mul);
}

//@toMatch: the statement from where to match
//@replacors: list of replacement (<op, list of index of params of fmul>)
void matchFMUL (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::FMul);
}

void matchSDIV (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::SDiv);
}
void matchUDIV (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::UDiv);
}
void matchFDIV (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::FDiv);
}

void matchSMOD (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::SRem);
}
void matchUMOD (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::URem);
}
void matchFMOD (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts) 
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::FRem);
}
void matchBITAND (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::And);//TODO
}
void matchBITOR (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::Or);
}
void matchBITXOR (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::Xor);
}
void matchBITSHIFTLEFT (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::Shl);
}
void matchABITSHIFTRIGHT (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::AShr);
}
void matchLBITSHIFTRIGHT (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)
{
    matchArithBinOp (toMatch, mutationOp, resultMuts, llvm::Instruction::LShr);
}
void matchALLNEGS (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)
{
    std::vector<llvm::Value *> toMatchClone;
    int pos = -1;
    for (auto *val:toMatch)
    {
        pos++;
        
        ///MATCH
        if (llvm::BinaryOperator *neg = llvm::dyn_cast<llvm::BinaryOperator>(val))
        {
            unsigned oprdId;
            if (llvm::Constant *theConst = llvm::dyn_cast<llvm::Constant>(neg->getOperand(0)))
            {
                oprdId = 1;
                if (neg->getOpcode() == llvm::Instruction::Sub)
                {
                    if (mutationOp.matchOp != mNEG || !llvm::dyn_cast<llvm::ConstantInt>(theConst)->equalsInt(0))
                        continue;
                }
                else if (neg->getOpcode() == llvm::Instruction::FSub)
                {
                    if (mutationOp.matchOp != mFNEG || !llvm::dyn_cast<llvm::ConstantFP>(theConst)->isExactlyValue(0.0))
                        continue;
                }
                else if (neg->getOpcode() == llvm::Instruction::Xor)
                {
                    if (mutationOp.matchOp != mBITNOT)
                        continue;
                    if (!llvm::dyn_cast<llvm::ConstantInt>(theConst)->equalsInt(-1)) 
                    {
                        if (llvm::isa<llvm::ConstantInt>(neg->getOperand(1)) && llvm::dyn_cast<llvm::ConstantInt>(neg->getOperand(1))->equalsInt(-1))
                            oprdId = 0;
                        else
                            continue;
                    }
                }
                else
                {
                    continue;
                }
            }
            else if (llvm::Constant *theConst = llvm::dyn_cast<llvm::Constant>(neg->getOperand(1)))
            {
                oprdId = 0;
                if (neg->getOpcode() == llvm::Instruction::Xor)
                {
                    if (mutationOp.matchOp != mBITNOT || !llvm::dyn_cast<llvm::ConstantInt>(theConst)->equalsInt(-1))
                        continue;
                }
                else
                {
                    continue;
                }
            }
            else
            {
                continue;
            }
            
            if (! checkCPTypeInIR (mutationOp.getCPType(0), llvm::dyn_cast<llvm::User>(toMatch[pos])->getOperand(oprdId)))
                continue;
                    
            std::vector<unsigned> posOfIRtoRemove({pos});
            for (auto &repl: mutationOp.mutantReplacorsList)
            {
                toMatchClone.clear();
                if (repl.first == mDELSTMT)
                {
                    doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, nullptr, nullptr, nullptr);
                }
                else
                {
                    assert ((repl.first != mKEEP_ONE_OPRD || (repl.second.size()==1 && repl.second[0] < 2)) && "Error in the replacor 'mKEEP_ONE_OPRD'");
                    
                    cloneStmtIR (toMatch, toMatchClone);
                    llvm::Value * oprdptr[2] = {nullptr, nullptr};
                    for (int i=0; i < repl.second.size(); i++)
                    {
                        if (repl.second[i] > llvmMutationOp::maxOprdNum)    
                        {   //Case where the operand of replacor is a constant
                            oprdptr[i] = constCreator (llvm::dyn_cast<llvm::User>(toMatchClone[pos])->getOperand(oprdId)->getType(), repl.second[i]);
                        }
                        else
                        {
                            oprdptr[i] = llvm::dyn_cast<llvm::User>(toMatchClone[pos])->getOperand(oprdId);
                        }
                    }
                    doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, oprdptr[0], oprdptr[1], toMatchClone[pos]);
                }
            }
        }
    }
}
void matchINC_DEC (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)
{
    std::vector<llvm::Value *> toMatchClone;
    int pos = -1;
    for (auto *val:toMatch)
    {
        pos++;
        
        ///MATCH
        if (llvm::StoreInst *store = llvm::dyn_cast<llvm::StoreInst>(val))
        {
            llvm::Value *addr = store->getOperand(1);
            
            if (llvm::BinaryOperator *modif = llvm::dyn_cast<llvm::BinaryOperator>(store->getOperand(0)))
            {
                llvm::LoadInst *load;
                unsigned notloadat01;
                if(load = llvm::dyn_cast<llvm::LoadInst>(modif->getOperand(0)))
                    notloadat01 = 1;
                else if(load = llvm::dyn_cast<llvm::LoadInst>(modif->getOperand(1)))
                    notloadat01 = 0;
                else
                    continue;
                
                if (load->getOperand(0) != addr)
                    continue;
                
                if (! checkCPTypeInIR (mutationOp.getCPType(0), load))
                    continue;
                                        
                llvm::Constant *constpart = llvm::dyn_cast<llvm::Constant>(modif->getOperand(notloadat01));
                if(!constpart)
                    continue;
                
                if(modif->getOpcode() == llvm::Instruction::Add) 
                {
                    if(llvm::dyn_cast<llvm::ConstantInt>(constpart)->equalsInt(1))
                    {
                        if (mutationOp.matchOp != mLEFTINC && mutationOp.matchOp != mRIGHTINC)
                            continue;
                    }
                    else if (llvm::dyn_cast<llvm::ConstantInt>(constpart)->equalsInt(-1))
                    {
                        if (mutationOp.matchOp != mLEFTDEC && mutationOp.matchOp != mRIGHTDEC)
                            continue;
                    }
                    else
                        continue;
                }
                else if(modif->getOpcode() == llvm::Instruction::FAdd) 
                {
                    if(llvm::dyn_cast<llvm::ConstantFP>(constpart)->isExactlyValue(1.0))
                    {
                        if (mutationOp.matchOp != mFLEFTINC && mutationOp.matchOp != mFRIGHTINC)
                            continue;
                    }
                    else if (llvm::dyn_cast<llvm::ConstantFP>(constpart)->isExactlyValue(-1.0))
                    {
                        if (mutationOp.matchOp != mFLEFTDEC && mutationOp.matchOp != mFRIGHTDEC)
                            continue;
                    }
                    else
                        continue;
                } 
                else if(modif->getOpcode() == llvm::Instruction::Sub) 
                {
                    if (notloadat01 == 0)   //Constants 1 or -1 should always be the right hand oprd
                        continue;
                    if (llvm::dyn_cast<llvm::ConstantInt>(constpart)->equalsInt(-1))
                    {
                        if (mutationOp.matchOp != mLEFTINC && mutationOp.matchOp != mRIGHTINC)
                            continue;
                    }
                    else if (llvm::dyn_cast<llvm::ConstantInt>(constpart)->equalsInt(1))
                    {
                        if (mutationOp.matchOp != mLEFTDEC && mutationOp.matchOp != mRIGHTDEC)
                            continue;
                    }
                    else
                        continue;
                }
                else if(modif->getOpcode() == llvm::Instruction::FSub) 
                {
                    if (notloadat01 == 0)   //Constants 1 or -1 should always be the right hand oprd
                        continue;
                    if(llvm::dyn_cast<llvm::ConstantFP>(constpart)->isExactlyValue(-1.0))
                    {
                        if (mutationOp.matchOp != mFLEFTINC && mutationOp.matchOp != mFRIGHTINC)
                            continue;
                    }
                    else if (llvm::dyn_cast<llvm::ConstantFP>(constpart)->isExactlyValue(1.0)) 
                    {
                        if (mutationOp.matchOp != mFLEFTDEC && mutationOp.matchOp != mFRIGHTDEC)
                            continue;
                    }
                    else
                        continue;
                }
                else
                {
                    continue;
                }
                
                llvm::Value * returningIR;
                
                //check wheter it is left or right inc-dec
                if (load->getNumUses() == 2 && modif->getNumUses() == 1)
                {
                    if (mutationOp.matchOp == mRIGHTINC || mutationOp.matchOp == mFRIGHTINC || mutationOp.matchOp == mRIGHTDEC || mutationOp.matchOp == mFRIGHTDEC)
                        returningIR = load;
                    else
                        continue;
                }
                else if (load->getNumUses() == 1 && modif->getNumUses() == 2)
                {
                    if (mutationOp.matchOp == mLEFTINC || mutationOp.matchOp == mFLEFTINC || mutationOp.matchOp == mLEFTDEC || mutationOp.matchOp == mFLEFTDEC)
                        returningIR = modif;
                    else
                        continue;
                }
                else if (load->getNumUses() == 1 && modif->getNumUses() == 1)
                {   //Here the increment-decrement do not return (this mutants will be duplicate for left and right)
                    /// if (mutationOp.matchOp == mLEFTINC || mutationOp.matchOp == mFLEFTINC || mutationOp.matchOp == mLEFTDEC || mutationOp.matchOp == mFLEFTDEC)
                        returningIR = nullptr;
                    /// else
                    ///    continue;
                }
                else
                {
                    continue;
                }
                
                int loadpos = depPosofPos(toMatch, load, pos);
                int modifpos = depPosofPos(toMatch, modif, pos);
                int returningIRpos = (returningIR == load) ? loadpos: modifpos;
                
                assert ((pos > loadpos && pos > modifpos) && "problem in IR order");
                
                std::vector<unsigned> posOfIRtoRemove({pos, modifpos});
                for (auto &repl: mutationOp.mutantReplacorsList)
                {
                    toMatchClone.clear();
                    if (repl.first == mDELSTMT)
                    {
                        doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, nullptr, nullptr, nullptr);
                    }
                    else
                    {
                        assert ((repl.first != mKEEP_ONE_OPRD || (repl.second.size()==1 && repl.second[0] < 2)) && "Error in the replacor 'mKEEP_ONE_OPRD'");
                        cloneStmtIR (toMatch, toMatchClone);
                        llvm::Value * oprdptr[2] = {nullptr, nullptr};
                        for (int i=0; i < repl.second.size(); i++)
                        {
                            if (repl.second[i] > llvmMutationOp::maxOprdNum)    
                            {   //Case where the operand of replacor is a constant
                                oprdptr[i] = constCreator (toMatchClone[loadpos]->getType(), repl.second[i]);
                            }
                            else
                            {
                                oprdptr[i] = toMatchClone[loadpos];
                            }
                        }
                        doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove,oprdptr[0], oprdptr[1], toMatchClone[returningIRpos]);
                    }
                }
            }
        }
    }
}

//@ Pointer increment-Decrement
void matchPINC_DEC (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)
{
    std::vector<llvm::Value *> toMatchClone;
    int pos = -1;
    for (auto *val:toMatch)
    {
        pos++;
        
        ///MATCH
        if (llvm::StoreInst *store = llvm::dyn_cast<llvm::StoreInst>(val))
        {
            llvm::Value *addr = store->getOperand(1);
            
            if (llvm::GetElementPtrInst *modif = llvm::dyn_cast<llvm::GetElementPtrInst>(store->getOperand(0)))
            {
                if (modif->getNumIndices() != 1)
                    continue;
                llvm::LoadInst *load = llvm::dyn_cast<llvm::LoadInst>(modif->getPointerOperand());
                if (!load)
                    continue;
                int indx;
                llvm::Value *indxVal = checkIsPointerIndexingAndGet(modif, indx);
                if (! indxVal || ! llvm::isa<llvm::ConstantInt>(indxVal))
                    continue;
                llvm::ConstantInt *constpart = llvm::dyn_cast<llvm::ConstantInt>(indxVal);
                if(constpart->equalsInt(1))
                {
                    if (mutationOp.matchOp != mPLEFTINC && mutationOp.matchOp != mPRIGHTINC)
                        continue;
                }
                else if (constpart->equalsInt(-1))
                {
                    if (mutationOp.matchOp != mPLEFTDEC && mutationOp.matchOp != mPRIGHTDEC)
                        continue;
                }
                else
                    continue;
                
                llvm::Value * returningIR;
                
                //check wheter it is left or right inc-dec
                if (load->getNumUses() == 2 && modif->getNumUses() == 1)
                {
                    if (mutationOp.matchOp == mPRIGHTINC || mutationOp.matchOp == mPRIGHTDEC)
                        returningIR = load;
                    else
                        continue;
                }
                else if (load->getNumUses() == 1 && modif->getNumUses() == 2)
                {
                    if (mutationOp.matchOp == mPLEFTINC || mutationOp.matchOp == mPLEFTDEC)
                        returningIR = modif;
                    else
                        continue;
                }
                else if (load->getNumUses() == 1 && modif->getNumUses() == 1)
                {   //Here the increment-decrement do not return (this mutants will be duplicate for left and right)
                    /// if (mutationOp.matchOp == mPRIGHTINC || mutationOp.matchOp == mPRIGHTDEC)
                    returningIR = nullptr;
                    /// else
                    ///     continue;
                }
                else
                {
                    continue;
                }
                
                int loadpos = depPosofPos(toMatch, load, pos);
                int modifpos = depPosofPos(toMatch, modif, pos);
                int returningIRpos = (returningIR == load) ? loadpos: modifpos;
                
                assert ((pos > loadpos && pos > modifpos) && "problem in IR order");
                
                std::vector<unsigned> posOfIRtoRemove({pos, modifpos});
                for (auto &repl: mutationOp.mutantReplacorsList)
                {
                    toMatchClone.clear();
                    if (repl.first == mDELSTMT)
                    {
                        doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, nullptr, nullptr, nullptr, rmPTR);
                    }
                    else
                    {
                        assert ((repl.first != mKEEP_ONE_OPRD || (repl.second.size()==1 && repl.second[0] < 2)) && "Error in the replacor 'mKEEP_ONE_OPRD'");
                        cloneStmtIR (toMatch, toMatchClone);
                        llvm::Value * ptroprd = nullptr, *valoprd = nullptr;
                        if (repl.second.size() == 2)    //size is 2
                        {
                            ptroprd = toMatchClone[loadpos];
                            assert (repl.second[1] > llvmMutationOp::maxOprdNum && "Problem here, must be hard coded constant int here");
                            valoprd = constCreator (indxVal->getType(), repl.second[1]);
                        }
                        else    //size is 1
                        {
                            if (repl.second[0] > llvmMutationOp::maxOprdNum)     
                            {   //The replacor should be CONST_VALUE_OF
                                ptroprd = constCreator (indxVal->getType(), repl.second[0]);
                                ptroprd = builder.CreateIntToPtr(ptroprd, (toMatchClone[returningIRpos])->getType());
                                if (! llvm::isa<llvm::Constant>(ptroprd))
                                    toMatchClone.insert(toMatchClone.begin() + pos + 1, ptroprd);    //insert right after the instruction to remove
                            }
                            else // pointer
                            {
                                ptroprd = toMatchClone[loadpos];
                            }
                        }
                        doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, ptroprd, valoprd, toMatchClone[returningIRpos], rmPTR);
                    }
                }
            }
        }
    }       
}        
        
std::map<enum ExpElemKeys, llvm::CmpInst::Predicate> & getPredRelMap()
{
    static std::map<enum ExpElemKeys, llvm::CmpInst::Predicate> mrel_IRrel_Map;
    if (mrel_IRrel_Map.empty())
    {
        typedef std::pair<enum ExpElemKeys, llvm::CmpInst::Predicate> RelPairType;
        mrel_IRrel_Map.insert(RelPairType (mFCMP_FALSE, llvm::CmpInst::FCMP_FALSE));
        mrel_IRrel_Map.insert(RelPairType (mFCMP_OEQ, llvm::CmpInst::FCMP_OEQ));
        mrel_IRrel_Map.insert(RelPairType (mFCMP_OGT, llvm::CmpInst::FCMP_OGT));
        mrel_IRrel_Map.insert(RelPairType (mFCMP_OGE, llvm::CmpInst::FCMP_OGE));
        mrel_IRrel_Map.insert(RelPairType (mFCMP_OLT, llvm::CmpInst::FCMP_OLT));
        mrel_IRrel_Map.insert(RelPairType (mFCMP_OLE, llvm::CmpInst::FCMP_OLE));
        mrel_IRrel_Map.insert(RelPairType (mFCMP_ONE, llvm::CmpInst::FCMP_ONE));
        mrel_IRrel_Map.insert(RelPairType (mFCMP_ORD, llvm::CmpInst::FCMP_ORD));
        mrel_IRrel_Map.insert(RelPairType (mFCMP_UNO, llvm::CmpInst::FCMP_UNO));
        mrel_IRrel_Map.insert(RelPairType (mFCMP_UEQ, llvm::CmpInst::FCMP_UEQ));
        mrel_IRrel_Map.insert(RelPairType (mFCMP_UGT, llvm::CmpInst::FCMP_UGT));
        mrel_IRrel_Map.insert(RelPairType (mFCMP_UGE, llvm::CmpInst::FCMP_UGE));
        mrel_IRrel_Map.insert(RelPairType (mFCMP_ULT, llvm::CmpInst::FCMP_ULT));
        mrel_IRrel_Map.insert(RelPairType (mFCMP_ULE, llvm::CmpInst::FCMP_ULE));
        mrel_IRrel_Map.insert(RelPairType (mFCMP_UNE, llvm::CmpInst::FCMP_UNE));
        mrel_IRrel_Map.insert(RelPairType (mFCMP_TRUE, llvm::CmpInst::FCMP_TRUE));
        mrel_IRrel_Map.insert(RelPairType (mICMP_EQ, llvm::CmpInst::ICMP_EQ));
        mrel_IRrel_Map.insert(RelPairType (mICMP_NE, llvm::CmpInst::ICMP_NE));
        mrel_IRrel_Map.insert(RelPairType (mICMP_UGT, llvm::CmpInst::ICMP_UGT));
        mrel_IRrel_Map.insert(RelPairType (mICMP_UGE, llvm::CmpInst::ICMP_UGE));
        mrel_IRrel_Map.insert(RelPairType (mICMP_ULT, llvm::CmpInst::ICMP_ULT));
        mrel_IRrel_Map.insert(RelPairType (mICMP_ULE, llvm::CmpInst::ICMP_ULE));
        mrel_IRrel_Map.insert(RelPairType (mICMP_SGT, llvm::CmpInst::ICMP_SGT));
        mrel_IRrel_Map.insert(RelPairType (mICMP_SGE, llvm::CmpInst::ICMP_SGE));
        mrel_IRrel_Map.insert(RelPairType (mICMP_SLT, llvm::CmpInst::ICMP_SLT));
        mrel_IRrel_Map.insert(RelPairType (mICMP_SLE, llvm::CmpInst::ICMP_SLE));
        assert (mrel_IRrel_Map.size() == 26 && "Error Inserting some element into mrel_IRrel_Map");
    }
    
    return mrel_IRrel_Map;
}

void matchRELATIONALS (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)
{
    const std::map<enum ExpElemKeys, llvm::CmpInst::Predicate> &mrel_IRrel_Map = getPredRelMap();
    
    std::vector<llvm::Value *> toMatchClone;
    int pos = -1;
    for (auto *val:toMatch)
    {
        pos++;
        
        ///MATCH
        if (llvm::CmpInst *cmp = llvm::dyn_cast<llvm::CmpInst>(val))
        {
            if (!(checkCPTypeInIR (mutationOp.getCPType(0), cmp->getOperand(0)) && checkCPTypeInIR (mutationOp.getCPType(1), cmp->getOperand(1))))
                continue;
                    
            auto irPred = mrel_IRrel_Map.at(mutationOp.matchOp);
            
            if (cmp->getPredicate() != irPred)
                continue;
                
            llvm::Constant *zero;
            llvm::CmpInst::Predicate neqPredHere;
            if (llvm::CmpInst::isIntPredicate(irPred))
            {
                zero = llvm::ConstantInt::get(cmp->getOperand(1)->getType(), 0);
                neqPredHere = mrel_IRrel_Map.at(mICMP_NE);
            }
            else if (llvm::CmpInst::isFPPredicate(irPred))
            {
                zero = llvm::ConstantFP::get(cmp->getOperand(1)->getType(), 0);
                neqPredHere = mrel_IRrel_Map.at(llvm::CmpInst::isOrdered(irPred)? mFCMP_ONE: mFCMP_UNE);
            }
            else
                assert (false && "Invalid Predicate for CMP");
            
            //std::map<enum ExpElemKeys, llvm::CmpInst::Predicate>::iterator map_it;
            std::vector<unsigned> posOfIRtoRemove({pos});    //the position where dumb will occupy (bellow)
            for (auto &repl: mutationOp.mutantReplacorsList)
            {
                toMatchClone.clear();
                if (repl.first == mDELSTMT)
                {
                    doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, nullptr, nullptr, nullptr);    //posOfIRtoRemove passed just function argument template
                }
                else
                {
                    cloneStmtIR (toMatch, toMatchClone);
                    std::map<enum ExpElemKeys, llvm::CmpInst::Predicate>::const_iterator map_it = mrel_IRrel_Map.find(repl.first);
                    if (map_it != mrel_IRrel_Map.end())
                    {   //directly replace the Predicate in case the replacor is also a CMP instruction
                        // NO CALL TO doReplacement HERE
                        llvm::dyn_cast<llvm::CmpInst>(toMatchClone[pos])->setPredicate(map_it->second);
                        llvm::Value *oprdptr[2];
                        oprdptr[0] = llvm::dyn_cast<llvm::User>(toMatchClone[pos])->getOperand(0);
                        oprdptr[1] = llvm::dyn_cast<llvm::User>(toMatchClone[pos])->getOperand(1);
                        llvm::dyn_cast<llvm::User>(toMatchClone[pos])->setOperand(0, oprdptr[repl.second[0]]);
                        llvm::dyn_cast<llvm::User>(toMatchClone[pos])->setOperand(1, oprdptr[repl.second[1]]);
                        resultMuts.push_back(toMatchClone); 
                        toMatchClone.clear();
                    }
                    else
                    {
                        assert ((repl.first != mKEEP_ONE_OPRD || (repl.second.size()==1 && repl.second[0] < 2)) && "Error in the replacor 'mKEEP_ONE_OPRD'");
                        
                        llvm::Value * oprdptr[2] = {nullptr, nullptr};
                        for (int i=0; i < repl.second.size(); i++)
                        {
                            if (repl.second[i] > llvmMutationOp::maxOprdNum)    
                            {   //Case where the operand of replacor is a constant
                                oprdptr[i] = constCreator (llvm::dyn_cast<llvm::User>(toMatchClone[pos])->getOperand(0)->getType(), repl.second[i]);
                            }
                            else
                            {
                                oprdptr[i] = llvm::dyn_cast<llvm::User>(toMatchClone[pos])->getOperand(repl.second[i]);  //0 and 1
                            }
                        }
                        
                        //Transform (x R y) to (Dumb != 0)   //Use bitcast instruction for dumb instead
                        llvm::Instruction * dumb = llvm::BinaryOperator::Create(llvm::Instruction::Add, oprdptr[0], oprdptr[1]);
                        llvm::dyn_cast<llvm::User>(toMatchClone[pos])->setOperand(0, dumb);
                        llvm::dyn_cast<llvm::User>(toMatchClone[pos])->setOperand(1, zero);
                        llvm::dyn_cast<llvm::CmpInst>(toMatchClone[pos])->setPredicate(neqPredHere);
                        toMatchClone.insert(toMatchClone.begin() + pos, dumb);
                        doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, oprdptr[0], oprdptr[1], dumb);
                    }
                }
            }
        }
    }
}

//XXX: For now AND replace only with OR and vice-versa
void matchAND_OR (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)
{
    std::vector<llvm::Value *> toMatchClone;
    int pos = -1;
    for (auto *val:toMatch)
    {
        pos++;
        
        ///MATCH
        if (llvm::BranchInst *br = llvm::dyn_cast<llvm::BranchInst>(val))
        {
            if (br->isUnconditional())
                continue;
            
            //if (!(checkCPTypeInIR (mutationOp.getCPType(0), cmp->getOperand(0)) && checkCPTypeInIR (mutationOp.getCPType(1), cmp->getOperand(1))))
            //    continue;
            
            assert(br->getNumSuccessors() == 2 && "conditional break should have 2 successors");
            
            llvm::BasicBlock *thenBB=nullptr;
            llvm::BasicBlock *elseBB=nullptr;
            llvm::BasicBlock *shortCircuitBB=nullptr;
            int shortCircuitSID = -1;
            int thenSID = -1;
            int elseSID = -1;
            if (mutationOp.matchOp == mAND)
            {
                shortCircuitBB = br->getSuccessor(0);
                shortCircuitSID = 0;
                elseBB = br->getSuccessor(1);
                elseSID = 1;
            }
            else if (mutationOp.matchOp == mOR)
            {
                shortCircuitBB = br->getSuccessor(1);
                shortCircuitSID = 1;
                thenBB = br->getSuccessor(0);
                thenSID = 0;
            }
            else
            {
                assert(false && "Unreachable: unexpected Instr (expect mAND or mOR)");
            }
                
            std::vector<llvm::BranchInst *> brs;
            std::queue<llvm::BasicBlock *> bbtosee;
            std::set<llvm::BasicBlock *> seenbb;
            
            bbtosee.push(shortCircuitBB);
            seenbb.insert(shortCircuitBB);
            while (!bbtosee.empty())
            {
                auto *curBB = bbtosee.front();
                bbtosee.pop();
                for (auto &popInstr: *curBB)
                {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                    for (llvm::Value::use_iterator ui=popInstr.use_begin(), ue=popInstr.use_end(); ui!=ue; ++ui)
                    {
                        auto &U = ui.getUse();
#else
                    for (auto &U: popInstr.uses())
                    {
#endif
                        llvm::Instruction *uInst = llvm::dyn_cast<llvm::Instruction>(U.getUser());
                        assert (uInst && "Wrong user in module (user is not of type Instruction)");
                        if (seenbb.count(uInst->getParent()) == 0)
                        {
                            seenbb.insert(uInst->getParent());
                            bbtosee.push(uInst->getParent());
                        }
                        if (auto *buInst = llvm::dyn_cast<llvm::BranchInst>(uInst))
                        {
                            brs.push_back(buInst);
                        }
                    }
                }
            }
            llvm::BranchInst *searchedBr = nullptr;
            for (auto *brF: brs)
            {
                if (brF->isUnconditional())
                {
                    if (seenbb.count(brF->getSuccessor(0)) == 0)
                    {
                        searchedBr = nullptr;
                        break;
                    }
                }
                else
                {
                    if (seenbb.count(brF->getSuccessor(0)) == 0 && seenbb.count(brF->getSuccessor(1)) == 0)
                    {
                        if (searchedBr)
                        {
                            searchedBr = nullptr;
                            break;
                        }
                        else
                            searchedBr = brF;
                    }
                    else if(seenbb.count(brF->getSuccessor(0)) == 0 || seenbb.count(brF->getSuccessor(1)) == 0)
                    {
                        searchedBr = nullptr;
                        break;
                    }
                }
            }
            
            if (! searchedBr)
                continue;
            
            if (mutationOp.matchOp == mAND)
            {
                if (elseBB == searchedBr->getSuccessor(1))
                    thenBB = searchedBr->getSuccessor(0);
                //else if (elseBB == searchedBr->getSuccessor(0))
                //    thenBB = searchedBr->getSuccessor(1);
                else
                    continue;
            }
            else if (mutationOp.matchOp == mOR)
            {
                if (thenBB == searchedBr->getSuccessor(0))
                    elseBB = searchedBr->getSuccessor(1);
                //else if (thenBB == searchedBr->getSuccessor(1))
                //    elseBB = searchedBr->getSuccessor(0);
                else
                    continue;
            }
            else
            {
                assert(false && "Unreachable: unexpected Instr (expect mAND or mOR)");
            }
                
            
            std::vector<unsigned> posOfIRtoRemove({pos});    
            for (auto &repl: mutationOp.mutantReplacorsList)
            {
                toMatchClone.clear();
                if (repl.first == mDELSTMT)
                {
                    doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, nullptr, nullptr, nullptr);
                }
                else
                {
                    cloneStmtIR (toMatch, toMatchClone);
                    if (repl.first == mAND || repl.first == mOR || repl.first == mKEEP_ONE_OPRD)
                    {   //directly replace
                        // NO CALL TO doReplacement HERE
                        llvm::BranchInst * clonedBr = llvm::dyn_cast<llvm::BranchInst>(toMatchClone[pos]);
                        switch (repl.first)
                        {
                            case mAND:
                            {
                                assert (mutationOp.matchOp == mOR && "only 'or' can change to 'and' here");
                                clonedBr->setSuccessor(shortCircuitSID, elseBB);
                                clonedBr->setSuccessor(thenSID, shortCircuitBB);
                                break;
                            }
                            case mOR:
                            {
                                assert (mutationOp.matchOp == mAND && "only 'and' can change to 'or' here");
                                clonedBr->setSuccessor(shortCircuitSID, thenBB);
                                clonedBr->setSuccessor(elseSID, shortCircuitBB);
                                break;
                            }
                            case mKEEP_ONE_OPRD:
                            {
                                if (mutationOp.matchOp == mAND)
                                {
                                    if (repl.second[0] == 0)
                                        clonedBr->setSuccessor(shortCircuitSID, thenBB);
                                    else if (repl.second[0] == 1)
                                        clonedBr->setSuccessor(elseSID, shortCircuitBB);
                                    else
                                        assert (false && "Invalid arg for mKEEP_ONE_OPRD when AND");
                                }
                                else if (mutationOp.matchOp == mOR)
                                {
                                    if (repl.second[0] == 0)
                                        clonedBr->setSuccessor(shortCircuitSID, elseBB);
                                    else if (repl.second[0] == 1)
                                        clonedBr->setSuccessor(thenSID, shortCircuitBB);
                                    else
                                        assert (false && "Invalid arg for mKEEP_ONE_OPRD when OR");
                                }
                                else
                                    assert (false && "Unreachable -- Or and And only supported here");
                                break;
                            }
                            default:
                                assert (false && "Unreachable");
                        }
                        resultMuts.push_back(toMatchClone); 
                        toMatchClone.clear();
                    }
                    else
                    {
                        assert (false && "Unsuported yet");
                        /*cloneStmtIR (toMatch, toMatchClone);
                        llvm::Value *oprdptr1 = llvm::dyn_cast<llvm::User>(toMatchClone[pos])->getOperand(repl.second[0]);
                        llvm::Value *oprdptr2 = llvm::dyn_cast<llvm::User>(toMatchClone[pos])->getOperand(repl.second[1]);
                        
                        //Transform (x R y) to (Dumb != 0)
                        llvm::Instruction * dumb = llvm::BinaryOperator::Create(llvm::Instruction::Add, nullptr, nullptr);
                        llvm::dyn_cast<llvm::User>(toMatchClone[pos])->setOperand(0, dumb);
                        llvm::dyn_cast<llvm::User>(toMatchClone[pos])->setOperand(1, zero);
                        toMatchClone.push_back(dumb);
                        doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, oprdptr1, oprdptr2, dumb);*/
                    }
                }
            }
        }
    }    
}

int depPosofPos (std::vector<llvm::Value *> &toMatch, llvm::Value *irinst, int pos, firstCheckBefore=true)
{
    int findpos;
    if (firstCheckBefore)
    {
        findpos = pos-1;
        for (; findpos>=0; findpos--)
            if (toMatch[findpos] == irinst)
                break;
        if (findpos<0)
        {
            for (findpos=pos+1; findpos<toMatch.size(); findpos++)
                if (toMatch[findpos] == irinst)
                    break;
            assert (toMatch.size() > findpos && "Impossible error (before)");
        }
    else
    {
        findpos = pos+1;
        for (; findpos<toMatch.size(); findpos++)
            if (toMatch[findpos] == irinst)
                break;
        if (findpos>=toMatch.size())
        {
            for (findpos=pos-1; findpos>=0; findpos--)
                if (toMatch[findpos] == irinst)
                    break;
            assert (findpos>=0 && "Impossible error (after)");
        }
    }
    return findpos;
}

void doReplacement (std::vector<llvm::Value *> &toMatch, std::vector<std::vector<llvm::Value *>> &resultMuts, std::pair<enum ExpElemKeys, std::vector<unsigned>> &repl, 
                    std::vector<llvm::Value *> &toMatchClone, std::vector<unsigned> &posOfIRtoRemove, llvm::Value *lhOprdptr, llvm::Value *rhOprdptr, llvm::Value *returningIR,
                    enum replacementModes repMode = rmNUMVAL)
{
    ///REPLACE    
    if (repl.first == mDELSTMT)
    {
        static llvm::Value *prevStmt = nullptr;
        assert (toMatchClone.empty() && "toMatchClone should be empty for delete");
        if (prevStmt != toMatch.front())  //avoid multiple deletion of same stmt
        {
            resultMuts.push_back(std::vector<llvm::Value *>());
            prevStmt = toMatch.front();
        }
    }
    else
    {
        std::vector<llvm::Value *> replacement;
        
        assert ((repl.first != mKEEP_ONE_OPRD || (repl.second.size()==1 && repl.second[0] < 2)) && "Error in the replacor 'mKEEP_ONE_OPRD'");
        
        llvm::Value * createdRes;
        switch (repMode)
        {
            case rmNUMVAL:
                createdRes = allCreators(repl.first, lhOprdptr, rhOprdptr, replacement);
                break;
            case rmPTR:
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)  
                llvm::DataLayout DL(toMatch.back()->getParent()->getParent()->getParent());    //TODO: Optimize this with static (changing when toMatch changes)
#else
                llvm::DataLayout DL(toMatch.back()->getModule());
#endif
                createdRes = allPCreators(repl.first, lhOprdptr, rhOprdptr, replacement, DL);
                break;
            case rmDEREF:
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                llvm::DataLayout DL(toMatch.back()->getParent()->getParent()->getParent());    //TODO: Optimize this with static (changing when toMatch changes)
#else
                llvm::DataLayout DL(toMatch.back()->getModule());
#endif
                createdRes = allPDerefCreators(repl.first, lhOprdptr, rhOprdptr, replacement, DL);
                break;
            default:
                assert (false && "Unreachable (repMode)");
        
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            //IRFlags non existant for these verisons
#else
        //Copy Flags if binop
        if (posOfIRtoRemove.size()==1 && toMatchClone[posOfIRtoRemove.front()] && llvm::isa<llvm::BinaryOperator>(toMatchClone[posOfIRtoRemove.front()]))
        {
            for (auto rinst: replacement)
            {
                if (auto * risbinop = llvm::dyn_cast<llvm::BinaryOperator>(rinst))
                    risbinop->copyIRFlags(toMatchClone[posOfIRtoRemove.front()]);
            }
        }
#endif
        
        std::vector<std::pair<llvm::User *, unsigned>> affectedUnO;
        //set uses of the matched IR to corresponding OPRD
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
        for (llvm::Value::use_iterator ui=returningIR->use_begin(), ue=returningIR->use_end(); ui!=ue; ++ui)
        {
            auto &U = ui.getUse();
            llvm::User *user = U.getUser();
            affectedUnO.push_back(std::pair<llvm::User *, unsigned>(user,ui.getOperandNo()));
            //user->setOperand(ui.getOperandNo(), createdRes);
#else
        for (auto &U: llvm::dyn_cast<llvm::Instruction>(returningIR)->uses())
        {
            llvm::User *user = U.getUser();
            affectedUnO.push_back(std::pair<llvm::User *, unsigned>(user,U.getOperandNo()));
            //user->setOperand(U.getOperandNo(), createdRes);
#endif
            //Avoid infinite loop because of setoperand ...
            //if (llvm::dyn_cast<llvm::Instruction>(returningIR)->getNumUses() <= 0) 
            //    break;
        }
        for(auto &affected: affectedUnO)
            if (affected.first != createdRes && std::find(replacement.begin(), replacement.end(), affected.first) == replacement.end())     //avoid 'use loop': a->b->a
                affected.first->setOperand(affected.second, createdRes);
            
        std::sort(posOfIRtoRemove.begin(), posOfIRtoRemove.end());
        for (int i=posOfIRtoRemove.size()-1; i >= 0; i--)
        {
            if (toMatchClone[posOfIRtoRemove[i]] && !llvm::isa<llvm::Constant>(toMatchClone[posOfIRtoRemove[i]]))
                delete toMatchClone[posOfIRtoRemove[i]];
            toMatchClone.erase(toMatchClone.begin() + posOfIRtoRemove[i]);
        }
        if (! replacement.empty())
            toMatchClone.insert(toMatchClone.begin() + posOfIRtoRemove[0], replacement.begin(), replacement.end());
        resultMuts.push_back(toMatchClone); 
    }
    
    toMatchClone.clear();
}

// Called function replcement
void matchCALL (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)
{
    std::vector<llvm::Value *> toMatchClone;
    int pos = -1;
    for (auto *val:toMatch)
    {
        pos++;
        
        ///MATCH
        if (llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(val))
        {
            llvm::Function *cf = call->getCalledFunction();
            for (auto &repl: mutationOp.mutantReplacorsList)
            {
                if (repl.first == mDELSTMT)
                {
                    toMatchClone.clear();
                    std::vector<unsigned> posOfIRtoRemove;  //just for prototype
                    doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, nullptr, nullptr, nullptr);
                }
                else
                {
                    assert (repl.first == mNEWCALLEE && "Error: CALL should have only 'NEWCALLEE as replacor'");
                    assert (repl.second[0] > llvmMutationOp::maxOprdNum);  //repl.second[0,1,...] should be >  llvmMutationOp::maxOprdNum
                    if (cf->getName() == llvmMutationOp::getConstValueStr(repl.second[0]))    //'repl.second[0]' is it the function to match?
                    {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                        llvm::Module * curModule = call->getParent()->getParent()->getParent();
#else
                        llvm::Module * curModule = call->getModule();
#endif
                        for (auto i=1; i < repl.second.size(); i++)
                        {
                            toMatchClone.clear();
                            cloneStmtIR (toMatch, toMatchClone);
                            llvm::Function *repFun = curModule->getFunction(llvmMutationOp::getConstValueStr(repl.second[i]));
                            if (!repFun)
                            {
                                llvm::errs() << "\n# Error: Replcaing function not found in module: " << llvmMutationOp::getConstValueStr(repl.second[i]) << "\n";
                                assert (false);
                            }
                            // Make sure that the two functions are compatible
                            if (cf->getFunctionType() != repFun->getFunctionType())
                            {
                                llvm::errs() << "\n#Error: the function to be replaced and de replacing are of different types:" << cf->getName() << " and " << repFun->getName() << "\n";
                                assert (false);
                            }                                
                            llvm::dyn_cast<llvm::CallInst>(toMatchClone[pos])->setCalledFunction(repFun);
                            resultMuts.push_back(toMatchClone); 
                        }
                    }
                }
            }
        }
    }
}

//
void matchRETURN_BREAK_CONTINUE (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)
{
    static llvm::Function *prevFunc = nullptr;
    static const llvm::Value *structRetUse = nullptr;
    //Here toMatch should have only one elem for 'br' and for ret, the last elem should be 'ret'
    llvm::Instruction * retbr = llvm::dyn_cast<llvm::Instruction>(toMatch.back());
    std::vector<llvm::Value *> toMatchClone;
    assert (mutationOp.mutantReplacorsList.size() == 1 && "Return, Break and Continue can only be deleted");
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
    llvm::Function *curFunc = retbr->getParent()->getParent(); 
#else
    llvm::Function *curFunc = retbr->getFunction();
#endif
    if (prevFunc != curFunc)
    {
        prevFunc = curFunc;
        assert (!structRetUse && "Error, Maybe the return struct vas not mutated for perv funtion!");
        // Check whether the function returns a struct
        if (curFunc->getReturnType()->isVoidTy())
        {
            for (llvm::Function::const_arg_iterator II = curFunc->arg_begin(), EE = curFunc->arg_end(); II != EE; ++II)
            {
                const llvm::Argument *param = &*II;
                if (param->hasStructRetAttr())
                {
                    structRetUse = param;
                    break;
                }
            }
        }
    }
    if(structRetUse && toMatch.size() == 3)     //1: bitcast dests, 2: bitcast src, 3: copy src to dest (llvm.memcpy)
    {
        bool found = false;
        for (auto *ins: toMatch)
            if (llvm::isa<llvm::BitCastInst>(ins) && llvm::dyn_cast<llvm::User>(ins)->getOperand(0) == structRetUse)
                found = true;
        if (found)
        {
            for (auto &repl: mutationOp.mutantReplacorsList)
            {
                if (repl.first == mDELSTMT)
                {
                    resultMuts.push_back(std::vector<llvm::Value *>());
                }
                else
                    assert ("The replacement for return break and continue should be delete stmt");
            }
        }
    }
    
    if (auto *br = llvm::dyn_cast<llvm::BranchInst>(retbr))
    {
        if (br->isConditional())
            return;
        assert (toMatch.size() == 1 && "unconditional break should be the only instruction in the statement");
        llvm::BasicBlock *targetBB = br->getSuccessor(0);
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
        llvm::Function::iterator FI = br->getParent()->getNextNode();//Iterator();
        if (FI) //!= br->getFunction()->end() && FI )
        {
#else
        llvm::Function::iterator FI = br->getParent()->getIterator();
        FI++;
        if (FI != br->getFunction()->end() && FI )
        {
#endif
            llvm::BasicBlock *followBB = &*FI;
            if (followBB != targetBB)
            {
                for (auto &repl: mutationOp.mutantReplacorsList)
                {
                    if (repl.first == mDELSTMT)
                    {
                        toMatchClone.clear();
                        cloneStmtIR (toMatch, toMatchClone);
                        llvm::dyn_cast<llvm::BranchInst>(toMatchClone.back())->setSuccessor(0, followBB);
                        resultMuts.push_back(toMatchClone);
                    }
                    else
                        assert ("The replacement for return break and continue should be delete stmt");
                }
            }
        }
    }
    else if (auto *ret = llvm::dyn_cast<llvm::ReturnInst>(retbr))
    {
        if (ret->getReturnValue() == nullptr)   //void type
            return;
            
        llvm::Type *retType = curFunc->getReturnType();
        llvm::IRBuilder<> builder(llvm::getGlobalContext());
        //Case of main (delete lead to ret 0 - integer)
        if (curFunc->getName().equals(G_MAIN_FUNCTION_NAME))
        {
            assert (retType->isIntegerTy() && "Main function must return an integer");
            for (auto &repl: mutationOp.mutantReplacorsList)
            {
                if (repl.first == mDELSTMT)
                {
                    toMatchClone.clear();
                    llvm::ReturnInst *newret = builder.CreateRet(llvm::ConstantInt::get(retType, 0));
                    toMatchClone.push_back(newret);
                    resultMuts.push_back(toMatchClone);
                }
                else
                    assert ("The replacement for return break and continue should be delete stmt");
            }
        }
        else if (retType->isIntOrIntVectorTy() || retType->isFPOrFPVectorTy () || retType->isPtrOrPtrVectorTy())
        {//Case of primitive and pointer(delete lead to returning of varable value without reaching definition - uninitialized)
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            llvm::DataLayout DL(ret->getParent()->getParent()->getParent());
#else
            llvm::DataLayout DL(ret->getModule());
#endif
            for (auto &repl: mutationOp.mutantReplacorsList)
            {
                if (repl.first == mDELSTMT)
                {
                    toMatchClone.clear();
                    llvm::AllocaInst *alloca = builder.CreateAlloca(retType);
                    alloca->setAlignment( DL.getTypeStoreSize(retType));
                    toMatchClone.push_back(alloca);
                    llvm::LoadInst *load = builder.CreateAlignedLoad(alloca, DL.getTypeStoreSize(retType));
                    toMatchClone.push_back(load);
                    llvm::ReturnInst *newret = builder.CreateRet(load);
                    toMatchClone.push_back(newret);
                    resultMuts.push_back(toMatchClone);
                }
                else
                    assert ("The replacement for return val should be delete stmt");
            }
        }
    }
}

void matchPADDSUB_DEREF (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)  // *(p+1)
{
    int pos = -1;
    bool stmtDeleted = false;
    for (auto *val:toMatch)
    {
        pos++;
        
        ///MATCH
        if (auto *gep = llvm::dyn_cast<llvm::GetElementPtrInst>(val))
        {
            // check the pointer displacement index (0)'s value
            int indx;
            llvm::Value *indxVal = checkIsPointerIndexingAndGet(gep, indx);
            if (! indxVal)
                continue;
            if (llvm::ConstantInt * constIndxVal = llvm::dyn_cast<llvm::ConstantInt>(indxVal))
            {
                if (constIndxVal->isZero())
                    continue;
                else if(constIndxVal->isNegative()) //Match PSUB
                {
                    if (mutationOp.matchOp != mPSUB_DEREF)
                        continue
                }
                else    //Match PADD
                {
                    if (mutationOp.matchOp != mPADD_DEREF)
                        continue
                }
            }
            else    //it is a non constant
            {   
                llvm::Instruction *tmpI = llvm::dyn_cast<llvm::Instruction>(indxVal);
                while (llvm::isa<llvm::CastInst>(tmpI))
                    tmpI = llvm::dyn_cast<llvm::User>(tmpI)->getOperand(0)
                if (llvm::isa<llvm::binaryOperator>(tmpI) && (tmpI->getOpcode() == llvm::Instruction::Sub || tmpI->getOpcode() == llvm::Instruction::FSub)) //match PSUB
                {
                    if (mutationOp.matchOp != mPSUB_DEREF)
                        continue
                }
                else    //match PADD
                {
                    if (mutationOp.matchOp != mPADD_DEREF)
                        continue
                }
            }
            
            std::vector<int> derefPosVect;
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
            for (llvm::Value::use_iterator ui=gep->use_begin(), ue=gep->use_end(); ui!=ue; ++ui)
            {
                auto &U = ui.getUse();
#else
            for (auto &U: llvm::dyn_cast<llvm::Instruction>(gep)->uses())
            {
#endif
                llvm::LoadInst *loadDeref = llvm::dyn_cast<llvm::LoadInst>(U.getUser());
                if (loadDeref)
                {
                    int derefPosTmp = depPosofPos (toMatch, loadDeref, pos, false /*search first after*/);
                    derefPosVect.push_back(derefPosTmp); 
                }
            }
            
            for (int derefPos: derefPosVect)
            {
                
                // Make sure the pointer is the right type  TODO: case where PADD (p,c), PADD(c,p), PADD(p,@)
                if (! checkCPTypeInIR (mutationOp.getCPType(0), gep->getpointerOperand()))
                    continue;
                
                assert (derefPos > pos && "use before definition ??");
                        
                int newPos = pos;
                if (indx > 0)
                    newPos++;
                    derefPos++;
                    
                if (indx < gep->getNumIndices()-1)
                    derefPos++;
                
                std::vector<unsigned> posOfIRtoRemove({newPos, derefPos});
                for (auto &repl: mutationOp.mutantReplacorsList)
                {
                    toMatchClone.clear();
                    if (repl.first == mDELSTMT)
                    {
                        doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, nullptr, nullptr, nullptr);
                    }
                    else
                    {
                        assert ((repl.first != mKEEP_ONE_OPRD && repl.first != mCONST_VALUE_OF /*|| (repl.second.size()==1 && repl.second[0] < 2)*/) && "Repl can't be 'mKEEP_ONE_OPRD' or 'CONST_OF'");
                        
                        cloneStmtIR (toMatch, toMatchClone);
                        std::vector<llvm::Value> extraIdx;
                        llvm::GetElementPointer * preGep=nullptr, postGep=nullptr;
                        llvm::IRBuilder<> builder(llvm::getGlobalContext());
                        if (indx > 0)
                        {
                            llvm::GetElementPointer * curGI = llvm::dyn_cast<llvm::GetElementPointerInst>(toMatchClone[pos]);
                            extraIdx.clear();
                            for (auto i=0; i<indx;i++)
                                extraIdx.push_back(*(curGI->idx_begin() + i));
                            llvm::GetElementPointer * preGep = builder.CreateInBoundsGEP(nullptr, curGI->getPointerOperand(), extraIdx);
                        }
                        if (indx < gep->getNumIndices()-1)
                        {
                            llvm::GetElementPointer * curGI = llvm::dyn_cast<llvm::GetElementPointerInst>(toMatchClone[pos]);
                            extraIdx.clear();
                            for (auto i=indx+1; i<gep->getNumIndices();i++)
                                extraIdx.push_back(*(curGI->idx_begin() + i));
                            llvm::GetElementPointer * postGep = builder.CreateInBoundsGEP(nullptr, curGI, extraIdx);
                            std::vector<std::pair<llvm::User *, unsigned>> affectedUnO;
                            //set uses of the matched IR to corresponding OPRD
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                            for (llvm::Value::use_iterator ui=curGI->use_begin(), ue=curGI->use_end(); ui!=ue; ++ui)
                            {
                                auto &U = ui.getUse();
                                llvm::User *user = U.getUser();
                                affectedUnO.push_back(std::pair<llvm::User *, unsigned>(user,ui.getOperandNo()));
#else
                            for (auto &U: llvm::dyn_cast<llvm::Instruction>(curGI)->uses())
                            {
                                llvm::User *user = U.getUser();
                                affectedUnO.push_back(std::pair<llvm::User *, unsigned>(user,U.getOperandNo()));
#endif
                            }
                            for(auto &affected: affectedUnO)
                                if (affected.first != postGep)// && std::find(replacement.begin(), replacement.end(), affected.first) == replacement.end())     //avoid 'use loop': a->b->a
                                    affected.first->setOperand(affected.second, postGep);
                        }
                        if (postGep)
                            toMatchClone.insert(toMatchClone.begin() + pos + 1, postGep);
                        if (preGep)
                            toMatchClone.insert(toMatchClone.begin() + pos, preGep);
                        llvm::Value * ptroprd = nullptr, *valoprd = nullptr;
                        if (repl.second.size() == 2)
                        {
                            if (preGep)
                                ptroprd = preGep;
                            else
                                ptroprd = llvm::dyn_cast<llvm::GetElementPointer>(toMatchClone[newPos])->getPointerOperand();
                            if (repl.second[1] > llvmMutationOp::maxOprdNum)
                                valoprd = constCreator (indxVal->getType(), repl.second[1]);
                            else
                                valoprd = indxVal;
                        }
                        else    //size is 1
                        {
                            /*if (repl.second[0] > llvmMutationOp::maxOprdNum)     
                            {   //The replacor should be CONST_VALUE_OF
                                ptroprd = constCreator (indxVal->getType(), repl.second[0]);
                                ptroprd = builder.CreateIntToPtr(ptroprd, preGep? preGep->getType(): llvm::dyn_cast<llvm::GetElementPointer>(toMatchClone[newPos])->getPointerOperand()->getType());
                                if (! llvm::isa<llvm::Constant>(ptroprd))
                                    toMatchClone.insert(toMatchClone.begin() + newPos + 1, ptroprd);    //insert right after the instruction to remove
                            }
                            else if (repl.second[0] == 1)   //non pointer
                            {   //The replacor should be either KEEP_ONE_OPRD
                                ptroprd = builder.CreateIntToPtr(indxVal, preGep? preGep->getType(): llvm::dyn_cast<llvm::GetElementPointer>(toMatchClone[newPos])->getPointerOperand()->getType());
                                if (! llvm::isa<llvm::Constant>(ptroprd))
                                    toMatchClone.insert(toMatchClone.begin() + newPos + 1, ptroprd);    //insert right after the instruction to remove
                            }
                            else // pointer
                            {
                                if (preGep)
                                    ptroprd = preGep;
                                else
                                    ptroprd = llvm::dyn_cast<llvm::GetElementPointer>(toMatchClone[newPos])->getPointerOperand();
                            }*/
                            assert (false && "No KEEP_ONE_OPRD ot CONST_OF here");
                        }
                        doReplacement (toMatch, resultMuts, repl, toMatchClone, posOfIRtoRemove, ptroprd, valoprd, toMatchClone[derefPos], rmPTR);
                    }
                }
            }
        }
    }
}


?//TODO TODO : Unfinished...
void matchPDEREF_ADDSUB (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)  // *(p)+1
{
    int pos = -1;
    for (auto *val:toMatch)
    {
        pos++;
        
        ///MATCH
        
        if (auto *deref = llvm::dyn_cast<llvm::LoadInst>(val))
        {
            if (llvm::isa<llvm::AllocaInst>(deref->getPointeroperand()))     //The pointer oprd should not be AllocaInst
                continue;
            
            
            if (llvm::isa<llvm::PointerType>(deref))
            {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                for (llvm::Value::use_iterator ui=deref->use_begin(), ue=deref->use_end(); ui!=ue; ++ui)
                {
                    auto &U = ui.getUse();
#else
                for (auto &U: llvm::dyn_cast<llvm::Instruction>(deref)->uses())
                {
#endif
                    llvm::LoadInst *loadDeref = llvm::dyn_cast<llvm::LoadInst>(U.getUser());
                    if (loadDeref)
                        addsubPos = depPosofPos (toMatch, loadDeref, pos, false /*search first after*/);
                }
            }
            else
            {
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
                for (llvm::Value::use_iterator ui=deref->use_begin(), ue=deref->use_end(); ui!=ue; ++ui)
                {
                    auto &U = ui.getUse();
#else
                for (auto &U: llvm::dyn_cast<llvm::Instruction>(deref)->uses())
                {
#endif
                    int addsubPos = -1;
                    llvm::BinaryOperator *addsub = llvm::dyn_cast<llvm::BinaryOperator>(U.getUser());
                    if (addsub)
                        addsubPos = depPosofPos (toMatch, addsub, pos, false /*search first after*/);
                }
            }
            //TODO check the use of Load (they should be add/sub or gep...)
            //Assume that val has to be of Instruction type
            if (mutationOp.matchOp == mPDEREF_ADD && llvm::dyn_cast<llvm::Instruction>(val)->getOpcode() != llvm::Instruction::Add)
                continue;
            if (mutationOp.matchOp == mPDEREF_SUB && llvm::dyn_cast<llvm::Instruction>(val)->getOpcode() != llvm::Instruction::Sub)
                continue;
                    
            llvm::Instruction *addsub = llvm::dyn_cast<llvm::Instruction>(val);
            
            bool opCPMismatch = false;
            bool atLeastOneLoadorGep = false;
            
            for (auto oprdID=0; oprdID < addsub->getNumOperands(); oprdID++)
            {
                llvm::Value *oprdi = addsub->getOperand(oprdID);
                if (! checkCPTypeInIR (mutationOp.getCPType(oprdID), oprdi))
                {
                    opCPMismatch = true;
                    break;
                }
                while (llvm::isa<llvm::CastInst>(oprdi))
                {
                    oprdi = llvm::dyn_cast<llvm::User>(oprdi)->getOperand(0);
                }
                if (auto * xxload = llvm::dyn_cast<llvm::LoadInst>(oprdi))
                    if (! llvm::isa<llvm::AllocaInst>(xxload->getPointeroperand()))
                        atLeastOneLoadorGep = true;
            }
            if (opCPMismatch || !atLeastOneLoadorGep)
                continue;
        }
    }
}

void matchPINCDEC_DEREF (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)  // *(++p)
{
    
}
void matchPDEREF_INCDEC (std::vector<llvm::Value *> &toMatch, llvmMutationOp &mutationOp, std::vector<std::vector<llvm::Value *>> &resultMuts)  // ++(*p)
{
    
}

/**** ADD HERE ****/

