##
# This script will update the list of headers in the file 'mutation_op_headers.h.in' which will be included in usermaps.h.
######

#! /bin/bash

set -u

topDir=$(readlink -f $(dirname $0))

[ $# = 1 ] || { echo "ERROR: EXPECTED 1 PARAMETER (OUTFILE), $# PASSED"; exit 1; }

outfile=$1

cd $topDir

lasttimestamp="/*"$(find -type d -exec stat {} --printf="%Y\n%Z\n" \; | sort -u | xargs)"*/"

test -f $outfile && [ "$(head -n 1 $outfile)" = "$lasttimestamp" ] && exit 0

echo "$lasttimestamp" > $outfile || { echo "ERROR: FAILED TO COLLECT HEADER FILES FOR INCLUDE (puting timestamp)"; exit 1; }
find -type f -name "*.h" | sed 's|^./||g' | grep -v "^$outfile$" | sed 's|^|#include "|g; s|$|"|g' >> $outfile || { echo "ERROR: FAILED TO COLLECT HEADER FILES FOR INCLUDE"; exit 1; }

