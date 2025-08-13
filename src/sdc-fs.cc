#include "sdc-fs.hh"

#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include <sys/stat.h>

#if defined(OPENSSL_FOUND) && OPENSSL_FOUND
#   include <openssl/opensslv.h>
#   include <openssl/md5.h>
#   include <openssl/evp.h>
#endif

#if defined(SNAPPY_FOUND) && SNAPPY_FOUND
#   include <snappy.h>
#endif

#if defined(ZLIB_FOUND) && ZLIB_FOUND
#   include <zlib.h>
#endif


namespace sdc {
namespace utils {

static DocumentProperties::FileCompression
get_compression_codec(const std::vector<char>& content) {
    if (content.empty()) throw std::runtime_error("Missing compression prefix byte");
    auto code = static_cast<unsigned char>(content[0]);
    if (code > DocumentProperties::kZLib)
        throw std::runtime_error("Unknown compression codec in data");
    return static_cast<DocumentProperties::FileCompression>(code);
}

std::vector<char>
DocumentProperties::decompress_content(const std::vector<char>& content) {
    DocumentProperties::FileCompression codec = get_compression_codec(content);
    const char* data = content.data() + 1;
    size_t size = content.size() - 1;

    switch (codec) {
    case DocumentProperties::kRaw:
        return std::vector<char>(data, data + size);

    case DocumentProperties::kSnappy: {
        #if defined(SNAPPY_FOUND) && SNAPPY_FOUND
        size_t uncompressed_len;
        if (!snappy::GetUncompressedLength(data, size, &uncompressed_len))
            throw std::runtime_error("Failed to get Snappy uncompressed size");
        std::vector<char> output(uncompressed_len);
        if (!snappy::RawUncompress(data, size, output.data()))
            throw std::runtime_error("Snappy decompression failed");
        return output;
        #else
        throw std::runtime_error("Snappy support not compiled in");
        #endif
    }

    case DocumentProperties::kZLib: {
        #if defined(ZLIB_FOUND) && ZLIB_FOUND
        // Guess output size and retry if needed
        uLongf outSize = size * 3;  // heuristic guess
        std::vector<char> output(outSize);
        int rc = uncompress(reinterpret_cast<Bytef*>(output.data()), &outSize,
                            reinterpret_cast<const Bytef*>(data), size);
        if (rc != Z_OK)
            throw std::runtime_error("ZLib decompression failed");
        output.resize(outSize);
        return output;
        #else
        throw std::runtime_error("ZLib support not compiled in");
        #endif
    }

    default:
        throw std::runtime_error("Unknown codec during decompression");
    }
}

std::vector<char>
DocumentProperties::compress_content(const std::vector<char>& raw
        , DocumentProperties::FileCompression codec) {
    std::vector<char> result;
    result.push_back(static_cast<char>(codec));

    switch (codec) {
    case DocumentProperties::kRaw:
        result.insert(result.end(), raw.begin(), raw.end());
        break;

    case DocumentProperties::kSnappy: {
        #if defined(SNAPPY_FOUND) && SNAPPY_FOUND
        std::string compressed;
        snappy::Compress(raw.data(), raw.size(), &compressed);
        result.insert(result.end(), compressed.begin(), compressed.end());
        break;
        #else
        throw std::runtime_error("Snappy compression requested but not available");
        #endif
    }

    case DocumentProperties::kZLib: {
        #if defined(ZLIB_FOUND) && ZLIB_FOUND
        uLongf bound = compressBound(raw.size());
        std::vector<char> zlibBuf(bound);
        if (compress(reinterpret_cast<Bytef*>(zlibBuf.data()), &bound,
                     reinterpret_cast<const Bytef*>(raw.data()), raw.size()) != Z_OK)
            throw std::runtime_error("ZLib compression failed");
        result.insert(result.end(), zlibBuf.begin(), zlibBuf.begin() + bound);
        break;
        #else
        throw std::runtime_error("ZLib compression requested but not available");
        #endif
    }

    default:
        throw std::runtime_error("Unknown codec requested during compression");
    }

    return result;
}

void
compute_md5_openssl(const unsigned char* data, size_t len, char out_hash[128]) {
    #if OPENSSL_VERSION_MAJOR >= 3
    // OpenSSL 3.0+ (provider-based EVP API)
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("Failed to allocate EVP_MD_CTX");

    if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data, len) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_MD_CTX: MD5 digest init/update failed");
    }

    unsigned char md[MD5_DIGEST_LENGTH];
    unsigned int md_len = 0;

    if (EVP_DigestFinal_ex(ctx, md, &md_len) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP_MD_CTX: MD5 digest finalization failed");
    }

    EVP_MD_CTX_free(ctx);
    #else
    // OpenSSL 1.x: legacy MD5 API
    unsigned char md[MD5_DIGEST_LENGTH];
    MD5(data, len, md);
    #endif

    // Format as hex string
    std::snprintf(out_hash, 128,
                  "%02x%02x%02x%02x%02x%02x%02x%02x"
                  "%02x%02x%02x%02x%02x%02x%02x%02x",
                  md[0], md[1], md[2], md[3],
                  md[4], md[5], md[6], md[7],
                  md[8], md[9], md[10], md[11],
                  md[12], md[13], md[14], md[15]);
}

int
check_doc_local_file(
          const std::string & filePath
        , DocumentProperties & entry
        , int flags
        , DocumentProperties::FileCompression codec
        ) {
    DocumentProperties current;
    struct stat st;
    if (stat(filePath.c_str(), &st) != 0)
        throw std::runtime_error("Failed to stat file: " + filePath);

    std::ifstream file(filePath, std::ios::binary);
    if (!file)
        throw std::runtime_error("Failed to open file: " + filePath);

    // Collect size and mod time
    if (flags & DocumentProperties::kSize)  current.size = static_cast<unsigned long>(st.st_size);
    if (flags & DocumentProperties::kMTime) current.mod_time = static_cast<long int>(st.st_mtime);

    std::vector<char> rawData;
    if ((flags & DocumentProperties::kMD5Sum) || (flags & DocumentProperties::kContent)) {
        rawData.resize(st.st_size);
        file.read(rawData.data(), st.st_size);
        if (!file)
            throw std::runtime_error("Failed to read file: " + filePath);
    }

    // Compute MD5
    if (flags & DocumentProperties::kMD5Sum) {
        compute_md5_openssl( reinterpret_cast<const unsigned char*>(rawData.data())
                , rawData.size()
                , current.hashsum
                );
    }

    // Compress
    if (flags & DocumentProperties::kContent) {
        std::vector<char> compressed;
        compressed.push_back(static_cast<char>(codec));

        switch (codec) {
        case DocumentProperties::kSnappy:
            #if defined(SNAPPY_FOUND) && SNAPPY_FOUND
            {
                std::string snappyOut;
                snappy::Compress(rawData.data(), rawData.size(), &snappyOut);
                compressed.insert(compressed.end(), snappyOut.begin(), snappyOut.end());
                break;
            }
            #else
            throw std::runtime_error("Snappy compression requested but not available");
            #endif
        case DocumentProperties::kZLib:
            #if defined(ZLIB_FOUND) && ZLIB_FOUND
            {
                uLongf bound = compressBound(rawData.size());
                std::vector<char> zlibBuf(bound);
                if (compress(reinterpret_cast<Bytef*>(zlibBuf.data()), &bound,
                             reinterpret_cast<const Bytef*>(rawData.data()), rawData.size()) != Z_OK)
                    throw std::runtime_error("Zlib compression failed");
                compressed.insert(compressed.end(), zlibBuf.begin(), zlibBuf.begin() + bound);
                break;
            }
            #else
            throw std::runtime_error("ZLib compression requested but not available");
            #endif
        case DocumentProperties::kRaw:
            compressed.insert(compressed.end(), rawData.begin(), rawData.end());
            break;
        default:
            throw std::runtime_error("Unknown compression codec requested");
        }

        current.content = std::move(compressed);
    }

    // If not in compare mode, just populate the entry and return
    if ((flags & DocumentProperties::kDoCompare) == 0) {
        if (flags & DocumentProperties::kSize)
            entry.size = current.size;
        if (flags & DocumentProperties::kMTime)
            entry.mod_time = current.mod_time;
        if (flags & DocumentProperties::kMD5Sum)
            std::memcpy(entry.hashsum, current.hashsum, sizeof(entry.hashsum));
        if (flags & DocumentProperties::kContent) entry.content = std::move(current.content);
        return 0;
    }

    // Compare mode: detect changes
    int changeMask = 0;

    if( (flags & DocumentProperties::kSize) && (entry.size != current.size))
        changeMask |= DocumentProperties::kSize;

    if( (flags & DocumentProperties::kMTime) && (entry.mod_time != current.mod_time))
        changeMask |= DocumentProperties::kMTime;

    if( (flags & DocumentProperties::kMD5Sum)
        && std::strncmp(entry.hashsum, current.hashsum, sizeof(entry.hashsum)) != 0)
        changeMask |= DocumentProperties::kMD5Sum;

    if (flags & DocumentProperties::kContent) {
        if (entry.content.empty())
            throw std::runtime_error("Empty content in entry (no compression marker)");

        DocumentProperties::FileCompression entryCodec = get_compression_codec(entry.content);
        DocumentProperties::FileCompression currentCodec = codec;

        // If codecs match, compare compressed data directly
        if (entryCodec == currentCodec) {
            if (entry.content != current.content)
                changeMask |= DocumentProperties::kContent;
        } else {
            // Decompress both, compare raw, and optionally recompress
            std::vector<char> entryRaw = DocumentProperties::decompress_content(entry.content);
            std::vector<char> currentRaw = DocumentProperties::decompress_content(current.content);

            if (entryRaw != currentRaw) {
                changeMask |= DocumentProperties::kContent;
            } else {
                // Same raw, but codec mismatch — recompress with preferred
                entry.content = DocumentProperties::compress_content(entryRaw, currentCodec);
                // mark recompression change
                changeMask |= DocumentProperties::kRecompressed;
            }
        }

        if (changeMask & DocumentProperties::kContent)
            entry.content = std::move(current.content);
    }    

    // Update changed fields
    if(changeMask & DocumentProperties::kSize)
        entry.size = current.size;
    if(changeMask & DocumentProperties::kMTime)
        entry.mod_time = current.mod_time;
    if(changeMask & DocumentProperties::kMD5Sum)
        std::memcpy(entry.hashsum, current.hashsum, sizeof(entry.hashsum));

    return changeMask;
}

}  // namespace ::sdc::utils
}  // namespace sdc

