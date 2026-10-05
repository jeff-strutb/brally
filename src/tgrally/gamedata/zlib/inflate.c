/* inflate.c -- zlib 1.0.4's zlib stream interface (inflate.c), as the ROM
 * has it.
 *
 * zlib 1.0.4, Copyright (C) 1995-1996 Mark Adler.  For conditions of
 * distribution and use, see the notice in tgr/zinfl.h.  Altered from the
 * original: K&R headers written as prototypes, traces removed.
 */
#include "tgr/zinfl.h"

/* -- declarations -- */
struct internal_state {
  enum {
    METHOD,
    FLAG,
    DICT4,
    DICT3,
    DICT2,
    DICT1,
    DICT0,
    BLOCKS,
    CHECK4,
    CHECK3,
    CHECK2,
    CHECK1,
    IDONE,
    IBAD
  } mode;
  union {
    uInt method;
    struct {
      uLong was;
      uLong need;
    } check;
    uInt marker;
  } sub;
  int nowrap;
  uInt wbits;
  inflate_blocks_statef *blocks;
};
int inflateReset(z_streamp z);
int inflateEnd(z_streamp z);
extern char *D_80368AC0;               /* the unpack scratch range, if any */
extern char *D_80368AC4;
extern int D_8028A888;                 /* frames the screen stays blank */
extern unsigned short D_801B5000[];    /* the frame buffers, borrowed */
extern char D_801DA800[];
void osViBlack(char on);
/* -- end declarations -- */

/* WHAT IT DOES: Restart a stream: zero the in and out totals and the
 * message, expect a zlib header (or blocks straight away without one) and
 * reset the block decoder. */
/* @implements 0x8023FF80 tgr inflateReset */
int inflateReset(z_streamp z)
{
  uLong c;

  if (z == Z_NULL || z->state == Z_NULL)
    return Z_STREAM_ERROR;
  z->total_in = z->total_out = 0;
  z->msg = Z_NULL;
  z->state->mode = z->state->nowrap ? BLOCKS : METHOD;
  inflate_blocks_reset(z->state->blocks, z, &c);
  return Z_OK;
}

/* WHAT IT DOES: Free a stream's decoder state. */
/* @implements 0x8023FFF0 tgr inflateEnd */
int inflateEnd(z_streamp z)
{
  uLong c;

  if (z == Z_NULL || z->state == Z_NULL || z->zfree == Z_NULL)
    return Z_STREAM_ERROR;
  if (z->state->blocks != Z_NULL)
    inflate_blocks_free(z->state->blocks, z, &c);
  ZFREE(z, z->state);
  z->state = Z_NULL;
  return Z_OK;
}

/* WHAT IT DOES: Set a stream up for inflating with a 2^w-byte window (a
 * negative w: raw deflate data, no zlib header or check), after checking
 * the caller was built against the same zlib.  Altered by the game: with no
 * allocator given, zcalloc hands out the unpack scratch range, or, when
 * there is none, the frame buffers (blanking the screen for 2 frames). */
/* @implements 0x80240070 tgr inflateInit2_ */
int inflateInit2_(z_streamp z, int w, const char *version, int stream_size)
{
  if (version == Z_NULL || version[0] != ZLIB_VERSION[0] ||
      stream_size != sizeof(z_stream))
      return Z_VERSION_ERROR;

  if (z == Z_NULL)
    return Z_STREAM_ERROR;
  z->msg = Z_NULL;
  if (z->zalloc == Z_NULL)
  {
    if (D_80368AC0 != Z_NULL)
    {
      D_80368AC8.next = D_80368AC0;
      D_80368AC8.end = D_80368AC4;
    }
    else
    {
      osViBlack(1);
      D_8028A888 = 2;
      D_80368AC8.next = (char *)D_801B5000;
      D_80368AC8.end = (char *)(D_801B5000 + ((unsigned int)D_801DA800 - (unsigned int)D_801B5000));
    }
    z->opaque = (voidpf)&D_80368AC8;
    z->zalloc = zcalloc;
  }
  if (z->zfree == Z_NULL) z->zfree = zcfree;
  if ((z->state = (struct internal_state *)
       ZALLOC(z,1,sizeof(struct internal_state))) == Z_NULL)
    return Z_MEM_ERROR;
  z->state->blocks = Z_NULL;

  z->state->nowrap = 0;
  if (w < 0)
  {
    w = - w;
    z->state->nowrap = 1;
  }

  if (w < 8 || w > 15)
  {
    inflateEnd(z);
    return Z_STREAM_ERROR;
  }
  z->state->wbits = (uInt)w;

  if ((z->state->blocks =
      inflate_blocks_new(z, z->state->nowrap ? Z_NULL : adler32, (uInt)1 << w))
      == Z_NULL)
  {
    inflateEnd(z);
    return Z_MEM_ERROR;
  }

  inflateReset(z);
  return Z_OK;
}

/* WHAT IT DOES: inflateInit2_ with the default 32K window. */
/* @implements 0x80240240 tgr inflateInit_ */
int inflateInit_(z_streamp z, const char *version, int stream_size)
{
  return inflateInit2_(z, DEF_WBITS, version, stream_size);
}

#define NEEDBYTE {if(z->avail_in==0)return r;r=Z_OK;}
#define NEXTBYTE (z->avail_in--,z->total_in++,*z->next_in++)

/* WHAT IT DOES: Inflate as much as the input and output allow: read and
 * check the zlib header (method, window size, header check, preset
 * dictionary id), run the block decoder, then read and compare the Adler-32
 * check; the state carries over between calls. */
/* @implements 0x8024026C tgr inflate */
int inflate(z_streamp z, int f)
{
  int r;
  uInt b;

  if (z == Z_NULL || z->state == Z_NULL || z->next_in == Z_NULL || f < 0)
    return Z_STREAM_ERROR;
  r = Z_BUF_ERROR;
  while (1) switch (z->state->mode)
  {
    case METHOD:
      NEEDBYTE
      if (((z->state->sub.method = NEXTBYTE) & 0xf) != Z_DEFLATED)
      {
        z->state->mode = IBAD;
        z->msg = (char*)"unknown compression method";
        z->state->sub.marker = 5;
        break;
      }
      if ((z->state->sub.method >> 4) + 8 > z->state->wbits)
      {
        z->state->mode = IBAD;
        z->msg = (char*)"invalid window size";
        z->state->sub.marker = 5;
        break;
      }
      z->state->mode = FLAG;
    case FLAG:
      NEEDBYTE
      b = NEXTBYTE;
      if (((z->state->sub.method << 8) + b) % 31)
      {
        z->state->mode = IBAD;
        z->msg = (char*)"incorrect header check";
        z->state->sub.marker = 5;
        break;
      }
      if (!(b & PRESET_DICT))
      {
        z->state->mode = BLOCKS;
        break;
      }
      z->state->mode = DICT4;
    case DICT4:
      NEEDBYTE
      z->state->sub.check.need = (uLong)NEXTBYTE << 24;
      z->state->mode = DICT3;
    case DICT3:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 16;
      z->state->mode = DICT2;
    case DICT2:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 8;
      z->state->mode = DICT1;
    case DICT1:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE;
      z->adler = z->state->sub.check.need;
      z->state->mode = DICT0;
      return Z_NEED_DICT;
    case DICT0:
      z->state->mode = IBAD;
      z->msg = (char*)"need dictionary";
      z->state->sub.marker = 0;
      return Z_STREAM_ERROR;
    case BLOCKS:
      r = inflate_blocks(z->state->blocks, z, r);
      if (r == Z_DATA_ERROR)
      {
        z->state->mode = IBAD;
        z->state->sub.marker = 0;
        break;
      }
      if (r != Z_STREAM_END)
        return r;
      r = Z_OK;
      inflate_blocks_reset(z->state->blocks, z, &z->state->sub.check.was);
      if (z->state->nowrap)
      {
        z->state->mode = IDONE;
        break;
      }
      z->state->mode = CHECK4;
    case CHECK4:
      NEEDBYTE
      z->state->sub.check.need = (uLong)NEXTBYTE << 24;
      z->state->mode = CHECK3;
    case CHECK3:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 16;
      z->state->mode = CHECK2;
    case CHECK2:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE << 8;
      z->state->mode = CHECK1;
    case CHECK1:
      NEEDBYTE
      z->state->sub.check.need += (uLong)NEXTBYTE;

      if (z->state->sub.check.was != z->state->sub.check.need)
      {
        z->state->mode = IBAD;
        z->msg = (char*)"incorrect data check";
        z->state->sub.marker = 5;
        break;
      }
      z->state->mode = IDONE;
    case IDONE:
      return Z_STREAM_END;
    case IBAD:
      return Z_DATA_ERROR;
    default:
      return Z_STREAM_ERROR;
  }
}

/* WHAT IT DOES: Supply the preset dictionary the header asked for: it must
 * match the header's Adler-32; only its last window-size bytes are kept. */
/* @implements 0x80240810 tgr inflateSetDictionary */
int inflateSetDictionary(z_streamp z, const Bytef *dictionary, uInt dictLength)
{
  uInt length = dictLength;

  if (z == Z_NULL || z->state == Z_NULL || z->state->mode != DICT0)
    return Z_STREAM_ERROR;

  if (adler32(1L, dictionary, dictLength) != z->adler) return Z_DATA_ERROR;
  z->adler = 1L;

  if (length >= ((uInt)1<<z->state->wbits))
  {
    length = (1<<z->state->wbits)-1;
    dictionary += dictLength - length;
  }
  inflate_set_dictionary(z->state->blocks, dictionary, length);
  z->state->mode = BLOCKS;
  return Z_OK;
}

/* WHAT IT DOES: Skip input up to the next full-flush marker (00 00 FF FF),
 * counting marker bytes across calls, and restart block decoding there. */
/* @implements 0x802408E8 tgr inflateSync */
int inflateSync(z_streamp z)
{
  uInt n;
  Bytef *p;
  uInt m;
  uLong r, w;

  if (z == Z_NULL || z->state == Z_NULL)
    return Z_STREAM_ERROR;
  if (z->state->mode != IBAD)
  {
    z->state->mode = IBAD;
    z->state->sub.marker = 0;
  }
  if ((n = z->avail_in) == 0)
    return Z_BUF_ERROR;
  p = z->next_in;
  m = z->state->sub.marker;

  while (n && m < 4)
  {
    if (*p == (Byte)(m < 2 ? 0 : 0xff))
      m++;
    else if (*p)
      m = 0;
    else
      m = 4 - m;
    p++, n--;
  }

  z->total_in += p - z->next_in;
  z->next_in = p;
  z->avail_in = n;
  z->state->sub.marker = m;

  if (m != 4)
    return Z_DATA_ERROR;
  r = z->total_in;  w = z->total_out;
  inflateReset(z);
  z->total_in = r;  z->total_out = w;
  z->state->mode = BLOCKS;
  return Z_OK;
}
