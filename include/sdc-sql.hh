#pragma once

#include "sdc-base.hh"
#include "sdc-db.h"
#include "sdc-fs.hh"

#include <limits>
#include <type_traits>
#include <vector>
#include <unordered_map>

namespace sdc {
namespace errors {
class SQLDBError : public RuntimeError {
public:
    SQLDBError(const char * msg) : RuntimeError(msg) {}
};
}  // namspace ::sdc::errors
namespace db {

typedef unsigned long EncodedDBKey_t;  // TODO: document, build-aprameterised
constexpr EncodedDBKey_t gUnsetKeyEncoded = std::numeric_limits<EncodedDBKey_t>::max();
inline bool key_is_unset(EncodedDBKey_t v) { return v == gUnsetKeyEncoded;}

template<typename T, typename EnableT=void> struct ValidityKeyTraits;

template<typename T>
struct ValidityKeyTraits<T, typename std::enable_if<std::is_integral<T>::value>::type> {
    static EncodedDBKey_t encode(T v) {
        return sdc::ValidityTraits<T>::is_set(v) ? v : gUnsetKeyEncoded;
    }
    static T decode(EncodedDBKey_t v) {
        return sdc::ValidityTraits<T>::is_set(v) ? v : gUnsetKeyEncoded;
    }
};

// Alias eponymous C definition from global ns to sdc::
typedef ::sdc_ItemID            ItemID;
typedef ::sdc_Key_t             Key_t;
typedef ::sdc_PeriodRecord      PeriodRecord;
typedef ::sdc_DocumentRecord    DocumentRecord;
typedef ::sdc_TypeRecord        TypeRecord;
typedef ::sdc_ColumnRecord      ColumnRecord;
typedef ::sdc_BlockRecord       BlockRecord;
typedef ::sdc_MetadataRecord    MetadataRecord;
typedef ::sdc_EntryRecord       EntryRecord;

struct iSQLIndex {
    virtual ~iSQLIndex() {}

    struct BlockExcerpt {
        /// ID of the document
        ItemID docID;
        /// Path to the document
        std::string docPath;
        /// Validity end key for this block
        EncodedDBKey_t toKey;
        /// Line number in the document this (block starts)
        IntradocMarkup_t blockBgn;
        /// Default type name (for entire document)
        std::string dftTypeName;
        /// Default validity range (for entire document)
        EncodedDBKey_t dftFrom, dftTo;
        // ...
    };

    virtual void get_update_ids(std::vector<BlockExcerpt> & dest
            , const std::string & typeName
            , EncodedDBKey_t oldKey
            , EncodedDBKey_t newKey
            ) = 0;

    ///\brief Loads information necessary to build
    ///       generic `iValidityIndex<>::DocumentEntry`
    virtual void load_doc_entry_info(ItemID docID
            , std::string & defaultDataType
            , EncodedDBKey_t & dftFrom, EncodedDBKey_t & dftTo
            , aux::MetaInfo & docMetaData
            ) = 0;

    /// Should return `true` if type with this name exists
    virtual bool has_type(const std::string & typeName) = 0;

    ///\brief Returns ID of existing document with given path
    /// \throws `errors::...` if document does not exists
    virtual ItemID get_document_id(const std::string & docPath) = 0;

    ///\brief Returns ID of type by name (creates or finds)
    virtual ItemID ensure_type(const std::string &) = 0;

    ///\brief Returns ID of period (existing or newly created)
    virtual ItemID ensure_period(EncodedDBKey_t from, EncodedDBKey_t to) = 0;

    virtual ItemID add_block(ItemID docID, ItemID typeID, ItemID periodID
                        , size_t blockBegin
                        ) = 0;

    // ... TODO: doc
    virtual ItemID add_document(const std::string & path
        , const utils::DocumentProperties & docProps
        , ItemID defaultTypeID
        , ItemID defaultPeriodID) = 0;
};  // struct iSQLIndex

/**\brief DB implementation of the index.
 *
 * Implements lookup operations using particular SQL database.
 *
 * Basically, the whole thing almost entirely relies on inner join of `periods`,
 * `documents` and `blocks` table. With given type name, it uses blocks
 * whose validity covers the given key (or key interval) to obtain document
 * IDs. Resulting document entries the populate the `Updates` list, sorted by
 * appearance.
 * */
template<typename KeyT>
class SQLValidityIndex : public iValidityIndex<KeyT, typename iDocuments<KeyT>::DocumentLoadingState> {
public:
    using typename iValidityIndex<KeyT, typename iDocuments<KeyT>::DocumentLoadingState>::DocumentEntry;
    using typename iValidityIndex<KeyT, typename iDocuments<KeyT>::DocumentLoadingState>::Updates;
protected:
    iSQLIndex & _sqlDB;
private:
    /// (cache) loaded documents by IDs to keep in memory
    ///
    /// Note, that this container is not freed using general interface and has
    /// to be explicitly emptied once user code no longer needs `Updates`.
    mutable std::unordered_map<ItemID, typename iValidityIndex<KeyT
                , typename iDocuments<KeyT>::DocumentLoadingState>::DocumentEntry *> _docs;
    /// (cache) reentrant vector to avoid reallocs
    mutable std::vector<iSQLIndex::BlockExcerpt> _updateRecords;
public:
    SQLValidityIndex(iSQLIndex & sqlDB) : _sqlDB(sqlDB) {}
    SQLValidityIndex(const SQLValidityIndex<KeyT> &) =delete;

    Updates load_doc_details_into_updates_list(KeyT key
            , const std::vector<iSQLIndex::BlockExcerpt> & updatesInfo
            ) const {
        Updates res;
        if(updatesInfo.empty()) return res;
        for(auto & updateInfo: updatesInfo) {
            auto it = _docs.find(updateInfo.docID);
            if(it != _docs.end()) {
                // entry is already loaded, use its pointer and proceed
                typename iDocuments<KeyT>::DocumentLoadingState auxInfo;
                // ^^^ initialize aux info:
                //  - iLoader::Defaults -- specfic for particular document
                //      - dft data type (string)
                //      - dft validity range
                //      - dft metadata
                //  - loader shared ptr
                //  - block bgn (line)
                EncodedDBKey_t dftFrom, dftTo;
                _sqlDB.load_doc_entry_info(updateInfo.docID
                        , auxInfo.docDefaults.dataType
                        , dftFrom, dftTo
                        , auxInfo.docDefaults.baseMD
                        );
                auxInfo.dataBlockBgn = updateInfo.blockBgn;
                auxInfo.loader = nullptr;                // TODO pick suitable loader?
                auxInfo.docDefaults.validityRange
                    = ValidityRange<KeyT>{ ValidityKeyTraits<KeyT>::decode(dftFrom)
                            , ValidityKeyTraits<KeyT>::decode(dftFrom)
                        };

                auto docEntry
                    = new typename iValidityIndex<KeyT, typename iDocuments<KeyT>::DocumentLoadingState>::DocumentEntry{
                              updateInfo.docPath
                            , ValidityKeyTraits<KeyT>::decode(updateInfo.toKey)
                            , auxInfo
                        };
                auto ir = _docs.emplace(updateInfo.docID, docEntry);
                assert(ir.second);
                it = ir.first;
            }
            // load document entry from DB index
            res.push_back({key, it->second});
        }
        return res;
    }

    ///\brief Queries list of "still valid" documents to be applied, in order
    ///
    /// Having type name and the "current" key value, queries `blocks` table
    /// for the document IDs whose names are than used to populate updates
    /// list, in order of their appearance.
    Updates updates( const std::string & typeName
                   , KeyT key
                   , bool noTypeIsOk=false ) const override {
        if(!noTypeIsOk) {
            if(!_sqlDB.has_type(typeName)) {
                throw sdc::errors::UnknownDataType(typeName);
            }
        }
        _updateRecords.clear();
        _sqlDB.get_update_ids( _updateRecords
                    , typeName
                    , gUnsetKeyEncoded
                    , ValidityKeyTraits<KeyT>::encode(key)
                    );
        return load_doc_details_into_updates_list(key, _updateRecords);
    }

    ///\brief Queries updates between two keys
    ///
    /// Having type name and the (from, to) key value pair, queries `blocks`
    /// table for the document IDs whose names are than used to populate updates
    /// list, in order of their appearance.
    Updates updates( const std::string & typeName
                   , KeyT oldKey, KeyT newKey
                   , bool noTypeIsOk=false
                   , bool keepStale=false
                   ) const override {
        if(!noTypeIsOk) {
            if(!_sqlDB.has_type(typeName)) {
                throw sdc::errors::UnknownDataType(typeName);
            }
        }
        // TODO: keepStale
        _updateRecords.clear();
        _sqlDB.get_update_ids( _updateRecords
                    , typeName
                    , ValidityKeyTraits<KeyT>::encode(oldKey)
                    , ValidityKeyTraits<KeyT>::encode(newKey)
                    );
        return load_doc_details_into_updates_list(newKey, _updateRecords);
    }

    ///\brief Queries latest document entry for certain run number and type,
    ///       ignoring their history
    typename Updates::value_type
        latest( const std::string & typeName
              , KeyT key
              ) const override {
        // TODO: test it
        _updateRecords.clear();
        _sqlDB.get_update_ids( _updateRecords
                    , typeName
                    , gUnsetKeyEncoded
                    , ValidityKeyTraits<KeyT>::encode(key)
                    );
        throw std::runtime_error("TODO: return latest update");
        //return load_doc_details_into_updates_list(key, _updateRecords);
    }

    ///\brief Adds entry to an index
    ///
    /// Upserts document and type if they do not exist.
    void
    add_entry( const std::string & docPath
             , const std::string & dataType
             , KeyT from_, KeyT to_
             , const typename iDocuments<KeyT>::DocumentLoadingState & docLoadingState
             ) override {
        EncodedDBKey_t from = ValidityKeyTraits<KeyT>::encode(from_)
                     , to = ValidityKeyTraits<KeyT>::encode(to_)
                     ;
        ItemID docID    = _sqlDB.get_document_id(docPath);
        ItemID typeID   = _sqlDB.ensure_type(docPath);
        ItemID periodID = _sqlDB.ensure_period(from, to);
        _sqlDB.add_block( docID, typeID, periodID
                        , docLoadingState.dataBlockBgn
                        );
    }
};

}  // namespace ::sdc::db
}  // namespace sdc

