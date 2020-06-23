#! /usr/bin/env python

from __future__ import print_function

import os
import json
import argparse
import tempfile

def main():
    # Parse the args
    parser = argparse.ArgumentParser()
    parser.add_argument("mart_out_dir", help="mart mutants generation folder"
                                                        " (e.g.: mart-out-0)")
    parser.add_argument("output_dir", help="Non existant oputput dir")
    parser.add_argument("--mart_bin_dir", default='', \
                                                help="custom mart binary dir")
    args = parser.parse_args()

    if not os.path.isdir(args.mart_out_dir):
        error_exit("specified mart mutants generation folder is not existing")
    
    if os.path.isdir(args.output_dir):
        error_exit("output_dir exist, please use non existant")
    if not os.path.isdir(os.path.dirname(args.output_dir)):
        error_exit("parent of output_dir not existing. create it")

    # get the bc with shortest name
    prepro_bc = []
    len_prepro_bc = 99999999999
    for f in os.listdir(args.mart_out_dir):
        if not f.endswith('.bc'):
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
    meta_bc = os.path.splitext(prepro_bc)[0] + ".MetaMu" \
                                            + os.path.splitext(prepro_bc)[1]

    mutantInfo_file = os.path.join(args.mart_out_dir, "mutantsInfos.json")
    fdupes_files = os.path.join(args.mart_out_dir, "fdupes_duplicates.json")
    with open(mutantInfo_file) as f:
        minf = json.load(f)
    with open(fdupes_files) as f:
        fdupes = json.load(f)

    candidate_mutants = set(minf)
    for rem, dup in fdupes.items():
        candidate_mutants -= set(dup)

    fd, mutant_list_file = tempfile.mkstemp()
    try:
        with os.fdopen(fd, 'w') as tmp:
            for mut_id in candidate_mutants:
                tmp.write("{}\n".format(mut_id))

        mart_selection_cmd = " ".join([
            os.path.join(args.mart_bin_dir, 'mart-utils'),
            prepro_bc,
            meta_bc,
            args.output_dir,
            '-mutant-list-file', mutant_list_file,
            '-write-mutants-bc',
        ])

        if os.system(mart_selection_cmd) != 0:
            error_exit ("Mart selection error. cmd: {}".format(mart_selection_cmd))

    finally:
        os.remove(mutant_list_file)

    print("# Done!")
#~ def main()

if __name__ == "__main__":
    main()

