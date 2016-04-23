#include <string>

/*
* this file is from: https://github.com/chafey/GZipCodec
* thanks for chafey's great work
*/

#define GZIP_NAMESPACE_BEGIN    namespace gzipcodec {  
#define GZIP_NAMESPACE_END      }

GZIP_NAMESPACE_BEGIN


#include <zlib.h>

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384
#define windowBits 15
#define GZIP_ENCODING 16

bool compress(const char *data, int len, std::string& compressedData, int level = -1)
{
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree  = Z_NULL;
    strm.opaque = Z_NULL;
    if (deflateInit2(&strm, level, Z_DEFLATED, windowBits | GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY) != Z_OK)
    {
        return false;
    }

    unsigned char out[CHUNK];
    strm.next_in = (unsigned char*)data;
    strm.avail_in = len;
    do {
        int have;
        strm.avail_out = CHUNK;
        strm.next_out = out;
        if (deflate(&strm, Z_FINISH) == Z_STREAM_ERROR) {
            return false;
        }
        have = CHUNK - strm.avail_out;
        compressedData.append((char*)out, have);
    } while (strm.avail_out == 0);

    return deflateEnd(&strm) == Z_OK;
}

bool uncompress(const char* compressedData, int len, std::string& data)
{
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree  = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in  = Z_NULL;
    if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {
        return false;
    }
    
    int ret;
    unsigned have;
    unsigned char out[CHUNK];

    strm.avail_in = len;
    strm.next_in  = (unsigned char*)compressedData;
    do {
        strm.avail_out = CHUNK;
        strm.next_out  = out;
        ret = inflate(&strm, Z_NO_FLUSH);
        switch (ret) {
        case Z_NEED_DICT:
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            inflateEnd(&strm);
            return false;
        }
        have = CHUNK - strm.avail_out;
        data.append((char*)out, have);
    } while (strm.avail_out == 0);

    return inflateEnd(&strm) == Z_OK;
}

inline bool compress(const std::string& data, std::string& compressedData, int level = -1)
{
    return compress(data.data(), data.size(), compressedData, level);
}

inline bool uncompress(const std::string& compressedData, std::string& data) 
{
    return uncompress(compressedData.data(), compressedData.size(), data);
}

GZIP_NAMESPACE_END
