#! /usr/bin/env python

"""
This script is used to extracts the various mutants 
'.bc' files from the MetaMutant '.bc' file
"""

from __future__ import print_function

import os
import json
import argparse
import tempfile
from distutils.spawn import find_executable

# Constant used in script
TARGET_TOOL_BINARY = 'mart-utils'
BC_SUFFIX = '.bc'
META_MUTANT_POSTFIX = ".MetaMu"
MUTANTS_INFOS_FILENAME = "mutantsInfos.json"
FDUPES_DUPLICATES_FILENAME = "fdupes_duplicates.json"
MART_UTILS_MUTANT_LIST_FILE_ARG = '-mutant-list-file'
MART_UTILS_WRITE_MUTANTS_BC_ARG = '-write-mutants-bc'
MART_UTILS_ENABLE_VERBOSE_ARG = "-verbose"


def error_exit(string):
    print("@ERROR: {}".format(string))
    assert False, string
#~ def error_exit()

def main():
    # Parse the args
    parser = argparse.ArgumentParser()
    parser.add_argument("mart_out_dir", help="mart mutants generation folder."
                                " This is the output folder generated by Mart"
                                " (e.g.: mart-out-0)")
    parser.add_argument("output_dir", help="Non existant output dir where the"
                                " generated file will be stored")
    parser.add_argument("--mart_bin_dir", default='',
                        help="custom Mart binary dir (in not on system path)")
    parser.add_argument("--verbose", action='store_true',
                            help="enable printing the script's "
                                    "execution progress log")
    args = parser.parse_args()

    tool = TARGET_TOOL_BINARY

    if args.mart_bin_dir:
        tool = os.path.join(args.mart_bin_dir, tool)
        if not os.path.isfile(tool):
            error_exit("{} not existing.".format(tool))
    else:
        # must exist in path
        if find_executable(tool) is None:
            error_exit("mart_bin_dir not set and {} not on path.".format(tool))

    if not os.path.isdir(args.mart_out_dir):
        error_exit("specified mart mutants generation folder is not existing")
    
    if os.path.isdir(args.output_dir):
        error_exit("output_dir exist, please use non existant")
    if not os.path.isdir(os.path.dirname(os.path.abspath(args.output_dir))):
        error_exit("parent of output_dir not existing. create it")

    # get the bc with shortest name
    prepro_bc = []
    # Initialize to highest possible value
    len_prepro_bc = float('inf')
    for f in os.listdir(args.mart_out_dir):
        if not f.endswith(BC_SUFFIX):
            continue
        if len(f) == len_prepro_bc:
            prepro_bc.append(f)
        elif len(f) < len_prepro_bc:
            prepro_bc = [f]
            len_prepro_bc = len(f)
    if len(prepro_bc) != 1:
        error_exit("invalid number of preprocessed .bc; found {}".format(\
                                                                    prepro_bc))
    prepro_bc = os.path.join(args.mart_out_dir, prepro_bc[0])
    meta_bc = os.path.splitext(prepro_bc)[0] + META_MUTANT_POSTFIX \
                                            + os.path.splitext(prepro_bc)[1]

    mutantInfo_file = os.path.join(args.mart_out_dir, MUTANTS_INFOS_FILENAME)
    fdupes_files = os.path.join(args.mart_out_dir, FDUPES_DUPLICATES_FILENAME)
    with open(mutantInfo_file) as f:
        minf = json.load(f)
    if os.path.isfile(fdupes_files):
        with open(fdupes_files) as f:
            fdupes = json.load(f)
    else:
        fdupes = {}

    candidate_mutants = set(minf)
    for rem, dup in fdupes.items():
        candidate_mutants -= set(dup)

    fd, mutant_list_file = tempfile.mkstemp()
    try:
        with os.fdopen(fd, 'w') as tmp:
            for mut_id in candidate_mutants:
                tmp.write("{}\n".format(mut_id))

        mart_utils_cmd = " ".join([
            tool,
            prepro_bc,
            meta_bc,
            args.output_dir,
            MART_UTILS_MUTANT_LIST_FILE_ARG, mutant_list_file,
            MART_UTILS_WRITE_MUTANTS_BC_ARG,
        ])
        if args.verbose:
            mart_utils_cmd += " " + MART_UTILS_ENABLE_VERBOSE_ARG

        filter_cmd = " 2>&1 | grep -v '^# Number of Mutants:   PreTCE'"

        if os.system(mart_utils_cmd + filter_cmd) != 0:
            error_exit ("{} error. cmd: {}".format(tool, mart_utils_cmd))

    finally:
        os.remove(mutant_list_file)

    print("# Done!")
#~ def main()

if __name__ == "__main__":
    main()
