/* uncompr.c -- zlib 1.0.4's one-call decompression (uncompr.c), as the ROM
 * has it.
 *
 * zlib 1.0.4, Copyright (C) 1995-1996 Jean-loup Gailly.  For conditions of
 * distribution and use, see the notice in tgr/zinfl.h.  Altered from the
 * original: K&R header written as a prototype, the function under the
 * game's name.
 */
#include "tgr/zinfl.h"

/* -- declarations -- */
int inflateInit_(z_streamp z, const char *version, int stream_size);
int inflate(z_streamp z, int f);
int inflateEnd(z_streamp z);
#define Z_FINISH 4
#define inflateInit(strm) inflateInit_((strm), ZLIB_VERSION, sizeof(z_stream))
/* -- end declarations -- */

/* WHAT IT DOES: Inflate a whole zlib stream (source, sourceLen) into dest
 * in one call (zlib's uncompress): *destLen is the room on entry and the
 * unpacked length on return; Z_BUF_ERROR when the output does not fit. */
/* @implements 0x80242890 tgr BrInflate */
int BrInflate(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
    z_stream stream;
    int err;

    stream.next_in = (Bytef*)source;
    stream.avail_in = (uInt)sourceLen;
    if ((uLong)stream.avail_in != sourceLen) return Z_BUF_ERROR;

    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;
    if ((uLong)stream.avail_out != *destLen) return Z_BUF_ERROR;

    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;

    err = inflateInit(&stream);
    if (err != Z_OK) return err;

    err = inflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        inflateEnd(&stream);
        return err;
    }
    *destLen = stream.total_out;

    err = inflateEnd(&stream);
    return err;
}
