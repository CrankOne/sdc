#include <cstdlib>
#include <gtest/gtest.h>
#include <stdexcept>

#include "sdc-base.hh"
#include "sdc-sql.hh"
#include "sdc-sqlite3.hh"
#include "sdc-db.h"

namespace sdc {
namespace test {

// Basic SQLite3 fixture
//

class TestSQLiteDB : public testing::Test {
protected:
    db::SQLite3 * _sqlite;
public:
    void SetUp() override;
    void TearDown() override;
};

void
TestSQLiteDB::SetUp() {
    _sqlite = new db::SQLite3(":memory:");
}

void
TestSQLiteDB::TearDown() {
    delete _sqlite;
}

// SQLite3 fixture with index sample data
//

class TestSQLiteDBIndex : public TestSQLiteDB {
public:
    void SetUp() override;
};

void
TestSQLiteDBIndex::SetUp() {
    TestSQLiteDB::SetUp();
    char * sql = NULL;
    int rc = sdc_read_sql_file("test-sample-index.sql", &sql);
    ASSERT_EQ(rc, 0);
    _sqlite->execute(sql);
    free(sql);
}

TEST_F(TestSQLiteDBIndex, basicRetrievalWorks) {
    // check basic queries
    std::vector<db::iSQLIndex::BlockExcerpt> excerpts;

    _sqlite->get_update_ids(excerpts, "SADCCalib", db::gUnsetKeyEncoded, 123);
    EXPECT_TRUE(excerpts.empty());
    excerpts.clear();

    _sqlite->get_update_ids(excerpts, "SADCCalib", db::gUnsetKeyEncoded, 1001);
    EXPECT_FALSE(excerpts.empty());
    excerpts.clear();

    _sqlite->get_update_ids(excerpts, "APVCalib", db::gUnsetKeyEncoded, 1830);
    EXPECT_FALSE(excerpts.empty());
    excerpts.clear();
}

}  // namespace ::sdc::test
}  // namespace sdc

//
//

// Just an ampty index to test trivial case
// --
// Still important case for basic functions.
class EmptyTestingSQLite3Index : public ::testing::Test
                               {
protected:
    // DB handle
    sdc::db::SQLite3 * _db;
    // SQL index relying on db
    sdc::db::SQLValidityIndex<int> * _index;
public:
    typedef sdc::ValidityIndex<int, sdc::iDocuments<int>::DocumentLoadingState>::Updates Updates;
    typedef sdc::iDocuments<int>::DocumentLoadingState AuxInfo;
    void SetUp() override {
        _db = new sdc::db::SQLite3(":memory:"
                //, &std::cout  // TODO envvar to ctrl debug printout
                );  
        _index = new sdc::db::SQLValidityIndex<int>(*_db);
    }
    void TearDown() override {
        delete _index;
        delete _db;
    }
};

TEST_F(EmptyTestingSQLite3Index, ThrowsErrorOnNoType) {
    auto u = _index->updates( "WrongType", 123, true );
    EXPECT_TRUE(u.empty());
    // TODO errors::NoData
}

TEST_F(EmptyTestingSQLite3Index, ReturnsEmptyUpdateOnNoExceptNoType) {
    Updates u;
    // overlay=true
    EXPECT_THROW( u = _index->updates( "WrongType", 123, false )
                , sdc::errors::UnknownDataType );
    // TODO errors::NoData
}

TEST_F(EmptyTestingSQLite3Index, CanNotAddBlockWithoutADcoument) {
    AuxInfo ai;
    EXPECT_THROW( _index->add_entry( "The Band of the Hawk"  // document ID
              , "Battle Beast"  // typename
              , 10, sdc::ValidityTraits<int>::unset  // validity period
              , ai)
            , std::runtime_error );
}

// Index with single entry with open bound
// --
// Corresponds to case when single entry defines validity period that starts
// from certain data and lasts forever
class OpenSingularSQLite3TestingIndex : public EmptyTestingSQLite3Index {
protected:
    void SetUp() override {
        EmptyTestingSQLite3Index::SetUp();
        AuxInfo ai = {
                  .docDefaults = {
                       .dataType = "Type1"
                     , .validityRange = {100, 150}
                     , .baseMD = {}
                  }
                , .loader = nullptr
                , .dataBlockBgn = 10
            };
        sdc::utils::DocumentProperties docProps = {
                .mod_time = 100500,
                .size = 100,
                .hashsum = "abcdef",
                .content = {}
            };
        sdc::db::ItemID defaultTypeID = _db->ensure_type("Battle Beast");
        sdc::db::ItemID defaultPeriodID = _db->ensure_period(100, 200);
        _db->add_document( "The Band of the Hawk"
                , docProps
                , defaultTypeID
                , defaultPeriodID
                );

        // common entry
        _index->add_entry( "The Band of the Hawk"  // document path
              , "Battle Beast"  // typename
              , 10, sdc::ValidityTraits<int>::unset  // validity period
              , ai
              );
    }
};

TEST_F(OpenSingularSQLite3TestingIndex, ThrowsErrorOnNoType) {
    // overlay=true
    Updates us = _index->updates( "WrongType", 123, true );
    EXPECT_TRUE(us.empty());
    // overlay=false
    us = _index->updates( "WrongType", 123, true );
    EXPECT_TRUE(us.empty());
}

TEST_F(OpenSingularSQLite3TestingIndex, ReturnsEmptyUpdateOnNoExceptNoType) {
    Updates us;
    // overlay=true
    EXPECT_THROW( us = _index->updates( "WrongType", 123, false )
                , sdc::errors::UnknownDataType );
    // overlay=false
    EXPECT_THROW( us = _index->updates( "WrongType", 123, false )
                , sdc::errors::UnknownDataType );
}

TEST_F(OpenSingularSQLite3TestingIndex, FindsProperUpdate ) {
    Updates::value_type l;
    auto us = _index->updates( "Battle Beast", 10, true); {
        EXPECT_EQ(us.size(), 1);
        const auto & entry = *us.begin();
        ASSERT_TRUE( entry.second );
        ASSERT_EQ( entry.first, 10 );
        const auto & update = *entry.second;
        EXPECT_EQ( update.docID, "The Band of the Hawk" );
        EXPECT_FALSE( sdc::ValidityTraits<int>::is_set(update.validTo) );
        //EXPECT_EQ( update.lineNo, 123 );
    }
    us = _index->updates( "Battle Beast", 10, false); {
        EXPECT_EQ(us.size(), 1);
        const auto & entry = *us.begin();
        ASSERT_TRUE( entry.second );
        ASSERT_EQ( entry.first, 10 );
        const auto & update = *entry.second;
        EXPECT_EQ( update.docID, "The Band of the Hawk" );
        EXPECT_FALSE( sdc::ValidityTraits<int>::is_set(update.validTo) );
        //EXPECT_EQ( update.lineNo, 123 );
    }
    l = _index->latest("Battle Beast", 10); {
        EXPECT_EQ( l.second->docID,  "The Band of the Hawk" );
        EXPECT_FALSE( sdc::ValidityTraits<int>::is_set(l.second->validTo) );
        //EXPECT_EQ( l.second->lineNo, 123 );
    }

    us = _index->updates( "Battle Beast", 999, true); {
        EXPECT_EQ(us.size(), 1);
        const auto & entry = *us.begin();
        ASSERT_TRUE( entry.second );
        ASSERT_EQ( entry.first, 10 );
        const auto & update = *entry.second;
        EXPECT_EQ( update.docID, "The Band of the Hawk" );
        EXPECT_FALSE( sdc::ValidityTraits<int>::is_set(update.validTo) );
        //EXPECT_EQ( update.lineNo, 123 );
    }
    us = _index->updates( "Battle Beast", 999, false); {
        EXPECT_EQ(us.size(), 1);
        const auto & entry = *us.begin();
        ASSERT_TRUE( entry.second );
        ASSERT_EQ( entry.first, 10 );
        const auto & update = *entry.second;
        EXPECT_EQ( update.docID, "The Band of the Hawk" );
        EXPECT_FALSE( sdc::ValidityTraits<int>::is_set(update.validTo) );
        //EXPECT_EQ( update.lineNo, 123 );
    }
    l = _index->latest("Battle Beast", 10); {
        EXPECT_EQ( l.second->docID,  "The Band of the Hawk" );
        EXPECT_FALSE( sdc::ValidityTraits<int>::is_set(l.second->validTo) );
        //EXPECT_EQ( l.second->lineNo, 123 );
    }
}

#if 0
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
#endif

