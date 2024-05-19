#include <iostream>
#include "sdc.hh"

//                                                ____________________________
// _____________________________________________/ User's calibration data type
//
// Some data type, typically a POD struct (but you cal add complex copyable
// attributes here still)
struct Foo {
    std::string one;
    float two;
    int three;
    int four;
    float formulaResult;
};

//                                         ___________________________________
// ______________________________________/ User's calibration data type parser
//
// To be able to parse it one has to define partial a template implementation
// in so-called "traits". It is done by specializing implementation of
// template struct `template <typename T> sdc::CalibDataTraits`, like following:
namespace sdc {
template<>
struct CalibDataTraits<Foo> {
    // A `typeName` static constant expression must define textual name used
    // in calib files to identify the data. This string is usualy given as
    // argument to `type=` metadata, preceding CSV-like data blocks.
    static constexpr auto typeName = "Foo";

    // Define how to parse line from a tabular content into single entry of
    // calib data type. There are couple of helper functions to leverage the
    // tokenization and trimming of the string line, but you can rely on your
    // own.
    static Foo parse_line( const std::string & line
                         , size_t lineNo
                         , const aux::MetaInfo & mi
                         , const std::string & docID
                         , sdc::aux::LoadLog * loadLogPtr=nullptr
                         ) {
        // insantiate new calib data item to fill
        Foo item;
        // one can query metadata valid for current CSV block as following.
        int someFactor = mi.get<int>("someFactor");

        // If TFormula supported:
        float factor = mi.get<double>("someFactor", std::nan("0"));

        item.formulaResult = mi.get<double>("someFormula", std::nan("0"));

        // Use columns tokenization helper from SDC to split the line on
        // key/value pairs by using `columns=` metadata:
        auto values = mi.get<aux::ColumnsOrder>("columns")
                .interpret(aux::tokenize(line), loadLogPtr);
        // fill the calibration entry item. Syntax of the `()` requires
        // the "column name" and permits for optional "default" value. Here
        // we insist that column "one" must be specified always and have
        // default values for "two".
        item.one = values("one");
        item.two = values("two", 1.0)*someFactor;
        // If no default parameter given, `()`-operator returns string; one can
        // cast it using `sdc::aux::lexical_cast<>()`:
        item.three = aux::lexical_cast<int>(values("three"));
        item.four  = values("four", 0);
        return item;
    }

    // Collection type. For simplest case it can be STL list or vector, but
    // one can define own types of collections with complex mapping and
    // additional checks and collection rules, if it is needed.
    template<typename T=Foo> using Collection=std::list<T>;
    // ^^^ alternatively one may prefer:
    //template<typename T=Foo> using Collection=std::map<std::string, T>;

    // Defines how to append entry within templated collection. Here you can
    // define some checks and advanced stuff. For simple cases we just append
    // the container with new entry.
    template<typename T=Foo>
    static void collect( Collection<T> & dest
                       , const T & newEntry
                       , const aux::MetaInfo &  // you can use metainfo
                       , size_t lineNo
                       ) { dest.push_back(newEntry); }
    // ^^^ alternatively, for your `Collection=std::map<std::string, T>, use
    // for instance:
    //                   { dest.emplace(newEntry.name, newEntry); }

};  // struct CalibDataTraits
}  // namespace sdc

//                                              _______________________________
// ___________________________________________/ Entry point with example usage

int
main(int argc, char * argv[]) {
    // Desired type to index calib data (provided in `runs=` metadata block in
    // documents)
    typedef int RunType;

    std::string docsPath = "../tests/assets/test1/one.txt";
    int runNo = 8458;
    if(argc == 3) {
        docsPath = argv[1];
        runNo = atoi(argv[2]);
    }

    std::ostream & os = std::cout;

    // Initialization
    ////////////////

    // Instantiate calibration documents collection.
    // It keeps all the discovered calibration items together with their
    // indices. Template parameter is your calibration key type (run number,
    // date, time, etc)
    sdc::Documents<RunType> docs;

    // Instantiate loader object
    // Loaders provide acces to the documents; loader instance essentially
    // defines acces to the documents and their particular grammar. SDC
    // provides `ExtCSVLoader` as a standard one
    docs.loaders.push_back(std::make_shared<sdc::ExtCSVLoader<RunType>>());

    // Simplest way to add files to the index
    // This method has number of useful, bot non-mandatory args for defaults:
    // default data type, default validity range, supplementary metadata to 
    // be provided to the parser, etc
    bool added = docs.add(docsPath);
    if(!added) {
        std::cerr << "Error: failed to add entries from \""
            << docsPath << "\"" << std::endl;
        return 1;
    }

    // Usage
    ///////
    
    os << "{\"index\":";
    docs.dump_to_json(os);

    #if 0
    // This is simplest possible retrieve of the collection of items valid for
    // given period. In our examplar documents we difned few entries for
    // different ranges and that's how collections of entries can be
    // retrieved (note that collection type is the `Collection` template from
    // traits above):
    std::list<Foo> entries = docs.load<Foo>(runNo);
    std::cout << "Loaded " << entries.size() << " entries for run #" << runNo << " (one: two three):" << std::endl;
    for(const auto & entry : entries) {
        std::cout << entry.one << ":\t" << entry.two << "\t" << entry.three << std::endl;
    }
    #else
    sdc::aux::LoadLog loadLog;
    os << ",\"updates\":";
    auto updates = docs.validityIndex.updates(sdc::CalibDataTraits<Foo>::typeName, runNo, false);
    bool isFirst = true;
    os << "[";
    typename sdc::CalibDataTraits<Foo>::template Collection<> dest;
    for(const auto & updEntry : updates) {
        if(!isFirst) os << ","; else isFirst = false;
        os << "{\"key\":\"" << sdc::ValidityTraits<RunType>::to_string(updEntry.first) << "\",\"update\":";
        updEntry.second->to_json(os);
        os << "}";
        docs.load_update_into<Foo>(updEntry, dest, runNo, &loadLog);
    }
    os << "],\"loadLog\":";
    loadLog.to_json(os);
    os << "}";
    #endif
}

