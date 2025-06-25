#pragma once

#include <sqlite3.h>

#include "sdc-sql.hh"

namespace sdc {
namespace db {

class SQLite3 : public iSQLIndex {
private:
    ::sqlite3 * _db;
    ::sqlite3_stmt * _selectDocsByTypeAndKey
                 , * _selectDocsByTypeInRange
                 ;
public:
    SQLite3(const char * dbname);
    ~SQLite3();

    ::sqlite3 * get_sqlite3_db_ptr() { return _db; }
    void execute(const char *);  // todo: iface?

    // iSQLIndex implementation
    //

    void get_update_ids(std::vector<BlockExcerpt> & dest
            , const std::string & typeName
            , EncodedDBKey_t oldKey
            , EncodedDBKey_t newKey
            ) override;

    void load_doc_entry_info(ItemID docID
            , std::string & defaultDataType
            , EncodedDBKey_t & dftFrom, EncodedDBKey_t & dftTo
            , aux::MetaInfo & docMetaData
            ) override;
};

}  // namespace ::sdc::db
}  // namespace sdc

