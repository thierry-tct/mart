#! /bin/bash

set -u
set -o pipefail

error_exit()
{
    echo
    echo "@ Error: $1"
    echo
    exit 1
}

TOPDIR=$(dirname $(readlink -f $0))

##### CONFIGURATION #####
#llc=llc-3.4
llc=/media/thierry/TestMutants/DG-dependency/llvm-3.8.0/build/bin/llc
CC=gcc
#########################

[ $# = 2 ] || error_exit "Expected 2 parameters, $# passed: $0 <directory (mull-out-0)> <remove mutants' \".bc\"? yes/no>"

Dir=$(readlink -f $1)
removeMutsBCs=$2
fdupesData=$Dir/"fdupes_duplicates.txt"

test -d $Dir || error_exit "directory $Dir do not exist"
cd $Dir

#Compile the generated mutants
CFLAGS="-lm"    #link with lm because gcc complain linking when fmod mutant is added
for m in `find -type f -name "*.bc"`
do
    $llc -O3 -filetype=obj -o ${m%.bc}.o $m || error_exit "Failed to compile mutant $m to object"
    $CC -O3 -o ${m%.bc} ${m%.bc}.o $CFLAGS || error_exit "Failed to compile mutant $m to executable"
    rm -f ${m%.bc}.o #$m
done

if test -d 'mutants' # && [ `ls 'mutants' | wc -l` -gt 0 ]        #no need to check non empty because the original is alway there
then
    [ "$removeMutsBCs" = "yes" ] && find 'mutants' -type f -name "*.bc" -exec rm -f {} + || error_exit "Failed to remove some mutants .bc files"
    
    which fdupes > /dev/null || error_exit "'fdupes' is not installed (or in the PATH): cannot remove further TCE dup/eq."
    fdupes -r -dN  'mutants' > $fdupesData 
    test -s $fdupesData  && find 'mutants' -type d -empty -exec rm -rf {} +  #Check if some duplicate mutants were found and remove their folders
fi

cd - >/dev/null

