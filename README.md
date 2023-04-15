[![Documentation Status](https://readthedocs.org/projects/sdc-lib/badge/?version=latest)](https://sdc-lib.readthedocs.io/en/latest/?badge=latest)

#  A Simple Calibration Data Library

This repository offers an approach to organize arbitrary calibration data based
on the file structure. If your application requires some bulky data to be
choosen and read on demand based on some simple key (timestamp, session, run
number, etc) and is not very demanding in terms of requests frequency, using
a DB might be an overkill.

# Documentation

For detailed discussion, short tutorial, grammar example and API documentation,
see the [documentation pages](https://sdc-lib.readthedocs.io/en/latest/index.html).

# Installation

One may use just `include/sdc-base.hh` as header-only lib.

To build SDC as a library, use CMake build procedure:

    $ mkdir build
    $ cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
    $ make install

Note, that if [ROOT](https://root.cern.ch) has been found during package
configuration, the library supports evaluation of simple in-text arithmetic
expressions (by [TFormula](https://root.cern.ch/doc/master/classTFormula.html),
embedded in all numerical getters).

To build static library just append cmake command
with `-DBUILD_SHARED_LIBS=OFF`.

# Naming

SDC stands for "Simple calibration data" / "self-descriptive calibrations".

# Author

[NA64 Collaboration (CERN)](https://na64.web.cern.ch/)
Designed and implemented by Renat R. Dusaev (NA64-CERN, TPU),
original ExtCSV grammar specification authored by Anton Karneeu (NA64-CERN, INR).

For licensing information, see LICENSE file in repository's root
dir (it's GNU GPL 2.1).
