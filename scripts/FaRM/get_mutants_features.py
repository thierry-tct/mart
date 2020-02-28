#! /usr/bin/env python

"""
    Extracts features of mutants (features used in FaRM)
    example: python script.FaRM/get_mutants_features.py mart-out-0 --save_raw --save_categories
"""

from __future__ import print_function

import os
import sys
import argparse
import shutil

from getCatMatrices import main_get_cat_matrices, loadJson, dumpJson

def error_exit(string):
    print("GetMutantsFeatures@ERROR: {}".format(string))
    assert False, string
#~ def error_exit()

mart_selection_outdir = 'Selection.out'
mart_gen_mutant_features_file = 'mutants-features.csv'
mart_selection_cache = 'mutantDependencies.cache.json'
raw_features_file = "raw-mutants-features.json"
categories_features_file = "categories-mutants-features.json"

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mart_out_dir", help="mart mutants generation folder"
                                                        " (e.g.: mart-out-0)")
    parser.add_argument("--save_raw", action='store_true', \
                                                    help="save raw features")
    parser.add_argument("--save_categories", action='store_true', \
                                            help="(not woring)save categorical features")
    parser.add_argument("--no_save_encoded", action='store_true', \
                                help="do not save one hot encoded features")
    parser.add_argument("--mart_bin_dir", default='', \
                                                help="custom mart binary dir")
    args = parser.parse_args()

    if not os.path.isdir(args.mart_out_dir):
        error_exit("specified mart mutants generation folder is not existing")
    
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

    mart_selection_cmd = " ".join([
        os.path.join(args.mart_bin_dir, 'mart-selection'),
        args.mart_out_dir,
        '-preprocessed-bc-file', prepro_bc,
        '-mutant-dep-cache',
        '-dump-features',
        '-no-selection',
    ])

    if os.system(mart_selection_cmd) != 0:
        error_exit ("Mart selection error. cmd: {}".format(mart_selection_cmd))
    
    if args.no_save_encoded:
        os.remove(os.path.join(args.mart_out_dir, mart_selection_outdir, \
                                                mart_gen_mutant_features_file))

    if args.save_raw:
        mrflist = loadJson(os.path.join(args.mart_out_dir, mart_selection_outdir, \
                                                    mart_selection_cache))
        obj = {}
        for i, rf in enumerate(mrflist):
            obj[str(i+1)] = rf
        dumpJson(obj, os.path.join(args.mart_out_dir, mart_selection_outdir, \
                                                            raw_features_file))

    if args.save_categories:
        error_exit ("This feature has bug. To be fixed")
        cat_out_file = os.path.join(args.mart_out_dir, mart_selection_outdir, \
                                                    categories_features_file)
        _feature_matrix = os.path.join(args.mart_out_dir, \
                        mart_selection_outdir, mart_gen_mutant_features_file)

        main_get_cat_matrices(_feature_matrix, cat_out_file, \
                            abstract_mtype_only=False, one_hot_encode=False)

    print('\n# Done')
    print ('# Features written in {}'.format(os.path.join(args.mart_out_dir, \
                                                    mart_selection_outdir)))
#~ def main()

if __name__ == '__main__':
    main()
