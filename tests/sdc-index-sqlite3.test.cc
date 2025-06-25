#include <cstdlib>
#include <gtest/gtest.h>
#include <stdexcept>

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

TEST_F(TestSQLiteDBIndex, simpleIDs) {
    // check basic queries
    std::vector<db::iSQLIndex::BlockExcerpt> excerpts;

    _sqlite->get_update_ids(excerpts, "SADCCalib", db::iSQLIndex::gKeyStart, 123);
    EXPECT_TRUE(excerpts.empty());
    excerpts.clear();

    _sqlite->get_update_ids(excerpts, "SADCCalib", db::iSQLIndex::gKeyStart, 1001);
    EXPECT_FALSE(excerpts.empty());
    excerpts.clear();

    _sqlite->get_update_ids(excerpts, "APVCalib", db::iSQLIndex::gKeyStart, 1500);
    EXPECT_FALSE(excerpts.empty());
    excerpts.clear();
}

}  // namespace ::sdc::test
}  // namespace sdc

