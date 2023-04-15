.. _index:

SDC Library
===========

This library addresses a common problem for scientific applications: retrieving
data of certain validity period.

Most often this kind of task arises for case of so-called "calibrations" data.
This need is usually resolved by means of databases, but DB-based approach
can be tedious to maintain, especially at the early stages of project when
particular object model is not clear.

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

Please, refer to our short :ref:`tutorial` for a brief overview on the simple usage
example.

.. toctree::
   :maxdepth: 2

   tutorial
   grammar
   api

Library sources (as well as this docs) are available on the `GitHub repo`_ and
are licensed for free distribution by LGPLv2.

.. _GitHub repo: https://github.com/crankOne/sdc

