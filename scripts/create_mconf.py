#! /usr/bin/python

import os, sys

def getMConf():
    VALS = ["@", "V", "C", "P"]
  # Match only
    MATCH_ONLY_DEL = ["STMT", "RETURN_BREAK_CONTINUE"]
  # Replace Only
    DELSTMT = ["DELSTMT"]
    OPERAND = ["OPERAND"]
  # Both Match and Replace
    # Arithmetic
    AOR = ["ADD", "SUB", "MUL", "DIV", "MOD"]
    BIT = ["BITAND", "BITOR", "BITXOR", "BITSHL", "BITSHR"]
    UNARY = ["BITNOT", "NEG", "ABS"]
    ASSIGN = ["ASSIGN"]
    INCDEC = ["LEFTINC", "LEFTDEC", "RIGHTINC", "RIGHTDEC"]
    ROR = ["EQ", "NEQ", "LT", "GT", "LE", "GE"]
    LOR = ["AND", "OR"]
    
    # Pointer
    P_AOR = ["PADD", "PSUB"]
    P_INCDEC = ["PLEFTINC", "PLEFTDEC", "PRIGHTINC", "PRIGHTDEC"]
    P_ROR = ["PEQ", "PNEQ", "PLT", "PGT", "PLE", "PGE"]
    
#~ def getMConf()

def main(args):
    assert len(args == 2), "expected 1 argument: output file"
    outfile = args[1]
    
    mconfString = getMConf()
    with open(outfile, "w") as fp:
        fp.write(mconfString)
#~ def main()
   
if __name__ == "__main__":
    main(sys.argv)
