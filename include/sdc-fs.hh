#pragma once

#include "sdc-config.h"

#include <vector>
#include <string>

namespace sdc {
namespace utils {

#if defined(OPENSSL_FOUND) && OPENSSL_FOUND
///\brief Computes MD5 checksum of given bytes array
///
/// Relies on OpenSSL routines internally.
void compute_md5_openssl(const unsigned char* data, size_t len, char out_hash[128]);
#endif

/**\brief Document properties struct
 *
 * Does not assume how document is accessed at this level.
 * */
struct DocumentProperties {
    /// Modification timestamp
    long int mod_time;
    /// File size, bytes
    unsigned long size;
    /// MD5 digest
    char hashsum[128];
    /// Compressed content
    std::vector<char> content;

    // Utils
    //

    /// File compression algorithm in use (first byte of `content`)
    enum FileCompression {
        kRaw    = 0,  ///< raw (uncompressed) data comparison
        kSnappy = 1,  ///< snappy, requires SNAPPY_FOUND
        kZLib   = 2   ///< zlib, requires ZLIB_FOUND
    };

    ///\brief Used to encode document properties being obtained/compared or
    ///       changes found
    ///
    /// During invocation of `check_doc_...()` functions user may want to set
    /// individual flags -- for customizable shallow comparison, in order to
    /// avoid loading/fetching of the whole document. For full-fledged
    /// comparison, use `kCompareAll` which enables full comparison.
    ///
    /// These codes are also used to denote found changes. Only `kSize`,
    /// `kMTime`, `kMD5Sum`, `kContent` and `kRecompressed` can be set
    /// indicating corresponding changes.
    ///
    /// \note `kRecompressed` does not really correspond to actual document
    ///       content change and used only in the result code returned
    ///       by `check()`, to denote that content has to be (re)compressed to
    ///       perform the comparsion.
    enum Flags {
        /// Only input flag -- enables comparison
        kDoCompare    = 0x1,
        /// File size property code
        kSize         = 0x2,
        /// Modification timestamp property code
        kMTime        = 0x4,
        /// MD5 checksum property code
        kMD5Sum       = 0x8,
        /// File content property code
        kContent      = 0x10,

        /// Only output flag -- compression algoritms differ
        kRecompressed = 0x20,

        /// Input shortcut to compare everything
        kCompareAll = kDoCompare | kSize | kMTime | kMD5Sum | kContent
    };

    /// Compresses data (annotated with codec in 1st byte)
    static std::vector<char> compress_content(const std::vector<char> & raw
            , DocumentProperties::FileCompression codec);
    /// De-compresses content (respecting the codec provided in data)
    static std::vector<char> decompress_content(const std::vector<char> & content);
};  // struct DocumentProperties

///\brief Get or compare file properties
///
/// Inspects a file to populate or compare attributes of \p entry structure,
/// including size, modification time, MD5 hash, and optionally compressed
/// content.
///
/// In "get values" mode (`kDoCompare` bit is unset in \p `flags`), it
/// fills the structure with properties denoted by `flags`, using the
/// specified compression \p codec.
///
/// In "detect changes" mode (`kDoCompare` is set), it compares the file
/// against values in \p entry to report differences via returned bitmask
/// and refill the \p entry struct with values obtained according
/// to `flags`.
///
/// \note If content differs only by compression codec, raw data is
///       compared and optionally recompressed to match the preferred
///       codec, which is indicated by a special return flag.
int
check_doc_local_file( const std::string & filePath
    , DocumentProperties & entry
    , int flags
    , DocumentProperties::FileCompression codec=DocumentProperties::kRaw
    );

}  // namespace ::sdc::utils
}  // namespace sdc

