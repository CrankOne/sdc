#include <iostream>
#include "sdc.hh"

// Some data type with some columns
struct Foo {
    int one;
    float two;
    int three;
    int four;
};

// To be able to parse it one has to define partial a template implementation
// in so-called "traits".
namespace sdc {
template<>
struct CalibDataTraits {
    // A `typeName` static constant expression must define textual name used
    // in calib files to identify the data
    static constexpr auto typeName = "foo";

    // Define how to parse line from a tabular content into single entry of
    // calib data type. There are couple of 
    static Foo parse_line( const std::string & line
                         , size_t lineNo
                         , const aux::MetaInfo & mi
                         , const std::string & filename
                         ) {

    }

    // Collection type. For simplest case it can be STL list or vector, but
    // one can define own types of collections with complex mapping and
    // additional checks and collection rules, if it is needed.
    template<typename T=Foo> using Collection=std::list<T>;
    // Defines how to append entry within templated collection. Here you can
    // define some checks and advanced stuff. For simple cases we just append
    // the container with new entry.
    template<typename T=Foo>
    static void collect( Collection<T> & dest
                       , const T & newEntry
                       , const aux::MetaInfo &  // you can use metainfo
                       , size_t  // line number
                       ) { dest.push_back(newEntry); }

};  // struct CalibDataTraits
}  // namespace sdc

int
main(int argc, char * argv[]) {

}

