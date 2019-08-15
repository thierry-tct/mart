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

[ $# = 3 ] || [ $# = 4 ] || error_exit "Expected 3 or 4 parameter, $# passed: $0 <klee, noklee> <mode (meta, all)> [<mutation config>] <input BC>"

TOPDIR=$(dirname $(readlink -f $0))

if [ "$1" = "klee" ]
then
    forKLEE=true
elif [  "$1" = "noklee" ]
then
    forKLEE=false
else
    error_exit "invalid klee enabled option (expect <klee, noklee>)"
fi

if [ "$2" = "meta" ]
then
    allMuts=false
elif [ "$2" = "all" ]
then
    allMuts=true
else
    error_exit "invalid mutant generation option (expect <meta, all>)"
fi

if [ $# = 3 ]
then
    mutConf=$TOPDIR/../test/operator/mutconf.conf
    echo "Using mutation config file $mutConf ..."
    test -f $mutConf || error_exit "Mutation config file($mutConf) non existent"
    inputBC=$(readlink -f $3)
    test -f $inputBC || error_exit "Input file($inputBC) non existent"
else
    mutConf=$(readlink -f $3)
    test -f $mutConf || error_exit "Mutation config file($mutConf) non existent"
    inputBC=$(readlink -f $4)
    test -f $inputBC || error_exit "Input file($inputBC) non existent"
fi

#Remove existing out Dirs
rm -rf mart-out-* klee-out-*

## ### SET TOOLS
ks_GenMu=$TOPDIR/../../build/tools/mart
test -f $ks_GenMu || error_exit "Mutation tool non existent"

llc=llc-3.4

ks_tool=rev_umes

cflags=""
if [ $forKLEE = true ]
then
    export LD_LIBRARY_PATH=/media/thierry/TestMutants/KLEE/klee-$ks_tool/build/Release+Debug+Asserts/lib/:${LD_LIBRARY_PATH:-""}
    cflags="-L /media/thierry/TestMutants/KLEE/klee-$ks_tool/build/Release+Debug+Asserts/lib/ -lkleeRuntest"
fi
# #~

#mutate
$ks_GenMu -write-mutants -mutant-config $mutConf $inputBC 2>&1

#Compile the generated mutants
for m in `find mart-out-0/mutants -type f -name "*.bc"`
do
    $llc -filetype=obj -o ${m%.bc}.o $m || error_exit "Failed to compile mutant $m to object"
    gcc -o ${m%.bc} ${m%.bc}.o $cflags || error_exit "Failed to compile mutant $m to executable"
    rm -f ${m%.bc}.o #$m
done


##################################################
########### KLEE-ks_tool part #######################
##################################################

removeDupTests()
{
    [ $# = 1 ] || error_exit "Expected 1 parameter (test case directory) in removeDupTests"
    local tcDir=$1
    local curDir=$(pwd)
    cd $tcDir
    mkdir readableKtests || error_exit "Failed to create readableKtests"
    for ktc in `ls | grep ".ktest$"`
    do
        $ktest_tool --write-ints $ktc 2>&1 | sed 1d > readableKtests/$ktc || error_exit "ktest-tool failed on ktest: $ktc"
    done
    cd readableKtests 
    local duplicatestc=$(fdupes -1 -f . | tr '\n' ' ') || error_exit "fdupes failed"
    for dup in $duplicatestc
    do
        rm -f ../$dup ../${dup%.ktest}* || error_exit "Failed to remove duplicate ktest case $tcDir/$dup"  
    done
    { cd .. && rm -rf readableKtests; } || error_exit "Failed to remove temporary dir readableKtests"  
    cd $curDir
}

stat_ks_tool=mart-out-0/$ks_tool
statKlee=mart-out-0/klee.ksstat

full_ks_tool=/media/thierry/TestMutants/KLEE/klee-$ks_tool/build/bin/klee-$ks_tool
klee=/media/thierry/TestMutants/KLEE/klee-$ks_tool/build/bin/klee
ktest_tool=$(dirname $klee)/ktest-tool

$full_ks_tool --libc=uclibc --posix-runtime --allow-external-sym-calls -search bfs mart-out-0/MetaMu_$(basename $inputBC) 2>&1 || error_exit "$ks_tool failed"
mv mart-out-0/klee-out-0 mart-out-0/klee-$ks_tool-out || error_exit "Failed to store $ks_tool output"
$klee --libc=uclibc --posix-runtime --allow-external-sym-calls -search bfs $inputBC 2>&1 || error_exit "KLEE failed"

echo "Num Gen Tests: "$(find mart-out-0/klee-$ks_tool-out -type f -name '*.ktest' | wc -l) > $stat_ks_tool
removeDupTests mart-out-0/klee-$ks_tool-out
echo "Num different Tests: "$(find mart-out-0/klee-$ks_tool-out -type f -name '*.ktest' | wc -l) >> $stat_ks_tool
printf "\nMut/Test $(find mart-out-0/klee-$ks_tool-out -type f -name '*.ktest' | tr '\n' ' ')\n" >> $stat_ks_tool
ks_toolTests=$(find mart-out-0/klee-$ks_tool-out -type f -name '*.ktest' | tr '\n' ' ')

echo "Num Gen Tests: "$(find klee-out-0 -type f -name '*.ktest' | wc -l) > $statKlee
removeDupTests klee-out-0
echo "Num different Tests: "$(find klee-out-0 -type f -name '*.ktest' | wc -l) >> $statKlee
printf "\nMut/Test $(find klee-out-0 -type f -name '*.ktest' | tr '\n' ' ')\n" >> $statKlee
kleeTests=$(find klee-out-0 -type f -name '*.ktest' | tr '\n' ' ')

# Replay mutants with $ks_tool and klee
# Original first
for tc in $ks_toolTests $kleeTests
do
    exe0=$(find ./mart-out-0/mutants/0/ -type f | grep -v ".bc$")
    KTEST_FILE=$tc $exe0 > $tc.mut0
    echo "EXIT CODE: $?" >> $tc.mut0
done

#mutants
for m in `ls mart-out-0/mutants | grep -v "^0$" | sort -h`
do
    for tc in $ks_toolTests $kleeTests
    do
        rm -f $tc.muti
        exei=$(find ./mart-out-0/mutants/$m/ -type f | grep -v ".bc$")
        KTEST_FILE=$tc timeout 2s $exei > $tc.muti$m
        echo "EXIT CODE: $?" >> $tc.muti$m
    done
    #collect data
    
    echo -n "$m" >> $stat_ks_tool
    for tc in $ks_toolTests
    do
        if diff -q $tc.mut0 $tc.muti$m > /dev/null; then
            echo -n " 0" >> $stat_ks_tool
        else
            echo -n " 1" >> $stat_ks_tool
        fi
    done
    echo >> $stat_ks_tool
    
    echo -n "$m" >> $statKlee
    for tc in $kleeTests
    do
        if diff -q $tc.mut0 $tc.muti$m > /dev/null; then
            echo -n " 0" >> $statKlee
        else
            echo -n " 1" >> $statKlee
        fi
    done
    echo >> $statKlee
done


#ktest=klee-last/test000001.ktest
#KTEST_FILE=$ktest ./$IRfile




