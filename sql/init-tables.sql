-- represents validity period
--      key type must be convertible to integer
CREATE TABLE IF NOT EXISTS periods (
        id INTEGER PRIMARY KEY,

        from_key INTEGER,
        to_key INTEGER,
        label VARCHAR(255),

        UNIQUE(from_key, to_key)
    );

-- type of the data, unique for block
--      Each item corresponds to a single C++-convertible data type, with
--      a set of properties (*columns*) defined
CREATE TABLE IF NOT EXISTS types (
        id INTEGER PRIMARY KEY,
        name VARCHAR(127),  -- SDC_DB_MAX_LEN_DATA_TYPE_NAME

        UNIQUE(name)
    );

-- document source used to obtain the data
--      These items corresponds to an artifact (a local file, remote resource,
--      etc) which is to be observed by SDC. Each document can provide multiple
--      ASCII *blocks*, each with type and validity period assigned
CREATE TABLE IF NOT EXISTS documents (
        id INTEGER PRIMARY KEY,

        path VARCHAR(255),  -- SDC_DB_MAX_LEN_DOCUMENT_PATH
        mod_time INTEGER,
        size INTEGER,
        hashsum VARCHAR(127),  -- SDC_DB_LEN_DOCUMENT_HASHSUM
        parse_error INTEGER,
        content blob,  -- compressed content of the document, unused for shallow ops
        default_type_id INTEGER,
        default_period_id INTEGER,

        UNIQUE(path),
        FOREIGN KEY(default_type_id) REFERENCES types(id),
        FOREIGN KEY(default_period_id) REFERENCES periods(id)
    );

-- column of block
--      a property of the data type.
--      column name must be unique within a type
CREATE TABLE IF NOT EXISTS columns (
        id INTEGER PRIMARY KEY,
        name VARCHAR(63),  -- SDC_DB_MAX_LEN_COLUMN_NAME
        type_id INTEGER,  -- FK of type this column addressed the data to
        FOREIGN KEY(type_id) REFERENCES types(id)
    );

-- block found in the document with validity period assigned
--      ASCII block inside of the document. At least one line with at least
--      one column
CREATE TABLE IF NOT EXISTS blocks (
        id INTEGER PRIMARY KEY,

        doc_id INTEGER,  -- FK of the document this block is defined in
        line_start INTEGER,
        type_id INTEGER,  -- FK of the type this block is defining
        period_id INTEGER, -- FK of the validity period this block is defining

        FOREIGN KEY(doc_id) REFERENCES documents(id),
        FOREIGN KEY(type_id) REFERENCES types(id),
        FOREIGN KEY(period_id) REFERENCES periods(id)
    );

-- key/value pair defined in arbitrary place of the document
--      These entries provide information considered valid from the place where
--      it was specified, until the end of the document
CREATE TABLE IF NOT EXISTS metadata (
        id INTEGER PRIMARY KEY,
        doc_id INTEGER,
        line INTEGER,
        key VARCHAR(63),  -- SDC_DB_MAX_LEN_MD_KEY
        value VARCHAR(255),  -- SDC_DB_MAX_LEN_MD_VALUE
        FOREIGN KEY(doc_id) REFERENCES documents(id)
    );

-- individual entry
CREATE TABLE IF NOT EXISTS entries (
        id INTEGER PRIMARY KEY,

        value VARCHAR(255),  -- SDC_DB_MAX_LEN_ENTRY_TEXT_VALUE
        block_id INTEGER,
        src_line_offset INTEGER,
        column_id INTEGER,

        FOREIGN KEY(block_id) REFERENCES block(id),
        FOREIGN KEY(column_id) REFERENCES columns(id)
    );
