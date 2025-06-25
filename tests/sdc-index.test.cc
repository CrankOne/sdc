#include "sdc-base.hh"
#include "sdc.hh"

#include <gtest/gtest.h>

// This file tests general updates querying for index object.
// No type information on the particular document is required at this level.

// Just an ampty index to test trivial case
// --
// Still important case for basic functions.
class EmptyTestingIndex : public ::testing::Test
                        , public ::sdc::ValidityIndex<int, sdc::aux::MetaInfo>
                        {};

TEST_F(EmptyTestingIndex, ThrowsErrorOnNoType) {
    auto u = updates( "WrongType", 123, true );
    EXPECT_TRUE(u.empty());
    // TODO errors::NoData
}

TEST_F(EmptyTestingIndex, ReturnsEmptyUpdateOnNoExceptNoType) {
    sdc::ValidityIndex<int, sdc::aux::MetaInfo>::Updates u;
    // overlay=true
    EXPECT_THROW( u = updates( "WrongType", 123, false )
                , sdc::errors::UnknownDataType );
    // TODO errors::NoData
}

// Index with single entry with open bound
// --
// Corresponds to case when single entry defines validity period that starts
// from certain data and lasts forever
class OpenSingularTestingIndex : public ::testing::Test
                               , public ::sdc::ValidityIndex<int, sdc::aux::MetaInfo>
                               {
protected:
    void SetUp() override {
        sdc::aux::MetaInfo mi;
        add_entry( "The Band of the Hawk"  // document ID
              , "Battle Beast"  // typename
              , 10, sdc::ValidityTraits<int>::unset  // validity period
              , mi
              );
    }
};


TEST_F(OpenSingularTestingIndex, ThrowsErrorOnNoType) {
    // overlay=true
    auto us = updates( "WrongType", 123, true );
    EXPECT_TRUE(us.empty());
    // overlay=false
    us = updates( "WrongType", 123, true );
    EXPECT_TRUE(us.empty());
}

TEST_F(OpenSingularTestingIndex, ReturnsEmptyUpdateOnNoExceptNoType) {
    Updates us;
    // overlay=true
    EXPECT_THROW( us = updates( "WrongType", 123, false )
                , sdc::errors::UnknownDataType );
    // overlay=false
    EXPECT_THROW( us = updates( "WrongType", 123, false )
                , sdc::errors::UnknownDataType );
}

TEST_F(OpenSingularTestingIndex, FindsProperUpdate ) {
    Updates::value_type l;
    auto us = updates( "Battle Beast", 10, true); {
        EXPECT_EQ(us.size(), 1);
        const auto & entry = *us.begin();
        ASSERT_TRUE( entry.second );
        ASSERT_EQ( entry.first, 10 );
        const auto & update = *entry.second;
        EXPECT_EQ( update.docID, "The Band of the Hawk" );
        EXPECT_FALSE( sdc::ValidityTraits<int>::is_set(update.validTo) );
        //EXPECT_EQ( update.lineNo, 123 );
    }
    us = updates( "Battle Beast", 10, false); {
        EXPECT_EQ(us.size(), 1);
        const auto & entry = *us.begin();
        ASSERT_TRUE( entry.second );
        ASSERT_EQ( entry.first, 10 );
        const auto & update = *entry.second;
        EXPECT_EQ( update.docID, "The Band of the Hawk" );
        EXPECT_FALSE( sdc::ValidityTraits<int>::is_set(update.validTo) );
        //EXPECT_EQ( update.lineNo, 123 );
    }
    l = latest("Battle Beast", 10); {
        EXPECT_EQ( l.second->docID,  "The Band of the Hawk" );
        EXPECT_FALSE( sdc::ValidityTraits<int>::is_set(l.second->validTo) );
        //EXPECT_EQ( l.second->lineNo, 123 );
    }

    us = updates( "Battle Beast", 999, true); {
        EXPECT_EQ(us.size(), 1);
        const auto & entry = *us.begin();
        ASSERT_TRUE( entry.second );
        ASSERT_EQ( entry.first, 10 );
        const auto & update = *entry.second;
        EXPECT_EQ( update.docID, "The Band of the Hawk" );
        EXPECT_FALSE( sdc::ValidityTraits<int>::is_set(update.validTo) );
        //EXPECT_EQ( update.lineNo, 123 );
    }
    us = updates( "Battle Beast", 999, false); {
        EXPECT_EQ(us.size(), 1);
        const auto & entry = *us.begin();
        ASSERT_TRUE( entry.second );
        ASSERT_EQ( entry.first, 10 );
        const auto & update = *entry.second;
        EXPECT_EQ( update.docID, "The Band of the Hawk" );
        EXPECT_FALSE( sdc::ValidityTraits<int>::is_set(update.validTo) );
        //EXPECT_EQ( update.lineNo, 123 );
    }
    l = latest("Battle Beast", 10); {
        EXPECT_EQ( l.second->docID,  "The Band of the Hawk" );
        EXPECT_FALSE( sdc::ValidityTraits<int>::is_set(l.second->validTo) );
        //EXPECT_EQ( l.second->lineNo, 123 );
    }
}

TEST_F( OpenSingularTestingIndex, HandlesEmptyUpdateOnOutOfRange ) {
    auto us = updates( "Battle Beast", 9, true);
    EXPECT_TRUE(us.empty());
    us = updates( "Battle Beast", 9, false);
    EXPECT_TRUE(us.empty());

    Updates::value_type l;
    EXPECT_THROW( l = latest("Battle Beast", 9)
                , sdc::errors::NoCalibrationData );
}

// Index with multiple entries with closed and open bounds
// --
// More close to practical usage
class TestingIndex : public ::testing::Test
                   , public ::sdc::ValidityIndex<int, sdc::aux::MetaInfo>
                   {
protected:
    // You can remove any or all of the following functions if their bodies would
    // be empty.
    //OpenSingularTestingIndex() : ::sdc::Index<int>() {}
    //~OpenSingularTestingIndex() override {}
    void SetUp() override {
        {
            sdc::aux::MetaInfo mi;
            mi.set("one", "1");
            add_entry( "The Band of the Hawk"  // document ID
                  , "Battle Beast"  // typename
                  , 10, sdc::ValidityTraits<int>::unset  // validity period
                  , mi
                  );
        }
        {
            sdc::aux::MetaInfo mi;
            mi.set("two", "2");
            add_entry( "King For A Day"  // document ID
                  , "Battle Beast"  // typename
                  , 10, 15  // validity period
                  , mi
                  );
            add_entry( "No More Hollywood Endings"  // document ID
                  , "Battle Beast"  // typename
                  , 10, 50  // validity period
                  , mi
                  );
            mi.set("three", "3");
            add_entry( "No More Hollywood Endings"  // document ID
                  , "Battle Beast"  // typename
                  , 15, 60  // validity period
                  , mi
                  );
        }
        {
            sdc::aux::MetaInfo mi;
            mi.set("three", "3", 25);
            add_entry( "Blind Trust"  // document ID
                  , "Cabaret Nocturne"  // typename
                  , 15, 25  // validity period
                  , mi
                  );
        }
    }
};

TEST_F(TestingIndex, FindsProperUpdatesAtStart ) {
    struct {
        int period[2];
        std::string name;
        std::map<std::string, std::string> mi;
    } expected[] = {
        { {10, -1}, "The Band of the Hawk",        {{"one",   "1"}} },
        { {10, 15}, "King For A Day",              {{"two",   "2"}} },
        { {10, 50}, "No More Hollywood Endings",   {{"two",   "2"}} },
    };
    auto us = updates( "Battle Beast", 10); {
        ASSERT_EQ(us.size(), sizeof(expected)/sizeof(*expected));
        int i = 0;
        for( const auto & entry : us ) {
            const auto & expOne = expected[i++];
            ASSERT_EQ( entry.first, expOne.period[0] );
            ASSERT_TRUE(entry.second);
            const auto & update = *entry.second;
            EXPECT_EQ( update.docID, expOne.name );
            if( expOne.period[1] == -1 ) {
                EXPECT_FALSE( sdc::ValidityTraits<int>::is_set(update.validTo) );
            } else {
                EXPECT_EQ( expOne.period[1], update.validTo );
            }
            // check metadata
            EXPECT_EQ( update.auxInfo.size(), expOne.mi.size() );
            for( auto mie : expOne.mi ) {
                EXPECT_EQ( mie.second, update.auxInfo.get_strexpr(mie.first) );
            }
        }
        // must return latest inserted of this type
        Updates::value_type l = latest("Battle Beast", 10); {
            const auto & expOne = expected[sizeof(expected)/sizeof(*expected)-1];
            EXPECT_EQ( l.second->docID, expOne.name );
            EXPECT_EQ( l.second->validTo, expOne.period[1] );
        }
    }
}

TEST_F(TestingIndex, FindsProperUpdatesAtMiddle ) {
    struct {
        int period[2];
        std::string name;
        std::map<std::string, std::string> mi;
    } expected[] = {
        { {10, -1}, "The Band of the Hawk",        {{"one",   "1"}} },
        { {10, 50}, "No More Hollywood Endings",   {{"two",   "2"}} },
        { {15, 60}, "No More Hollywood Endings",   {{"two",   "2"}, {"three", "3"}} },
    };
    auto us = updates( "Battle Beast", 15); {
        ASSERT_EQ(us.size(), sizeof(expected)/sizeof(*expected));
        int i = 0;
        for( const auto & entry : us ) {
            const auto & expOne = expected[i++];
            ASSERT_EQ( entry.first, expOne.period[0] );
            ASSERT_TRUE(entry.second);
            const auto & update = *entry.second;
            EXPECT_EQ( update.docID, expOne.name );
            if( expOne.period[1] == -1 ) {
                EXPECT_FALSE( sdc::ValidityTraits<int>::is_set(update.validTo) );
            } else {
                EXPECT_EQ( expOne.period[1], update.validTo );
            }
            // check metadata
            EXPECT_EQ( update.auxInfo.size(), expOne.mi.size() );
            for( auto mie : expOne.mi ) {
                EXPECT_EQ( mie.second, update.auxInfo.get_strexpr(mie.first) );
            }
        }
        // must return latest inserted of this type
        Updates::value_type l = latest("Battle Beast", 15); {
            const auto & expOne = expected[sizeof(expected)/sizeof(*expected)-1];
            EXPECT_EQ( l.second->docID, expOne.name );
            EXPECT_EQ( l.second->validTo, expOne.period[1] );
        }
        l = latest("Cabaret Nocturne", 24); {
            EXPECT_EQ( l.second->docID, "Blind Trust" );
            EXPECT_EQ( l.second->validTo, 25 );
        }
    }
}

TEST_F(TestingIndex, FindsProperUpdatesAtEnd ) {
    struct {
        int period[2];
        std::string name;
        std::map<std::string, std::string> mi;
    } expected[] = {
        { {10, -1}, "The Band of the Hawk",        {{"one",   "1"}} },
    };
    // must return latest inserted of this type
    Updates::value_type l = latest("Battle Beast", 999); {
        const auto & expOne = expected[sizeof(expected)/sizeof(*expected)-1];
        EXPECT_EQ( l.second->docID, expOne.name );
        EXPECT_FALSE( sdc::ValidityTraits<int>::is_set( l.second->validTo ) );
    }
    auto us = updates( "Battle Beast", 999); {
        ASSERT_EQ(us.size(), sizeof(expected)/sizeof(*expected));
        int i = 0;
        for( const auto & entry : us ) {
            const auto & expOne = expected[i++];
            ASSERT_EQ( entry.first, expOne.period[0] );
            ASSERT_TRUE(entry.second);
            const auto & update = *entry.second;
            EXPECT_EQ( update.docID, expOne.name );
            if( expOne.period[1] == -1 ) {
                EXPECT_FALSE( sdc::ValidityTraits<int>::is_set(update.validTo) );
            } else {
                EXPECT_EQ( expOne.period[1], update.validTo );
            }
            // check metadata
            EXPECT_EQ( update.auxInfo.size(), expOne.mi.size() );
            for( auto mie : expOne.mi ) {
                EXPECT_EQ( mie.second, update.auxInfo.get_strexpr(mie.first) );
            }
        }
    }
}

TEST_F(TestingIndex, HandlesEmptyUpdateOnOutOfRange ) {
    auto us = updates( "Battle Beast", 9, true);
    EXPECT_TRUE(us.empty());
    us = updates( "Battle Beast", 9, false);
    EXPECT_TRUE(us.empty());

    Updates::value_type l;
    EXPECT_THROW( l = latest("Battle Beast", 9)
                , sdc::errors::NoCalibrationData );

    us = updates( "Cabaret Nocturne", 9, true);
    EXPECT_TRUE(us.empty());
    us = updates( "Cabaret Nocturne", 9, false);
    EXPECT_TRUE(us.empty());

    EXPECT_THROW( l = latest("Cabaret Nocturne", 9)
                , sdc::errors::NoCalibrationData );
}

TEST(Updates, complexCaseProperlySorted) {
    typedef sdc::iValidityIndex<int, sdc::iDocuments<int>::DocumentLoadingState>::DocumentEntry
        DocumentEntry;
    typedef sdc::iValidityIndex<int, sdc::iDocuments<int>::DocumentLoadingState>::Updates
        Updates;

    std::list<std::pair<std::string, size_t>> excPaths = {
        {"one/1",       12},
        {"three/c",     10},
        {"two/b",       10},
        {"two/z/seven", 10},
        {"three/b",     10},
        {"two/a",       10},
        {"three/a",     10},
        {"one/2",       10},
        {"two/z/seven", 15},
        {"one/1",       10},
    };

    std::vector<std::pair<std::string, size_t>> chck = {
        {"three/a", 10}, {"three/b", 10}, {"three/c", 10},
        {"two/z/seven", 10}, {"two/z/seven", 15},
        {"two/a", 10}, {"two/b", 10},
        {"one/1", 10}, {"one/1", 12}, {"one/2", 10}
    };
    ASSERT_EQ(chck.size(), excPaths.size());

    std::vector<std::string> bases = {
        "three/",
        "two/z",
        "two/",
        "one"
    };

    {
        std::list<DocumentEntry> updStorage;
        Updates upds;
        for(const auto & pp: excPaths) {
            DocumentEntry docEntry;
            docEntry.docID = pp.first;
            docEntry.auxInfo.dataBlockBgn = pp.second;
            updStorage.push_back(docEntry);
            upds.push_back({123, &updStorage.back()});
        }

        sdc::iDocuments<int>::sort_updates(upds, bases);

        ASSERT_EQ(upds.size(), excPaths.size());

        size_t nEl = 0;
        for(const auto & item : upds) {
            EXPECT_EQ(item.second->docID, chck[nEl].first);
            EXPECT_EQ(item.second->auxInfo.dataBlockBgn, chck[nEl].second)
                << " element #" << nEl << " (" << chck[nEl].first << ")";
            ++nEl;
        }
    }
}
