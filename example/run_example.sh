#! /bin/bash 

set -u
set -e

thisdir=$(dirname $(readlink -f $0))
sample_src=sample.c
sample_bc=sample.bc

# remove any possible previous run
rm -rf $thisdir/mart-out-*

# cd into current dir
cd $thisdir

# compile sample into llvm bitcode
clang -g -c -emit-llvm $sample_src $sample_bc

# 1. Generate using all mutant operators and everywhere (Only meta mutants)
echo ">> a. Generating mutants with default operators and everywhere (only meta)"
mart $sample_bc
echo "# Done Generating (a) into folder $thisdir/mart-out-0"

# 2. Generate using all mutant operators and everywhere (Separate mutants also)
echo ">> b. Generating mutants with default operators and everywhere (separate also)"
mart -write-mutants $sample_bc
echo "# Done Generating (b) into folder $thisdir/mart-out-1"

# 3. Generate using specified mutant operators and only for a single function (meta only)
echo ">> c. Generating mutants with specified operators and a single function (met only)"
mart -mutant-config mutant_conf.mconf -mutant-scope mutant_scope.json $sample_bc
echo "# Done Generating (c) into folder $thisdir/mart-out-2"

cd - > /dev/null

