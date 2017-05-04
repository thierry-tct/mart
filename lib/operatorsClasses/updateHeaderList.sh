##
# This script will update the list of headers in the file 'mutation_op_headers.h.in' which will be included in usermaps.h.
######

#! /bin/bash

set -u

topDir=$(readlink -f $(dirname $0))

outfile="mutation_op_headers.h.inc"

cd $topDir

find -type f -name "*.h" | sed 's|^./||g' | grep -v "^$outfile$" | sed 's|^|#include "|g; s|$|"|g' > $outfile || { echo "ERROR: FAILED TO COLLECT HEADER FILES FOR INCLUDE"; exit 1; }

