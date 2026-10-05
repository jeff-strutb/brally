/* zinfl.h -- the parts of zlib 1.0.4's zlib.h, zutil.h and inflate headers
 * the ROM's inflate uses.
 *
 * zlib 1.0.4, Copyright (C) 1995-1996 Jean-loup Gailly and Mark Adler.
 * This software is provided 'as-is', without any express or implied
 * warranty.  Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the zlib license: the origin must not
 * be misrepresented, altered source versions must be plainly marked as such,
 * and this notice may not be removed from any source distribution.
 * This file is an altered version: the declarations are gathered from the
 * original headers into one.
 */
#ifndef ZINFL_H
#define ZINFL_H

typedef unsigned char Byte;
typedef unsigned int uInt;
typedef unsigned int uLong;
typedef Byte Bytef;
typedef char charf;
typedef int intf;
typedef uInt uIntf;
typedef uLong uLongf;
typedef void *voidpf;
typedef unsigned char uch;
typedef unsigned short ush;
typedef unsigned int ulg;

#define Z_NULL 0
#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT     2
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)
#define Z_VERSION_ERROR (-6)

typedef voidpf (*alloc_func)(voidpf opaque, uInt items, uInt size);
typedef void (*free_func)(voidpf opaque, voidpf address);

struct internal_state;

typedef struct z_stream_s {
  Bytef *next_in;
  uInt avail_in;
  uLong total_in;
  Bytef *next_out;
  uInt avail_out;
  uLong total_out;
  char *msg;
  struct internal_state *state;
  alloc_func zalloc;
  free_func zfree;
  voidpf opaque;
  int data_type;
  uLong adler;
  uLong reserved;
} z_stream;
typedef z_stream *z_streamp;

#define ZALLOC(strm, items, size) \
           (*((strm)->zalloc))((strm)->opaque, (items), (size))
#define ZFREE(strm, addr)  (*((strm)->zfree))((strm)->opaque, (voidpf)(addr))
#define TRY_FREE(s, p) {if (p) ZFREE(s, p);}


/* The game's allocator: zcalloc carves 8-aligned blocks off a range and
 * never frees; inflateInit2_ points the stream's opaque at this. */
typedef struct BrZHeap {
  char *next;
  char *end;
} BrZHeap;
extern BrZHeap tgr_zheap;                /* the zlib heap range (native: zlib's own state) */
#define D_80368AC8 tgr_zheap
#define zmemcpy memcpy

typedef uLong (*check_func)(uLong check, const Bytef *buf, uInt len);

/* inftrees.h */
typedef struct inflate_huft_s inflate_huft;
struct inflate_huft_s {
  union {
    struct {
      Byte Exop;
      Byte Bits;
    } what;
    Bytef *pad;
  } word;
  union {
    uInt Base;
    inflate_huft *Next;
  } more;
};

/* infcodes.h */
struct inflate_codes_state;
typedef struct inflate_codes_state inflate_codes_statef;

/* infutil.h */
typedef enum {
  TYPE,
  LENS,
  STORED,
  TABLE,
  BTREE,
  DTREE,
  CODES,
  DRY,
  DONE,
  BAD
} inflate_block_mode;

struct inflate_blocks_state {
  inflate_block_mode mode;
  union {
    uInt left;
    struct {
      uInt table;
      uInt index;
      uIntf *blens;
      uInt bb;
      inflate_huft *tb;
    } trees;
    struct {
      inflate_huft *tl;
      inflate_huft *td;
      inflate_codes_statef *codes;
    } decode;
  } sub;
  uInt last;
  uInt bitk;
  uLong bitb;
  Bytef *window;
  Bytef *end;
  Bytef *read;
  Bytef *write;
  check_func checkfn;
  uLong check;
};
typedef struct inflate_blocks_state inflate_blocks_statef;

#define Z_DEFLATED 8
#define MAX_WBITS 15
#define DEF_WBITS MAX_WBITS
#define PRESET_DICT 0x20
#define ZLIB_VERSION "1.0.4"

inflate_blocks_statef *inflate_blocks_new(z_streamp z, check_func c, uInt w);
int inflate_blocks(inflate_blocks_statef *s, z_streamp z, int r);
int inflate_blocks_free(inflate_blocks_statef *s, z_streamp z, uLongf *c);
void inflate_set_dictionary(inflate_blocks_statef *s, const Bytef *d, uInt n);
uLong adler32(uLong adler, const Bytef *buf, uInt len);
voidpf zcalloc(voidpf opaque, unsigned items, unsigned size);
void zcfree(voidpf opaque, voidpf ptr);
void inflate_codes_free(inflate_codes_statef *c, z_streamp z);
inflate_codes_statef *inflate_codes_new(uInt bl, uInt bd, inflate_huft *tl, inflate_huft *td,
                                        z_streamp z);
int inflate_codes(inflate_blocks_statef *s, z_streamp z, int r);
int inflate_fast(uInt bl, uInt bd, inflate_huft *tl, inflate_huft *td,
                 inflate_blocks_statef *s, z_streamp z);
int inflate_flush(inflate_blocks_statef *s, z_streamp z, int r);
extern uInt inflate_mask[17];
int inflate_trees_bits(uIntf *c, uIntf *bb, inflate_huft **tb, z_streamp z);
int inflate_trees_dynamic(uInt nl, uInt nd, uIntf *c, uIntf *bl, uIntf *bd,
                          inflate_huft **tl, inflate_huft **td, z_streamp z);
int inflate_trees_fixed(uIntf *bl, uIntf *bd, inflate_huft **tl, inflate_huft **td);

/* infutil.h: update pointers and return */
#define UPDBITS {s->bitb=b;s->bitk=k;}
#define UPDIN {z->avail_in=n;z->total_in+=p-z->next_in;z->next_in=p;}
#define UPDOUT {s->write=q;}
#define UPDATE {UPDBITS UPDIN UPDOUT}
#define LEAVE {UPDATE return inflate_flush(s,z,r);}
/*   get bytes and bits */
#define LOADIN {p=z->next_in;n=z->avail_in;b=s->bitb;k=s->bitk;}
#define NEEDBYTE {if(n)r=Z_OK;else LEAVE}
#define NEXTBYTE (n--,*p++)
#define NEEDBITS(j) {while(k<(j)){NEEDBYTE;b|=((uLong)NEXTBYTE)<<k;k+=8;}}
#define DUMPBITS(j) {b>>=(j);k-=(j);}
/*   output bytes */
#define WAVAIL (uInt)(q<s->read?s->read-q-1:s->end-q)
#define LOADOUT {q=s->write;m=(uInt)WAVAIL;}
#define WRAP {if(q==s->end&&s->read!=s->window){q=s->window;m=(uInt)WAVAIL;}}
#define FLUSH {UPDOUT r=inflate_flush(s,z,r); LOADOUT}
#define NEEDOUT {if(m==0){WRAP if(m==0){FLUSH WRAP if(m==0) LEAVE}}r=Z_OK;}
#define OUTBYTE(a) {*q++=(Byte)(a);m--;}
/*   load local pointers */
#define LOAD {LOADIN LOADOUT}
int inflate_trees_free(inflate_huft *t, z_streamp z);
void inflate_blocks_reset(inflate_blocks_statef *s, z_streamp z, uLongf *c);

#endif
