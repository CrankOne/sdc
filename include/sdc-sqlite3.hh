#pragma once

#include <sqlite3.h>

#include "sdc-sql.hh"

namespace sdc {
namespace errors {
class SQLite3DBError : public SQLDBError {
public:
    SQLite3DBError(const char * msg) : SQLDBError(msg) {}
    SQLite3DBError(::sqlite3 * db, const char * desc);
};
}  // namespace ::sdc::errors
namespace db {

class SQLite3Helper;  // fwd, internal class

/**\brief Implements SQLite3-based driver for calibration index
 *
 * This class implements `iSQLIndex` interface providing updates lookup within
 * an SQLite3 database which `types`, `periods`, `documents`, `metadata` and
 * `blocks` tables (i.e. `columns` and `entries` tables can be omitted).
 * */
class SQLite3 : public iSQLIndex {
private:
    ::sqlite3 * _db;
    ::sqlite3_stmt * _selectDocsByTypeAndKey
                 , * _selectDocsByTypeInRange
                 , * _checkTypeExists, * _findType, * _addType
                 , * _findPeriod, * _addPeriod
                 , * _findDocumentByPath
                 , * _addBlock
                 , * _addDocument
                 ;
    std::list<::sqlite3_stmt *> _prepStmts;
protected:
    std::ostream * _sqlEvalLog;
    int _check_prepare( const char * strStmt, ::sqlite3_stmt *&stmt);
    int _check_step(::sqlite3_stmt *);
    int _step(SQLite3Helper &);
    //const size_t _encKeyLen;
public:
    SQLite3( const char * dbname
           //, size_t encodedKeyLength
           , std::ostream * sqlEvalLog=nullptr
           );
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

    bool has_type(const std::string & typeName) override;

    ItemID get_document_id(const std::string & docPath) override;
    ItemID add_document(const std::string & path
            , const utils::DocumentProperties & docProps
            , ItemID defaultTypeID
            , ItemID defaultPeriodID
            ) override;

    ItemID ensure_type(const std::string &) override;
    ItemID ensure_period(EncodedDBKey_t from, EncodedDBKey_t to) override;
    ItemID add_block(ItemID docID, ItemID typeID, ItemID periodID
                        , size_t blockBegin
                        ) override;
};

}  // namespace ::sdc::db
}  // namespace sdc

