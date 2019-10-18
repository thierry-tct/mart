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

[ $# = 5 ] || error_exit "Expected 5 parameters, $# passed: $0 <llvmBinaryDir> <directory (mart-out-0)> <tmpFuncModuleFolder> <remove mutants' \".bc\"? yes/no> <extra linking flags>"

llvm_bin_dir=$(readlink -f $1)
Dir=$(readlink -f $2)
tmpFuncModuleFolder=$3
removeMutsBCs=$4
extraLinkingFlags=$5
fdupesData=$Dir/"fdupes_duplicates.txt"
fdupesJson=$Dir/"fdupes_duplicates.json"
mutantsFolder="mutants.out"

##### CONFIGURATION #####
#llc=llc-3.4
llc=llc
if [ "$llvm_bin_dir" != "" ]
then
    llc="$llvm_bin_dir/$llc"
fi

#CC=gcc
CC="$llvm_bin_dir/clang"
#########################

test -d $Dir || error_exit "directory $Dir do not exist"
entryDir=`pwd`
cd $Dir

#Compile the generated mutants
CFLAGS="-lm"    #link with lm because gcc complain linking when fmod mutant is added
CFLAGS+=" $extraLinkingFlags"
for m in `find -maxdepth 1 -type f -name "*.bc"`
do
    $llc -O0 -filetype=obj -o ${m%.bc}.o $m || error_exit "Failed to compile bitcode $m to object (in $Dir)"
    $CC -O3 -o ${m%.bc} ${m%.bc}.o $CFLAGS || error_exit "Failed to compile object ${m%.bc}.o of bitcode $m to executable (in $Dir)"
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
    
    sed -i'' 's|\.bc|.o|g' "$tmpFuncModuleFolder/mapinfo"
    while read in
    do 
        x_path=`printf "$in" | cut -d' ' -f1`
        x_dir=$(dirname $x_path)
        x=$(basename $x_path)
        cd $x_dir || error_exit "Failed to cd into x_dir ($x_dir)"
        $llc -O0 -filetype=obj -o $x ${x%.o}.bc || error_exit "Failed to compile mutant $m to object (cmd: $llc -O0 -filetype=obj -o $x ${x%.o}.bc)"
        cd - > /dev/null # go back from x_dir
        $CC -O3 -o ${x_path%.o} $in $CFLAGS || error_exit "Failed to compile mutant $m to executable (cmd: $CC -O3 -o ${x_path%.o} $in $CFLAGS)"
        rm -f $x_path #$m
        echo "$bcCount/$nBCs ($SECONDS s) done $x_path!"   ##DEBUG
        bcCount=$((bcCount+1))
    done < "$tmpFuncModuleFolder/mapinfo"
    [ "$removeMutsBCs" = "yes" ] && { find $mutantsFolder -type f -name "*.bc" -exec rm -f {} + || error_exit "Failed to remove some mutants .bc files"; }
    rm -rf $tmpFuncModuleFolder || error_exit "Failed to remove temporary function module folder"
    
    which fdupes > /dev/null || error_exit "'fdupes' is not installed (or in the PATH): cannot remove further TCE dup/eq."
    fdupes -r -dN  $mutantsFolder > $fdupesData 
    test -s $fdupesData  && find $mutantsFolder -type d -empty -exec rm -rf {} +  #Check if some duplicate mutants were found and remove their folders
    
    # Create fdupes_duplicates.json
    echo "{" > $fdupesJson || error_exit "Failed to create fdupesJson1"
    newdup=1
    while read in
    do
        # in can be (1) empty line <separator>, (2) master mutant <[+]...>, (3) duplicates <[-]...>
        if echo $in | grep '\[-]' > /dev/null; then
            # duplicate
            [ $newdup -eq 1 ] || error_exit "Invalid fdupes results: newdup not 1 when [-]"
            echo -n "\"$(basename $(dirname $(echo $in | awk '{print $2}')))\"," >> $fdupesJson
        elif echo $in | grep '\[+]' > /dev/null; then
            # master
            [ $newdup -eq 0 ] || error_exit "Invalid fdupes results: newdup not 0 when [+]"
            newdup=1
            echo -n "    \"$(basename $(dirname $(echo $in | awk '{print $2}')))\": [" >> $fdupesJson
        else
            if [ $newdup -eq 1 ]; then
                sed -i'' '$s|,$|],|' $fdupesJson
                echo >> $fdupesJson
            fi
            newdup=0
        fi
    done < $fdupesData
    sed -i'' '$s|,$||' $fdupesJson
    echo >> $fdupesJson
    echo "}" >> $fdupesJson || error_exit "Failed to create fdupesJson3"
    rm -f $fdupesData
fi

cd $entryDir

