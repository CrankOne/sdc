#pragma once

#include "sdc-base.hh"
#include "sdc-db.h"

#include <vector>
#include <unordered_map>

namespace sdc {
namespace db {

typedef unsigned long EncodedDBKey_t;  // TODO: document, build-aprameterised
template<typename T> struct ValidityKeyTraits;

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
    static constexpr EncodedDBKey_t gKeyStart = 0;

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
    /// 
    virtual void load_doc_entry_info(ItemID docID
            , std::string & defaultDataType
            , EncodedDBKey_t & dftFrom, EncodedDBKey_t & dftTo
            , aux::MetaInfo & docMetaData
            ) = 0;

    // ... TODO: whatever else is needed for SQLiteIndex
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
    mutable std::vector<ItemID> _itemIDs;
public:
    Updates load_doc_details_into_updates_list(KeyT key
            , const std::vector<iSQLIndex::BlockExcerpt> & updatesInfo
            ) {
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
                        , auxInfo.baseMD
                        );
                auxInfo.dataBlockBgn = updateInfo.blockBgn;
                auxInfo.loader = nullptr;                // TODO pick suitable loader?
                auxInfo.docDefaults.validityRange
                    = ValidityRange<KeyT>( ValidityKeyTraits<KeyT>::decode(dftFrom)
                            , ValidityKeyTraits<KeyT>::decode(dftFrom)
                            );

                auto docEntry
                    = new typename iValidityIndex<KeyT, typename iDocuments<KeyT>::DocumentLoadingState>::DocumentEntry{
                              updateInfo.docPath
                            , ValidityKeyTraits<KeyT>::decode(updateInfo.toKey)
                            , auxInfo
                        };
                auto ir = _docs.emplace(updateInfo.docID, &docEntry);
                assert(ir.first);
                it = ir.second;
            }
            // load document entry from DB index
            res.emplace(key, it->second);
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
        // TODO: noTypeIsOk
        _itemIDs.clear();
        _sqlDB.get_update_ids( _itemIDs
                    , typeName
                    , iSQLIndex::gKeyStart
                    , ValidityKeyTraits<KeyT>::encode(key)
                    , noTypeIsOk
                    , true
                    );
        return load_doc_details_into_updates_list(_itemIDs);
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
        // TODO: noTypeIsOk
        // TODO: keepStale
        _itemIDs.clear();
        _sqlDB.get_update_ids( _itemIDs
                    , typeName
                    , ValidityKeyTraits<KeyT>::encode(oldKey)
                    , ValidityKeyTraits<KeyT>::encode(newKey)
                    );
        return load_doc_details_into_updates_list(_itemIDs);
    }

    ///\brief Queries latest document entry for certain run number and type,
    ///       ignoring their history
    typename Updates::value_type
        latest( const std::string & typeName
              , KeyT key
              ) const override {
        // TODO: test it
        _itemIDs.clear();
        _sqlDB.get_update_ids( _itemIDs
                    , typeName
                    , ValidityKeyTraits<KeyT>::keyStart
                    , ValidityKeyTraits<KeyT>::encode(key)
                    );
        return load_doc_details_into_updates_list(_itemIDs);
    }
};

}  // namespace ::sdc::db
}  // namespace sdc

