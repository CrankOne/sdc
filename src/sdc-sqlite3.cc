#include "sdc-base.hh"
#include "sdc-sql.hh"
#include "sdc-sqlite3.hh"

#include <stdexcept>

namespace sdc {
namespace db {

void
SQLite3::execute(const char * sql) {
    char * errMsg = NULL;
    if( sqlite3_exec(_db, sql, 0, 0, &errMsg) != SQLITE_OK ) {
        std::string errMsgCpp(errMsg);
        sqlite3_free(errMsg);
        throw std::runtime_error(errMsgCpp);  // TODO: dedicated exception class
    }
}

static const char gSelectDocsByTypeAndKey[] = R"~(
SELECT 
    d.id AS document_id,
    d.path,
    b.line_start,
    p.to_key AS block_end_validity_period,
    t.name AS default_type_name,
    dp.from_key AS default_period_from_key,
    dp.to_key AS default_period_to_key
FROM 
    documents d
JOIN 
    blocks b ON d.id = b.doc_id
JOIN 
    types t ON b.type_id = t.id
JOIN 
    periods p ON b.period_id = p.id
JOIN 
    periods dp ON d.default_period_id = dp.id  -- Join for the default period
WHERE
    t.name = ?  -- The given type name
    AND ? BETWEEN p.from_key AND p.to_key;
)~";

static const char gSelectDocsByTypeInRange[] = R"~(
SELECT
    d.id AS document_id,
    d.path,
    b.line_start,
    p.to_key AS block_end_validity_period,
    t.name AS default_type_name,
    dp.from_key AS default_period_from_key,
    dp.to_key AS default_period_to_key
FROM 
    documents d
JOIN 
    blocks b ON d.id = b.doc_id
JOIN 
    types t ON b.type_id = t.id
JOIN 
    periods p ON b.period_id = p.id
JOIN 
    periods dp ON d.default_period_id = dp.id  -- Join for the default period
WHERE
    t.name = ?  -- The given type name
    AND p.from_key <= ?   -- The given "from" key must not be greater than block's "to_key"
    AND p.to_key >= ?; -- The given "to" key must not be less than block's "from_key"
)~";

SQLite3::SQLite3(const char * dbname) : _db(nullptr) {
    if(sqlite3_open(dbname, &_db)) {
        _db = nullptr;
        throw std::runtime_error("can't open db");  // TODO: dedicated exception
    }
    // init DB if need
    char * sql = NULL;
    int rc = sdc_read_sql_file("init-tables.sql", &sql);
    if(0 != rc)
        throw std::runtime_error("Can't (re-)initialize database due"
            " to SDC SQL script loading error.");
    execute(sql);
    free(sql);
    // compile statements
    //const char * endPtr = NULL;
    rc = 0;
    if(SQLITE_OK != (rc = sqlite3_prepare_v2(_db
            , gSelectDocsByTypeAndKey
            , -1 // sizeof(gSelectDocsByTypeAndKey)
            , &_selectDocsByTypeAndKey
            , NULL  // &endPtr
            ))) {
        throw std::runtime_error(sqlite3_errmsg(_db));  // TODO: dedicate exc
    }
    if(SQLITE_OK != (rc = sqlite3_prepare_v2(_db
            , gSelectDocsByTypeInRange
            , -1  // sizeof(gSelectDocsByTypeInRange)
            , &_selectDocsByTypeInRange
            , NULL  // &endPtr
            ))) {
        throw std::runtime_error(sqlite3_errmsg(_db));  // TODO: dedicate exc
    }
}

SQLite3::~SQLite3() {
    if(_selectDocsByTypeAndKey) sqlite3_finalize(_selectDocsByTypeAndKey);
    // ... other stmts
    if(_db) sqlite3_close(_db);
}

void
SQLite3::get_update_ids(std::vector<BlockExcerpt> & dest
            , const std::string & typeName
            , EncodedDBKey_t oldKey
            , EncodedDBKey_t newKey
            ) {

    // select the statement:
    ::sqlite3_stmt * stmt = NULL;
    if( oldKey == iSQLIndex::gKeyStart ) {
        stmt = _selectDocsByTypeAndKey;
    } else {
        stmt = _selectDocsByTypeInRange;
    }
    sqlite3_reset(stmt);

    // input parameters:
    // 1. type name
    if( sqlite3_bind_text(stmt, 1, typeName.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(_db));  // TODO: dedicate exc
    }
    if(oldKey == iSQLIndex::gKeyStart) {
        // 2. new key (covered by validity period)
        if( sqlite3_bind_int(stmt, 2, newKey) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(_db));  // TODO: dedicate exc
        }
    } else {
        // 2. old key (covered by validity period)
        if( sqlite3_bind_int(stmt, 2, oldKey) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(_db));  // TODO: dedicate exc
        }
        // 3. new key (covered by validity period)
        if( sqlite3_bind_int(stmt, 3, newKey) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(_db));  // TODO: dedicate exc
        }
    }

    // TODO: dbg log?
    //std::cout << sqlite3_expanded_sql(stmt) << std::endl;  // XXX

    // Execute the statement and process the results
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        // Process each row
        do {
            dest.push_back(BlockExcerpt{
                      .docID    = sqlite3_column_int(stmt, 0)
                    , .docPath  = (const char *) sqlite3_column_text(stmt, 1)
                    , .toKey    = static_cast<EncodedDBKey_t>(sqlite3_column_int64(stmt, 3))
                    , .blockBgn = static_cast<IntradocMarkup_t>(sqlite3_column_int(stmt, 2))
                    , .dftTypeName = (const char *) sqlite3_column_text(stmt, 4)
                    , .dftFrom  = static_cast<EncodedDBKey_t>(sqlite3_column_int64(stmt, 5))
                    , .dftTo    = static_cast<EncodedDBKey_t>(sqlite3_column_int64(stmt, 6))
                    // ...
                    });
        } while ((rc = sqlite3_step(stmt)) == SQLITE_ROW);
    } else if (rc == SQLITE_DONE) {
        return;  // nothing found
    } else {
        throw std::runtime_error(sqlite3_errmsg(_db));  // TODO: dedicate exc
    }
}



void
SQLite3::load_doc_entry_info(ItemID docID
            , std::string & defaultDataType
            , EncodedDBKey_t & dftFrom, EncodedDBKey_t & dftTo
            , aux::MetaInfo & docMetaData
            ) {
    #if 1
    throw std::runtime_error("TODO: SQLite3::load_doc_entry_info()");
    #else
    // Load document's defaults
    if( sqlite3_bind_int(_selectDocPropertiesForAuxInfo, 1, docID) != SQLITE_OK) {
        //printf("Failed to bind type_name: %s\n", sqlite3_errmsg(db));
        throw std::runtime_error(sqlite3_errmsg(_db));  // TODO: dedicate exc
    }

    int rc = sqlite3_step(_selectDocPropertiesForAuxInfo);
    if (rc == SQLITE_ROW) {
        // Extract type_name, from_key, and to_key
        defaultDataType = (const char *)sqlite3_column_text(_selectDocPropertiesForAuxInfo, 0);
        dftFrom = sqlite3_column_int64(_selectDocPropertiesForAuxInfo, 1);
        dftTo = sqlite3_column_int64(_selectDocPropertiesForAuxInfo, 2);
    } else if (rc == SQLITE_DONE) {
        throw std::runtime_error("no document with ID requested");  // TODO
    } else {
        throw std::runtime_error(sqlite3_errmsg(_db));  // TODO: dedicate exc
    }
    #endif
}

}  // namespace ::sdc::db
}  // namespace sdc

