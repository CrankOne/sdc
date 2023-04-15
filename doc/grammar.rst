.. _grammar:

Default File Grammar
====================

Essentially, SDC library. What the data source *are* is customizable. However
SDC offers utility classes to maintain ASCII files in the directory structure
by certain location.

By default SDC relies on simple grammar for subject files, assuming simple
columnar syntax for data blocks, ``#`` for comments and simplistic
``key=value`` syntax of *metadata* lines:

.. code-block:: cfg

    # Comments are started with `#' and lasts till the newline
    myMetadata = 123  # this is a user-defined metadata line
    # Below is the data block in use
    detector1.1   1.23    4     foo     1e-12
    detector2.1   4.56    6     bar     NaN
    detector3.1   7.89    7     foo     1.2e-12

Subject files can contain multiple columnar data blocks with individual
validity periods, blocks can be overriden, vary by set of columns, etc. By
default, the validity period is specified by ``runs=`` metadata key and set
of columns can be changed by ``columns=``:

.. code-block:: cfg

    runs=1-10
    # this will make block below to be valid only for runs #1-10
    columns=magnitude,module,label,suppression
    detector1.1   1.23    4     foo     1e-12
    detector2.1   4.56    6     bar     NaN
    detector3.1   7.89    7     foo     1.2e-12

    runs=5
    # this data block will be valid only for run #5
    columns=suppression,label
    detector2.1   2e-12     foo

For this specification above, data queried for run #5 will contain additional
(overriden or conflicting, depending on way how one interpret it in C++ side)
entry for ``detector2.1`` setting ``suppression`` and ``label`` columns only.
This way one can selectively override small amount of entries for few runs
within larger validity periods (a common case for systems under
maintenance/severe instability, etc).
