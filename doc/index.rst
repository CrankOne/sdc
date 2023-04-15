.. _index:

SDC Library
===========

This library addresses a common problem for scientific applications: retrieving
data of certain validity period.

Most often this kind of task arises for case of so-called "calibrations" data.
This need is usually resolved by means of databases, but DB-based approach
can be tedious to maintain, especially at the early stages of project when
particular object model is not clear.

SDC library offers a set of C++ based utilities to retrieve (parse and query)
data organized in filesystem subtree, by providing few boilerplate classes and
routines.

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

