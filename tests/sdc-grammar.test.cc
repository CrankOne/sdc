#include "sdc.hh"

#include <gtest/gtest.h>

namespace sdc {
namespace test {

struct AuxInfo {};

//
// Test definitions
// - an example document in default grammar
static const char tstSDCTest1[] = R"TST(# This is a testing sample, 1
runs = 100-500
type=TestType1
columns = b, c

1   4.56
# ^^^ this CSV block starts at line #5
2	7.89    # blah blah, 8

3   0.12  # line #10

runs = 500-1000
type=TestType1
columns = a, b, c

1   4.56    0.12  # line #16
2   7.89	7.89  

3   0.12    4.56   # foo # bar
)TST";

TEST( ExtCSVLoader, defaultPreparsingValid ) {
    ExtCSVLoader<int> l;

    std::istringstream iss(tstSDCTest1);
    auto m = l.get_doc_struct(iss);
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(m.size(), 2);
    auto it = m.begin();
    {
        EXPECT_NE(it, m.end()) << " block at line 6 not found";
        EXPECT_EQ(it->validityRange.from, 100);
        EXPECT_EQ(it->validityRange.to,   501);
        EXPECT_EQ(it->dataType, "TestType1");
    }
    ++it;
    {
        EXPECT_NE(it, m.end()) << " block at line 16 not found";
        EXPECT_EQ(it->validityRange.from, 500);
        EXPECT_EQ(it->validityRange.to,   1001);
        EXPECT_EQ(it->dataType, "TestType1");
    }
}

TEST( ExtCSVLoader, SDCDefaultParsingValid ) {
    ExtCSVLoader<int> l;

    std::istringstream iss(tstSDCTest1);

    struct {
        char toks[4][32];
    } expected1[] = {
        {{"1", "4.56", ""}},
        {{"2", "7.89", ""}},
        {{"3", "0.12", ""}},
        {{""}}
    }, expected2[] = {
        {{"1", "4.56", "0.12", ""}},
        {{"2", "7.89", "7.89", ""}},
        {{"3", "0.12", "4.56", ""}},
        {{""}}
    };

    auto * expToks = expected2;
    int i = 0;
    l.read_data( iss, 600, "TestType1", 0
            , [&]( const aux::MetaInfo & mi
                 , const std::string & line
                 ){
                 size_t lineNo = mi.get<size_t>("@lineNo");
                 auto toks = aux::tokenize(line);
                 int j = 0;
                 for( const auto & tok : toks ) {
                    EXPECT_NE( expToks[i].toks[j][0], '\0' )
                        << "token #" << j << " of line #" << lineNo
                        << " (" << i << ")" << std::endl;
                    if( '\0' == expToks[i].toks[j][0] ) return false;
                    EXPECT_EQ( tok, expToks[i].toks[j] )
                        << "token #" << j << " of line #" << lineNo
                        << " (" << i << ")" << std::endl;
                    ++j;
                 }
                 ++i;
                 return true;
            } );

    expToks = expected1;
    i = 0;
    l.read_data( iss, 110, "TestType1", 0
            , [&]( const aux::MetaInfo & mi
                 , const std::string & line
                 ){
                 size_t lineNo = mi.get<size_t>("@lineNo");
                 auto toks = aux::tokenize(line);
                 int j = 0;
                 for( const auto & tok : toks ) {
                    EXPECT_NE( expToks[i].toks[j][0], '\0' )
                        << "token #" << j << " of line #" << lineNo
                        << " (" << i << ")" << std::endl;
                    if( '\0' == expToks[i].toks[j][0] ) return false;
                    EXPECT_EQ( tok, expToks[i].toks[j] )
                        << "token #" << j << " of line #" << lineNo
                        << " (" << i << ")" << std::endl;
                    ++j;
                 }
                 ++i;
                 return true;
            } );
}

// Some alternative grammar for "extended CSV"
//  - no comments
//  - metadata marker is `#` with single expression form expected
//  - CSV tokens are delimited with commas
//  - no type metadata (assumed to be externally set, i.e. by file name ot type)
//  - no runs range metadata (assumed to be externally set, i.e. by file name)
static const char tstSDCTest2[] = R"TST(#123 345
1,23, 45
2,34,56
3, 45,67
#234 567
1,67,89
2, 78, 91
)TST";

TEST( ExtCSVLoader, SDCParsingFailsOnNoType ) {
    // instantiate loader
    ExtCSVLoader<size_t> l;
    // customize grammar
    l.grammar.commentChar = '\0';  // no comments allowed
    l.grammar.metadataMarker = '#';  // different marker for metadata lines
    l.grammar.metadataKeyTag.clear();  // no metadata key tag
    l.grammar.metadataTypeTag.clear();  // no metadata type tag
    // check failure (no type)
    std::istringstream iss(tstSDCTest2);
    ASSERT_THROW( l.get_doc_struct(iss)
                , sdc::errors::NoDataTypeDefined );
}

TEST( ExtCSVLoader, SDCParsingFailsOnNoValidity ) {
    // instantiate loader
    ExtCSVLoader<size_t> l;
    // customize grammar
    l.grammar.commentChar = '\0';  // no comments allowed
    l.grammar.metadataMarker = '#';  // different marker for metadata lines
    l.grammar.metadataKeyTag.clear();  // no metadata key tag
    l.grammar.metadataTypeTag.clear();  // no metadata type tag
    l.defaults.dataType = "TestType2";
    // check failure (no type)
    std::istringstream iss(tstSDCTest2);
    ASSERT_THROW( l.get_doc_struct(iss)
                , sdc::errors::NoValidityRange );
}

TEST( ExtCSVLoader, SDCCustomizedParsingValid ) {
    // instantiate loader
    ExtCSVLoader<size_t> l;
    // customize the grammar
    l.grammar.commentChar = '\0';  // no comments allowed
    l.grammar.metadataMarker = '#';  // different marker for metadata lines
    l.grammar.metadataKeyTag.clear();  // no metadata key tag
    l.grammar.metadataTypeTag.clear();  // no metadata type tag

    l.defaults.validityRange.from = 1;
    l.defaults.validityRange.to  = 10;
    l.defaults.dataType = "TestType2";

    {
        std::istringstream iss(tstSDCTest2);
        auto m = l.get_doc_struct(iss);

        EXPECT_EQ( m.size(), 1 );
        // Document part(s) must have same type and validity range
        for( auto entry : m ) {
            EXPECT_EQ( entry.dataType, "TestType2" );
            EXPECT_EQ( entry.validityRange.from, 1 );
            EXPECT_EQ( entry.validityRange.to,  10 );
        }
    }

    // Read data + metadata and check
    const struct {
        char toks[4][32];
        std::string md;
    } expected[] = {
        {{"1", "23", "45", ""}, "123 345"},
        {{"2", "34", "56", ""}, "123 345"},
        {{"3", "45", "67", ""}, "123 345"},
        {{"1", "67", "89", ""}, "234 567"},
        {{"2", "78", "91", ""}, "234 567"},
        {{""}}
    };
    {
        size_t i = 0;
        std::istringstream iss(tstSDCTest2);
        l.read_data( iss, 5, "TestType2", 0
                   , [&]( const aux::MetaInfo & mi
                        , const std::string & line ) {
                    size_t lineNo = mi.get<size_t>("@lineNo");
                    auto toks = aux::tokenize(line, ',');
                    int j = 0;
                    for( const auto & tok : toks ) {
                       EXPECT_NE( expected[i].toks[j][0], '\0' )
                           << "token #" << j << " of line #" << lineNo
                           << " (" << i << ")" << std::endl;
                       if( '\0' == expected[i].toks[j][0] ) return false;
                       EXPECT_EQ( tok, expected[i].toks[j] )
                           << "token #" << j << " of line #" << lineNo
                           << " (" << i << ")" << std::endl;
                       EXPECT_EQ( mi.get<std::string>("", "", lineNo), expected[i].md )
                           << "token #" << j << " of line #" << lineNo
                           << " (" << i << ")" << std::endl;
                       ++j;
                    }
                    ++i;
                    return true;
            } );
    }
}

}  // namespace ::sdc::test
}  // namespace sdc

