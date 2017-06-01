#! /usr/bin/python

import os, sys

def getMConf():
    AOR = ["ADD", "SUB", "MUL", "DIV", "MOD"]
    BIT = ["BITAND", "BITOR", "BITXOR", "BITSHL", "BITSHR"]
    UNARY = ["BITNOT", "NEG", "ABS", "OPERAND"]
    ASSIGN = ["ASSIGN"]
    INCDEC = ["LEFTINC", "LEFTDEC", "RIGHTINC", "RIGHTDEC"]
    ROR = ["EQ", "NEQ", "LT", "GT", "LE", "GE"]
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
