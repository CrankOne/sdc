#include <iostream>
#include "sdc.hh"

struct Foo {
    std::string one;
    float two;
    int three;
    int four;
    float formulaResult;
};

namespace sdc {
template<>
struct CalibDataTraits<Foo> {
    static constexpr auto typeName = "Foo";

    static Foo parse_line( const std::string & line
                         , size_t lineNo
                         , const aux::MetaInfo & mi
                         , const std::string & docID
                         , sdc::aux::LoadLog * loadLogPtr=nullptr
                         ) {
        Foo item;
        int someFactor = mi.get<int>("someFactor");
        item.formulaResult = mi.get<double>("someFormula", std::nan("0"));
        auto values = mi.get<aux::ColumnsOrder>("columns")
                .interpret(aux::tokenize(line), loadLogPtr);
        item.one = values("one");
        item.two = values("two", 1.0)*someFactor;
        item.three = aux::lexical_cast<int>(values("three"));
        item.four  = values("four", 0);
        return item;
    }

    template<typename T=Foo> using Collection=std::list<T>;

    template<typename T=Foo>
    static void collect( Collection<T> & dest
                       , const T & newEntry
                       , const aux::MetaInfo &  // you can use metainfo
                       , size_t lineNo
                       ) { dest.push_back(newEntry); }

};  // struct CalibDataTraits
}  // namespace sdc

//                                              _______________________________
// ___________________________________________/ Entry point with example usage

int
main(int argc, char * argv[]) {
    typedef int RunType;

    std::string docsPath;
    int runNo = -1;
    if(argc == 3) {
        docsPath = argv[1];
        runNo = atoi(argv[2]);
    }
    if(runNo < 0) {
        std::cerr << "Error: negative run number: " << runNo << std::endl;
        return 1;
    }
    if(docsPath.empty()) {
        std::cerr << "Error: empty source documents path." << std::endl;
        return 1;
    }

    sdc::Documents<RunType> docs;
    docs.loaders.push_back(std::make_shared<sdc::ExtCSVLoader<RunType>>());
    bool added = docs.add(docsPath);
    if(!added) {
        std::cerr << "Error: failed to add entries from \""
            << docsPath << "\"" << std::endl;
        return 1;
    }

    return sdc::json_loading_log<Foo, RunType>(runNo, docs, std::cout);
}

