#include "NumericNegation_Base/NumericArithNegation_Base.h"
#include "NumericNegation_Base/operators/IntegerMinus.h"
#include "NumericNegation_Base/operators/FPMinus.h"
#include "NumericNegation_Base/operators/BitNot.h"
#include "ReplaceOnly_Base/ReplaceOnly_Base.h"
#include "ReplaceOnly_Base/operators/IntegerAbs.h"
#include "ReplaceOnly_Base/operators/KeepOneOperand.h"
#include "ReplaceOnly_Base/operators/RemoveCases.h"
#include "ReplaceOnly_Base/operators/ShuffleCaseDestinations.h"
#include "ReplaceOnly_Base/operators/DeleteStatement.h"
#include "ReplaceOnly_Base/operators/ConstantValueOf.h"
#include "ReplaceOnly_Base/operators/FPAbs.h"
#include "ReplaceOnly_Base/operators/NewCallee.h"
#include "ReplaceOnly_Base/operators/ShuffleArgs.h"
#include "PointerIncDec_Base/PointerIncDec_Base.h"
#include "PointerIncDec_Base/operators/PointerLeftDec.h"
#include "PointerIncDec_Base/operators/PointerRightInc.h"
#include "PointerIncDec_Base/operators/PointerLeftInc.h"
#include "PointerIncDec_Base/operators/PointerRightDec.h"
#include "PointerArithBinop_Base/PointerArithBinop_Base.h"
#include "PointerArithBinop_Base/operators/PointerAdd.h"
#include "PointerArithBinop_Base/operators/PointerSub.h"
#include "NumericAssign_Base/NumericAssign_Base.h"
#include "NumericAssign_Base/operators/IntegerAssign.h"
#include "NumericAssign_Base/operators/FPAssign.h"
#include "NumericArithIncDec_Base/NumericArithIncDec_Base.h"
#include "NumericArithIncDec_Base/operators/IntegerLeftInc.h"
#include "NumericArithIncDec_Base/operators/FPRightDec.h"
#include "NumericArithIncDec_Base/operators/IntegerRightInc.h"
#include "NumericArithIncDec_Base/operators/FPRightInc.h"
#include "NumericArithIncDec_Base/operators/FPLeftDec.h"
#include "NumericArithIncDec_Base/operators/IntegerRightDec.h"
#include "NumericArithIncDec_Base/operators/FPLeftInc.h"
#include "NumericArithIncDec_Base/operators/IntegerLeftDec.h"
#include "NumericAndPointerRelational_Base/IntegerRelational_Base/IntegerRelational_Base.h"
#include "NumericAndPointerRelational_Base/IntegerRelational_Base/operators/IntegerNotEqual.h"
#include "NumericAndPointerRelational_Base/IntegerRelational_Base/operators/UIntegerLessThan.h"
#include "NumericAndPointerRelational_Base/IntegerRelational_Base/operators/UIntegerLessOrEqual.h"
#include "NumericAndPointerRelational_Base/IntegerRelational_Base/operators/UIntegerGreaterOrEqual.h"
#include "NumericAndPointerRelational_Base/IntegerRelational_Base/operators/SIntegerLessThan.h"
#include "NumericAndPointerRelational_Base/IntegerRelational_Base/operators/UIntegerGreaterThan.h"
#include "NumericAndPointerRelational_Base/IntegerRelational_Base/operators/SIntegerLessOrEqual.h"
#include "NumericAndPointerRelational_Base/IntegerRelational_Base/operators/SIntegerGreaterOrEqual.h"
#include "NumericAndPointerRelational_Base/IntegerRelational_Base/operators/IntegerEqual.h"
#include "NumericAndPointerRelational_Base/IntegerRelational_Base/operators/SIntegerGreaterThan.h"
#include "NumericAndPointerRelational_Base/FPRelational_Base/FPRelational_Base.h"
#include "NumericAndPointerRelational_Base/FPRelational_Base/operators/OFPNotEqual.h"
#include "NumericAndPointerRelational_Base/FPRelational_Base/operators/UFPLessThan.h"
#include "NumericAndPointerRelational_Base/FPRelational_Base/operators/OFPGreaterThan.h"
#include "NumericAndPointerRelational_Base/FPRelational_Base/operators/UFPEqual.h"
#include "NumericAndPointerRelational_Base/FPRelational_Base/operators/OFPEqual.h"
#include "NumericAndPointerRelational_Base/FPRelational_Base/operators/UFPNotEqual.h"
#include "NumericAndPointerRelational_Base/FPRelational_Base/operators/OFPLessOrEqual.h"
#include "NumericAndPointerRelational_Base/FPRelational_Base/operators/UFPGreaterOrEqual.h"
#include "NumericAndPointerRelational_Base/FPRelational_Base/operators/OFPGreaterOrEqual.h"
#include "NumericAndPointerRelational_Base/FPRelational_Base/operators/OFPLessThan.h"
#include "NumericAndPointerRelational_Base/FPRelational_Base/operators/UFPGreaterThan.h"
#include "NumericAndPointerRelational_Base/FPRelational_Base/operators/UFPLessOrEqual.h"
#include "NumericAndPointerRelational_Base/PointerRelational_Base/operators/PointerEqual.h"
#include "NumericAndPointerRelational_Base/PointerRelational_Base/operators/PointerLessThan.h"
#include "NumericAndPointerRelational_Base/PointerRelational_Base/operators/PointerNotEqual.h"
#include "NumericAndPointerRelational_Base/PointerRelational_Base/operators/PointerLessOrEqual.h"
#include "NumericAndPointerRelational_Base/PointerRelational_Base/operators/PointerGreaterOrEqual.h"
#include "NumericAndPointerRelational_Base/PointerRelational_Base/operators/PointerGreaterThan.h"
#include "NumericAndPointerRelational_Base/PointerRelational_Base/PointerRelational_Base.h"
#include "NumericAndPointerRelational_Base/NumericAndPointerRelational_Base.h"
#include "DerefPointerArithIncDec_Base/DerefPointerArithIncDec_Base.h"
#include "NumericArithBinop_Base/NumericArithBinop_Base.h"
#include "NumericArithBinop_Base/operators/FPMul.h"
#include "NumericArithBinop_Base/operators/BitAnd.h"
#include "NumericArithBinop_Base/operators/FPSub.h"
#include "NumericArithBinop_Base/operators/BitShiftLeft.h"
#include "NumericArithBinop_Base/operators/SignedIntegerMod.h"
#include "NumericArithBinop_Base/operators/IntegerAdd.h"
#include "NumericArithBinop_Base/operators/BitLShiftRight.h"
#include "NumericArithBinop_Base/operators/IntegerMul.h"
#include "NumericArithBinop_Base/operators/FPDiv.h"
#include "NumericArithBinop_Base/operators/BitXor.h"
#include "NumericArithBinop_Base/operators/UnsignedIntegerDiv.h"
#include "NumericArithBinop_Base/operators/FPMod.h"
#include "NumericArithBinop_Base/operators/UnsignedIntegerMod.h"
#include "NumericArithBinop_Base/operators/BitAShiftRight.h"
#include "NumericArithBinop_Base/operators/BitOr.h"
#include "NumericArithBinop_Base/operators/SignedIntegerDiv.h"
#include "NumericArithBinop_Base/operators/FPAdd.h"
#include "NumericArithBinop_Base/operators/IntegerSub.h"
#include "Logical_Base/Logical_Base.h"
#include "Logical_Base/operators/LogicalOr.h"
#include "Logical_Base/operators/LogicalAnd.h"
#include "MatchOnly_Base/MatchOnly_Base.h"
#include "MatchOnly_Base/MemoryPointerAddress_Base/MemoryPointerAddress_Base.h"
#include "MatchOnly_Base/MemoryPointerAddress_Base/operators/MemoryAddressValue.h"
#include "MatchOnly_Base/MemoryPointerAddress_Base/operators/MemoryPointerVariable.h"
#include "MatchOnly_Base/NumericVariable_Base/NumericVariable_Base.h"
#include "MatchOnly_Base/NumericVariable_Base/operators/FPNumericVariable.h"
#include "MatchOnly_Base/NumericVariable_Base/operators/IntNumericVariable.h"
#include "MatchOnly_Base/SpecialCases/SwitchCases.h"
#include "MatchOnly_Base/SpecialCases/ReturnBreakContinue.h"
#include "MatchOnly_Base/SpecialCases/AllStatements.h"
#include "MatchOnly_Base/SpecialCases/FunctionCall.h"
#include "MatchOnly_Base/NumericConstant_Base/NumericConstant_Base.h"
#include "MatchOnly_Base/NumericConstant_Base/operators/FPNumericConstant.h"
#include "MatchOnly_Base/NumericConstant_Base/operators/IntNumericConstant.h"
#include "MatchOnly_Base/NumericExpression_Base/NumericExpression_Base.h"
#include "MatchOnly_Base/NumericExpression_Base/operators/IntNumericExpression.h"
#include "MatchOnly_Base/NumericExpression_Base/operators/FPNumericExpression.h"
#include "DerefPointerArithBinop_Base/DerefPointerArithBinop_Base.h"
#include "GenericMuOpBase.h"