/* inftrees.c -- zlib 1.0.4's Huffman table building (inftrees.c), as the
 * ROM has it.
 *
 * zlib 1.0.4, Copyright (C) 1995-1996 Mark Adler.  For conditions of
 * distribution and use, see the notice in tgr/zinfl.h.  Altered from the
 * original: K&R headers written as prototypes, debug code removed, huft_build
 * and falloc global (the build tool needs every function named).
 */
#include "tgr/zinfl.h"

/* -- declarations -- */
#define base more.Base
#define next more.Next
#define exop word.what.Exop
#define bits word.what.Bits

int huft_build(uIntf *b, uInt n, uInt s, uIntf *d, uIntf *e, inflate_huft **t,
                      uIntf *m, z_streamp zs);
voidpf falloc(voidpf q, uInt n, uInt s);

#define BMAX 15
#define N_MAX 288

#define FIXEDH 530
/* -- end declarations -- */

char inflate_copyright[] = " inflate 1.0.4 Copyright 1995-1996 Mark Adler ";

/* Tables for deflate from PKZIP's appnote.txt. */
static uInt cplens[31] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};
static uInt cplext[31] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 192, 192};
static uInt cpdist[30] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
        8193, 12289, 16385, 24577};
static uInt cpdext[30] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
        12, 12, 13, 13};

/* WHAT IT DOES: Build the decoding tables for a set of code lengths:
 * count the lengths, check the set is not oversubscribed, sort the values
 * by length, and fill tables of at most m bits per level, linked through a
 * dummy first entry; returns Z_BUF_ERROR for an incomplete set. */
/* @implements 0x80240A30 tgr huft_build */
int huft_build(uIntf *b, uInt n, uInt s, uIntf *d, uIntf *e, inflate_huft **t,
                      uIntf *m, z_streamp zs)
{

  uInt a;
  uInt c[BMAX+1];
  uInt f;
  int g;
  int h;
  register uInt i;
  register uInt j;
  register int k;
  int l;
  register uIntf *p;
  inflate_huft *q;
  struct inflate_huft_s r;
  inflate_huft *u[BMAX];
  uInt v[N_MAX];
  register int w;
  uInt x[BMAX+1];
  uIntf *xp;
  int y;
  uInt z;


  p = c;
#define C0 *p++ = 0;
#define C2 C0 C0 C0 C0
#define C4 C2 C2 C2 C2
  C4
  p = b;  i = n;
  do {
    c[*p++]++;
  } while (--i);
  if (c[0] == n)
  {
    *t = (inflate_huft *)Z_NULL;
    *m = 0;
    return Z_OK;
  }


  l = *m;
  for (j = 1; j <= BMAX; j++)
    if (c[j])
      break;
  k = j;
  if ((uInt)l < j)
    l = j;
  for (i = BMAX; i; i--)
    if (c[i])
      break;
  g = i;
  if ((uInt)l > i)
    l = i;
  *m = l;


  for (y = 1 << j; j < i; j++, y <<= 1)
    if ((y -= c[j]) < 0)
      return Z_DATA_ERROR;
  if ((y -= c[i]) < 0)
    return Z_DATA_ERROR;
  c[i] += y;


  x[1] = j = 0;
  p = c + 1;  xp = x + 2;
  while (--i) {
    *xp++ = (j += *p++);
  }


  p = b;  i = 0;
  do {
    if ((j = *p++) != 0)
      v[x[j]++] = i;
  } while (++i < n);


  x[0] = i = 0;
  p = v;
  h = -1;
  w = -l;
  u[0] = (inflate_huft *)Z_NULL;
  q = (inflate_huft *)Z_NULL;
  z = 0;

  for (; k <= g; k++)
  {
    a = c[k];
    while (a--)
    {
      while (k > w + l)
      {
        h++;
        w += l;

        z = g - w;
        z = z > (uInt)l ? l : z;
        if ((f = 1 << (j = k - w)) > a + 1)
        {
          f -= a + 1;
          xp = c + k;
          if (j < z)
            while (++j < z)
            {
              if ((f <<= 1) <= *++xp)
                break;
              f -= *xp;
            }
        }
        z = 1 << j;

        if ((q = (inflate_huft *)ZALLOC
             (zs,z + 1,sizeof(inflate_huft))) == Z_NULL)
        {
          if (h)
            inflate_trees_free(u[0], zs);
          return Z_MEM_ERROR;
        }
        *t = q + 1;
        *(t = &(q->next)) = Z_NULL;
        u[h] = ++q;

        if (h)
        {
          x[h] = i;
          r.bits = (Byte)l;
          r.exop = (Byte)j;
          r.next = q;
          j = i >> (w - l);
          u[h-1][j] = r;
        }
      }

      r.bits = (Byte)(k - w);
      if (p >= v + n)
        r.exop = 128 + 64;
      else if (*p < s)
      {
        r.exop = (Byte)(*p < 256 ? 0 : 32 + 64);
        r.base = *p++;
      }
      else
      {
        r.exop = (Byte)(e[*p - s] + 16 + 64);
        r.base = d[*p++ - s];
      }

      f = 1 << (k - w);
      for (j = i >> w; j < z; j += f)
        q[j] = r;

      for (j = 1 << (k - 1); i & j; j >>= 1)
        i ^= j;
      i ^= j;

      while ((i & ((1 << w) - 1)) != x[h])
      {
        h--;
        w -= l;
      }
    }
  }


  return y != 0 && g != 1 ? Z_BUF_ERROR : Z_OK;
}

/* WHAT IT DOES: Build the table for the 19 bit-length code lengths of a
 * dynamic block; an oversubscribed or incomplete set is a data error. */
/* @implements 0x802410E0 tgr inflate_trees_bits */
int inflate_trees_bits(uIntf *c, uIntf *bb, inflate_huft **tb, z_streamp z)
{
  int r;

  r = huft_build(c, 19, 19, (uIntf*)Z_NULL, (uIntf*)Z_NULL, tb, bb, z);
  if (r == Z_DATA_ERROR)
    z->msg = (char*)"oversubscribed dynamic bit lengths tree";
  else if (r == Z_BUF_ERROR)
  {
    inflate_trees_free(*tb, z);
    z->msg = (char*)"incomplete dynamic bit lengths tree";
    r = Z_DATA_ERROR;
  }
  return r;
}

/* WHAT IT DOES: Build a dynamic block's literal/length and distance tables
 * from its code lengths; a bad set frees what was built and is a data
 * error (the distance messages repeat zlib 1.0.4's literal/length text). */
/* @implements 0x80241180 tgr inflate_trees_dynamic */
int inflate_trees_dynamic(uInt nl, uInt nd, uIntf *c, uIntf *bl, uIntf *bd,
                          inflate_huft **tl, inflate_huft **td, z_streamp z)
{
  int r;

  if ((r = huft_build(c, nl, 257, cplens, cplext, tl, bl, z)) != Z_OK)
  {
    if (r == Z_DATA_ERROR)
      z->msg = (char*)"oversubscribed literal/length tree";
    else if (r == Z_BUF_ERROR)
    {
      inflate_trees_free(*tl, z);
      z->msg = (char*)"incomplete literal/length tree";
      r = Z_DATA_ERROR;
    }
    return r;
  }

  if ((r = huft_build(c + nl, nd, 0, cpdist, cpdext, td, bd, z)) != Z_OK)
  {
    if (r == Z_DATA_ERROR)
      z->msg = (char*)"oversubscribed literal/length tree";
    else if (r == Z_BUF_ERROR) {
      inflate_trees_free(*td, z);
      z->msg = (char*)"incomplete literal/length tree";
      r = Z_DATA_ERROR;
    }
    inflate_trees_free(*tl, z);
    return r;
  }

  return Z_OK;
}

static int fixed_built = 0;
static inflate_huft fixed_mem[FIXEDH];
static uInt fixed_bl;
static uInt fixed_bd;
static inflate_huft *fixed_tl;
static inflate_huft *fixed_td;

/* WHAT IT DOES: The fixed tables' allocator: take n entries off the end of
 * the static table pool, counting down the entries left. */
/* @implements 0x802412EC tgr falloc */
voidpf falloc(voidpf q, uInt n, uInt s)
{
  *(intf *)q -= n+s-s;
  return (voidpf)(fixed_mem + *(intf *)q);
}

/* WHAT IT DOES: Hand back the fixed-code tables (7-bit literal/length,
 * 5-bit distance), building them from the static pool the first time. */
/* @implements 0x80241314 tgr inflate_trees_fixed */
int inflate_trees_fixed(uIntf *bl, uIntf *bd, inflate_huft **tl, inflate_huft **td)
{
  if (!fixed_built)
  {
    int k;
    unsigned c[288];
    z_stream z;
    int f = FIXEDH;

    z.zalloc = falloc;
    z.zfree = Z_NULL;
    z.opaque = (voidpf)&f;

    for (k = 0; k < 144; k++)
      c[k] = 8;
    for (; k < 256; k++)
      c[k] = 9;
    for (; k < 280; k++)
      c[k] = 7;
    for (; k < 288; k++)
      c[k] = 8;
    fixed_bl = 7;
    huft_build(c, 288, 257, cplens, cplext, &fixed_tl, &fixed_bl, &z);

    for (k = 0; k < 30; k++)
      c[k] = 5;
    fixed_bd = 5;
    huft_build(c, 30, 0, cpdist, cpdext, &fixed_td, &fixed_bd, &z);

    fixed_built = 1;
  }
  *bl = fixed_bl;
  *bd = fixed_bd;
  *tl = fixed_tl;
  *td = fixed_td;
  return Z_OK;
}

/* WHAT IT DOES: Free a chain of tables built by huft_build: reverse the
 * links kept in each table's dummy first entry, then free from there. */
/* @implements 0x8024150C tgr inflate_trees_free */
int inflate_trees_free(inflate_huft *t, z_streamp z)
{
  register inflate_huft *p, *q, *r;

  p = Z_NULL;
  q = t;
  while (q != Z_NULL)
  {
    r = (q - 1)->next;
    (q - 1)->next = p;
    p = q;
    q = r;
  }
  while (p != Z_NULL)
  {
    q = (--p)->next;
    ZFREE(z,p);
    p = q;
  }
  return Z_OK;
}
