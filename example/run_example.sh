#! /bin/bash 

set -u
set -e

CLANG_EXE=${CLANG_EXE:-clang}
MART_EXE=${MART_EXE:-mart}

thisdir=$(dirname $(readlink -f $0))
sample_src=sample.c
sample_bc=sample.bc

# remove any possible previous run
rm -rf $thisdir/mart-out-*

# cd into current dir
cd $thisdir

# compile sample into llvm bitcode
$CLANG_EXE -g -c -emit-llvm $sample_src -o $sample_bc

# 1. Generate using all mutant operators and everywhere (Only meta mutants)
echo ">> a. Generating mutants with default operators and everywhere (only meta)"
echo ">>> COMMAND (CWD=$(pwd)): $MART_EXE $sample_bc"
$MART_EXE $sample_bc
echo "# Done Generating (a) into folder $thisdir/mart-out-0"
echo "==========================="; echo

# 2. Generate using all mutant operators and everywhere (Separate mutants also)
echo ">> b. Generating mutants with default operators and everywhere (separate also)"
echo ">>> COMMAND: $MART_EXE -write-mutants $sample_bc"
$MART_EXE -write-mutants $sample_bc
echo "# Done Generating (b) into folder $thisdir/mart-out-1"
echo "==========================="; echo

# 3. Generate using specified mutant operators and only for a single function (meta only)
echo ">> c. Generating mutants with specified operators and a single function (met only)"
echo ">>> COMMAND: $MART_EXE -mutant-config mutant_conf.mconf -mutant-scope mutant_scope.json $sample_bc"
$MART_EXE -mutant-config mutant_conf.mconf -mutant-scope mutant_scope.json $sample_bc
echo "# Done Generating (c) into folder $thisdir/mart-out-2"

echo "==========================="; echo

# 4. Generate using specified mutant operators and only for a single function (meta only), with post generation no compilation
echo ">> d. Generating mutants with specified operators and a single function (met only), with no post generation compilation"
echo ">>> COMMAND: $MART_EXE -mutant-config mutant_conf.mconf -mutant-scope mutant_scope.json -no-compilation $sample_bc"
$MART_EXE -mutant-config mutant_conf.mconf -mutant-scope mutant_scope.json -no-compilation $sample_bc
echo "# Done Generating (d) into folder $thisdir/mart-out-3"

cd - > /dev/null

