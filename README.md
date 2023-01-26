#  A Simple Calibration Data Library

This repository offers an approach to organize arbitrary calibration data based
on the file structure. If your application requires some bulky data to be
choosen and read on demand based on some simple key (timestamp, session, run
number, etc) and is not very demanding in terms of requests frequency, using
a DB might be a huge overkill.

Most of the time we use tabular ASCII files with simple columnar format (.csv
or similar) to represent a set of measurements, wiring schemes or time series.
During prototyping stage, when data scheme is not very clear, it is pretty much
tedious to toss around with SQL code writing schema, converters and selectors.

SDC offers:

 * a simplistic yet customizable grammar to define columnar data types
 * basic metadata support (using both a file grammar and file name or path)
 * ORM-like template-based interfacing with C++
 * customizable parsing procedure
 * very simple usage; getting a parsed data is as simple as referencing an
   element by index in array.

SDC can be used as both, header-only or static or dynamic library.

Drawbacks:

 * no support for cache implemented so far; this means, SDC has to re-scan file
   structures and read the files every time it starts up in the application.
   Can slow down the initialization at large amount of files.
 * cumbersome header file, somewhat hard for average non-professional
   C++ programmer to read.

SDC is good at initial stages of the project, so its drawbacks are somewhat
naturally avoid once developers decide to move to DB.

# Usage and Examples

A file containing calibration information has simple grammar:

 * comment lines starting from certaing char will be ignored (`#` by default)
 * lines containing metadata delimiter (`=` by default) will be interpreted
   as *metadata* definitions
 * non-empty lines matching certain expresion are considered as tabular data

So, for instance with default settings, file of the following content is
an SDC document:

    # Commented info
    runs=123-456
    type=MyCalib
    foo 123.45 12.34
    bar 234.56 78.99

A *metadata* is a very important feature as it sets the *validity*
and *data type* for certain tabular content, used to properly parse and
retrieve data blocks. It can be set from within a file, from filename, or
externally from C++ code. However type and validity must be set for every
block in one way or another.

# Installation

One may use just `include/sdc-base.hh` as header-only lib.

To build SDC as a library, use CMake build procedure:

    $ mkdir build
    $ cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
    $ make install

Note, that if [ROOT](https://root.cern.ch) has been found during package
configuration, the library supports evaluation of simple in-text arithmetic
expressions.

# Naming

SDC stands for "Simple calibration data" / "self-descriptive calibrations".

# Author

Renat R. Dusaev, <renat.dusaev@cern.ch>

For licensing information, see LICENSE file in repository's root
dir (it's a GNU GPL 2.1).
