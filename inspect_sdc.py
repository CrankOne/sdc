#/usr/bin/env python

"""
A wrapper script for user's SDC applications to print/query items loading
sequence. Example:

    $ python3 inspect_sdc.py -r build/sdc-inspect -d tests/assets/test2/one.txt -k2 -cone=uno

Run with to get more detailed usage information.
"""

import sys, subprocess, json

class ExecutableFailed(RuntimeError):
    """Thrown in case of application failure"""
    def __init__(self, cmd, rc):
        self.cmd = cmd
        self.rc = rc

class EmptyData(RuntimeError):
    """Thrown if no data loaded at all by SDC for certain key."""
    pass

class SelectError(KeyError):
    """Thrown if slicers (selection) results in no data loaded for such conditions."""
    def __init__(self, key, slicers):
        super().__init__(key)
        self.slicers = slicers


def run_loader(executable, path, key):
    """
    Runs given executable, expects stdout to provide JSON object.
    """
    args = [executable, path, key]
    p = subprocess.Popen(args, stdout=subprocess.PIPE)
    streamdata = p.communicate()[0]
    if p.returncode:
        raise ExecutableFailed(args, p.returncode)
    data = json.loads(streamdata)
    return data


def get_load_log_pd(data, indexColumns=None):
    import pandas as pd
    if indexColumns is None: indexColumns=[]
    df = pd.DataFrame(data)
    if df.empty:
        raise EmptyData()
    df = pd.pivot( df
        , values='v', columns='c'
        , index=['srcID', 'lineNo']
        )
    # apply indexing, if selected
    idx = None
    if indexColumns:
        idx = list(c[0] if type(c) is list else c for c in indexColumns)
    if idx:
        df = df.set_index(idx, append=True)
    # apply slicing, if selected
    slicers = {}
    if indexColumns:
        slicers = dict(filter(lambda c: type(c) is list, indexColumns))
        # create indexer template (with empty slicers)
        indexer = [slice(None)]*len(df.index.levels)
        # add custom slicers
        for n, idx in slicers.items():
            indexer[df.index.names.index(n)] = idx
        try:
            df = df.loc[tuple(indexer),:]
        except KeyError as e:
            raise SelectError(e.args[0], slicers)
    return df
    

def print_load_log(data, indexColumns=None, stream=sys.stdout):
    """
    Uses pandas to print the data loading log, as retrieved by `run_loader()`.
    """
    df = get_load_log_pd(data, indexColumns=indexColumns)
    stream.write(str(df) + '\n')


def instantiate_cmdargs_parser():
    """
    Instantiates default command line arguments parser.
    """
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
    return p


def main(args):
    """
    Entry point script, parameterised with parsed standard arguments (see
    ``instantiate_cmdargs_parser()``.
    """
    # Load the data for given conditions
    try:
        data = run_loader(args.exec, args.path, args.key)
    except ExecutableFailed as e:
        sys.stderr.write(f"`{' '.join(e.cmd)}' failed with exit code {e.rc}.\n")
        return e.rc
    # Now in data["loadLog"] we have all the cells parsed from file(s), in
    # order. This data can be represented as a table or dataframe for sorting,
    # querying, etc
    try:
        print_load_log(data["loadLog"], indexColumns=args.index_column)
    except EmptyData as e:
        sys.stderr.write(f"No data loaded for key \"{args.key}\" from {args.path}.\n")
        return 1
    except SelectError as e:
        sys.stderr.write(f"No data loaded for key \"{args.key}\" from {args.path}"
                f" with selection {e.slicers}: no items for \"{e.args[0]}\"\n")
        return 1


if "__main__" == __name__:
    p = instantiate_cmdargs_parser()
    args = p.parse_args()
    sys.exit(main(args))

