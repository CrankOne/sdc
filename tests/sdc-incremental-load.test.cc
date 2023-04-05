#include "sdc.hh"

#include <gtest/gtest.h>

/* Idea of this test is to check "incremental" loading of the calibration data.
 * It means, that for certain data structure (let it be a pair of integer
 * values, say, a and b) we do load data for its fields from multiple sources
 * allowing for a missed data. Resulting strucure must consist only of data
 * defined up to a certain moment.
 *
 *  Run #            a           b       Loaded data
 *    1    (undefined) (undefined)       a=undefined, b=undefined
 *    2    (undefined)           1       a=undeifned, b=1
 *    3             1  (undefined)       a=1, b=1
 *    4    (undefined) (undefined)       a=1, b=1
 *    5    (undefined)           2       a=1, b=2
 *    6             3            4       a=3, b=4 
 *
 * Having a complex validity ranges (with limited validity) we assume
 * following "documents" and loading results:
 *
 *  # "one":
 *       runs=6-...
 *  1    3 4
 *       run=1-...
 *  2    0 0
 *
 *  # "two":
 *      runs=2-3
 *  1   0 1
 *      run=5-...
 *  2   0 2
 *
 *  # "three":
 *      runs=3-...
 *  1   1 0
 *
 *  Expected results:
 *   run#     a b
 *      1   - 0 0  (taken from "one":2)
 *      2   - 0 1  (taken from "one":2 + "two":1)
 *      3   - 1 0  (taken from "one":2 + "three":1; "two":1 is not valid anymore)
 *      4   - 1 0  (taken from "one":2 + "three":1)
 *      5   - 1 2  (taken from "one":2 + "three":1 + "two":2)
 *      6   - 3 4  (taken from "one":2 + "three":1 + "two":2 + "one:1")
 *
 * The goal of mixing is to check that incremental loading algorithm correctly
 * addresses mixed incremental load (say, for run 3 it will pass through
 * "one":2, "two":1, "three":1).
 */

#if 0
namespace sdc {
namespace test {

/// A testing calibration data object for incremental reading
struct CalibData {
    int a, b;  // 0 means (undefined)
    CalibData() : a(0), b(0) {}
};

}  // namespace ::sdc::test

// Traits for testing type -- how to read and append
template<>
struct CalibDataTraits<test::CalibData> {
    // String key for the type
    static constexpr auto typeName = "TestData/Incremental";
    // A collection type of this incremental data
    typedef test::CalibData Collection;
    // Adds entry to the collection
    static inline void collect( Collection & c
                              , const test::CalibData & entry
                              , const aux::MetaInfo & mi
                              , size_t
                              ) {
        if( entry.a > 0 ) c.a = entry.a;
        if( entry.b > 0 ) c.b = entry.b;
    }
    // 
    static inline test::CalibData parse_line(
                                  const std::string & line
                                , size_t lineNo
                                , const aux::MetaInfo & m
                                , const std::string & filename
                                ) {
        test::CalibData r;
        // Parses CSV content of the line
        auto toks_ = sdc::aux::tokenize(line);
        std::vector<std::string> toks(toks_.begin(), toks_.end());
        assert( 2 == toks.size() );
        if( toks[0] != "undefined" ) r.a = sdc::aux::lexical_cast<int>(toks[0]);
        if( toks[1] != "undefined" ) r.b = sdc::aux::lexical_cast<int>(toks[1]);
        return r;
    }
};

namespace test {

static struct {
    const char name[32];
    struct {
        int runsRange[2];
        int a, b;
    } defs[2];  // max defs per doc
} docs[] = {
        { "one", {
          { { 6, 0 }, 3, 4 },
          { { 1, 0 }, 0, 0 },
        }},
        { "two", {
          { { 2, 3 }, 0, 1 },
          { { 5, 0 }, 0, 2 },
        }},
        { "three", {
          { { 3, 0 }, 1, 0 },
          { { 0, 0 }, 0, 0 },  // unused
        }}
    };

static const struct {
    int runNo, a, b;
    std::list<std::string> sources;
} expectedLoadings[] = {
    { 1,   0, 0, { "one:2" } },
    { 2,   0, 1, { "one:2", "two:1" } },
    { 3,   1, 0, { "one:2", "three:1" } },
    { 4,   1, 0, { "one:2", "three:1" } },
    { 5,   1, 2, { "one:2", "three:1", "two:2" } },
    { 6,   3, 4, { "one:2", "three:1", "two:2", "one:1" } },
};

// A testing document loader
//
// Does not really open a file, uses syntetic in-memory stream instead.
struct MockLoader : public Documents<int>::iLoader {
    bool can_handle(const std::string & docID) const override {
        return docID != "ignore me";
    }

    std::list<Documents<int>::DataBlock>
                get_doc_struct( const std::string & docID ) override {
        std::list<Documents<int>::DataBlock> r;
        for( size_t i = 0; i < sizeof(docs)/sizeof(*docs); ++i ) {
            const auto s = docs[i];
            if( docID != s.name ) continue;
            Documents<int>::DataBlock db
                    = {CalibDataTraits<test::CalibData>::typeName};
            for( int j = 0; j < 2; ++j ) {
                if( s.defs[j].runsRange[0] == s.defs[j].runsRange[0]
                 && s.defs[j].runsRange[0] == 0 ) continue;
                db.validityRange.from = s.defs[j].runsRange[0] == 0
                                      ? ValidityTraits<int>::unset
                                      : s.defs[j].runsRange[0];
                db.validityRange.to   = s.defs[j].runsRange[1] == 0
                                      ? ValidityTraits<int>::unset
                                      : s.defs[j].runsRange[1];
                r.push_back(db);
            }
            return r;
        }
        assert(0);  // unexpected doc ID
    }
    
    void read_data( const std::string & docID
                  , int k
                  , const std::string & forType
                  , std::function<bool ( const typename aux::MetaInfo &
                                       , size_t
                                       , const std::string &)> cllb
                  ) override {
        // Iterate over "documents" representation, find ones suitable for
        // given runs range and make string "line" to be parsed. This is
        // typically done by lookup within a file or something, but for testing
        // purposes we just pretend...
        for( size_t i = 0; i < sizeof(docs)/sizeof(*docs); ++i ) {
            const auto s = docs[i];
            if( docID != s.name ) continue;
            // ok, we found a doc -- locate definition now
            for( int j = 0; j < 2; ++j ) {
                const auto & cdef = s.defs[j];

                if( cdef.runsRange[0] && cdef.runsRange[0]  > k ) continue;
                if( cdef.runsRange[1] && cdef.runsRange[1] <= k ) continue;

                std::ostringstream oss;
                if( cdef.a ) {
                    oss << cdef.a;
                } else {
                    oss << "undefined";
                }
                oss << "\t";
                if( cdef.b ) {
                    oss << cdef.b;
                } else {
                    oss << "undefined";
                }
                aux::MetaInfo mi;
                cllb(mi, j, oss.str());
                return;
            }
            assert(0);
        }
        assert(0);  // unexpected doc ID
    }
};

}  // namespace ::sdc::test
}  // namespace sdc

class MockDocsIndex : public ::testing::Test
                    , public ::sdc::Documents<int>
                    {
protected:
    void SetUp() override {
        // add handler
        loaders.push_back(std::make_shared<sdc::test::MockLoader>());
        // add some "docs"
        add( "one" );
        add( "two" );
        add( "three" );
    }
};

TEST_F(MockDocsIndex, FixtureSetupComplete) {
    ASSERT_FALSE( add( "ignore me" ) );
    // ...
}

TEST_F(MockDocsIndex, LoadsCorrect) {
    for( size_t i = 0
       ; i < sizeof(sdc::test::expectedLoadings)/sizeof(*sdc::test::expectedLoadings)
       ; ++i ) {
        const auto & expected = sdc::test::expectedLoadings[i];
        sdc::test::CalibData d =
            load<sdc::test::CalibData>( sdc::test::expectedLoadings[i].runNo );
        EXPECT_EQ(d.a, expected.a) << " for " << expected.runNo;
        EXPECT_EQ(d.b, expected.b) << " for " << expected.runNo;
    }
}
#endif
