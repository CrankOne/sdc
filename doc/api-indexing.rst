.. _api-indexing:

Data Indexing
=============

Main library's functionality and topmost abstraction level is represented by
class :cpp:class:`sdc::Documents`. This is a template class maintaining index
of the documents of multiple types indexed with same key type (single parameter
of this template), facilitating addition and query operations.

At more detailed level :cpp:class:`sdc::Documents` consists of a set of
:cpp:class:`sdc::ValidityIndex` class instances defining validity index for
particular C++ data type.

:cpp:class:`sdc::Documents` also exposes :cpp:class:`sdc::Documents::iLoader`
interface that should be implementedfor any new data source format intended for
usage.

For a very simplisitc scenario of single walkthrough/query cycle, a demo
function is exposed as a part of library's API:

.. doxygenfunction:: sdc::load_from_fs

However, for more complex cases, below is the list of top-level instrumentation
providing SDC core functionality as it is intended to be used in complex API.

.. doxygengroup:: indexing
   :members:

