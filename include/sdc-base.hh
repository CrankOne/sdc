/* SDC - A self-descriptive calibration data format library.
 * Copyright (C) 2022  Renat R. Dusaev  <renat.dusaev@cern.ch>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>. */

#pragma once

/**\file
 * \brief Simple calibration data utilities
 *
 * This is header-only library providing parser and indexing for the
 * _self-descriptive calibration_ data (or _simple calibration data_) format.
 *
 * A (very) basic usage:
 *
 *     // Load calibration data of type `CaloCalibData', valid for run 5103,
 *     // based on content of path/to/calib/files dir
 *     vector<CaloCalibData> cdata = load_calib( "path/to/calib/files", 5103 );
 *
 * That's it for the basic usage from initial specification.
 *
 * Advanced Usage
 * ==============
 *
 * Advanced usage scenarios imply using of not only the `CaloCalibData`, but
 * other types as well. This lib provides reentrant index for cached calibration
 * data query and acquizition, debug dumps, etc.
 *
 * `Documents` is a cache template object keeping information about multiple
 * sources of data matching certain _validity periods_. What type is used to
 * identify validity is subject of user's definition (it's a template
 * parameter further referred as `KeyT`). It must be of any aggregate type
 * (since we require it has a `constexpr` value for `unset` meaning).
 *
 * `Documents<KeyT>` can be composed incrementally, by adding various
 * documents at a runtime.
 *
 *     // Create a reentrant index object
 *     sdc::Documents<RunNo_t> docs;
 *     // ...
 *     docs.add("myfile1.txt");
 *     docs.add("myfile2.txt");
 *
 * then, same `Index` object can be used multiple times, to query and load
 * data for particular run:
 *
 *      auto caloCalibs = idx.load<CaloCalibData>(2374);
 *      auto apvCalibs = idx.load<APVPedestals>(4123);
 *
 * You can extend lib's capabilities to parse additional data types via C++
 * traits technique -- see `CalibDataTraits` template struct. Defining new
 * calibration data type would require user code to implement certain routines
 * like one for parsing structure from the CSV line (perhaps, with metadata
 * info provided by `MetaInfo` instance).
 *
 * Other classes defined in this file are _loaders_ that are responsible of
 * loading data from certain sources and look-up callables responsible for
 * gathering calibration files.
 *
 * Technical Restrictions
 * ======================
 *
 * This header requires rudimentary C++11 support that is provided at least by
 * GNU Compiler Collection starting from GCC 4.8; yet some compiler-related
 * bugs were observed for GCC 4.8:
 *  * `auto' return type crashed compiler, see:
 *      https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56014
 *    => resolved by specifying explicit return type for Index::get_entries_for()
 *  * regular expression is not fully supported, regex_compile causes
 *    segmentation fault;
 *    => foreseen different implementation for `is_numeric_literal()', less
 *    efficient
 * */

///\defgroup compile-definitions Macros steering general features.
///\defgroup type-traits Template type traits definitions.
///\defgroup indexing Utils related to index of certain data type
///\defgroup utils Various applied functions and tools (e.g. string manipulations)
///\defgroup errors Exception types

//
// Common includes
#include <cassert>
#include <stdexcept>
#include <unordered_map>
#include <list>
#include <map>
#include <set>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <functional>
#include <sstream>
#include <vector>
#include <limits>
#include <memory>
#include <algorithm>
// POSIX-specific
#include <fts.h>
#include <sys/stat.h>

/**\def SDC_NO_IMPLEM
 * \brief Disables inline implementation of SDC routines
 *
 * This macro may is used to control the way how SDC is used. When defined to
 * a true preprocessor value, the implementation is disabled in the header
 * file. If the macro is not defined, a self-implemented header is implied.
 *
 * A false preprocessor value has the intenral special meaning of the
 * implementation being compiled within a (shared) object file.
 *
 * \ingroup definitions
 * */

#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
#   include <string>
#   include <iostream>
#   include <iterator>
#   include <functional>
#   include <unordered_set>
#   include <ctype.h>
// POSIX-specific
#   include <glob.h>
#   include <fnmatch.h>
// ROOT
#   ifndef SDC_NO_ROOT
#       include <TFormula.h>
#       include <RVersion.h>
//  Issues reported with TFormula::IsValid() for version 5.x.x. Disable
//  validation for versions <6.0.0 (alternaitvely, can be forced
//  with -DSDC_TFORMULA_VALIDATION=0
#   ifndef SDC_TFORMULA_VALIDATION
#       if ROOT_VERSION_CODE >= ROOT_VERSION(6, 0, 0)
#           define SDC_TFORMULA_VALIDATION 1
#       else
#           define SDC_TFORMULA_VALIDATION 0
#       endif
#   endif
#   endif
#else
// ROOT (forward definitions)
#   ifndef NO_ROOT
class TFormula;  // fwd
#   endif
#endif

/**\def SDC_INTRADOC_MARKUP_T
 * \brief Type of markup data within a document.
 * 
 *  For ASCII columnar files it is usually a line number, for binary files this
 *  can be an offset, etc. Defined for the entire library.
 * 
 * \ingroup compile-definitions
 * */
#ifndef SDC_INTRADOC_MARKUP_T
#   define SDC_INTRADOC_MARKUP_T size_t
#endif

/**\def SDC_INLINE
 * \brief A helper macro forcing inline definitions when needed.
 *
 * \note Depends on `SDC_NO_IMPLEM` definition
 *
 * \ingroup compile-definitions
 * */

/**\def SDC_ENDDECL
 * \brief A helper macro enclosing declarations when needed.
 *
 * \note Depends on `SDC_NO_IMPLEM` definition
 *
 * \ingroup compile-definitions
 * */

// If macro is enabled, only declarations remain here. Use it for linkage
// compatibility.
#if defined(SDC_NO_IMPLEM) && SDC_NO_IMPLEM
#   define SDC_INLINE /* no inline */
#   define SDC_ENDDECL ;
#else
#   define SDC_ENDDECL /*;*/
#   ifndef SDC_INLINE
#       define SDC_INLINE inline
#   endif
#endif

// Compiler version macros to switch between implementations of some routines
#ifdef __GNUC__
/**\def GNU_C_COMPILER_VERSION
 * \brief Aux macro defining GCC compiler version
 *
 * Chooses alternative implementation for some features inducing bugs in
 * ancient GCC */
#   define GNU_C_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif

#if GNU_C_COMPILER_VERSION >= 40900
    // GCC < 4.9 has broken support for C++11 regular expressions
    #include <regex>

    #ifndef SDC_NUMERIC_REGEX
    #   define SDC_NUMERIC_REGEX "^[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?$"
    #endif
#endif

//
// Logging
#ifndef WARN_LOG   // TODO
#   include <iostream>
#   define WARN_LOG std::cerr
#endif

namespace sdc {

typedef SDC_INTRADOC_MARKUP_T IntradocMarkup_t;

#ifndef ENABLE_SDC_FIX001
#   define ENABLE_SDC_FIX001 1
#endif

//                                                   __________________________
// ________________________________________________/ Run ID & runs range types

/**\brief Validity identifier type
 *
 * A traits may be specialized for particular validity key type: run number,
 * astronomical time, etc, though generic implementation is good enought for
 * simple types like integer run number, `time_t` etc.
 *
 * \ingroup type-traits
 * */
template<typename T>
struct ValidityTraits {
    static constexpr T unset = 0;
    static constexpr char strRangeDelimiter = '-';
    static inline bool is_set(T id) { return unset != id; }
    static inline bool less(T a, T b) { return a < b; }
    static inline std::string to_string(T rn) { return std::to_string(rn); }
    static inline void advance(T & rn) { ++rn; };
    static inline T from_string(const std::string & stre);  // implemented below
    using Less = std::less<T>;
};

/**\brief A struct representing runs range; come at hand with intersection
 *       operator
 *
 * Parameterised with validity key type, defines a validity range -- usually,
 * a time or run number interval when certain data considered valid.
 *
 * \ingroup indexing
 * */
template<typename T>
struct ValidityRange {
    T from  ///< validity period start (inclusively)
    , to;  ///< validity period end (exclusively)

    ///\brief Returns intersection of two runs range
    ///
    /// Result may have `to` >= `from`
    /// that corresponds to an empty intersection -- checked by to-bool
    /// operator
    ValidityRange<T> operator&(const ValidityRange & b) const {
        ValidityRange<T> rr { ValidityTraits<T>::unset
                            , ValidityTraits<T>::unset
                            };
        if( ValidityTraits<T>::is_set(from) ) {
            if( ValidityTraits<T>::is_set(b.from) )
                rr.from = ValidityTraits<T>::less(from, b.from) ? b.from : from;
            else
                rr.from = from;
        } else {
            if( ValidityTraits<T>::is_set(b.from)) rr.from = b.from;
            // both unset otherwise => keep unset
        }
        if( ValidityTraits<T>::is_set(to) ) {
            if( ValidityTraits<T>::is_set(b.to) )
                rr.to = ValidityTraits<T>::less(to, b.to) ? to : b.to;
            else
                rr.to = to;
        } else {
            if( ValidityTraits<T>::is_set(b.to)) rr.to = b.to;
            // both unset otherwise => keep unset
        }
        return rr;
    }

    ///\brief Returns whether range defines at least one run number
    ///
    /// At least single limit unset means `true`. Otherwise, is the result of
    /// `from < to`
    explicit operator bool() const {
        // if one is not set and other is set, the range is non-empty
        if( ! (ValidityTraits<T>::is_set(from)
            && ValidityTraits<T>::is_set(to) ) ) return true;
        return ValidityTraits<T>::less(from, to);
    }
};

/**\brief Prints human-readable string for given validity range
 *
 * Produced string will consist of validity bounds delimited with marker
 * provided by `ValidityTraits<T>::strRangeDelimiter`. The validity bounds
 * are printed as it is defined by `ValidityTraits<T>::to_string()` except for
 * "unset" value which is denoted with "...".
 *
 * \ingroup utils */
template<typename T>
std::ostream & operator<<(std::ostream & os, const ValidityRange<T> & rr ) {
    if( ValidityTraits<T>::is_set(rr.from) )
        os << ValidityTraits<T>::to_string(rr.from);
    else
        os << "...";
    //if( ValidityTraits<T>::is_set(rr.to) && ValidityTraits<T>::is_set(rr.from) )
    os << ValidityTraits<T>::strRangeDelimiter;
    if( ValidityTraits<T>::is_set(rr.to) )
        os << ValidityTraits<T>::to_string(rr.to);
    else
        os << "...";
    return os;
}

//                                                                      _______
// ___________________________________________________________________/ Errors

namespace errors {

///\brief Generc SDC error
///
/// User code may catch exceptions of this class to block error propagation
/// from the SDC module.
///
///\ingroup errors
class RuntimeError : public std::runtime_error {
public:
    RuntimeError(const std::string & reason) : std::runtime_error(reason) {}
};

///\brief Indicates an error induced by user code implementation error
///
/// Typically arises when some API assumptions violated by user code
/// extensions/implementation.
///
///\ingroup errors
class UserAPIError : public RuntimeError {
public:
    /// Accepts description of the error
    UserAPIError(const std::string & reason) : RuntimeError(reason) {}
};

///\brief `Documents::iLoader` implementation failed to fullfill some requirements
///
///\ingroup errors
class LoaderAPIError : public UserAPIError {
public:
    /// Pointer to the loader subclass instance that caused current API error
    void * loaderPtr;
    /// Accepts loader pointer and description of the error
    LoaderAPIError( void * loaderPtr_
                  , const std::string & reason)
        : UserAPIError(reason)
        , loaderPtr(loaderPtr_)
        {}
};

///\brief Thrown on generic file I/O error (no access, doesn't exist, etc)
///
/// Generally means that "something" prevent us from accessing the file (it
/// doesn't exist, access issue, etc).
/// C/C++ does not define system-agnostic way to report on access error, so
/// currently it is not very detailed error.
///
///\ingroup errors
class IOError : public RuntimeError {
public:
    /// Problematic filename
    std::string filename;
public:
    /// Accepts name of the problematic file and reason description
    IOError( const std::string & filename_
           , const std::string & details )
        : RuntimeError(std::string("Filesystem IO error \""
                    + filename_ + "\": " + details))
        , filename(filename_)
        {}
    /// Accepts only reason description, filename should be appended upwards on
    /// stack
    IOError( const std::string & details )
        : RuntimeError(details)
        {}
};

///\brief Generic error while parsing a file (grammatic or semantic issue)
///
/// This error is pretty generic one, generally providing exhaustive details
/// of the lexical or semantic issue appeared within file. Some ddetails missed
/// by child classes constructors are mostly appended during unwinding of
/// ty/catch blocks.
///
///\ingroup errors
class ParserError : public RuntimeError {
public:
    /// Buffer for formatted error message returned by `what()`
    mutable char _errbf[1024];
    std::string exprTok  ///< expression or token caused the error
              , docID  ///< file caused the error
              ;
    size_t lineNo;  ///< number of the line within the file caused the error
public:
    /// Generic ctr, requres some positional info for user code feedback
    ParserError( const std::string & reason_
               , const std::string & tok=""
               , const std::string & docID_=""
               , size_t lineNo_=0
               )
        : RuntimeError(reason_)
        , exprTok(tok)
        , docID(docID_)
        , lineNo(lineNo_)
        {}

    ///\brief Returns formatted error description
    ///
    /// Has a number of switches defining conditional output of the error
    /// message with all details available (document ID, line number, token,
    /// etc).
    virtual const char* what() const noexcept override {
        size_t nWritten = 0;
        if( ! docID.empty() ) {
            nWritten += snprintf( _errbf + nWritten, sizeof(_errbf) - nWritten
                , "at document %s", docID.c_str() );
        }
        if( lineNo && nWritten < sizeof(_errbf)) {
            nWritten += snprintf( _errbf + nWritten, sizeof(_errbf) - nWritten
                , "%c%zu ", (docID.empty() ? '#' : ':'), lineNo );
        }
        if( RuntimeError::what()
         && '\0' != *RuntimeError::what()
         && nWritten < sizeof(_errbf)
         ) {
            nWritten += snprintf( _errbf + nWritten, sizeof(_errbf) - nWritten
                , "%s%s", (nWritten ? ": " : ""), RuntimeError::what() );
        }
        if( (!exprTok.empty()) && nWritten < sizeof(_errbf) ) {
            nWritten += snprintf( _errbf + nWritten, sizeof(_errbf) - nWritten
                , ", \"%s\"", exprTok.c_str() );
        }
        return _errbf;
    }
};


///\brief Thrown, if no metadata can be found for certain key in the
///       current file
///
///\ingroup errors
class NoMetadataEntryInFile : public ParserError {
public:
    /// Name of the problematic parameter in metadata
    const std::string name;
public:
    /// Accepts only the parameter name; details should be appended upwards on
    /// stack.
    NoMetadataEntryInFile( const std::string & name_ )
        : ParserError("no metadata entry `" + name_ + "' defined")
        , name(name_)
        {}
};

///\brief Thrown if metadata for certain key exists, but not before given
///       line number
///\ingroup errors
class NoCurrentMetadataEntry : public ParserError {
public:
    /// Name of the problematic parameter in metadata
    std::string mdEntryName;
public:
    /// Expects metadata parameter name and current line number
    NoCurrentMetadataEntry( const std::string & mdEntryName_, size_t lineNo_ )
        : ParserError( "metadata entry \"" + mdEntryName_ + "\" was expected"
                       " to be defined before this line"
                     , ""  // tok
                     , ""  // file
                     , lineNo_  // line number
                     )
        , mdEntryName(mdEntryName_)
        {}

    /// Accepts full stack of error details
    NoCurrentMetadataEntry( const std::string & mdName
                          , const std::string & details
                          , const std::string & token
                          , const std::string & file
                          , size_t lineNo_
                          ) : ParserError( details, token, file, lineNo_ )
                            , mdEntryName(mdName)
                            {}

    /// Returns formatted version of the error; relies on `ParserError::what()`
    /// and appends name of the problematic parameter if available.
    virtual const char* what() const noexcept override {
        // Compose initial part of the error message
        const char * errbf = ParserError::what();
        assert(errbf == _errbf);
        // Append with details on metadata key
        size_t nWritten = strlen(errbf);
        if( (!exprTok.empty()) && nWritten < sizeof(_errbf) ) {
            nWritten += snprintf( _errbf + nWritten
                                , sizeof(_errbf) - nWritten
                , " (key \"%s\")"
                , exprTok.c_str() );
        }
        return _errbf;
    }

};

///\brief Thrown if index fails to infer validity period for CSV block in file
///
///\ingroup errors
class NoValidityRange : public NoCurrentMetadataEntry {
public:
    /// Expects presumed name of the validity range metadata tag and line
    /// number to which this type of metadata was not defined
    NoValidityRange( const std::string & validityRangeTag
                   , size_t lineNo_
                   )
        : NoCurrentMetadataEntry( validityRangeTag
                , "unable to resolve runs validity range of block starting"
                  " from here"
                , ""  // tok
                , ""  // file (set by outern context)
                , lineNo_  // line number
                )
        {}
};

///\brief Thrown if index fails to set data type for file
///
///\ingroup errors
class NoDataTypeDefined : public NoCurrentMetadataEntry {
public:
    /// Expects presumed name of the data type metadata tag and line
    /// number to which this type of metadata was not defined
    NoDataTypeDefined( const std::string & dataTypeTag
                     , size_t lineNo_
                     )
        : NoCurrentMetadataEntry( dataTypeTag
                , "unable to resolve data type of block starting from here"
                , ""  // tok
                , ""  // file (set by outern context)
                , lineNo_  // line number
                ) {}
};

///\brief No calibration of certain type name defined (at all -- not just for
///       certain run).
///
///\ingroup errors
class UnknownDataType : public RuntimeError {
public:
    std::string typeName;
    UnknownDataType( const std::string tn )
        : RuntimeError(std::string( "No documents indexed for calibration data type: \"" + tn + "\"" ))
        , typeName(tn) {}
};

///\brief Unable to find calibration data for certain run number
///
/// Thrown when single data entry is requested from index, but no instance can
/// be found to correspond given validity key.
///
///\ingroup errors
class NoCalibrationData : public RuntimeError {
public:
    /// Type of interest for the query
    std::string typeName;
    /// Constructs key type-agnostic error message with type name metadata tag
    /// and validity key value of cewrtain type
    template<typename ValidityKeyT>
    NoCalibrationData( const std::string & tn
                     , ValidityKeyT key )
        : RuntimeError(std::string( "Could not find calibration of type \""
                                  + tn + "\" for key " + ValidityTraits<ValidityKeyT>::to_string(key) ))
        , typeName(tn) {}
};

///\brief Thrown when user parser function requests column that was not defined
///       in a metadata
///
///\ingroup errors
class NoColumnDefinedForTable : public ParserError {
public:
    /// Accepts name of the field of interest as problematic token
    NoColumnDefinedForTable(const std::string & fieldName)
        : ParserError( "No column of name in the table"
                     , fieldName ) {}
};

///\brief Document referenced, but no handlers are capable to handle it are
///       registered in current context
///
///\ingroup errors
class NoLoaderForDocument : public RuntimeError {
public:
    /// Identifier of the questionable document
    std::string docID;
    /// Accepts problematic document ID
    NoLoaderForDocument( const std::string di )
        : RuntimeError(std::string( "Can't parse document: \"" + di + "\"."
                    " None of registered loaders can handle it" ))
        , docID(di) {}
};

/// Base class of overlappign regions exception; never thrown directly, but
/// used to define overlapping regions errors within a file or between files.
///
///\ingroup errors
class OverlappingRangesError : public RuntimeError {
public:
    std::string dataType  ///< str name of the problematic data type
              , fileName  ///< (second) problematic document ID
              ;
    size_t prevEntryLineNo
         , thisEntryLineNo
         ;
protected:
    /// Accepts details on the problematic entries in two files
    OverlappingRangesError( const std::string & dataType_
                          , const std::string & fileName_
                          , size_t prevEntryLineNo_
                          , size_t thisEntryLineNo_
                          , const std::string & errMsg )
        : RuntimeError( errMsg )
        , dataType(dataType_)
        , fileName(fileName_)
        , prevEntryLineNo(prevEntryLineNo_)
        , thisEntryLineNo(thisEntryLineNo_)
        {}
};

///\brief found prohibited overlapping regions within a file (may be
///       disabled by the policy)
///
///\ingroup errors
class OverlappingRangesForDataTypeInFile : public OverlappingRangesError {
public:
    /// Accepts details on the problematic entries in a file
    OverlappingRangesForDataTypeInFile( const std::string & dataType_
                                      , const std::string & fileName_
                                      , size_t prevEntryLineNo_
                                      , size_t thisEntryLineNo_
                                      , const std::string & errMsg )
        : OverlappingRangesError( dataType_, fileName_, prevEntryLineNo_
                , thisEntryLineNo_, errMsg ) {}
};

///\brief found prohibited overlapping regions within a file (may be
///       disabled by the policy)
///
///\ingroup errors
class OverlappingRangesForDataTypeInFiles : public OverlappingRangesError {
public:
    /// Current problematic document ID
    std::string thisFileName;
    /// Accepts details on the problematic entries in files
    OverlappingRangesForDataTypeInFiles( const std::string & dataType_
                                       , const std::string & prevFileName_
                                       , size_t prevEntryLineNo_
                                       , const std::string & thisFileName_
                                       , size_t thisEntryLineNo_
                                       , const std::string & errMsg )
        : OverlappingRangesError( dataType_, prevFileName_, prevEntryLineNo_
                , thisEntryLineNo_, errMsg )
        , thisFileName(thisFileName_)
        {}
};

///\brief A "nested" exception class, used for complex cases
///
///\ingroup errors
template<typename E1>
class NestedError : public E1 {
private:
    char _e2What[512];
    mutable char _errbf[2048];
public:
    /// Accepts "embedded" exception instance and encompassing exception ctr args
    template<typename E2, typename ... ArgsE1>
    NestedError( E2 & e2, ArgsE1 ... argsE1 )
        : E1(argsE1...)
        {
        strncpy(_e2What, e2.what(), sizeof(_e2What)-1);
        _e2What[sizeof(_e2What)-1] = '\0';
    }

    /// Prints formatted error on both exceptions
    virtual const char* what() const noexcept override {
        snprintf( _errbf, sizeof(_errbf)
                , "Error ``%s'' occured, %s", _e2What, E1::what());
        return _errbf;
    }
};

}  // namespace ::sdc::errors

namespace aux {

class MetaInfo;  // fwd
template<typename> class Documents;

class LoadLog {
private:
    std::string _currentSrcID;
    size_t _lineNumber;
    struct Entry {
        const std::string srcID, columnName, value;
        size_t lineNumber;
    };
    std::list<Entry> _entries;
public:
    void set_source(const std::string & srcID, size_t lineNo) {
        _currentSrcID = srcID;
        _lineNumber = lineNo;
    }
    void add_entry( const std::string & columnLabelStr
                  , const std::string & valueStr
                  ) {
        _entries.push_back(Entry{_currentSrcID, columnLabelStr, valueStr, _lineNumber});
    }
    void to_json(std::ostream & os) const {
        os << "[";
        bool isFirst = true;
        for(const auto & entry : _entries) {
            if(!isFirst) os << ","; else isFirst = false;
            os << "{\"srcID\":\"" << entry.srcID << "\",\"lineNo\":" << entry.lineNumber
                << ",\"c\":\"" << entry.columnName << "\",\"v\":\"" << entry.value << "\"}";
        }
        os << "]";
    }
};

//                                         ___________________________________
// ______________________________________/ Utility string processing functions

///\brief Returns `true` if provided path matches wildcard expression
///
/// A C++ wrapper over C `fnmatch()` (3) function.
///
/// May throw `runtime_error()` in case of internal `fnmatch()` failure.
///
///\ingroup utils
SDC_INLINE bool
matches_wildcard( const std::string & pat
                , const std::string & path
                , int fnmatchFlags=0x0
                ) SDC_ENDDECL
#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
{
    int rc = fnmatch( pat.c_str(), path.c_str(), fnmatchFlags );
    if( 0 == rc ) return true;
    if( FNM_NOMATCH == rc ) return false;
    throw std::runtime_error( "Unexpected fnmatch() return code." );
}
#endif

///\brief Trims spaces from left and right of the line
///
///\ingroup utils
SDC_INLINE std::string
trim(const std::string & strexpr ) SDC_ENDDECL
#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
{
    std::string buf(strexpr);
    size_t n = 0;
    while( n < buf.size() && std::isspace(buf[n]) ) { ++n; }
    buf = buf.substr(n);
    if( ! buf.empty() ) {
        n = buf.size() - 1;
        while( n && std::isspace(buf[n]) ) { --n; }
        buf = buf.substr(0, n+1);
    }
    return buf;
}
#endif

///\brief Helper function for tokenization
///
///\ingroup utils
SDC_INLINE std::list<std::string>
tokenize(const std::string & expr, char delim) SDC_ENDDECL
#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
{
    std::list<std::string> r;
    size_t b = 0, e;
    do {
        e = expr.find(delim, b);
        std::string tok = expr.substr( b, e == std::string::npos
                                          ? std::string::npos
                                          : e - b
                                     );
        tok = ::sdc::aux::trim(tok);
        r.push_back(tok);
        b = e+1;
    } while( e != std::string::npos );
    return r;
}
#endif

///\brief Helper function for tokenization (by spaces)
///
///\ingroup utils
SDC_INLINE std::list<std::string>
tokenize(const std::string & expr) SDC_ENDDECL
#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
{
    std::istringstream iss(expr);
    return { std::istream_iterator<std::string>{iss}
           , std::istream_iterator<std::string>{}
           };
}
#endif

///\brief Reads next meaningful line from stream. Returns `false' on EOF
///
/// Used to obtain line from ASCII documents in line-based formats
/// (like "extended" CSV).
///
/// `CommentCallableT` is expected to be a callable object (function or class
/// instance) of signature `(const std::string &) -> std::pair<size_t, size_t>`,
/// i.e. must accept `std::string` and return a pair denoting of comment
/// start and end. If no comment symbol is provided in the line, must return
/// `std::string::npos` as first. The second of pair must be end of comment, if
/// comment end marker is located in the given line (thus, allowing inline
/// comments). If no end marker is in this line, must return
/// `std::string::npos` or position of the last char. If this is still a
/// multiline comment, a zero position must be returned for end denoting that
/// caller must iterate further and consider subsequent line(s) as starting
/// from comment.
///
/// \todo Support for multiline comments (useful for Donskov's para-XML)
///
///\ingroup utils
template<typename CommentCallableT>
bool getline( std::istream & ifs
                   , std::string & buf
                   , size_t & lineNo
                   , CommentCallableT comment_f
                   ) {
    do {
        if( ! ifs ) {
            buf.clear();
            return false;
        }
        std::getline( ifs, buf );
        ++lineNo;
        {  // strip comments
            std::pair<size_t, size_t> commentBounds;
            while( (commentBounds = comment_f(buf)).first != std::string::npos ) {
                buf = buf.replace(commentBounds.first, commentBounds.second, "");
                assert( commentBounds.second != 0 ); // TODO: to support
                // multiline comments we shall check second element of
                // returned pair, and if it is 0, iterate with STL's getline()
                // until comment_f() will return non-npos. This feature is to
                // be implemented at some point...
            }
        }
        buf = sdc::aux::trim(buf);
    } while(buf.empty());
    buf = sdc::aux::trim(buf);
    return true;
}

//                                                      _______________________
// ___________________________________________________/ Lexical Cast Utilities

///\brief Returns `true` if given string expression looks like a numeric literal
///
///\ingroup utils
inline bool
is_numeric_literal( const std::string & s ) {
    if( 'n' == tolower(s[0]) && 'a' == tolower(s[1]) && 'n' == tolower(s[2])
    && s.size() == 3 ) {
        return true;
    }  // inf?
    #if GNU_C_COMPILER_VERSION >= 40900
    static const std::regex rx(SDC_NUMERIC_REGEX);
    return std::regex_match(s, rx);
    #else
    // GCC 4.8 has malfunctioning support for regular expressons
    std::size_t pos;
    try {
        std::stold(s, &pos);
    } catch(std::invalid_argument&) {
        return false;
    } catch(std::out_of_range&) {
        return false;
    }
    return pos == s.size();
    #endif
}

///\brief Lexical cast traits; define to-/from- string conversions for various types
///
///\ingroup utils
template<typename T, class=void> struct LexicalTraits;

///\brief SDC's function for lexical cast. Meant to be used with specializations
///
///\ingroup utils
template<typename T> T lexical_cast(const std::string & s);

///\brief Specialization for STL string (trivial)
///
///\ingroup utils
template<> SDC_INLINE std::string
lexical_cast<std::string>(const std::string & s) SDC_ENDDECL
#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
{
    return s;
}
#endif

///\brief Specialization for boolean value
///
///\ingroup utils
template<> SDC_INLINE bool
lexical_cast<bool>(const std::string & strexpr) SDC_ENDDECL
#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
{
    static const std::set<std::string> trueLiterals
        = { "True", "true", "TRUE", "yes", "1" },
        falseLiterals = { "False", "false", "FALSE", "no", "0" };

    auto it = trueLiterals.find(strexpr);
    if( it != trueLiterals.end() ) return true;
    it = falseLiterals.find(strexpr);
    if( it != falseLiterals.end() ) return false;

    throw errors::ParserError( "expression does not look like boolean literal"
            , strexpr );
}
#endif

///\brief Specialization for standard signed integer
///
///\ingroup utils
template<> SDC_INLINE int
lexical_cast<int>(const std::string & strexpr) SDC_ENDDECL
#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
{
    try {
        return std::stoi(strexpr);
    } catch( const std::invalid_argument & ) {
        throw errors::ParserError( "stoi(): no conversion can be performed"
                                 , strexpr );
    } catch( const std::out_of_range & ) {
        throw errors::ParserError( "stoi(): out of range", strexpr);
    }
}
#endif

///\brief Specialization for long unsigned integer
///
///\ingroup utils
template<> SDC_INLINE unsigned long
lexical_cast<unsigned long>(const std::string & strexpr) SDC_ENDDECL
#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
{
    try {
        return std::stoul(strexpr);
    } catch( const std::invalid_argument & ) {
        throw errors::ParserError( "stoul(): no conversion can be performed"
                                 , strexpr );
    } catch( const std::out_of_range & ) {
        throw errors::ParserError( "stoul(): out of range", strexpr);
    }
}
#endif

///\brief Specialization for long unsigned integer
///
///\ingroup utils
template<> SDC_INLINE long int
lexical_cast<long int>(const std::string & strexpr) SDC_ENDDECL
#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
{
    try {
        return std::stol(strexpr);
    } catch( const std::invalid_argument & ) {
        throw errors::ParserError( "stol(): no conversion can be performed"
                                 , strexpr );
    } catch( const std::out_of_range & ) {
        throw errors::ParserError( "stol(): out of range", strexpr);
    }
}
#endif

///\brief Specialization for single-precision floating point number (supports
///       arithmetics)
///
///\ingroup utils
template<> SDC_INLINE float
lexical_cast<float>(const std::string & strexpr) SDC_ENDDECL
#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
{
    if( is_numeric_literal(strexpr) ) {
        try {
            return std::stof(strexpr);
        } catch( const std::invalid_argument & ) {
            throw errors::ParserError( "stof(): no conversion can be performed"
                                     , strexpr );
        } catch( const std::out_of_range & ) {
            throw errors::ParserError( "stof(): out of range", strexpr);
        }
    }
    #ifdef SDC_NO_ROOT
    throw errors::ParserError( "expression does not match a numeric literal pattern"
            , strexpr );
    #else
    // try to directly evaluate arithmetic expression using TFormula
    TFormula f("tmpFormula", strexpr.c_str());
    #if defined(SDC_TFORMULA_VALIDATION) && SDC_TFORMULA_VALIDATION
    if( ! f.IsValid() ) {
        throw errors::ParserError( "invalid numerical literal, formula, or"
                " arithmetic expression", strexpr);
    }
    #endif
    const double r = f.Eval(0.);
    return r;
    #endif
}
#endif

///\brief Specialization for single-precision floating point number (supports
///       arithmetics)
///
///\ingroup utils
template<> SDC_INLINE double
lexical_cast<double>(const std::string & strexpr) SDC_ENDDECL
#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
{
    if( is_numeric_literal(strexpr) ) {
        try {
            return std::stod(strexpr);
        } catch( const std::invalid_argument & ) {
            throw errors::ParserError( "stod(): no conversion can be performed"
                                     , strexpr );
        } catch( const std::out_of_range & ) {
            throw errors::ParserError( "stod(): out of range", strexpr);
        }
    }
    #ifdef SDC_NO_ROOT
    throw errors::ParserError( "expression does not match a numeric literal pattern"
            , strexpr );
    #else
    // try to directly evaluate arithmetic expression using TFormula
    TFormula * f = new TFormula("tmpFormula", strexpr.c_str());
    #if defined(SDC_TFORMULA_VALIDATION) && SDC_TFORMULA_VALIDATION
    if( ! f->IsValid() ) {
        throw errors::ParserError( "invalid numerical literal, formula, or"
                " arithmetic expression", strexpr);
    }
    #endif
    const double r = f->Eval(0.);
    delete f;
    return r;
    #endif
}
#endif

// ... (other simple casts here)

///\brief Specialization for runs range
///
/// This pair is extremely important, so do some advanced checks and provide
/// all the details on failure to assure we've set it properly
///
///\ingroup utils
template<typename T>
struct LexicalTraits< ValidityRange<T> > {
    static ValidityRange<T> from_string(const std::string & strexpr) {
        size_t delimPos = strexpr.find(ValidityTraits<T>::strRangeDelimiter);
        ValidityRange<T> rr = { T(ValidityTraits<T>::unset)
                              , T(ValidityTraits<T>::unset)
                              };
        assert(delimPos == std::string::npos || delimPos < strexpr.size());
        try {
            if(delimPos) {
                std::string subtok(strexpr.substr(0, delimPos));
                subtok = aux::trim(subtok);
                if(subtok != "...") {
                    rr.from = ValidityTraits<T>::from_string(subtok);
                } else {
                    throw errors::ParserError( "Left open bounds for validity"
                            " range is not permitted", strexpr );
                }
            }
            if(std::string::npos != delimPos) {
                std::string subtok(strexpr.substr(delimPos+1));
                subtok = aux::trim(subtok);
                if(subtok != "...") {
                    rr.to = ValidityTraits<T>::from_string(subtok);
                    ValidityTraits<T>::advance(rr.to);
                } else {
                    rr.to = T(ValidityTraits<T>::unset);
                }
            } else {
                // defined for a single run -- set "to" to "from + 1"
                if( !ValidityTraits<T>::is_set(rr.from) ) {
                    throw errors::ParserError( "Bad runs range expression"
                                             , strexpr );
                }
                rr.to = rr.from;
                ValidityTraits<T>::advance(rr.to);
            }
        } catch( const std::invalid_argument & ) {
            throw errors::ParserError( "Conversion can't be performed"
                                     , strexpr);
        } catch( const std::out_of_range & ) {
            throw errors::ParserError( "Value out of range", strexpr);
        }
        return rr;
    }

    // TODO: not covered by UT.
    static std::string to_string( const ValidityRange<T> & rr ) {
        std::stringstream ss;
        if( ValidityTraits<T>::is_set(rr.from) ) {
            ss << ValidityTraits<T>::to_string(rr.from);
        } else {
            ss << "...";
        }
        ss << ValidityTraits<T>::strRangeDelimiter;
        if( ValidityTraits<T>::is_set(rr.to) ) {
            ss << ValidityTraits<T>::to_string(rr.to);
        } else {
            ss << "...";
        }
        return ss.str();
    }
};

///\brief Generic implementation of `lexical_cast<>()` based on STL
///
///\ingroup utils
template<typename T> T lexical_cast(const std::string & s) {
    return LexicalTraits<T>::from_string(s);
}

///\brief An object wrapper over `lexical_cast<>()`
///
///\ingroup utils
struct Value : public std::string {
    Value(const std::string & s) : std::string(s) {}
    template<typename T> operator T() const {
        return lexical_cast<T>(*this);
    }
};


///\brief For given key finds a range of "most recent" entries
///
/// By "most recent" we imply:
///  - left bound is first to be equal to or just one before the key
///    (equivalent to lower_bound() for reverse sequence for preserved
///    insertion order)
///  - right bound goes just after the key
///
/// For instance instance, for 10 <= k < 14 and follwoing sequence
///      3/a, 5/a, 10/a, 10/b, 10/c, 14/a, 20/a
///                ^^^^-left ....... ^^^^-right
/// The "most recent" range will cover, in insertion order, elements 10/a,
/// 10/b, 10/c, and the right (second) will refer ro 14/a.
///
/// Utility purpose of this function is to find and enqueue updates of
/// calibration data for certain validity period. To do that we must use most
/// recent ones with repsect to this period and apply them in proper order.
///
/// This function must be slightly more efficient for large maps than direct
/// lookup, however it is used only for non-overlay strategy.
///
///\ingroup utils
template<typename T> std::pair< typename T::const_iterator
                              , typename T::const_iterator
                              >
inv_eq_range( const T & m
            , const typename T::key_type k ) {
    if( m.empty() || typename T::key_compare()(k, m.begin()->first) )
        return {m.begin(), m.begin()};
    auto p(m.equal_range(k));
    if( p.first == p.second ) {
        p.first = typename T::const_reverse_iterator(m.lower_bound(k)).base();
        --p.first;
    }
    p.first = m.lower_bound(p.first->first);
    return p;
}

///\brief An utility metadata type for columns description
///
///\ingroup utils
struct ColumnsOrder : public std::unordered_map<std::string, int> {
    /// Auxiliary class representing semantically-parsed expression
    ///
    /// An instance of parsed CSV line tokens interpreted acording to a particular
    /// column order
    struct CSVLine : public std::unordered_map<std::string, std::string> {
        /// Returns a token with given semantics (column name) or throws an error
        Value operator()(const std::string & name) const {
            auto it = this->find(name);
            if( this->end() == it )
                throw errors::NoColumnDefinedForTable(name);  // TODO
            return it->second;
        }
        /// Returns a parsed value with given semantics or default one
        template<typename T>
        T operator()(const std::string & name, const T & default_) {
            auto it = this->find(name);
            if( this->end() == it ) return default_;
            return lexical_cast<T>(it->second);
        }
    };  // class CSVLine

    #if 0
    /// Checks that parsed columns all are from certain set
    ColumnsOrder & validate( const std::set<std::string> & allowed ) {
        for( const auto & e : *this ) {
            if( allowed.find(e.first) == allowed.end() )
                throw errors::BadColumn()
        }
    }
    #endif

    CSVLine interpret(const std::list<std::string> & toks_, LoadLog * loadLogPtr=nullptr);
};

#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
SDC_INLINE ColumnsOrder::CSVLine
ColumnsOrder::interpret(const std::list<std::string> & toks_, LoadLog * loadLogPtr) {
    std::vector<std::string> toks(toks_.begin(), toks_.end());
    CSVLine l;
    for( const auto & meaning : *this ) {
        if(toks.size() <= (unsigned int) meaning.second) {
            char errBuf[256];
            snprintf(errBuf, sizeof(errBuf)
                    , "Columns number mismatch; no column #%d expected"
                    " for \"%s\" in current line (has only %zu columns)"
                    , meaning.second + 1, meaning.first.c_str(), toks.size()
                    );
            throw errors::ParserError(errBuf);
        }
        l[meaning.first] = toks[meaning.second];
        if(!loadLogPtr) continue;
        loadLogPtr->add_entry(meaning.first, toks[meaning.second]);
    }
    return l;
}
#endif

///\brief Parses columns order definition
///
///\ingroup utils
template<> SDC_INLINE ColumnsOrder
lexical_cast<ColumnsOrder>(const std::string & strexpr) SDC_ENDDECL
#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
{
    ColumnsOrder ord;
    auto columns = tokenize(strexpr, ',');
    size_t nTok = 0;
    for(auto col : columns) {
        ord[col] = nTok++;
    }
    return ord;
}
#endif

//                                                         ____________________
// ______________________________________________________/ Filesystem Routines

/**\brief auxiliary class providing recursive iteration over FS hierarchy
 *
 * This class is essentially a wrapper over `fts_open()` family of functions
 * and provides a generator returning a filesystem entry path for recursive
 * traversal with a callble object.
 *
 * It is used in SDC to discover calibration files in directories.
 *
 * \note `input` array is assumed to be managed externally
 * \ingroup utils
 * */
class FTSBase {
protected:
    /// Reentrant filesystem handle for `fts_open()` family of functions
    FTS * _fhandle;
    FTSENT * _parent  ///< Ptr to parent entry of `fts_open()` functions
         , * _child  ///< Ptr to child entry of `fts_open()` functions
         ;
protected:
    ///\brief Returns, whether the FTS entry passes the filtering
    ///
    /// User can re-define this in subclass to provide additional filtering
    /// for recursive FS entries iteration (e.g. by name, size, modification
    /// date, etc).
    virtual bool _fits( FTSENT * c ) const {
        return c->fts_info == FTS_F;
    }
    /// Sets state to the next FS entry
    void _next_child() {
        assert(_fhandle);
        do {  // get first non-dir entry
            _child = fts_read(_fhandle);
        } while( _child && !_fits(_child) );
    }
public:
    /// (internal) FS entry comparison callback for `fts_open()` functions
    static inline int compare_ftsent( const FTSENT ** first, const FTSENT ** second ) {
        //return (strcmp((*first)->fts_name, (*second)->fts_name));
        return (strcoll((*first)->fts_name, (*second)->fts_name));
    }

    /// Initializes iterator on a dirs; can be made to provide also `stat`
    /// struct for FS entries.
    FTSBase( char * const * input
           , int ftsOpenFlags=FTS_COMFOLLOW | FTS_NOCHDIR
           , int (*compar)(const FTSENT **, const FTSENT **)=&FTSBase::compare_ftsent
           );

    ~FTSBase();

    /// \brief calls object on one of the entries and switches to the next
    ///
    /// \param callable object must implement invocation operator for `(FTSENT *)`.
    /// \returns whether more entries are available
    template<typename T>
    bool call_on_next( T callable ) {
        _next_child();
        if( ! _child ) return false;  // we're done
        callable(_child);
        return true;
    }

    ///\brief Returns path to next FS entry or empty if no more children
    std::string operator()() {
        _next_child();
        if( !_child ) return "";
        std::string path = _child->fts_path;
        return path;
    }
};  // class FTSBase

#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
SDC_INLINE
FTSBase::FTSBase( char * const * input
                , int ftsOpenFlags
                , int (*compar)(const FTSENT **, const FTSENT **)
                ) : _fhandle(NULL)
    {
    char ftsErrBuf[256];
    assert(input);
    assert(*input);
    _fhandle = fts_open( input
                       , ftsOpenFlags | FTS_NOCHDIR
                       , compar );
    // ^^^ `FTS_NOCHDIR` is required here as typically SDC immediately opens
    //     returned file by the (possibly) relative path. If this flag is
    //     not set, `fts_open()` chdir's to the dir struct invalidating
    //     relative paths.
    if( !_fhandle ) {
        int ftsOpenErrNo = errno;
        char * strerrResult =
            strerror_r(ftsOpenErrNo, ftsErrBuf, sizeof(ftsErrBuf));
        (void)strerrResult;  // supress warning on NDEBUG
        assert(strerrResult == ftsErrBuf);
        std::string errDetails(ftsErrBuf);
        errDetails = "fts_open() error: " + errDetails;
        size_t nByte = 0;
        for( char * const * ci = input
           ; *ci && '\0' != **ci && nByte < sizeof(ftsErrBuf)
           ; ++ci ) {
            nByte += snprintf(ftsErrBuf + nByte, sizeof(ftsErrBuf) - nByte
                    , "\"%s\", ", *ci );
        }
        fts_close(_fhandle);  // free handle
        _fhandle = NULL;
        throw errors::IOError(errDetails + " on " + ftsErrBuf);
    }
    // get into a dir provided by input
    _parent = fts_read(_fhandle);
    if(!_parent) {
        int ftsOpenErrNo = errno;
        fts_close(_fhandle);  // free handle
        _fhandle = NULL;
        char * strerrResult
            = strerror_r(ftsOpenErrNo, ftsErrBuf, sizeof(ftsErrBuf));
        (void) strerrResult;  // supress warning on NDEBUG
        assert(strerrResult == ftsErrBuf);
        std::string errDetails(ftsErrBuf);
        throw errors::IOError(ftsErrBuf, ftsErrBuf);
    }
    assert(_fhandle);
}
#endif

#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
SDC_INLINE
FTSBase::~FTSBase() {
    if(_fhandle)
        fts_close(_fhandle);  // free handle
}
#endif

/**\brief Iterates over FS subtree recursively supporting additional filtering
 *
 * Most common cases to travers FS subtree is name/size discrimination. This
 * subclass of `FTSBase` implements this by providing accept/reject name
 * patterns (reject takes precedence) and size discrimination (takes precedence
 * over name) as well as some convenient string conversion for ctr.
 *
 * To find out why certain file was omitted, one may set public logstream
 * ptr for the instance.
 *
 * \ingroup utils
 * */
class FS {
protected:
    /// Helper subclass performing filtering by name and size
    struct FTSFiltered : public FTSBase {
        std::ostream * logStreamPtr;
        std::vector<std::string> acceptPatterns
                               , rejectPatterns
                               ;
        ::off_t fileSizeMin  ///< Min size limit, if non-zero
              , fileSizeMax  ///< Max size limit, if non-zero
              ;
        ///\brief Overriden to check additional criteria
        bool _fits( FTSENT * ) const override;
        ///\brief Returns pointer to current children stats
        ///
        /// May return NULL if the instance is invalid state or `FTS_NOSTAT`
        /// flag was set within `ftsOpenFlags` argument to ctr.
        ///
        ///\note If `FTS_NOSTAT` was set, returned ptr may be undefined
        ///      (according to `man fts_open()`).
        struct stat * cur_child_stat_ptr() {
            if(!this->_child) return NULL;
            return this->_child->fts_statp;
        }

        FTSFiltered( char * const * input
                   , int ftsOpenFlags=FTS_COMFOLLOW | FTS_NOCHDIR
                   , int (*compar)(const FTSENT **, const FTSENT **)=&FTSBase::compare_ftsent
                   ) : FTSBase(input, ftsOpenFlags, compar)
                     , logStreamPtr(nullptr)
                     {}
        inline virtual ~FTSFiltered(){}
    };
protected:
    std::list<std::string> _pathsList;
    std::vector<const char *> _dirPaths;
    std::vector<const char *>::const_iterator _cFileIt;
    std::vector<const char *> _standaloneFiles;
    FTSFiltered * _fts;
public:
    FS( const std::string & paths
      , const std::string & acceptPatterns=""
      , const std::string & rejectPatterns=""
      , ::off_t fileSizeMin=0, ::off_t fileSizeMax=0
      );

    FS(const FS &);
    //FS(FS &&);  // TODO?

    ~FS();

    bool set_logstream(std::ostream * ptr) {
        if( !_fts ) {
            return false;
        }
        _fts->logStreamPtr = ptr;
        return true;
    }

    ///\brief Simple call operator for simple `Documents::add_from()`
    ///
    /// Changes state of this generator instance. If 
    std::string operator()() {
        std::string p;
        if(_fts) p = (*_fts)();
        if(!p.empty()) return p;
        if( _cFileIt != _standaloneFiles.end() ) return *(_cFileIt++);
        return "";
    }
};  // class FS

#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
SDC_INLINE bool
FS::FTSFiltered::_fits( FTSENT * c ) const {
    std::string filepath( c->fts_path );
    //*logStreamPtr << "xxx " << filepath << std::endl;  // XXX
    if( ! FTSBase::_fits(c) ) {
        if( logStreamPtr )
                *logStreamPtr << "  \"" << filepath
                        << "\" not a file" << std::endl;
        return false;
    }
    // check size and name if need
    {  // size
        if( fileSizeMin && fileSizeMin > c->fts_statp->st_size ) {
            if( logStreamPtr )
                *logStreamPtr << "  file \"" << filepath
                        << "\" too small (" << c->fts_statp->st_size
                        << "b < " << fileSizeMin << std::endl;
            return false;  // file too small
        }
        if( fileSizeMax && fileSizeMax < c->fts_statp->st_size ) {
            if( logStreamPtr )
                *logStreamPtr << "  file \"" << filepath
                        << "\" too big (" << c->fts_statp->st_size
                        << "b > " << fileSizeMin << std::endl;
            return false;  // file too big
        }
    }
    {  // name
        bool doAccept = acceptPatterns.empty();
        for( const auto & accPat : acceptPatterns ) {
            doAccept |= aux::matches_wildcard(accPat, filepath);
            if(doAccept) {
                if( logStreamPtr )
                    *logStreamPtr << "  file \"" << filepath
                            << "\" accepted by pattern \"" << accPat
                            << "\"" << std::endl;
                break;
            }
        }
        if(!doAccept) {
            if( logStreamPtr )
                    *logStreamPtr << "  file \"" << filepath
                            << "\" did not fit any \"accept\" pattern"
                            << std::endl;
            return false;  // hasn't been accepted by pattern
        }
        for( const auto & rejPat : rejectPatterns ) {
            doAccept &= !aux::matches_wildcard(rejPat, filepath);
            if(!doAccept) {
                if( logStreamPtr )
                    *logStreamPtr << "  file \"" << filepath
                            << "\" rejected by pattern \"" << rejPat
                            << "\"" << std::endl;
                break;
            }
        }
        if(!doAccept) return false;  // rejected by pattern
    }
    if( logStreamPtr )
        *logStreamPtr << "    file \"" << filepath
                << "\" accepted" << std::endl;
    return true;
}
#endif

#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
SDC_INLINE
FS::FS( const std::string & paths_
      , const std::string & acceptPatterns
      , const std::string & rejectPatterns
      , ::off_t fileSizeMin, ::off_t fileSizeMax
      ) {
    // tokenize paths and, if need, patterns
    _pathsList = aux::tokenize(paths_, ':');

    std::list<std::string> acceptList
                         , rejectList
                         ;
    if( !acceptPatterns.empty() ) acceptList = aux::tokenize(acceptPatterns, ':');
    if( !rejectPatterns.empty() ) rejectList = aux::tokenize(rejectPatterns, ':');
    #if 1
    for(std::string & cpath : _pathsList) {
        if(cpath.empty()) continue;
        struct stat cPathStat;
        stat(cpath.c_str(), &cPathStat);  // also follows symlinks
        switch( cPathStat.st_mode & S_IFMT ) {
            case S_IFDIR : _dirPaths.push_back(cpath.c_str()); break;
            case S_IFREG : _standaloneFiles.push_back(cpath.c_str()); break;
            default:
                std::cerr << "Ignoring path \"" << cpath
                    << "\" (not a file or directory." << std::endl;
                // ^^^ TODO: redirect warning
        };
    }
    #else
    std::transform( pathsList.begin(), pathsList.end()
                  , std::back_inserter(paths)
                  , []( const std::string & stls ) { return stls.c_str(); }
                  );
    #endif
    _dirPaths.push_back(NULL);  // sentinel for `fts_open()'
    _cFileIt = _standaloneFiles.begin();
    if( _dirPaths.size() > 1 ) {
        // Instantiate FTS wrapper object
        _fts = new FTSFiltered( const_cast<char * const *>(_dirPaths.data())
                            // ^^^ todo: why does it return `const char **` instead
                            //     of `const char * const *` ?
                            , (fileSizeMin | fileSizeMax) ? 0x0 : FTS_NAMEONLY
                            );
        // set patterns and sizes
        _fts->acceptPatterns
            = std::vector<std::string>(acceptList.begin(), acceptList.end());
        _fts->rejectPatterns
            = std::vector<std::string>(rejectList.begin(), rejectList.end());
        _fts->fileSizeMin = fileSizeMin;
        _fts->fileSizeMax = fileSizeMax;
    } else {
        _fts = nullptr;
    }
}
#endif

#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
SDC_INLINE
FS::FS(const FS & o) : _pathsList(o._pathsList)
                     , _fts(nullptr)
                     {
    std::unordered_set<std::string> files( o._standaloneFiles.begin()
                                         , o._standaloneFiles.end() );
    for(const std::string & cptok : _pathsList) {
        if(cptok.empty()) continue;
        if(files.find(cptok) == files.end()) {
            _dirPaths.push_back(cptok.c_str());
        } else {
            _standaloneFiles.push_back(cptok.c_str());
        }
    }
    _dirPaths.push_back(NULL);
    _cFileIt = _standaloneFiles.begin();
    if( _dirPaths.size() > 1 ) {
        // Create FTS instance
        _fts = new FTSFiltered( const_cast<char * const *>(_dirPaths.data())
                              , (o._fts->fileSizeMin | o._fts->fileSizeMax) ? 0x0 : FTS_NAMEONLY
                              );
        _fts->logStreamPtr = o._fts->logStreamPtr;
        // set patterns and sizes
        _fts->acceptPatterns = o._fts->acceptPatterns;
        _fts->rejectPatterns = o._fts->rejectPatterns;
        _fts->fileSizeMin = o._fts->fileSizeMin;
        _fts->fileSizeMax = o._fts->fileSizeMax;
    }
}
#endif

#if (!defined(SDC_NO_IMPLEM)) || !SDC_NO_IMPLEM
SDC_INLINE
FS::~FS() {
    if(_fts) delete _fts;
    _fts = nullptr;
}
#endif

//                                                              _______________
// ___________________________________________________________/ Metadata Index

///\brief A dictionary of file's meta information
///
/// Provides indexing and caching utilities to keep and retrieve metadata
/// entries of various types. Caches results to avoid re-creation of the
/// complex types (like TFormula).
///
///\ingroup indexing
struct MetaInfo
    : protected std::unordered_multimap< std::string
                                       , std::pair<size_t, std::string>
                                       > {
public:
    typedef std::unordered_multimap< std::string
                                   , std::pair<size_t, std::string>
                                   > Parent;
private:
    /// Key for metainfo value cache
    typedef std::tuple<std::string, size_t, const std::type_info &> CacheKey;
    /// Hashing function for cache value
    struct CacheKeyHash
            /*: public std::unary_function<CacheKey, std::size_t>*/ {
        std::size_t operator()(const CacheKey & k) const {
            return std::hash<std::string>{}(std::get<0>(k))
                 ^ (std::get<1>(k) << 1)
                 ^ (std::get<2>(k).hash_code() >> 1);
        }
    };
    /// Basic cache entry
    struct BaseMetaInfoCache { virtual ~BaseMetaInfoCache(){} };
    /// Specialized cache entry
    template<typename T> struct MetaInfoCachedValue : public BaseMetaInfoCache {
        T value;
        MetaInfoCachedValue(const std::string & strv) : value(aux::lexical_cast<T>(strv)) {}
    };
private:
    /// A dictionary of cached metainfo values
    mutable std::unordered_map< CacheKey
                              , std::shared_ptr<BaseMetaInfoCache>
                              , CacheKeyHash
                              > _cache;
    /// Dictionary of MD aliases
    ///
    /// Mapping is from alias to "true" name (one to many)
    std::unordered_map<std::string, std::string> _aliases;
    /// Dictionary of reverse aliases
    ///
    /// Mapping is from "true" name to alias (many to one)
    std::unordered_multimap<std::string, std::string> _revAliases;
public:
    using Parent::iterator;
    using Parent::const_iterator;
    using Parent::begin;
    using Parent::end;
    using Parent::equal_range;
    using Parent::empty;
    using Parent::size;

    MetaInfo() {}

    /// Copies only the MD key/value pairs, cache is not copied
    MetaInfo(const MetaInfo & o) : Parent(o) {}
    MetaInfo& operator=(const MetaInfo & o) {
      if (&o != this) { // skip self-assign
        clear();
        insert(o.cbegin(), o.cend());
        _cache.clear();
      }
      return *this;
    }

    ///\brief Defines MD name alias
    bool define_alias(const std::string & aliasName, const std::string & trueName_) {
        std::string trueName = resolve_alias_if_need(trueName_);
        auto it1 = _aliases.emplace(aliasName, trueName);
        if(!it1.second) {
            if(it1.first->second != trueName)
                return false;  // may indicate serious error: colliding aliases for different "true types"
            return true;
        }
        _revAliases.emplace(trueName, aliasName);
        return true;
    }

    ///\brief Tries to resolve aliased MD, otherwise returns argument intact
    std::string resolve_alias_if_need(const std::string & name) const {
        auto it = _aliases.find(name);
        if(_aliases.end() == it)
            return name;
        return it->second;
    }

    /// A parent container
    //typedef std::unordered_multimap< std::string
    //                               , std::pair<size_t, std::string>
    //                               > Parent;
    /// Retrieves a value by key, returns map by line numbers
    std::map<size_t, std::string> operator[]( const std::string & name_ ) const {
        std::string name = resolve_alias_if_need(name_);
        auto eqr = equal_range(name);
        std::map<size_t, std::string> m;
        std::transform( eqr.first, eqr.second
                , std::inserter(m, m.end())
                , [](const std::pair< std::string
                                    , std::pair<size_t, std::string> > &p
                     ) {return p.second;}
                );
        return m;
    }

    ///\brief Returns `false` if no such key exists for any line
    bool has(const std::string & name) const
        { return end() != find(name); }

    /// \brief Retrieves a value by key from the metadata (defined before
    /// certain line number)
    ///
    /// The `lineNo' parameter means that, in case of names collision, only
    /// entry that is on that line or line before will be considered. This is
    /// done to handle cases, when values of the metadata are overriden within
    /// a single file.
    ///
    /// If `foundLineNo` ptr is provided, it will be set to the found entry,
    /// if any.
    ///
    /// \throws `sdc::errors::NoMetadataEntry` error if no values(s) found.
    /// \throws `sdc::errors::NoCurrentMetadataEntry` if no value(s) found for line
    std::string get_strexpr( const std::string & name_
                           , size_t lineNo=std::numeric_limits<size_t>::max()
                           , size_t * foundLineNo=nullptr
                           ) const {
        const auto vs = this->operator[](name_);
        if( vs.empty() ) {
            // no metadata with such key is defined in file (at all)
            throw errors::NoMetadataEntryInFile(name_);
        }
        std::map<size_t, std::string>::const_reverse_iterator it(vs.upper_bound(lineNo));
        if( it == vs.rend() ) {
            // no metadata entry with such key defined in file (till this line)
            throw errors::NoCurrentMetadataEntry(name_, lineNo);
        }
        if( foundLineNo ) *foundLineNo = it->first;
        return it->second;
    }
    
    ///\brief Retrieves a value by key from the metadata and performs lexical
    ///       cast.
    ///
    /// Locates the metadata variable by forwards call to non-template
    /// `meta_value()` method and tries to return a cached value of
    /// this type. If no cache found, does a lexical
    /// cast on the entry using `ValueTraits` and updates the cache.
    ///
    /// Can also propagate excpetions from `lexical_cast<>()`.
    ///
    /// \throws `sdc::errors::NoMetadataEntry` error if no values(s) found.
    /// \throws `sdc::errors::NoCurrentMetadataEntry` if no value(s) found for line
    template<typename T> T
    get( const std::string & name_
       , size_t lineNo=std::numeric_limits<size_t>::max() ) const {
        size_t lFound = 0;
        auto strexpr = this->get_strexpr(name_, lineNo, &lFound);
        std::string name = resolve_alias_if_need(name_);
        // try to retrieve the cache
        const CacheKey k = CacheKey{name, lFound, typeid(T)};
        auto cacheIt = _cache.find(k);
        if( _cache.end() == cacheIt ) {
            auto ir = _cache.emplace( k,
                std::shared_ptr<BaseMetaInfoCache>(new MetaInfoCachedValue<T>(
                        strexpr) ) );
            assert(ir.second);
            cacheIt = ir.first;
        }
        #ifndef NDEBUG
        // in debug build, make sure we have same cached value
        assert( static_cast<MetaInfoCachedValue<T>*>(cacheIt->second.get())->value
             == lexical_cast<T>(strexpr) );
        #endif
        return static_cast<MetaInfoCachedValue<T>*>(cacheIt->second.get())->value;
    }

    ///\brief Retrieves a value by key from the metadata and performs lexical
    ///       cast or uses default value
    ///
    /// Locates the metadata variable by forwards call to non-template
    /// `meta_value()` method and tries to return a cached value of
    /// this type. If no cache found, does a (recaching) lexical
    /// cast on the entry using `ValueTraits` and updates the cache.
    ///
    /// Can throw other exceptions defined for lexical cast failures
    template<typename T> T
    get( const std::string & name_
       , const T & default_
       , size_t lineNo=std::numeric_limits<size_t>::max()
       ) const {
        const auto vs = this->operator[](name_);
        std::map<size_t, std::string>::const_reverse_iterator
                    it(vs.upper_bound(lineNo));
        if( it == vs.rend() ) {
            return default_;
        }
        // standard re-caching get (todo: optimize it?)
        return this->get<T>(name_, lineNo);
    }

    ///\brief Value setter for user code
    ///
    /// Sets the metadata value (string).
    void set( const std::string & name_
            , const std::string & value
            , size_t lineNo=std::numeric_limits<size_t>::min()
            ) {
        std::string name = resolve_alias_if_need(name_);
        emplace(name, std::pair<size_t, std::string>(lineNo, value));
    }

    void drop( const std::string & name
             , size_t lineNo=std::numeric_limits<size_t>::min() ) {
        // drop() is not something one uses often, so sub-optimal performance
        // here should be fine...
        //std::erase_if(_cache, [&name](const auto & cacheItem {
        //                return std::get<0>(cacheItem.first) == name)
        //            }));
        for(auto it = _cache.begin(); it != _cache.end(); ) {
            if(std::get<0>(it->first) == name) {
                _cache.erase(it++);
            } else ++it;
        }
        Parent::erase(name);
    }

    /// Dumps current MD content as JSON dictionary
    void to_json(std::ostream & os) const {
        os << "{\"entries\":{";
        bool first = true;
        for(const auto & entry : *this) {
            if(first) first = false; else os << ",";
            os << "\"" << entry.first << "\":[" << entry.second.first
               << ",\"" << entry.second.second << "\"]";
        }
        os << "},\"aliases\":{";
        first = true;
        for(const auto & aliasEntry : _aliases) {
            if(first) first = false; else os << ",";
            os << "\"" << aliasEntry.first << "\":\""
                << aliasEntry.second << "\"";
        }
        os << "}}";
    }
};  // class MetaInfo

#if 0  // TODO?
/// Specialize getting meta-value of runs range with "default" one; the point
/// is to apply default values to the bounds that aren't set
template<typename ValidityKeyT> inline ValidityRange<ValidityKeyT>
MetaInfo::get< ValidityRange<ValidityKeyT> >(
                  const std::string & name
                , const ValidityRange<ValidityKeyT> & default_
                , size_t lineNo  //=std::numeric_limits<size_t>::max()
                ) const {
    const auto vs = this->operator[](name);
    std::map<size_t, std::string>::const_reverse_iterator
                it(vs.upper_bound(lineNo));
    if( it == vs.rend() ) {
        return default_;
    }
    auto rr = aux::lexical_cast<ValidityRange<ValidityKeyT> >(it->second);
    // If left or right limit of the range is left open, apply default
    // substitution
    if( ! ::sdc::is_set(rr.from) ) rr.from = default_.from;
    if( ! ::sdc::is_set(rr.to)   ) rr.to   = default_.to;
    return rr;
}
#endif

}  // namespace aux

template<typename T> T
ValidityTraits<T>::from_string(const std::string & stre) {
    return aux::lexical_cast<size_t>(stre);
}

//                                                                _____________
// _____________________________________________________________/ Main Classes

template<typename KeyT> class Documents;  // fwd

/**\brief Basic storage for document entries, supporting typical queries
 *
 * This collection enumerates documents with arbitrary user info by their type
 * and validity period, and provides interface for basic retrieval: `updates()`
 * and `latest()`.
 *
 * Must be parameterised with validity key type (e.g. run number or time+date)
 * and type of user info kept.
 *
 * \ingroup typed indexing
 * */
template< typename KeyT
        , typename AuxInfoT
        >
class ValidityIndex {
public:
    typedef KeyT Key;
    typedef AuxInfoT AuxInfo;
    /// Shortcut for validity range type in use
    typedef ValidityRange<KeyT> Range;
    /// A data type of the document kept
    struct DocumentEntry {
        /// Identifier to the document
        std::string docID;
        /// End of validity period for calibration; considered only if set
        KeyT validTo;
        /// Any other user data associated with this document entry
        AuxInfoT auxInfo;

        void to_json(std::ostream & os) const {
            os << "{"
               << "\"docID\":\"" << docID << "\","
                   << "\"validTo\":\"" << ValidityTraits<KeyT>::to_string(validTo) << "\","
                   << "\"auxInfo\":";
            auxInfo.to_json(os);
            os << "}";
        }
    };
    /// List of updates to be applied, returned by querying operations
    typedef std::list< std::pair<KeyT, const DocumentEntry *> > Updates;
protected:
    /// By-run index of the calibration files
    typedef std::multimap< KeyT, DocumentEntry
                         , typename ValidityTraits<KeyT>::Less
                         > DocsIndex;
    /// By-type dictionary of indexes
    std::unordered_map<std::string, DocsIndex> _types;
public:
    ///\brief Adds document entry of certain type with runs range
    ///
    /// This method memorizes settings of the calibration entry as it must be
    /// then scheduled for the update with CSV data collecting function.
    ///
    /// Foreseen usage scenario may imply pre-parsing the document in order to
    /// obtain calibration data type, runs range and any data for `auxInfo`
    /// instance associated with particular doc.
    typename DocsIndex::iterator
    add_entry( const std::string & docID
             , const std::string & dataType
             , KeyT from, KeyT to
             , const AuxInfoT & auxInfo
             ) {
        auto ir1 = _types.emplace(dataType, DocsIndex());
        auto ir2 = ir1.first->second.emplace( from
                        , DocumentEntry{docID, to, auxInfo} );
        return ir2;
    }

    /**\brief Returns list of "still valid" documents to be applied, in order
     *
     * This querying method returns documents list to be processed in order to
     * get incrementally-updated data (not just latest one as returned by
     * `latest()` getter). The returned entries are guaranteed to be valid
     * for certain validity key (e.g. run number).
     *
     * Foreseen usecases:
     *  - Incrementally-built data (e.g. sums)
     *  - Partially conditioned data; for instance, energy calibration is the
     *    subject for one document (with one validity timespan) and energy is
     *    for another, but both must be applied altogether, within a single
     *    output data structure.
     *
     * If `noTypeIsOk` is `true`, the method does not throw exceptions and just
     * returns an empty list if no calibration data type can be retrieved.
     * (yet, if type exists, but no data present no exception thrown anyway).
     *
     * \throws `sdc::errors::UnknownDataType` if not such data type defined.
     * */
    Updates updates( const std::string & typeName
                   , KeyT key
                   , bool noTypeIsOk=false ) const {
        // find by-types index
        auto typeIt = _types.find(typeName);
        if( _types.end() == typeIt ) {
            if( noTypeIsOk ) return Updates();  // empty list on no-type
            throw errors::UnknownDataType(typeName);
        }
        Updates us;
        // locate latest item for certain key
        auto upb = typeIt->second.upper_bound(key);
        // iterate till the item, omitting entries that are not currently
        // valid, put valid entries into updates list
        for( auto it = typeIt->second.begin()
           ; it != upb
           ; ++it ) {
            if( ValidityTraits<KeyT>::is_set(it->second.validTo)
             && ( typename ValidityTraits<KeyT>::Less()(it->second.validTo, key)
               || it->second.validTo == key ) ) {
                continue;  // stale
            }
            us.push_back(typename Updates::value_type( it->first
                                                     , &(it->second)));
        }
        return us;
    }

    /**\brief Finds updates between two keys
     *
     * Useful for incremental ad-hoc updates, when collected data must be
     * modified with respect to some previous state.
     *
     * \todo UT
     * */
    Updates updates( const std::string & typeName
                   , KeyT oldKey, KeyT newKey
                   , bool noTypeIsOk=false
                   , bool keepStale=false
                   ) const {
        // find by-types index
        auto typeIt = _types.find(typeName);
        if( _types.end() == typeIt ) {
            if( noTypeIsOk ) return Updates();  // empty list on no-type
            throw errors::UnknownDataType(typeName);
        }
        Updates us;

        auto oldIt = ValidityTraits<KeyT>::is_set(oldKey)
                   ? typeIt->second.upper_bound(oldKey)
                   : typeIt->second.begin()
                   ;
        auto newIt = ValidityTraits<KeyT>::is_set(newKey)
                   ? typeIt->second.upper_bound(newKey)
                   : typeIt->second.end()
                   ;
        for( auto it = oldIt; it != newIt; ++it ) {
            if( (!keepStale) && ValidityTraits<KeyT>::is_set(it->second.validTo)
             && ( typename ValidityTraits<KeyT>::Less()(it->second.validTo, newKey)
               || it->second.validTo == newKey ) ) {
                continue;  // stale
            }
            us.push_back(typename Updates::value_type( it->first
                                                     , &(it->second)));
        }
        return us;
    }

    /**\brief Returns latest document entry for certain run number and type
     *
     * In case of few valid entries found for certain run, the latest
     * inserted will be returned.
     *
     * \throws `sdc::errors::UnknownDataType` if not such data type defined and
     *         `noTypeIsOk` is false.
     * \throws `sdc::errors::NoData` if no valid data can be found for given key
     *
     * \todo Optimize me. Lookup loop seems to be suboptimal.
     */
    typename Updates::value_type
        latest( const std::string & typeName
              , KeyT key
              ) const {
        // find by-types index
        auto typeIt = _types.find(typeName);
        if( _types.end() == typeIt ) {
            throw errors::UnknownDataType(typeName);
        }
        const auto & index = typeIt->second;
        // iterate backwards until first document with fitting range is met or
        // first element is reached
        auto it = index.upper_bound(key);
        if( it == index.begin() )
            throw errors::NoCalibrationData(typeName, key);  // empty upper bound
        for(; ; --it) {
            if( index.end() == it ) continue;
            if( (!ValidityTraits<KeyT>::is_set(it->second.validTo))
             || typename ValidityTraits<KeyT>::Less()(key, it->second.validTo)
              ) {
                if( typename ValidityTraits<KeyT>::Less()(key, it->first) ) continue;
                return typename Updates::value_type( it->first
                                                   , &(it->second)
                                                   );
            }
            if( it == index.begin() ) throw errors::NoCalibrationData(typeName, key);
        }
    }

    /// Returns immutable index entries
    const std::unordered_map<std::string, DocsIndex> &
        entries() const {return _types;}

    friend class Documents<KeyT>;
};  // class ValidityIndex

/// Data traits defining the to-structure conversion procedure
template<typename T> struct CalibDataTraits;

/**\brief Representation of calibration data documents collection
 *
 * This stateful object maintains collecteion of loaders with validity index
 * of certain documents automating document lookup and loading.
 *
 * First template argument is the validity key in use (run ID, time, date, etc),
 * the second is an arbitrary auxiliary information for lookup within a
 * document -- line number, cached ID of database entry, etc.
 *
 * \note Has few public collections that must be set prior to usage
 * \ingroup indexing
 * */
template<typename KeyT>
class Documents {
public:
    /// Description of the data block found in the document
    struct DataBlock {
        /// Data type provided by block described
        std::string dataType;
        /// Validity range for described block
        ValidityRange<KeyT> validityRange;
        /// Line number of data block start (or other internal markup marker
        /// encoded)
        IntradocMarkup_t blockBgn;
    };

    /**\brief A document reader of certain format
     *
     * Validity range and type assignment are controlled by this class.
     *
     * Supported data sources (files formats, DB sessions, remote connections,
     * etc) implement this interface to provide data loading.
     *
     * This class allows runtime extension with different formats. By format
     * we imply here a grammar of the document (e.g. CSV, YAML, XML, etc), not
     * the particular data type (e.g., not the structure of certain data
     * type). Thus, this interface is not something ordinary user would like
     * to implement.
     * If no validity range or type is set for the data block, default are
     * used.
     */
    struct iLoader {
        /// A callback function type, performing reading of the string
        /// expression into calibration data type
        typedef std::function< bool ( const typename aux::MetaInfo &
                                    , size_t
                                    , const std::string & )
                             > ReaderCallback;
        ///\brief Externally set validity defaults for the loader
        struct Defaults {
            ///\brief Default type assumed for any data block
            ///
            /// Empty string must be considered as a requirement for all the
            /// data blocks must have the defined type.
            std::string dataType;
            ///\brief Default validity range assumed for any data block
            ///
            /// _Both_ undefined borders must be considered as a requirement
            /// for all the blocks to have defined validity range.
            ValidityRange<KeyT> validityRange;
            ///\brief Basic metadata content
            aux::MetaInfo baseMD;

            void to_json(std::ostream & os) const {
                os << "{\"dataType\":\"" << dataType
                   << "\",\"validityRange\":[\""
                   << ValidityTraits<KeyT>::to_string(validityRange.from) << "\",\""
                   << ValidityTraits<KeyT>::to_string(validityRange.to)
                   << "\"],\"baseMD\":";
                baseMD.to_json(os);
                os << "}";
            }
        } defaults;

        iLoader() : defaults {"", { KeyT(ValidityTraits<KeyT>::unset)
                                  , KeyT(ValidityTraits<KeyT>::unset)
                                  }, aux::MetaInfo()
            } {}


        ///\brief Shall return whether this instance is capable to process
        /// document with given ID
        ///
        /// This method may perform additional check for a document, like
        /// filename validation versus certain pattern, etc. May even open the
        /// file and check basic validity, even though a better way may be to
        /// customize initial lookup algorithm.
        virtual bool can_handle(const std::string & docID) const {return true;}

        /**\brief Retrieves the document structure
         *
         * Returned is the map identifying different data blocks for indexing.
         */
        virtual std::list<DataBlock> get_doc_struct( const std::string & docID ) = 0;

        /**\brief Retrieves the data
         *
         * Shall perform acquizition of the particular data for given type and
         * validity range.
         *
         * If default data type is empty and data block provides no data type,
         * `NoDataTypeDefined` exception is thrown.
         *
         * If both default to/from validity range bounds are not specified and
         * data block within a doc provides no validity range, `NoValidityRange`
         * exception is thrown.
         *
         * \param docID document identifier to read
         * \param k validity ID
         * \param forType blocks of this only type will be read
         * \param acceptFrom inter-document markup identifying section to read
         * \param cllb callable object to forward line parsing to
         */
        virtual void read_data( const std::string & docID
                              , KeyT k
                              , const std::string & forType
                              , IntradocMarkup_t acceptFrom
                              , ReaderCallback cllb
                              ) = 0;
    };

    /// Colllection of loaders, capable to obtain structures
    std::list< std::shared_ptr<iLoader> > loaders;
    /// Data block snapshot with cached loader handle to read it
    struct DocumentLoadingState {
        /// Loader settings at the current parser state
        ///
        /// This field keeps loader defaults at the pre-parsing state that can
        /// be different from (more common) loader's defaults. Frequent usage
        /// is to inject metadata based on filename.
        typename iLoader::Defaults docDefaults;
        /// Pointer to loader in use
        std::shared_ptr<iLoader> loader;
        /// Last block start marker
        IntradocMarkup_t dataBlockBgn;
        /// Prints document loading state as JSON
        void to_json(std::ostream & os) const {
            os << "{"
               << "\"defaults\":";
            docDefaults.to_json(os);
            os << "}";
        }
    };
    /// Index of documents with polymorphic aux info
    ValidityIndex<KeyT, DocumentLoadingState> validityIndex;

    typedef typename ValidityIndex<KeyT, DocumentLoadingState>::Updates::value_type Update;
public:
    // TODO: doc
    template<typename T> void
    load_update_into( const typename ValidityIndex< KeyT
                                                  , DocumentLoadingState
                                                  >::Updates::value_type upd
                    , typename CalibDataTraits<T>::template Collection<> & dest
                    , KeyT forKey
                    , aux::LoadLog * loadLogPtr=nullptr
                    ) const {
        // doc entry to read (has docID, valid-to, auxinfo which is of this
        // class' DocumentLoadingState -- defaults+loader )
        const typename ValidityIndex<KeyT, DocumentLoadingState>::DocumentEntry *
            docEntryPtr = upd.second;
        // particular loader ptr
        iLoader *
            loaderPtr = docEntryPtr->auxInfo.loader.get();
        // copy of loader's defaults to be restored
        const typename iLoader::Defaults dftsBck = loaderPtr->defaults;
        // set metadata to one saved on pre-parsing
        loaderPtr->defaults = docEntryPtr->auxInfo.docDefaults;
        // Here static and dynamic polymorphism join.
        // We use C++ lambda function to make runtime-polymorphic handler
        // to read the data into statically-derived data structure.
        try {
            loaderPtr->read_data( docEntryPtr->docID
                  , forKey
                  , CalibDataTraits<T>::typeName
                  , docEntryPtr->auxInfo.dataBlockBgn
                  , [&]( const typename aux::MetaInfo & meta
                       , size_t lineNo
                       , const std::string & expression ) {
                            if(loadLogPtr) loadLogPtr->set_source(docEntryPtr->docID, lineNo);
                            try {
                                CalibDataTraits<T>::collect( dest
                                        , CalibDataTraits<T>::parse_line(
                                                expression
                                              , lineNo
                                              , meta
                                              , docEntryPtr->docID
                                              , loadLogPtr
                                              )
                                        , meta
                                        , lineNo
                                        );
                            } catch( errors::RuntimeError & e ) {
                                throw errors::NestedError<errors::ParserError>( e
                                    , "while parsing or collecting data block"
                                    , expression
                                    , docEntryPtr->docID
                                    , meta.get<size_t>("@lineNo"
                                        , std::numeric_limits<size_t>::max()
                                        , std::numeric_limits<size_t>::max()
                                        ) );
                            }
                            if(loadLogPtr) loadLogPtr->set_source("(none)", 0);
                            return true;
                        }
                );
        } catch( errors::ParserError & e ) {
            // append info on faulty file, if needed
            if( e.docID.empty() ) e.docID = docEntryPtr->docID;
            loaderPtr->defaults = dftsBck;
            throw;
        } catch( errors::IOError & e ) {
            // append info on faulty file, if needed
            if( e.filename.empty() ) e.filename = docEntryPtr->docID;
            loaderPtr->defaults = dftsBck;
            throw;
        } catch(...) {
            loaderPtr->defaults = dftsBck;
            throw;
        }
        loaderPtr->defaults = dftsBck;
    }
public:
    /**\brief Add new entry to the validity index pre-parsing its meta
     *
     * If no loader is explicitly specified (`loader` is null) uses
     * `iLoader::can_handle()` to find out appropriate loader for the
     * document (iterates list, first suitable is taken). Then uses
     * `iLoader::get_doc_struct()` to obtain document reflection info
     * and adds new entry to the validity index.
     *
     * First bool flag in paried parameters is used to override the defaults at
     * the beginning of document parsing: when flag is set, default type or
     * default validity range will be set to provided ones. When flag is not
     * set, loader's default values will be used.
     *
     * Note that type, validity range and metadata will be saved in the index,
     * so if there were some, say, metainfo mix-ins, it'll be saved and
     * propagated whenever the document is retrieved.
     *
     * Returns `false` if no appropriate loader is found for the document
     * or no meaningful data can be read from the documents with current
     * loaders.
     */
    bool add( const std::string & docID
            , const std::pair<bool, std::string> & defaultType={false, ""}
            , const std::pair<bool, ValidityRange<KeyT>> & defaultValidity={false, { KeyT(ValidityTraits<KeyT>::unset)
                                                                                   , KeyT(ValidityTraits<KeyT>::unset)}}
            , const std::pair<bool, aux::MetaInfo> & mi={false, {}}
            , std::shared_ptr<iLoader> loader=nullptr
            ) {
        if(!loader) {
            for( auto h : loaders ) {
                if( !h->can_handle(docID) ) continue;
                loader = h;
                break;
            }
            if( !loader ) {
                return false;
                //throw errors::NoLoaderForDocument(docID);
            }
        }
        assert(loader);
        // save current loader defaults to restore them afterwards
        const auto prevDfts = loader->defaults;
        // if specified, override parameters with externally given ones
        if(defaultType.first)
            loader->defaults.dataType = defaultType.second;
        if(defaultValidity.first)
            loader->defaults.validityRange = defaultValidity.second;
        if(mi.first)
            loader->defaults.baseMD = mi.second;
        try {
            const auto docStruct = loader->get_doc_struct(docID);
            for( const auto & block : docStruct ) {
                // Get data type
                if( block.dataType.empty() ) {
                    throw errors::LoaderAPIError( loader.get(),
                            "`iLoader' implementation returned empty type for"
                            " data block (docID=" + docID + ")" );
                };
                ValidityRange<KeyT> runsRange { block.validityRange.from
                                              , block.validityRange.to
                                              };
                typedef ValidityTraits<KeyT> VT;
                if(!(VT::is_set(runsRange.from)    // loader generated no validity
                  || VT::is_set(runsRange.to)
                  ) ) {
                    throw errors::LoaderAPIError( loader.get(),
                            "`iLoader' implementation returned empty validity"
                            " range for data block (docID=" + docID + ")" );
                }
                // add the document to the index
                // NOTE: important to note, that defaults saved here
                //       corresponds to the state extracted
                validityIndex.add_entry( docID // document ID
                                       , block.dataType  // data type
                                       , block.validityRange.from
                                       , block.validityRange.to
                                       , DocumentLoadingState{loader->defaults, loader, block.blockBgn }
                                       );
            }
            loader->defaults = prevDfts;
            return !docStruct.empty();
        } catch( errors::ParserError & e ) {
            loader->defaults = prevDfts;
            if(e.docID.empty())
                e.docID = docID;
            throw;
        } catch( errors::IOError & e ) {
            loader->defaults = prevDfts;
            if(e.filename.empty())
                e.filename = docID;
            throw;
        } catch( ... ) {
            loader->defaults = prevDfts;
            throw;
        }
    }

    /**\brief Uses callable (generator) instance to add multiple documents
     *
     * This version explotis of simple string-only returning generator and
     * sequentially passes (using `add()`) through all the document IDs
     * returned untill generator returns empty string.
     *
     * \returns number of documents added to the index with given generator.
     * */
    size_t
    add_from( std::function<std::string ()> && callable ) {
        size_t nAdded = 0;
        std::string docID;
        while(!(docID = callable()).empty()) {
            if(this->add(docID)) ++nAdded;
        }
        return nAdded;
    }

    /**\brief Uses callable (generator) instance to add multiple documents with
     *      modified metadata
     *
     * This version explotis generator returning document ID (as string),
     * type and metadata. The generator will be considered as empty when
     * its callable instance will return an empty string.
     *
     * First bool flag in paried parameters is the same as in
     * `Documents::add()` -- it is used to override the defaults at
     * the beginning of document parsing: when flag is set, default type or
     * default validity range will be set to provided ones. When flag is not
     * set, loader's default values will be used.
     *
     * \returns number of documents added to the index with given generator.
     * */
    size_t
    add_from( std::function<std::string ( std::pair<bool, std::string> &
                                        , std::pair<bool, ValidityRange<KeyT>> &
                                        , std::pair<bool, aux::MetaInfo> &
                                        , std::shared_ptr<iLoader> & loader
                                        ) > && callable ) {
        size_t nAdded = 0;
        for(;;) {
            std::string docID;
            std::pair<bool, std::string> type = {false, ""};
            std::pair<bool, ValidityRange<KeyT>> vr
                    = {false, {ValidityTraits<KeyT>::unset, ValidityTraits<KeyT>::unset}};
            std::pair<bool, aux::MetaInfo> mi = {false, {}};
            std::shared_ptr<iLoader> loader = nullptr;

            docID = callable(type, vr, mi, loader);
            if(docID.empty()) break;
            if(this->add(docID, type, vr, mi, loader)) ++nAdded;
        }
        return nAdded;
    }

    ///\brief Loads calibration data entries, in "overlay mode"
    ///
    /// This methood queries indexes for "still valid" data of certain type
    /// (see `ValidityIndex::updates()`) and performs sequential loading of
    /// those entries.
    ///
    /// Useful for partially-defined data that must be updated incrementally.
    template<typename T> typename CalibDataTraits<T>::template Collection<>
    load( KeyT key, bool noTypeIsOk=false, aux::LoadLog * loadLogPtr=nullptr) const {
        typename CalibDataTraits<T>::template Collection<> dest;
        const auto updates = validityIndex.updates(
                CalibDataTraits<T>::typeName, key, noTypeIsOk );
        for( const auto & upd : updates ) {
            load_update_into<T>(upd, dest, key, loadLogPtr);
        }
        return dest;
    }

    ///\brief Loads "most recent" calibration data entry
    ///
    /// This methood queries indexes for "most recent" data of certain type
    /// (see `ValidityIndex::latest()`) and loads it.
    ///
    /// Useful for data that is expected to be fully defined in single
    /// document.
    template<typename T> typename CalibDataTraits<T>::template Collection<>
    get_latest(KeyT key, aux::LoadLog * loadLogPtr=nullptr) const {
        typename CalibDataTraits<T>::template Collection<> dest;
        load_update_into<T>( validityIndex.latest(CalibDataTraits<T>::typeName, key)
                            , dest
                            , key
                            , loadLogPtr );
        return dest;
    }

    /// Dumps content to a JSON object.
    ///
    /// Might be useful for third-party routines.
    void dump_to_json(std::ostream & os) const {
        os << "{\"indexObject\":\"" << this << "\","
           << "\"loaders\":[";
        // "loaders" section
        bool first = true;
        for( const auto & loaderP : loaders ) {
            if(!first) { os << ","; }
            else { first = false; }
            auto & l = *loaderP.get();
            // default type of the loader (if set)
            os << "{\"loaderObject\":\"" << loaderP.get() << "\","
               << "\"defaultType\":\"" << l.defaults.dataType << "\",";
            // default validity range of the loader (if set)
            os << "\"defaultValidity\":[";
            if( sdc::ValidityTraits<KeyT>::is_set(
                        l.defaults.validityRange.from) ) {
                os << "\""
                   << sdc::ValidityTraits<KeyT>::to_string(
                           l.defaults.validityRange.from)
                   << "\"";
            } else {
                os << "null";
            }
            os << ",";
            if( sdc::ValidityTraits<KeyT>::is_set(
                        l.defaults.validityRange.from) ) {
                os << "\""
                   << sdc::ValidityTraits<KeyT>::to_string(
                           l.defaults.validityRange.from)
                   << "\"";
            } else {
                os << "null";
            }
            os << "]";  // end of the loader's validity range
            os << "}";  // end of the loader
        }
        os << "],";  // end of the "loaders" section 
        os << "\"byType\":";  // "index" section
        if( validityIndex._types.empty() ) {
            os << "null";
        } else {
            os << "{";  // types
            bool firstType = true;
            for( const auto & typeEntry : validityIndex._types ) {
                if( !firstType ) os << ","; else firstType = false;
                os << "\"" << typeEntry.first << "\":[";
                bool firstValidityEntry = true;
                for( const auto & validityEntry : typeEntry.second ) {
                    if(!firstValidityEntry) os << ",";
                    else firstValidityEntry = false;
                    os << "{";  // validityEntry bgn
                    os << "\"docID\":\"" << validityEntry.second.docID << "\",";
                    //os << "\"aux\": \"" << (void*) validityEntry.second.auxInfo << ",";
                    os << "\"validity\":[";
                    if( ValidityTraits<KeyT>::is_set(validityEntry.first) ) {
                        os << "\"" << ValidityTraits<KeyT>::to_string(validityEntry.first) << "\"";
                    } else {
                        os << "null";
                    }
                    os << ",";
                    if( ValidityTraits<KeyT>::is_set(validityEntry.second.validTo) ) {
                        os << "\"" << ValidityTraits<KeyT>::to_string(
                                        validityEntry.second.validTo) << "\"";
                    } else {
                        os << "null";
                    }
                    os << "],";  // validity
                    os << "\"loader\":\"" << validityEntry.second.auxInfo.loader.get() << "\"";
                    os << "}";  // validityEntry end
                }
                os << "]";
            }
            os << "}";  // types
        }
        os << "}" << std::endl;  // end of the "documents" object
    }
};  // Documents

//                                                      _______________________
// ___________________________________________________/ SrcInfo Helper Wrapper

///\brief A thin wrapper template struct around certain calibration data
/// data type that provides information on the data location (source) as well
template<typename T>
struct SrcInfo {
    T data;
    size_t lineNo;
    std::string srcDocID;
};

/**\brief A traits for type containing also the filename that provided
 *        calibration entry
 *
 * A clarification that we have to explicitly prohibit same cells within
 * different files was added to the specification at the later stage. In order
 * to accomplish this requirement, we slightly mdified the type that index
 * returns by adding a filename for every entry considered. Post-processing
 * function that makes a resulting vector of entries will then assure that all
 * the cells of same name came from same file.
 *
 * \ingroup type-traits
 * */
template<typename T>
struct CalibDataTraits< SrcInfo<T> > {
    static constexpr auto typeName = CalibDataTraits<T>::typeName;
    /// A collection type of the parsed entries
    template<typename TT=SrcInfo<T>>
        using Collection = typename CalibDataTraits<T>::template Collection<TT>;
    //typedef std::list< SrcInfo<T> > Collection;
    /// An action performed to put newly parsed data into collection
    template<typename TT=SrcInfo<T>>
    static inline void collect( Collection<TT> & c
                              , const SrcInfo<T> & e
                              , const aux::MetaInfo & mi
                              , size_t ll
                              ) { 
        CalibDataTraits<T>::template collect<SrcInfo<T>>(c, e, mi, ll);
    }
    /// Forwards call to wrapped traits
    static SrcInfo<T>
    parse_line( const std::string & line
              , size_t lineNo
              , const aux::MetaInfo & m
              , const std::string & filename
              , aux::LoadLog * loadLogPtr=nullptr
              ) {
        return SrcInfo<T>{
                  CalibDataTraits<T>::parse_line(line, lineNo, m, filename)
                , lineNo
                , filename
                };
    }
};

//                                                                     ________
// __________________________________________________________________/ Loaders

/**\brief An extended CSV data file format stream-based loader
 *
 * This loader uses ASCII stream to load the data in structures. The grammar
 * is expected to be somewhat "extended CSV" (CSV is for comma separated values
 * format): the stream may contain multiple  CSV blocks with metadata lines in
 * between. Guidelines:
 *
 *   1. Empty lines and lines containing only spaces are ignored.
 *   2. It can contain a comment starting with special character (`#` by
 *      default). What is after the `#` symbol is ignored.
 *   3. Can contain a metadata definition in a form of key/value delimited
 *      with a special character (`=` by default). Value can be empty.
 *   4. Can contain a columnar data (called "CSV blocks") -- a line with
 *      space-delimited tokens (strings and numbers)
 *
 * This way each meaningful file is organized with in *sections* (one or more).
 * Each *section* starts with a block of metadata definitions that are
 * associated to the CSV content block that follows just after.
 *
 *     # A comment
 *     # This is where section 1 starts
 *     key1=value1  # metadata parameter `key1'
 *     key2=value2  # metadata parameter `key2'
 *     runs=2543-3012  # metadata parameter `runs'
 *     # Here the CSV block goes (comment ignored):
 *     foo          123  57          16     quz     # a line of CSV block
 *     bar          123  71.23e+12   57     blah
 *     # ...
 *     # Another section goes here
 *     type=OtherCalibDataType
 *     runs=0  # validity range can contain only the beginning
 *     GEM 1 1 23 blah,blah  # another CSV block starts here
 *     # ...
 *
 * Essentially, boundaries between CSV blocks are defined by validity metadata
 * definition (containing `type` or `runs` tags). Except for this, each CSV
 * block inherits values from above, meaning that `key1` and `key2` will be
 * defined for both block#1 and block#2.
 *
 * \ingroup utils
 * */
template<typename KeyT>
class ExtCSVLoader : public Documents<KeyT>::iLoader {
public:
    /// Alias for current validity traits
    typedef ValidityTraits<KeyT> VT;

    /// A grammar, available for public changes
    struct Grammar {
        char commentChar
           , metadataMarker;
        std::string metadataKeyTag
                  , metadataTypeTag
                  ;
    } grammar;

    /// Interface structure of reentrant state used to parse the CSV document
    struct iState {
        /// Returns position of comment's start/stop
        virtual std::pair<size_t, size_t> handle_comment( const std::string & line ) = 0;
        /// Shall try to treat the given line as metadata and return whether it
        /// is a line with metadata
        virtual uint32_t handle_metadata( const std::string & line, size_t lineNo ) = 0;
        /// Handles CSV line
        virtual bool handle_csv(const std::string & line, size_t lineNo) = 0;
        /// Handles CSV block start
        virtual void handle_csv_start(size_t lineNo) = 0;
    };

    /// Basic implementation of the comment locating function, compatible with
    /// `aux::getline()`; commonly used by `iState` subclasses
    static std::pair<size_t, size_t>
    locate_comment_char( char c, const std::string & line ) {
        if( '\0' == c ) return { std::string::npos, std::string::npos };
        return { line.find(c)
               , std::string::npos
               };
    }

    ///\brief Reentrant state used for pre-parsing
    ///
    /// Simplified state -- keeps track only on the validity key and type
    /// of current CSV block(s).
    struct PreparsingState : public iState {
        /// A reference to currently used "ExtCSV" grammar
        const Grammar & g;
        /// Validity of current CSV block
        ValidityRange<KeyT> validity;
        /// Data type of current CSV block
        std::string type;
        /// Resulting document structure
        std::list<typename Documents<KeyT>::DataBlock> r;

        PreparsingState( const Grammar & g_
                       , const ValidityRange<KeyT> & validity_
                       , const std::string & type_
                       ) : g(g_)
                         , validity(validity_)
                         , type(type_)
                         {}

        /// Treats basic single-char comment syntax
        std::pair<size_t, size_t> handle_comment( const std::string & line ) override {
            return locate_comment_char(g.commentChar, line);
        }
        /// We currently look only for validity key and data type metadata,
        /// if provided by current grammar settings or defaults
        uint32_t handle_metadata( const std::string & line, size_t lineNo ) override {
            uint32_t rCode = 0x0;
            if( '\0' == g.metadataMarker ) return rCode;
            auto eqP = line.find( g.metadataMarker );
            if( eqP == std::string::npos ) return rCode;
            const std::string key = aux::trim(line.substr(0, eqP));
            
            rCode |= 0x1;
            if( (!g.metadataKeyTag.empty())
             && key == g.metadataKeyTag ) {
                validity
                    = aux::LexicalTraits< ValidityRange<KeyT> >
                         ::from_string( line.substr(eqP + 1));
                rCode |= 0x2;
            }
            if( (!g.metadataTypeTag.empty())
                    && key == g.metadataTypeTag ) {
                type = aux::trim(line.substr(eqP + 1));
                rCode |= 0x2;
            }
            return rCode;
        }
        /// Does nothing
        bool handle_csv(const std::string &, size_t) override { return true; }
        /// Appends CSV block start marking
        void handle_csv_start(size_t lineNo) override {
            // TODO: handle defaults
            // Assure the data type / validity range are set (or
            // take defaults)
            typename Documents<KeyT>::DataBlock db {type, validity, lineNo};
            if( db.dataType.empty() ) {
                db.dataType = type;
            }
            if( db.dataType.empty() ) {
                throw errors::NoDataTypeDefined( g.metadataTypeTag
                                               , lineNo );
            }
            if(!( ValidityTraits<KeyT>::is_set(db.validityRange.from)
               || ValidityTraits<KeyT>::is_set(db.validityRange.to)
               ) ) {
                db.validityRange = validity;
            }
            if(!( ValidityTraits<KeyT>::is_set(db.validityRange.from)
               || ValidityTraits<KeyT>::is_set(db.validityRange.to)
               ) ) {
                throw errors::NoValidityRange( g.metadataTypeTag
                                             , lineNo );
            }
            r.push_back(db);
        }
    };  // PreparsingState

    /// Reentrant state used for pre-parsing, default simple grammar handling
    struct ParsingState : public iState {
        /// Currently used "ExtCSV" grammar
        const Grammar & g;
        /// A validity range for current CSV block
        ValidityRange<KeyT> cVal;
        /// A data type of current CSV block
        std::string cType;
        /// Data type of interest
        const std::string & forType;
        /// Validity key of interest
        const KeyT forKey;
        /// CSV data parsing callback
        typename Documents<KeyT>::iLoader::ReaderCallback cllb;
        /// Current metadata storage
        aux::MetaInfo md;

        ParsingState( Grammar & g_
                    , const ValidityRange<KeyT> & cVal_
                    , const std::string & cType_
                    , const std::string & forType_
                    , const KeyT forKey_
                    , typename Documents<KeyT>::iLoader::ReaderCallback cllb_
                    , const aux::MetaInfo baseMD
                    ) : g(g_)
                      , cVal(cVal_)
                      , cType(cType_)
                      , forType(forType_)
                      , forKey(forKey_)
                      , cllb(cllb_)
                      , md(baseMD)
                      {}

        /// Treats basic single-char comment syntax
        std::pair<size_t, size_t> handle_comment( const std::string & line ) override {
            return locate_comment_char(g.commentChar, line);
        }

        /// Full support for the metadata
        uint32_t handle_metadata( const std::string & line, size_t lineNo ) override {
            uint32_t r = 0x0;
            if( '\0' == g.metadataMarker ) return r;
            auto eqP = line.find( g.metadataMarker );
            if( eqP == std::string::npos ) return r;
            const std::string key = aux::trim( line.substr(0, eqP) )
                            , val = aux::trim( line.substr(eqP + 1) )
                            ;
            md.set( key, val, lineNo );
            r |= 0x1;
            if( (!g.metadataKeyTag.empty())
             && g.metadataKeyTag == key ) {
                cVal = aux::LexicalTraits< ValidityRange<KeyT> >
                        ::from_string(val);
                r |= 0x2;
            }
            if( (!g.metadataTypeTag.empty())
             && g.metadataTypeTag == key ) {
                cType = val;
                r |= 0x2;
            }
            return r;
        }
        /// Forwards execution to data parsing callable for the range that must
        /// be read
        bool handle_csv(const std::string & line, size_t lineNo) override {
            assert((bool) cVal);
            assert(!cType.empty());
            if( cType != forType ) return true;  // other type
            if( ValidityTraits<KeyT>::is_set(cVal.from)
              && forKey < cVal.from ) return true;  // not valid yet
            if( ValidityTraits<KeyT>::is_set(cVal.to)
              && (cVal.to < forKey || cVal.to == forKey ) ) return true;  // not valid already
            char bf[32];
            snprintf(bf, sizeof(bf), "%zu", lineNo);
            md.set("@lineNo", bf);
            assert(md.get<size_t>("@lineNo") == lineNo);
            bool ret = cllb(md, lineNo, line);
            assert(md.get<size_t>("@lineNo") == lineNo);
            md.drop("@lineNo");
            return ret;
        }
        /// Does nothing
        void handle_csv_start(size_t lineNo) override {}
    };  // struct ParsingState
protected:
    /// Aux function iterating over CSV/SDC lines in stream
    size_t _parse_stream( std::istream & inputStream
                        , iState & state
                        , IntradocMarkup_t acceptCSVFromLine
                        , bool onlyThisBlock=false
                        ) {
        // This is the most important method of (pre-)parsing the documents;
        // it steers the logic of indexing CSV blocks wrt document structure.
        std::string line;
        size_t lineCount = 0;
        bool indexNextCSVLine = true;
        bool thisBlockPassed = false;
        // read next line:
        while( aux::getline( inputStream, line, lineCount
                    , [&](const std::string & l){return state.handle_comment(l);}
                    ) ) {
            // by default we assume new line being read to be metadata
            // expression, `handle_metadata()` accepts line and should test it
            // against "is metadata" condition. If the result of metadata
            // treatment suceed (0x1 flag set), further line treatment is
            // blocked. Additionally, if processed metadata is of runs range
            // type (0x2 flag is set in return code), we should break the
            // document and make next (first) CSV line to be indexed.
            uint32_t mdFlags = state.handle_metadata(line, lineCount);
            if( mdFlags ) {
                if(mdFlags & 0x2) indexNextCSVLine = true;
                continue;
            }
            if(lineCount < acceptCSVFromLine) continue;  // omit irrelevant CSV
            if(indexNextCSVLine && onlyThisBlock) {
                if(thisBlockPassed) return lineCount;
                thisBlockPassed = true;
            }
            if( ! state.handle_csv(line, lineCount) ) {
                continue;  // not a CSV
            }
            if( indexNextCSVLine ) {
                // One gets here only if current line is CSV and
                // `indexNextCSVLine' is set -- memorize the new CSV block
                // start
                state.handle_csv_start(lineCount);
                indexNextCSVLine = false;
            }
        }
        return lineCount;
    }
public:  // iLoader interface implementation
    /// Initializes default grammar
    ExtCSVLoader() : grammar{ '#', '=', "runs", "type" } {}

    /**\brief Preliminary parses of SDC file retrieving only basic info
     *
     * Looks up for CSV blocks with validity range and data type defined in
     * metadata blocks. Returns basic document structure for further usage.
     *
     * Permits for blocks having no data type and/or validity range.
     * */
    std::list<typename Documents<KeyT>::DataBlock> 
            get_doc_struct( std::istream & ifs ) {
        PreparsingState state( grammar
                             , this->defaults.validityRange
                             , this->defaults.dataType
                             );
        _parse_stream( ifs, state, 0 );
        return state.r;
    }

    /**\brief Opens file and forwards parsing to stream version.
     *
     * Opens the file (currently, only supports local files), and retrieves
     * only the document structure to add the principal metadata in index
     * (data type and the validity range).
     *
     * \todo Support for remote location, user-defined access, etc.
     */
    std::list<typename Documents<KeyT>::DataBlock>
                get_doc_struct( const std::string & docID) override {
        // open file (closed by its destructor at exit)
        std::ifstream ifs(docID, std::ios::in);
        if(!ifs.good()) {
            throw errors::IOError(docID, "could not create input stream");
        }
        this->defaults.baseMD.set( "@docID"
                                 , docID
                                 , std::numeric_limits<size_t>::min()
                                 );
        auto r = get_doc_struct(ifs);
        this->defaults.baseMD.drop( "@docID"
                                  , std::numeric_limits<size_t>::min()
                                  );
        return r;
    }
    
    /** Stream version of data block reading
     *
     * Iteratively reads what is supposed to be a CSV block(s) for certain runs
     * range, calling the provided callable object.
     */
    void read_data( std::istream & ifs
                  , KeyT k
                  , const std::string & forType
                  , IntradocMarkup_t acceptCSVFromLine
                  , typename Documents<KeyT>::iLoader::ReaderCallback cllb
                  ) {
        ParsingState state( grammar
                          , this->defaults.validityRange
                          , this->defaults.dataType
                          , forType
                          , k
                          , cllb
                          , this->defaults.baseMD
                          );
        _parse_stream( ifs, state, acceptCSVFromLine, ENABLE_SDC_FIX001 );
    }

    /** Opens file and forwards parsing to stream version.
     *
     * Opens the file (currently, only supports local files), and forwards
     * invokation to stream version of `read_data()`.
     *
     * \todo Support for remote location.
     */
    void read_data( const std::string & docID
                  , KeyT k
                  , const std::string & forType
                  , IntradocMarkup_t acceptCSVFromLine
                  , typename Documents<KeyT>::iLoader::ReaderCallback cllb
                  ) override {
        // open file (closed by its destructor at exit)
        std::ifstream ifs(docID);
        this->defaults.baseMD.set( "@docID"
                                 , docID
                                 , std::numeric_limits<size_t>::min()
                                 );
        read_data( ifs, k, forType, acceptCSVFromLine, cllb );
        this->defaults.baseMD.drop( "@docID"
                                  , std::numeric_limits<size_t>::min()
                                  );
    }
};  // class ExtCSVLoader

//                                                                      _______
// ___________________________________________________________________/ Utils

/**\brief Example loading items of certain type for certain validity key
 *
 * This function is handy for certain (ad hoc) cases, albeit it must not be
 * used for large applications as it does not benefit from reentrant indeces
 * and persistent data structures.
 *
 * \ingroup utils
 * */
template<typename KeyT, typename DataTypeT>
typename CalibDataTraits<DataTypeT>::template Collection<DataTypeT>
load_from_fs( const std::string & rootpath
            , KeyT k
            , const std::string & acceptPatterns="*.txt:*.dat"
            , const std::string & rejectPatterns="*.swp:*.swo:*.bak:*.BAK:*.bck:~*:*-orig.txt:*.dev"
            , size_t upSizeLimitBytes=1024*1024*1024
            , std::ostream * logStreamPtr=nullptr
            ) {
    // 1. SETUP
    // create calibration document index by run number
    sdc::Documents<KeyT> docs;
    // Create a loader object and add it to the index for automated binding.
    // This type of loader (ExtCSVLoader) is pretty generic one and implies
    // a kind of "extended" grammar for CSV-like files.
    auto extCSVLoader = std::make_shared< sdc::ExtCSVLoader<KeyT> >();
    docs.loaders.push_back(extCSVLoader);
    // ^^^ one can customize this loader's grammar by modifying its public
    //     `grammar` attribute or its defaults. For instance, we assume all the
    //     discovered files to have `CaloCellCalib` data type by default:
    extCSVLoader->defaults.dataType = CalibDataTraits<DataTypeT>::typeName;

    // create filesystem iterator to walk through all the dirs and their
    // subdirs looking for files matching certain wildcards and size criteria
    sdc::aux::FS fs( rootpath
                   , acceptPatterns // (opt) accept patterns
                   , rejectPatterns // (opt) reject patterns
                   , 10  // (opt) min file size, bytes
                   , upSizeLimitBytes  // (opt) max file size, bytes
                   );
    if(logStreamPtr) {
        fs.set_logstream(logStreamPtr);
    }
    // use this iterator to fill documents index by recursively traversing FS
    // subtree and pre-parsing all matching files
    size_t nDocsOnIndex = docs.add_from(fs);
    if(logStreamPtr) {
        *logStreamPtr << "Indexed " << nDocsOnIndex << " document(s) at "
            << rootpath
            << " (accept=\"" << acceptPatterns << "\", reject=\""
            << rejectPatterns << ", size=(10-" << upSizeLimitBytes << ")."
            << std::endl;
    }

    // 2. LOAD DATA FOR CERTAIN RUN ID
    // We now load the data into collection.
    // Usually, when no origin info is required for the data being fetched,
    // one can simply call `docs.load<CustomDataType>(runNo)` and that's it.
    // However, for `CaloCellCalib` it is requested to apply additional check
    // on the origin, so we use a templated wrapper `SrcInfo<T>` here to
    // gain some info on the source document for every entry.
    return docs.template load< DataTypeT >(k);
}

/**\brief Prints loading log as JSON data (for debugging)
 *
 * JSON object writted to stdout contains:
 *  * "index" -- an information on created index
 *  * "updates" -- descriptive list of updates queued for loading
 *  * "loadLog" -- list of loaded items, in order
 *
 * Note, that produced JSON object is large and may not be suitable for general
 * purpose. Main designation of this function is to inspect complex cases on
 * minified subset of input data.
 *
 * For usage example see `inspec_sdc.py` script distributed with SDC sources.
 * */
template<typename CalibDataT, typename KeyT> int
json_loading_log( KeyT key
                , sdc::Documents<KeyT> & docs
                , std::ostream & os
                ) {
    os << "{\"index\":";
    docs.dump_to_json(os);
    sdc::aux::LoadLog loadLog;
    os << ",\"updates\":";
    auto updates = docs.validityIndex.updates(sdc::CalibDataTraits<CalibDataT>::typeName, key, false);
    bool isFirst = true;
    os << "[";
    typename sdc::CalibDataTraits<CalibDataT>::template Collection<> dest;
    for(const auto & updEntry : updates) {
        if(!isFirst) os << ","; else isFirst = false;
        os << "{\"key\":\"" << sdc::ValidityTraits<KeyT>::to_string(updEntry.first) << "\",\"update\":";
        updEntry.second->to_json(os);
        os << "}";
        docs.template load_update_into<CalibDataT>(updEntry, dest, key, &loadLog);
    }
    os << "],\"loadLog\":";
    loadLog.to_json(os);
    os << "}";
    return 0;
}


}  // namespace sdc

// cleanup macros defined by this header
#undef SDC_INLINE
#undef SDC_ENDDECL
