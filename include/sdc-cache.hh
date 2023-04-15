#pragma once

#include "sdc-base.hh"

namespace sdc {
namespace errors {

}  // namespace ::sdc::errors

template<typename KeyT> void
save_index( std::ostream & os
          , const ValidityIndex<KeyT, typename Documents<KeyT>::DocumentEntry> & index
          , const std::unordered_map< const typename Documents<KeyT>::iLoader *
                                    , const std::string > & loadersDict
          ) {

}

template<typename KeyT> bool
restore_index( std::istream & is
             , const ValidityIndex<KeyT, typename Documents<KeyT>::DocumentEntry> &
             , const std::unordered_map< const std::string
                                       , typename Documents<KeyT>::iLoader * > &
             , bool doUpdate=false
             ) {

}

}  // namespace sdc

