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

[ $# = 3 ] || error_exit "Expected 3 parameters, $# passed: $0 <directory (mull-out-0)> <tmpFuncModuleFolder> <remove mutants' \".bc\"? yes/no>"

Dir=$(readlink -f $1)
tmpFuncModuleFolder=$2
removeMutsBCs=$3
fdupesData=$Dir/"fdupes_duplicates.txt"
mutantsFolder="mutants.out"

test -d $Dir || error_exit "directory $Dir do not exist"
cd $Dir

#Compile the generated mutants
CFLAGS="-lm"    #link with lm because gcc complain linking when fmod mutant is added
for m in `find -maxdepth 1 -type f -name "*.bc"`
do
    $llc -O0 -filetype=obj -o ${m%.bc}.o $m || error_exit "Failed to compile mutant $m to object"
    $CC -O3 -o ${m%.bc} ${m%.bc}.o $CFLAGS || error_exit "Failed to compile mutant $m to executable"
    rm -f ${m%.bc}.o #$m
    echo "# xx ($SECONDS s) done $m!"   ##DEBUG
done

if test -d $mutantsFolder # && [ `ls 'mutants' | wc -l` -gt 0 ]        #no need to check non empty because the original is alway there
then
    nBCs=$(find $mutantsFolder -type f -name "*.bc" | wc -l)
    bcCount=1
    
    echo "The functions BCs -> Object..."
    test -d $tmpFuncModuleFolder || { mkdir $tmpFuncModuleFolder; find $mutantsFolder  -type f -name "*.bc" > "$tmpFuncModuleFolder/mapinfo"; }
    for m in `cut -d' ' -f2 $tmpFuncModuleFolder/mapinfo | sort -u | grep -v "^$"`; do
        $llc -O0 -filetype=obj -o ${m%.bc}.o $m || error_exit "Failed to compile mutant $m to object"
    done
    
    sed -i'' 's|.bc|.o|g' "$tmpFuncModuleFolder/mapinfo"
    while read in
    do 
        x=`printf "$in" | cut -d' ' -f1`
        $llc -O0 -filetype=obj -o $x ${x%.o}.bc || error_exit "Failed to compile mutant $m to object (cmd: $llc -O0 -filetype=obj -o $x ${x%.o}.bc)"
        $CC -O3 -o ${x%.o} $in $CFLAGS || error_exit "Failed to compile mutant $m to executable (cmd: $CC -O3 -o ${x%.o} $in $CFLAGS)"
        rm -f $x #$m
        echo "$bcCount/$nBCs ($SECONDS s) done $x!"   ##DEBUG
        bcCount=$((bcCount+1))
    done < "$tmpFuncModuleFolder/mapinfo"
    [ "$removeMutsBCs" = "yes" ] && find $mutantsFolder -type f -name "*.bc" -exec rm -f {} + || error_exit "Failed to remove some mutants .bc files"
    rm -rf $tmpFuncModuleFolder || error_exit "Failed to remove temporary function module folder"
    
    which fdupes > /dev/null || error_exit "'fdupes' is not installed (or in the PATH): cannot remove further TCE dup/eq."
    fdupes -r -dN  $mutantsFolder > $fdupesData 
    test -s $fdupesData  && find $mutantsFolder -type d -empty -exec rm -rf {} +  #Check if some duplicate mutants were found and remove their folders
fi

cd - >/dev/null

