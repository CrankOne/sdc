#ifndef H_SDC_DATABASE_H
#define H_SDC_DATABASE_H 1

#include "sdc-config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Max length of document path, bytes */
#define SDC_DB_MAX_LEN_DOCUMENT_PATH 256
/** Length of hashsusm, bytes */
#define SDC_DB_LEN_DOCUMENT_HASHSUM 128
/** Max length of data type name, bytes */
#define SDC_DB_MAX_LEN_DATA_TYPE_NAME 128
/** Max length of column name, bytes */
#define SDC_DB_MAX_LEN_COLUMN_NAME 64
/** Max length of metadata key, bytes */
#define SDC_DB_MAX_LEN_MD_KEY 64
/** Max length of metadata value, bytes */
#define SDC_DB_MAX_LEN_MD_VALUE 256
/** Max length of single value text, bytes */
#define SDC_DB_MAX_LEN_ENTRY_TEXT_VALUE 256

/**\brief Item ID type used in DB */
typedef int sdc_ItemID;
/**\brief C type to store the validity key */
typedef void * sdc_Key_t;

/* Table entries
 */

/**\brief Record type of the `periods` table */
struct sdc_PeriodRecord {
    sdc_ItemID id;

    sdc_Key_t from, to;
};

/**\brief Record of the `documents` table */
struct sdc_DocumentRecord {
    sdc_ItemID id;

    char path[SDC_DB_MAX_LEN_DOCUMENT_PATH];
    unsigned long modTime;
    unsigned long size;
    char hashsum[SDC_DB_LEN_DOCUMENT_HASHSUM];
    int parseError;
    char * content;
};

/**\brief Record of the `types` table */
struct sdc_TypeRecord {
    sdc_ItemID id;

    char name[SDC_DB_MAX_LEN_DATA_TYPE_NAME];
};

/**\brief Record of the `columns` table */
struct sdc_ColumnRecord {
    sdc_ItemID id;

    char name[SDC_DB_MAX_LEN_COLUMN_NAME];
    sdc_ItemID typeID;
};

/**\brief Record of the `blocks` table */
struct sdc_BlockRecord {
    sdc_ItemID id;

    sdc_ItemID docID;
    unsigned long lineStart, lineEnd;
    sdc_ItemID typeID;
    sdc_ItemID validity;
};

/**\brief Record of the `metadata` table */
struct sdc_MetadataRecord {
    sdc_ItemID id;

    sdc_ItemID docID;
    unsigned long lineNo;
    char key[SDC_DB_MAX_LEN_MD_KEY]
       , value[SDC_DB_MAX_LEN_MD_VALUE];
};

/**\brief Record of the `entries` table */
struct sdc_EntryRecord {
    sdc_ItemID id;

    char value[SDC_DB_MAX_LEN_ENTRY_TEXT_VALUE];
    sdc_ItemID blockID;
    unsigned long lineOffset;
    sdc_ItemID columnID;
};

/**\brief Loads the entire file content into null-terminated string and
 *        returns `malloc()`'d buffer
 *
 * Given file path will be appended to the content of `SDC_SQL_SCRIPTS_PATH`.
 * String buffer is allocated using `malloc()`, so returned ptr must be
 * `free()`'d by user code.
 *
 * Upon error, prints error description to `stderr` and returns error code
 * as described:
 *
 * \returns 0 if file content is read
 * \returns 1 if `SDC_SQL_SCRIPTS_PATH` variable is not defined or empty
 * \returns 2 if file does not exist or failed to open
*  \returns 3 on memory allocation failure
 * */
int sdc_read_sql_file(const char *filepath, char ** destBuffer);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* H_SDC_DATABASE_H */
