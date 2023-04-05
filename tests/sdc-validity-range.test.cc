#include "sdc.hh"

#include <gtest/gtest.h>

TEST( ValidityRangeTest, fullOpenRangeSpansEverywhere ) {
    using sdc::ValidityRange;
    typedef sdc::ValidityTraits<int> VT;
    ValidityRange<int> r{ VT::unset, VT::unset };
    EXPECT_TRUE( (bool) r );
}

TEST( ValidityRangeTest, halfOpenRangeConsideredAsTrue ) {
    using sdc::ValidityRange;
    typedef sdc::ValidityTraits<int> VT;

    {
        ValidityRange<int> r{ 1, VT::unset };
        EXPECT_TRUE( (bool) r );
    }

    {
        ValidityRange<int> r{ VT::unset, 1 };
        EXPECT_TRUE( (bool) r );
    }
}

TEST( ValidityRangeTest, closedRangeIsTrue ) {
    using sdc::ValidityRange;

    ValidityRange<int> r{ 5, 10 };
    EXPECT_TRUE( (bool) r );
}

TEST( ValidityRangeTest, TrivialEmptyRangeIsFalse ) {
    using sdc::ValidityRange;

    ValidityRange<int> r{ 10, 10 };
    EXPECT_FALSE( (bool) r );
}

TEST( ValidityRangeTest, EmptyRangeIsFalse ) {
    using sdc::ValidityRange;

    ValidityRange<int> r{ 11, 10 };
    EXPECT_FALSE( (bool) r );
}

TEST( ValidityRangeTest, IntersectionIsValid ) {
    using sdc::ValidityRange;
    typedef sdc::ValidityTraits<int> VT;

    struct {
        const ValidityRange<int> v[2];
        ValidityRange<int> result;
        bool expected;
    } cases[] = {
        // Intersection with full open range
        { {{ VT::unset, VT::unset }, { VT::unset, VT::unset }}, { VT::unset, VT::unset }, true  },  // case #0
        { {{ VT::unset,        10 }, { VT::unset, VT::unset }}, { VT::unset,        10 }, true  },  // case #1
        { {{        10, VT::unset }, { VT::unset, VT::unset }}, {        10, VT::unset }, true  },  // case #2
        { {{        10,        10 }, { VT::unset, VT::unset }}, {        10,        10 }, false },  // case #3
        { {{        10,        20 }, { VT::unset, VT::unset }}, {        10,        20 }, true  },  // case #4
        { {{        20,        10 }, { VT::unset, VT::unset }}, {        20,        10 }, false },  // case #5
        // Intersection with semi-open range, edge cases, right bound
        { {{ VT::unset, VT::unset }, { VT::unset,        10 }}, { VT::unset,        10 }, true  },  // case #6
        { {{ VT::unset,        10 }, { VT::unset,        10 }}, { VT::unset,        10 }, true  },  // case #7
        { {{        10, VT::unset }, { VT::unset,        10 }}, {        10,        10 }, false },  // case #8
        { {{        10,        10 }, { VT::unset,        10 }}, {        10,        10 }, false },  // case #9
        { {{        10,        20 }, { VT::unset,        10 }}, {        10,        10 }, false },  // case #10
        { {{        20,        10 }, { VT::unset,        10 }}, {        20,        10 }, false },  // case #11
        // Intersection with semi-open range, edge cases, left bound
        { {{ VT::unset, VT::unset }, { 10,        VT::unset }}, {        10, VT::unset }, true  },  // case #12
        { {{ VT::unset,        10 }, { 10,        VT::unset }}, {        10,        10 }, false },  // case #13
        { {{        10, VT::unset }, { 10,        VT::unset }}, {        10, VT::unset }, true  },  // case #14
        { {{        10,        10 }, { 10,        VT::unset }}, {        10,        10 }, false },  // case #15
        { {{        10,        20 }, { 10,        VT::unset }}, {        10,        20 }, true  },  // case #16
        { {{        20,        10 }, { 10,        VT::unset }}, {        20,        10 }, false },  // case #17
        // Intersection with semi-open range, right bound
        { {{ VT::unset, VT::unset }, { VT::unset,        15 }}, { VT::unset,        15 }, true  },  // case #18
        { {{ VT::unset,        10 }, { VT::unset,        15 }}, { VT::unset,        10 }, true  },  // case #19
        { {{        10, VT::unset }, { VT::unset,        15 }}, {        10,        15 }, true  },  // case #20
        { {{        10,        10 }, { VT::unset,        15 }}, {        10,        10 }, false },  // case #21
        { {{        10,        20 }, { VT::unset,        15 }}, {        10,        15 }, true  },  // case #22
        { {{        20,        10 }, { VT::unset,        15 }}, {        20,        10 }, false },  // case #23
        // Intersection with semi-open range, left bound
        { {{ VT::unset, VT::unset }, { 15,        VT::unset }}, {        15, VT::unset }, true  },  // case #24
        { {{ VT::unset,        10 }, { 15,        VT::unset }}, {        15,        10 }, false },  // case #25
        { {{        10, VT::unset }, { 15,        VT::unset }}, {        15, VT::unset }, true  },  // case #26
        { {{        10,        10 }, { 15,        VT::unset }}, {        15,        10 }, false },  // case #27
        { {{        10,        20 }, { 15,        VT::unset }}, {        15,        20 }, true  },  // case #28
        { {{        20,        10 }, { 15,        VT::unset }}, {        20,        10 }, false },  // case #29
        // Fully set intersections
        { {{        10,        20 }, {        10,        20 }}, {        10,        20 }, true  },  // case #30
        { {{        12,        18 }, {        10,        20 }}, {        12,        18 }, true  },  // case #31
        { {{        10,        18 }, {        12,        20 }}, {        12,        18 }, true  },  // case #32
        { {{        10,        15 }, {        15,        20 }}, {        15,        15 }, false },  // case #33
        { {{        10,        12 }, {        18,        20 }}, {        18,        12 }, false },  // case #34
    };

    for( size_t i = 0; i < sizeof(cases)/sizeof(*cases); ++i ) {
        for( int order = 0; order < 2; ++order ) {
            const auto c = cases[i];
            ValidityRange<int> r = order ? (c.v[0] & c.v[1]) : (c.v[1] & c.v[0]);
            EXPECT_EQ( r.from, c.result.from ) << " for case #" << i << " order=" << order;
            EXPECT_EQ( r.to,   c.result.to   ) << " for case #" << i << " order=" << order;
            EXPECT_EQ( c.expected, (bool) r )  << " for case #" << i << " order=" << order;
        }
    }
}

// (deprected) tests for `inv_eq_range()`

TEST(InvEqRange, EmptyForEmptyRange) {
    std::multimap<int, std::string> m;
    auto rng = sdc::aux::inv_eq_range(m, 123);
    EXPECT_EQ( rng.first, rng.second );
    EXPECT_EQ( rng.first, m.begin() );
}

TEST(InvEqRange, ValidForSingularOpenRange) {
    std::multimap<int, std::string> m;
    m.emplace(15, "foo");
    auto rng = sdc::aux::inv_eq_range(m, -999);
    EXPECT_EQ( rng.first, rng.second );
    EXPECT_EQ( rng.first, m.begin() );
    rng = sdc::aux::inv_eq_range(m, 15);
    EXPECT_EQ( rng.first,  m.begin() );
    EXPECT_EQ( rng.second, m.end() );
    rng = sdc::aux::inv_eq_range(m, 999);
    EXPECT_EQ( rng.first,  m.begin() );
    EXPECT_EQ( rng.second, m.end() );
}

TEST(InvEqRange, ValidForNonSingularOpenRange) {
    std::multimap<int, std::string> m;
    m.emplace(15, "foo");
    m.emplace(15, "bar");
    auto rng = sdc::aux::inv_eq_range(m, -999);
    EXPECT_EQ( rng.first, rng.second );
    EXPECT_EQ( rng.first, m.begin() );
    rng = sdc::aux::inv_eq_range(m, 15);
    EXPECT_EQ( rng.first,  m.begin() );
    EXPECT_EQ( rng.second, m.end() );
    rng = sdc::aux::inv_eq_range(m, 999);
    EXPECT_EQ( rng.first,  m.begin() );
    EXPECT_EQ( rng.second, m.end() );
}

TEST(InvEqRange, ChoosesValidRanges) {
    std::multimap< int, std::string> m;

    m.emplace(  31, "31-a" );  // breaks order
    m.emplace(   5, "05-a" );
    m.emplace(  10, "10-a" );
    m.emplace(  10, "10-b" );
    m.emplace(  10, "10-c" );
    m.emplace(  14, "14-a" );
    m.emplace(  19, "19-a" );
    m.emplace(  19, "19-b" );
    m.emplace(  20, "20-a" );
    m.emplace(  30, "30-a" );
    m.emplace(  30, "30-b" );
    m.emplace(  31, "31-b" );

    struct {
        int key;
        std::vector<std::string> expectedUpdates;
    } expected[] = {
              { -999, {} }  // nothing
            , { 3,    {} }  // nothing
            , { 5,    {"05-a"} }
            , { 10,   {"10-a", "10-b", "10-c"} }
            , { 13,   {"10-a", "10-b", "10-c"} }
            , { 15,   {"14-a"} }
            , { 19,   {"19-a", "19-b"} }
            , { 20,   {"20-a"} }
            , { 25,   {"20-a"} }
            , { 30,   {"30-a", "30-b"} }
            , { 32,   {"31-a", "31-b"} }
            , { 999,  {"31-a", "31-b"} }
        };

    for( size_t i = 0; i < sizeof(expected)/sizeof(*expected); ++i ) {
        const auto & entry = expected[i];
        auto rng = sdc::aux::inv_eq_range(m, entry.key);
        size_t count = 0;
        for( auto it = rng.first; it != rng.second; ++it ) {
            ASSERT_LT(count, entry.expectedUpdates.size())
                << " at entry #" << i << " (" << entry.key << ") -> "
                << entry.expectedUpdates[0] << " ...";
            EXPECT_EQ( it->second, entry.expectedUpdates[count++] )
                << " at entry #" << i << " (" << entry.key << ") -> "
                << entry.expectedUpdates[0] << " ...";
        }
        EXPECT_EQ(count, entry.expectedUpdates.size());
    }
}
