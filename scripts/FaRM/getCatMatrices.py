#! /usr/bin/python

from __future__ import print_function

import sys
import os
from pathlib import Path
useful_dir = os.path.join(Path(__file__).parent.parent.parent, \
                                                            'tools', 'useful')
sys.path.insert(0, useful_dir)
import create_mconf
try:
    import pandas as pd
except ImportError:
    os.system('pip install pandas')
    import pandas as pd
import json
try:
    import dask.dataframe as dd
    Use_Dask = True
except ImportError:
    Use_Dask = False

def loadJson(filename):
    with open(filename) as f:
        return json.load(f)
#~ def loadJson()

def dumpJson(obj, filename):
    with open(filename, 'w') as f:
        json.dump(obj, f)
#~ def dumpJson()

def loadCSV (filename):
    if Use_Dask:
        mat = dd.read_csv(filename, engine="python", header=0, dtype='object') #assume_missing=True)
    else:
        chunksize = 100000
        chunks = []
        for chunk in pd.read_csv(filename, engine="python", header=0, chunksize=chunksize): #, low_memory=False):
            chunks.append(chunk)
        mat = pd.concat(chunks, axis=0)
        #mat = pd.read_csv(filename, dtype=float, engine="python", header=0)
        #mat = pd.read_csv(filename, engine="python", header=0)
    return mat
#~ def loadCSV()

def onehotencode(listdata):
    df = pd.get_dummies(listdata)
    #print len(list(df))
    return df

NO_EXTRAMUTTYPES_FEAT = 'no-extra-muttypes'
NO_CATEGORICAL_FEAT = 'no-categorical'
ALL_FEAT = 'all'
CLUSTERING_FEAT = 'clustering'
CLUSTERING_FEAT_TYPES = [CLUSTERING_FEAT]
FEATURES_SETS = [ALL_FEAT, NO_EXTRAMUTTYPES_FEAT, NO_CATEGORICAL_FEAT] + CLUSTERING_FEAT_TYPES

def filterFeatures(features_list, feat_set):
    res_flist = []
    for f in features_list:
        if feat_set == NO_EXTRAMUTTYPES_FEAT:
            if f.endswith("-astparent") or  f.endswith("-inctrldep") or  f.endswith("-indatadep") or  f.endswith("-outctrldep") or  f.endswith("-outdatadep") or f.endswith("-outdatadep") or f.endswith("-outdatadep$") or f.endswith("-Operand-DataTypeContext"):
                continue
        elif feat_set == NO_CATEGORICAL_FEAT:
            if '-' in f and f not in ['HasLiteralChild-ChildContext', 'HasIdentifierChild-ChildContext', 'HasOperatorChild-ChildContext']:
                continue
        elif feat_set == CLUSTERING_FEAT:
            # DBG 
            if False:
                if not f.endswith('-BBType') and not f.endswith('-ASTp') and not f.endswith('-Return-DataTypeContext') and f not in ['HasIdentifierChild-ChildContext',\
                        'HasLiteralChild-ChildContext', 'HasOperatorChild-ChildContext', 'Complexity']: # and not f[:-1].endswith('--mutantType'):
                    continue
            # ~DBG
            # eliminate mutantTypes except level 
            mtypelevel = '3' # belong to ['1', '2', '3']
            if f[:-1].endswith('--mutantType') and not f[-1] == mtypelevel:
                continue
            elif f == 'ml_prediction':
                continue
        res_flist.append(f)
    return res_flist
#~ def filterFeatures()

def main_get_cat_matrices(feature_matix, out_cat_matrix, abstract_mtype_only=False, one_hot_encode=False):
    matrix1 = os.path.abspath(feature_matix)

    out_matrix1 = os.path.abspath(out_cat_matrix)

    # load matrices
    df1 = loadCSV(matrix1)

    # remove useless features
    raw_ordered_flist1 = list(df1.columns) 
    ordered_flist1 = filterFeatures(raw_ordered_flist1, NO_EXTRAMUTTYPES_FEAT)

    df1 = df1.drop(list(set(raw_ordered_flist1) - set(ordered_flist1)), axis=1)

    # compute
    if Use_Dask:
        df1 = df1.compute()

    MutantType = "mutanttype"

    objdf1 = {}

    if abstract_mtype_only:
        cat_feat1 = [x for x in ordered_flist1 if x.endswith('-Replacer') or x.endswith('-Matcher')]
    else:
        # get categorical features
        cat_feat1 = set(ordered_flist1) - set(filterFeatures(ordered_flist1, NO_CATEGORICAL_FEAT))

    # transform matrices to have categorical values
    for objdf, df, cat_feat in [(objdf1, df1, cat_feat1)]:
        catnamemap = []
        matchers = []
        replacers = []
        for c in cat_feat:
            val, cat = c.rsplit('-',1)
            if cat == 'DataTypeContext':
                val, mid = val.rsplit('-',1)
                cat = '-'.join([mid, cat])

            if cat == 'Matcher':
                matchers.append((c,val,MutantType))
                if MutantType not in objdf:
                    objdf[MutantType] = []
            elif cat == 'Replacer':
                replacers.append((c,val,MutantType))
                if MutantType not in objdf:
                    objdf[MutantType] = []
            else:
                catnamemap.append((c, val, cat))
                if cat not in objdf:
                    objdf[cat] = []
        for index, row in df.iterrows():
            active = []
            match = None
            replace = None
            for c,val,cat in catnamemap:
                xv = float(row[c]) 
                if xv == 1.0:
                    objdf[cat].append(val)
                else:
                    assert xv == 0.0, "must be 0.0 or 1.0, found: "+str(row[c])+". index="+str(index)+". field is: "+c
            for c,val,cat in matchers:
                xv = float(row[c]) 
                if xv == 1.0:
                    assert match is None
                    match = val
                else:
                    assert xv == 0.0, "must be 0.0 or 1.0, found: "+str(row[c])+". index="+str(index)
            for c,val,cat in replacers:
                xv = float(row[c]) 
                if xv == 1.0:
                    assert replace is None
                    replace = val
                else:
                    assert xv == 0.0, "must be 0.0 or 1.0, found: "+str(row[c])+". index="+str(index)
            assert match is not None and replace is not None
            mtype = '!'.join([match, replace])
            objdf[MutantType].append(mtype)
            #print objdf
            for cc in objdf:
                #print cc, len(objdf[cc]) , len(objdf[MutantType])
                assert len(objdf[cc]) == len(objdf[MutantType])

    df1 = df1.drop(cat_feat1, axis=1)

    # change categorical values granularitay(MutantType)
    for mtypeL, level in [(MutantType+'1', 1),(MutantType+'2', 2),(MutantType+'3',3)]:
        objdf1[mtypeL] = list(objdf1[MutantType])
        for i in range(len(objdf1[mtypeL])):
            objdf1[mtypeL][i] = create_mconf.getOpClass(objdf1[MutantType][i], level=level)[0]+"--mutantType"+str(level)
    del objdf1[MutantType]

    print (len(list(df1)))

    if one_hot_encode:
        onehots = [onehotencode(objdf1[v]) for v in objdf1]
        df1 = pd.concat([df1.reset_index(drop=True)]+onehots, axis=1)
        '''for i in onehots:
            for c in list(i):
                assert c not in df1
                df1[c] = list(i[c])'''
    else:
        df1 = pd.concat([df1, pd.DataFrame(objdf1)], axis=1)

    print (len(list(df1)))

    # write new matrices
    df1.to_csv(out_matrix1, index=False)
