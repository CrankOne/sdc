#/usr/bin/env python

import sys, subprocess, json
import pandas as pd
import numpy as np

"""
$ inspect_sdc.py (-t,--type) <typeName>) (-k,--key) <key> (-d,--dir) <path> \\
        [-c,--index-column <index1>[=<val1>] [-c,--index-column <index2>[=<val2>]]...]
"""

def run_loader(executable, path, key):
    """
    Runs given executable, expects stdout to provide JSON object.
    """
    args = [executable, path, key]
    p = subprocess.Popen(args, stdout=subprocess.PIPE)
    streamdata = p.communicate()[0]
    if p.returncode:
        sys.stderr.write(f"Wrapper script exit due to error in the app ({p.returncode})")
        sys.exit(p.returncode)
    data = json.loads(streamdata)
    #print(json.dumps(data, indent=2))
    return data

def main():
    import argparse
    p = argparse.ArgumentParser( description="An inspector jfor SDC data loding"
            " workflow. Has to be used combined with SDC's discover application"
            " for calibration data inspection and debugging.")
    p.add_argument( '-r', '--exec', help="Executable to run. Specific to data"
            " type (string) to be loaded"
            , type=str, required=True )
    p.add_argument( '-k', '--key', help="Validity ket to load calibrations for."
            , type=str, required=True )
    p.add_argument( '-d', '--path', help="Base path for calibrations data."
            , type=str, required=True )
    p.add_argument( '-c', '--index-column', help="A column to index data."
            , type=lambda a: [a.split('=')] if '=' in a else [a,] )
    args = p.parse_args()
    # Load the data for given conditions
    data = run_loader(args.exec, args.path, args.key)
    # Now in data["loadLog"] we have all the cells parsed from file(s), in
    # order. This data can be represented as a table or dataframe for sorting,
    # querying, etc

    #
    # use pandas
    df = pd.DataFrame(data["loadLog"])
    if df.empty:
        sys.stderr.write(f"No data loaded for key {args.key} from {args.path}.\n")
        return 1
    df = pd.pivot( df
        , values='v', columns='c'
        , index=['srcID', 'lineNo']
        )
    # apply indexing, if selected
    idx = None
    if args.index_column:
        idx = list(c[0] if type(c) is list else c for c in args.index_column)
    if idx:
        df = df.set_index(idx, append=True)
    # apply slicing, if selected
    slicers = {}
    if args.index_column:
        slicers = dict(filter(lambda c: type(c) is list, args.index_column))
        # create indexer template (with empty slicers)
        indexer = [slice(None)]*len(df.index.levels)
        # add custom slicers
        for n, idx in slicers.items():
            indexer[df.index.names.index(n)] = idx
        df = df.loc[tuple(indexer),:]

    sys.stdout.write(str(df) + '\n')

if "__main__" == __name__:
    sys.exit(main())

