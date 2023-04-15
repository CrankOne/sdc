.. _tutorial:

Tutorial
========

This short tutorial consider very basic usage of SDC API. It shows how to
introduce a data type, define a parsing routine and utilize it with simple
filesystem subtree.

Having set of data files one would probably start with very simple usage
scenario of direct retrieving set of updates. By having set of columnar ASCII
files it should be pretty easy to build a library of them and load it with
``load_data_for()`` utility function.

More elaborated and flexible scenario would imply more complicated patterns
like observer/subscriber design patter, DFS sort and parallel queues. SDC is
made to be compatible whith this kind of demands, yet it does not provide
this instumentation by itself.

Data Type
---------

Let's assume your library has some calibration data type defined as follows:

.. code-block:: c++

    struct ChannelCalibration {
        std::string label;
        int background;
        float scale;
        double covariance;
    };

It can be any kind of copyable type in C++, we put here some simple set of
attributes just for demo.

Data files
----------

A file containing calibration information has simple grammar:

 * comment lines starting from certaing char will be ignored (`#` by default)
 * lines containing metadata delimiter (`=` by default) will be interpreted
   as *metadata* definitions
 * non-empty lines matching certain expresion are considered as tabular data

Although the grammar is assumed to be rather intuitive, see the dedicated
documentation section for futher details.

Imagine then that you have a file structure like this:

.. code-block::

    ├── modifications/
    │   └── erratum.txt
    └── main.txt

Where, for instance, ``main.txt`` keeps general data:

.. code-block:: cfg
   :caption: main.txt
   
    # A testing run
    runs=1
    columns=label,background,scale,covariance
    DET1-1      99      9.99        9.99

    # Runs 2-10 went as usual, except for few minor distractions caused by weather
    runs=2-10
    columns=label,background,scale,covariance
    DET1-1      10      1.01        0.10
    DET1-2      20      0.85        0.09
    DET2-1      30      0.93        0.12

    # Starting from run #7 a new detector (2-2) was added to extend the acceptance,
    # detector 1-2 also was re-calibrated for better stability and background
    # reduction
    runs=7-15

    DET1-2       5      0.85        0.07
    DET2-2      10      1.01        0.10

...and ``corrections/`` dir is reserved for some ad-hoc corrections. For
instance:

.. code-block:: cfg
   :caption: correction/erratum.txt

    # During run #3 an experimental setups was struck by a lightning and we had
    # noise slightly higher than usual
    runs=3
    columns=covariance
    DET1-1      0.21
    DET1-2      0.17
    DET2-1      0.34

    # During runs #12,13 a harsh gamma-ray galactic jet illuminated the planet,
    # so background and scaling factor were different.
    runs=12-13
    columns=background,covariance
    DET1-2      50      1.2345e6
    DET2-2      63      2.3456e7

Then one would probably want to retrieve from this files a valid set of values
in the form of ``ChannelCalibration`` type. To make it possible one has to tell
SDC generic code how to interpret the data blocks and which kind of collection
must be used to keep this data. This kind of things can be done in modern C++
by specializing *template traits* in a certain way.

Parsing Routines
----------------

SDC data type traits type is called ``CalibDataTraits<>`` and it is not defined
by default. It should define following members:

1. Static string ``constexpr`` called ``typeName``; this static field is used
   in documents to refer to a particular data type while reading (one can
   combine various data types within a file).
2. A template alias for collection type, called ``Collection<>``. Referred
   template (usually, an ``std::vector`` or a ``std::map``, but one can
   refer to its own types) is used to collect items listed within blocks. For
   example, one can use map to uniquely index over labels in first column of
   the examplar document above.
3. A method of certain signature to parse line into data item
   called ``parse_line()``.
4. A method of certain signature to put item into collection
   called ``collect()``.

Let's assume following features of the ``ChannelCalibration`` data type:

1. Verbose name in text files will be ``"channels-calib"``
2. For the sake of simplicity, let's assume the collection to be an instance
   of ``std::vector<>``.
3. Collection method will not check the labels to be unique.

Then the definition of corresponding traits will be:

.. code-block:: c++
   :caption: Traits example.

    namespace sdc {
    template<>
    struct CalibDataTraits<ChannelCalibration> {
        static constexpr auto typeName = "channels-calib";
        template<typename T=ChannelCalibration> using Collection=std::vector<T>;

        template<typename T=ChannelCalibration>
        static inline void collect( Collection<T> & col
                                  , const T & item
                                  , const aux::MetaInfo & mi
                                  ) { col.push_back(item); }

        static ChannelCalibration
                parse_line( const std::string & line
                          , const aux::MetaInfo & m
                          );
    };
    }  // namespace sdc

To simplify common needs for ``parse_line()`` logic SDC offers a set of
utility routines for line tokenization and value parsing, which can be used
effectively with ``MetaInfo`` instance to contextually interpret the data in
columns:

.. code-block:: c++
   :caption: Simple line parsing function example

    namespace sdc {
    ChannelCalibration sdc::CalibDataTraits<ChannelCalibration>::parse_line(
            const std::string & line, const aux::MetaInfo & mi ) {
        // subject instance
        ChannelCalibration item;
        // tokenize line
        std::list<std::string> tokens = aux::tokenize(line);
        // use columns order provided in beforementioned `columns=' metadata
        // line to get the proxy object (a "CSV line") for easy by-column
        // retrieval
        aux::ColumnsOrder::CSVLine csv
                = mi.get<aux::ColumnsOrder>("columns").interpret(tokens);
        // now, once can set item's fields like
        item.label      = csv("label");
        item.background = csv("background");
        item.scale      = csv("scale");
        item.covariance = csv("covariance");
        return item;
    }
    }  // namespace sdc

In this example code we assume that all columns are given for a data type, yet
it is not the case for our ``erratum.txt`` file -- a bit more elaborated code
will be shown at the end of this tutorial.

A *metadata* is a very important feature as it sets the *validity*
and *data type* for certain tabular content, used to properly parse and
retrieve data blocks. It can be set from within a file, from filename, or
externally from C++ code. However type and validity must be set for every
block in one way or another.

Retrieving Updates
------------------

Now, one can use :cpp:func:`sdc::load_from_fs` template function to load relevant
information for runs by run number from ``main.txt``.

.. code-block:: c++

    int runNumber = 3;
    std::vector<ChannelCalibration> entries
        = sdc::load_from_fs<int, ChannelCalibration>("path/to/main.txt", runNumber );

Returned collection of entries will vary depending on the run number given.
If run not covered by the files is requested, no entries will be returned.
The :cpp:func:`sdc::load_from_fs` function can handle a whole directory recursively, but
first one have to slightly modify the entry parsing code of ``parse_line()``
function above to deal with the case when not full set of columns provided.

For entries specified multiple times SDC will provide updates based on their
order of appearance in the file or during file structure scan. These entries
will be consequently provided to corresponding ``parse_line()`` routine. One
can provide multiple files to ``load_from_fs<>()`` routine by separating paths
by column symbol (``:``), and by this routine it is guaranteed that:

1. If files and directories are given in the list, **files will be processed
   last**.
2. With group of files or directories, they will be processed **in alphabetic
   order**.
3. Directory entries will be scanned by alphabetical order (so in our example
   it is guaranteed that ``corrections/`` files will be read after ``main.txt``)

This way, ``parse_line()`` will be called multiple times, for every item of
corresponding run number.

Collisions and Defaults
-----------------------

How exactly case of colliding entry should be resolved depends on the user
implementation. For various use cases user might want to:

1. (Selectively) override values with new ones
2. Keep previous values for certain fields
3. Emit error as collision is prohibited

Since our ``erratum.txt`` seemed to imply rather first scenario, we have to
foresee some "undefined" values for ``csv()`` getters. This can be done by
providing second (default) argument to its call:

.. code-block:: c++
   :caption: Getting values with default values

    item.background = csv("background", 0);
    item.scale      = csv("scale",      -1.0);
    item.covariance = csv("covariance", std::nan("0"));

If one dump the collection returned by :cpp:func:`sdc::load_from_fs` a following sequence
will appear for "run 3":

.. code-block::

    DET1-1: background=10, scale=1.01, cov=0.1
    DET1-2: background=20, scale=0.85, cov=0.09
    DET2-1: background=30, scale=0.93, cov=0.12
    DET1-1: background=0, scale=-1, cov=0.21
    DET1-2: background=0, scale=-1, cov=0.17
    DET2-1: background=0, scale=-1, cov=0.34

One can resolve conflicts during ``parse_line()`` or ``collect()`` for certain
data type, relying on the beforementioned rules.
