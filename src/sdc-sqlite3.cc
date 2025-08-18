#include "sdc-base.hh"
#include "sdc-sql.hh"
#include "sdc-sqlite3.hh"

#include <climits>
#include <stdexcept>

namespace sdc {
namespace db {

class SQLite3Helper {
protected:
    ::sqlite3 & _db;
    ::sqlite3_stmt * _s;
    int _narg;
public:
    SQLite3Helper(::sqlite3 * db
            , ::sqlite3_stmt * s)
        : _db(*db), _s(s), _narg(0) {
        sqlite3_reset(_s);
        sqlite3_clear_bindings(_s);
    }

    struct NULL_ {};

    int bind(const std::string & v) {
        int rc;
        if( (rc = sqlite3_bind_text(_s, ++_narg, v.c_str(), -1, SQLITE_TRANSIENT)) != SQLITE_OK) {
            throw errors::SQLite3DBError(sqlite3_errmsg(&_db));
        }
        return rc;
    }

    int bind(int v) {
        int rc;
        if( (rc = sqlite3_bind_int(_s, ++_narg, v)) != SQLITE_OK) {
            throw errors::SQLite3DBError(sqlite3_errmsg(&_db));
        }
        return rc;
    }

    int bind(long int v) {
        int rc;
        if( (rc = sqlite3_bind_int64(_s, ++_narg, v)) != SQLITE_OK) {
            throw errors::SQLite3DBError(sqlite3_errmsg(&_db));
        }
        return rc;
    }

    int bind(const std::vector<char> & v) {   // no dedicated function to check bind blob result
        int rc = sqlite3_bind_blob(_s, ++_narg,
                          v.data(),
                          static_cast<int>(v.size()),
                          SQLITE_TRANSIENT);    
        if( rc != SQLITE_OK) {
            throw errors::SQLite3DBError(sqlite3_errmsg(&_db));
        }
        return rc;
    }

    int bind_null() {
        int rc;
        if( (rc = sqlite3_bind_null(_s, ++_narg)) != SQLITE_OK) {
            throw errors::SQLite3DBError(sqlite3_errmsg(&_db));
        }
        return rc;
    }

    int bind(unsigned long int v) {
        if(v > LONG_MAX) {
            // This is the data limitation of SQLite3 -- it can not store
            // unsigned long int values. Perhaps we will overcome this
            // later...
            throw errors::SQLite3DBError("Unsigned long integer value exceeds"
                    " long integer max -- can't bind value to SQLite3 i64");
        }
        int rc;
        if( (rc = sqlite3_bind_int64(_s, ++_narg, v)) != SQLITE_OK) {
            throw errors::SQLite3DBError(sqlite3_errmsg(&_db));
        }
        return rc;
    }

    template<size_t N>
    int bind(const unsigned char (&arr)[N]) {
        int rc = sqlite3_bind_blob(_s, ++_narg, arr, N, SQLITE_TRANSIENT);
        if( rc != SQLITE_OK) {
            throw errors::SQLite3DBError(sqlite3_errmsg(&_db));
        }
        return rc;
    }

    friend class SQLite3;
};

template<typename T>
SQLite3Helper & operator<<(SQLite3Helper & h, const T & value) {
    h.bind(value);
    return h;
}

template<>
SQLite3Helper & operator<< <SQLite3Helper::NULL_>(SQLite3Helper & h
        , const SQLite3Helper::NULL_ & value) {
    h.bind_null();
    return h;
}

//template<size_t N>
//SQLite3Helper & operator<< <>(SQLite3Helper & h
//        , const SQLite3Helper::NULL_ & value) {
//    h.bind_null();
//    return h;
//}

void
SQLite3::execute(const char * sql) {
    char * errMsg = NULL;
    if(_sqlEvalLog) {
        *_sqlEvalLog << "SQLite3 exec: ```" << std::endl
            << sql << std::endl << "```" << std::endl;
    }
    if( sqlite3_exec(_db, sql, 0, 0, &errMsg) != SQLITE_OK ) {
        std::string errMsgCpp(errMsg);
        sqlite3_free(errMsg);
        throw errors::SQLite3DBError(errMsgCpp.c_str());
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
    AND (
            (p.from_key IS NULL OR ? >= p.from_key)
            AND
            (p.to_key IS NULL OR ? <= p.to_key)
        );
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
    AND (p.from_key IS NULL OR p.from_key <= ?)
    AND (p.to_key   IS NULL OR p.to_key   >= ?);
)~";

static const char gCheckTypeExists[] = "SELECT 1 FROM types WHERE name = ? LIMIT 1;";
static const char gFindType[] = "SELECT id FROM types WHERE name = ?;";
static const char gInsertType[] = "INSERT INTO types (name) VALUES (?);";

static const char gFindPeriodExact[] = "SELECT id FROM periods WHERE from_key = ? AND to_key = ?;";
static const char gInsertPeriod[] = "INSERT INTO periods (from_key, to_key) VALUES (?, ?);";

static const char gFindDocumentByPath[] = "SELECT id FROM documents WHERE path = ?;";

static const char gAddBlock[] = R"~(INSERT INTO blocks (doc_id, line_start, type_id, period_id)
VALUES (?, ?, ?, ?);)~";

static const char gAddDocument[] = R"~(
INSERT INTO documents (
            path, mod_time, size, hashsum, content, default_type_id, default_period_id
        ) VALUES (?, ?, ?, ?, ?, ?, ?);
)~";

int
SQLite3::_check_prepare( const char * strStmt
        , ::sqlite3_stmt *&stmt) {
    int rc;
    if(SQLITE_OK != (rc = sqlite3_prepare_v2(_db
            , strStmt
            , -1 // sizeof(gSelectDocsByTypeAndKey)
            , &stmt
            , NULL  // &endPtr
            ))) {
        const char * errDetails = sqlite3_errmsg(_db);
        // TODO: put str text to dedicated exception class instance
        std::cerr << "Error occured while preparing SQLite3 statement:"
            << std::endl << "```"
            << strStmt
            << std::endl << "```" << std::endl << errDetails << std::endl
            ;
        throw errors::SQLite3DBError(errDetails);
    }
    _prepStmts.push_back(stmt);
    return rc;
}

int
SQLite3::_check_step(::sqlite3_stmt * s) {
    int rc;
    if(_sqlEvalLog) {
        *_sqlEvalLog << "SQLite3 step: ```" << std::endl
            << sqlite3_expanded_sql(s)
            << std::endl
            << "```" << std::endl;
    }
    rc = sqlite3_step(s);
    if(rc != SQLITE_DONE && rc != SQLITE_ROW) {
        throw errors::SQLite3DBError(sqlite3_errmsg(_db));
    }
    return rc;
}

int
SQLite3::_step(SQLite3Helper & h) {
    return _check_step(h._s);
}

SQLite3::SQLite3(const char * dbname
        //, size_t encodedKeyLength
        , std::ostream * sqlEvalLog)
            : _db(nullptr)
            , _sqlEvalLog(sqlEvalLog)
            //, _encKeyLen(encodedKeyLength)
            {
    if(_sqlEvalLog) {
        *_sqlEvalLog << "Opening SQLite3 DB \"" << dbname << "\"." << std::endl;
    }
    if(sqlite3_open(dbname, &_db)) {
        _db = nullptr;
        throw errors::SQLite3DBError("can't open db");
    }
    // init DB if need
    char * sql = NULL;
    int rc = sdc_read_sql_file("init-tables.sql", &sql);
    if(0 != rc)
        throw errors::SQLite3DBError("Can't (re-)initialize database due"
            " to SDC SQL script loading error.");
    execute(sql);
    free(sql);
    if(_sqlEvalLog) {
        *_sqlEvalLog << "Preparing SQLite3 statements..." << std::endl;
    }
    // compile statements
    _check_prepare(gSelectDocsByTypeAndKey,  _selectDocsByTypeAndKey);
    _check_prepare(gSelectDocsByTypeInRange, _selectDocsByTypeInRange);

    _check_prepare(gCheckTypeExists,         _checkTypeExists);
    _check_prepare(gFindType,                _findType);
    _check_prepare(gInsertType,              _addType);

    _check_prepare(gFindPeriodExact,         _findPeriod);
    _check_prepare(gInsertPeriod,            _addPeriod);

    _check_prepare(gFindDocumentByPath,      _findDocumentByPath);
    _check_prepare(gAddDocument,             _addDocument);

    _check_prepare(gAddBlock,                _addBlock);
    if(_sqlEvalLog) {
        *_sqlEvalLog << "Done with SQLite3 statements preparations." << std::endl;
    }
}

SQLite3::~SQLite3() {
    for(auto it = _prepStmts.rbegin(); it != _prepStmts.rend(); ++it) {
        sqlite3_finalize(*it);
    }
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
    SQLite3Helper h(_db
            , oldKey == gUnsetKeyEncoded
            ? _selectDocsByTypeAndKey
            : _selectDocsByTypeInRange
            );
    // 1. type name
    h << typeName;
    if( key_is_unset(oldKey) ) {
        // 2. new key (covered by validity period)
        if(key_is_unset(newKey)) {
            h << SQLite3Helper::NULL_{} << SQLite3Helper::NULL_{};
        } else {
            h << newKey;
            // 3. same key (covered by validity period)
            h << newKey;
        }
    } else {
        // 2. old key (covered by validity period)
        if(key_is_unset(oldKey)) {
            h << SQLite3Helper::NULL_{};
        } else {
            h << oldKey;
        }
        // 3. new key (covered by validity period)
        if(key_is_unset(newKey)) {
            h << newKey;
        } else {
            h << oldKey;
        }
    }

    // Execute the statement and process the results
    int rc = _step(h);
    if (rc == SQLITE_ROW) {
        // Process each row
        do {
            auto be = BlockExcerpt{
                      .docID    = sqlite3_column_int(h._s, 0)
                    , .docPath  = (const char *) sqlite3_column_text(h._s, 1)
                    , .toKey    = static_cast<EncodedDBKey_t>(sqlite3_column_int64(h._s, 3))
                    , .blockBgn = static_cast<IntradocMarkup_t>(sqlite3_column_int(h._s, 2))
                    , .dftTypeName = (const char *) sqlite3_column_text(h._s, 4)
                    , .dftFrom  = static_cast<EncodedDBKey_t>(sqlite3_column_int64(h._s, 5))
                    , .dftTo    = static_cast<EncodedDBKey_t>(sqlite3_column_int64(h._s, 6))
                    // ...
                    };
            if(key_is_unset(be.toKey))   be.toKey   = gUnsetKeyEncoded;
            if(key_is_unset(be.dftFrom)) be.dftFrom = gUnsetKeyEncoded;
            if(key_is_unset(be.dftTo))   be.dftTo   = gUnsetKeyEncoded;
            dest.push_back(be);
        } while ((rc = _step(h)) == SQLITE_ROW);
    } else if (rc == SQLITE_DONE) {
        return;  // nothing found
    } else {
        throw errors::SQLite3DBError(sqlite3_errmsg(_db));  // TODO: dedicate exc
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

    int rc = _check_step(_selectDocPropertiesForAuxInfo);
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

bool
SQLite3::has_type(const std::string & typeName) {
    int rc;

    // Reset the statement in case it has been used previously
    SQLite3Helper h(_db, _checkTypeExists);

    // Bind the name parameter
    h << typeName;

    // Execute the statement
    rc = _step(h);
    if (rc == SQLITE_ROW) {
        // Type exists
        return 1;
    } else if (rc == SQLITE_DONE) {
        // No row found
        return 0;
    } else {
        // Some error occurred
        return -2;
    }
}

ItemID
SQLite3::ensure_type(const std::string & typeName) {
    int rc;
    {
        // Reset the statement in case it has been used previously
        SQLite3Helper h(_db, _findType);

        h << typeName;

        rc = _step(h);
        if( rc == SQLITE_ROW) {
            // period found
            return sqlite3_column_int(_findType, 0);
        }
    }

    {
        // period not found -- create
        SQLite3Helper h(_db, _addType);
        h << typeName;
        rc = _step(h);
        if(rc != SQLITE_DONE) {
            throw errors::SQLite3DBError(sqlite3_errmsg(_db));
        }
    }
    return sqlite3_last_insert_rowid(_db);
}

ItemID
SQLite3::ensure_period(EncodedDBKey_t from, EncodedDBKey_t to) {
    int rc;
    {
        SQLite3Helper h(_db, _findPeriod);

        if(key_is_unset(from)) h << SQLite3Helper::NULL_{};
        else h << from;

        if(key_is_unset(to)) h << SQLite3Helper::NULL_{};
        else h << to;

        rc = _step(h);
        if( rc == SQLITE_ROW) {
            // period found
            return sqlite3_column_int(_findPeriod, 0);
        }
    }

    // period not found -- create
    {
        SQLite3Helper h(_db, _addPeriod);
        h << from << to;
        rc = _step(h);
        if(rc != SQLITE_DONE) {
            throw errors::SQLite3DBError(sqlite3_errmsg(_db));
        }
    }
    return sqlite3_last_insert_rowid(_db);
}

ItemID
SQLite3::get_document_id(const std::string & docPath) {
    int rc;
    {
        SQLite3Helper h(_db, _findDocumentByPath);
        h << docPath;

        rc = _step(h);
        if( rc == SQLITE_ROW) {
            // doc found
            return sqlite3_column_int(_findDocumentByPath, 0);
        }
    }

    // doc not found -- error
    throw errors::SQLite3DBError("No document with given path");
}

ItemID
SQLite3::add_document( const std::string & path
        , const utils::DocumentProperties & docProps
        , ItemID defaultTypeID
        , ItemID defaultPeriodID
        ) {
    int rc;
    SQLite3Helper h(_db, _addDocument);

    h << path
      << docProps.mod_time << docProps.size
      << docProps.hashsum
      << docProps.content
      << defaultTypeID
      << defaultPeriodID
      ;

    rc = _step(h);
    if( rc == SQLITE_DONE) {
        return sqlite3_last_insert_rowid(_db);
    }
    // insertion failed
    throw errors::SQLite3DBError(sqlite3_errmsg(_db));
}

ItemID
SQLite3::add_block(ItemID docID, ItemID typeID, ItemID periodID
                        , size_t blockBegin
                        ) {
    int rc;
    SQLite3Helper h(_db, _addBlock);
    h << docID << blockBegin << typeID << periodID;

    rc = _step(h);
    if( rc == SQLITE_ROW) {
        // doc found
        return sqlite3_column_int(_addBlock, 0);
    }

    return sqlite3_last_insert_rowid(_db);
}

}  // namespace ::sdc::db
}  // namespace sdc

