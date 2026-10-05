/* infblock.c -- zlib 1.0.4's inflate block decoding (infblock.c), as the
 * ROM has it.
 *
 * zlib 1.0.4, Copyright (C) 1995-1996 Mark Adler.  For conditions of
 * distribution and use, see the notice in tgr/zinfl.h.  Altered from the
 * original: K&R headers written as prototypes, traces removed.
 */
#include "tgr/zinfl.h"

/* -- declarations -- */
/* -- end declarations -- */

/* Table for deflate from PKZIP's appnote.txt. */
static uInt border[] = { /* Order of the bit length code lengths */
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

/* WHAT IT DOES: Reset a block decoder: hand back the running check value,
 * free whatever the block in progress had allocated (the bit-length table,
 * or the code tables and decoder), clear the bit buffer, empty the window
 * and restart the check value. */
/* @implements 0x8023EDB0 tgr inflate_blocks_reset */
void inflate_blocks_reset(inflate_blocks_statef *s, z_streamp z, uLongf *c)
{
  if (s->checkfn != Z_NULL)
    *c = s->check;
  if (s->mode == BTREE || s->mode == DTREE)
    ZFREE(z, s->sub.trees.blens);
  if (s->mode == CODES)
  {
    inflate_codes_free(s->sub.decode.codes, z);
    inflate_trees_free(s->sub.decode.td, z);
    inflate_trees_free(s->sub.decode.tl, z);
  }
  s->mode = TYPE;
  s->bitk = 0;
  s->bitb = 0;
  s->read = s->write = s->window;
  if (s->checkfn != Z_NULL)
    z->adler = s->check = (*s->checkfn)(0L, Z_NULL, 0);
}

/* WHAT IT DOES: Allocate a block decoder with a w-byte window and the given
 * check function, and reset it; 0 when either allocation fails. */
/* @implements 0x8023EE84 tgr inflate_blocks_new */
inflate_blocks_statef *inflate_blocks_new(z_streamp z, check_func c, uInt w)
{
  inflate_blocks_statef *s;

  if ((s = (inflate_blocks_statef *)ZALLOC
       (z,1,sizeof(struct inflate_blocks_state))) == Z_NULL)
    return s;
  if ((s->window = (Bytef *)ZALLOC(z, 1, w)) == Z_NULL)
  {
    ZFREE(z, s);
    return Z_NULL;
  }
  s->end = s->window + w;
  s->checkfn = c;
  s->mode = TYPE;
  inflate_blocks_reset(s, z, &s->check);
  return s;
}

/* WHAT IT DOES: Decode deflate blocks into the window while input and
 * output allow: the block header, a stored block's length and bytes, a
 * dynamic block's code-length tables (bit-length codes, then the
 * run-length coded literal/length and distance lengths), the fixed tables,
 * and the codes themselves through inflate_codes; the state carries over
 * between calls and the window is flushed to the output on the way out. */
/* @implements 0x8023EF4C tgr inflate_blocks */
int inflate_blocks(inflate_blocks_statef *s, z_streamp z, int r)
{
  uInt t;
  uLong b;
  uInt k;
  Bytef *p;
  uInt n;
  Bytef *q;
  uInt m;

  LOAD

  while (1) switch (s->mode)
  {
    case TYPE:
      NEEDBITS(3)
      t = (uInt)b & 7;
      s->last = t & 1;
      switch (t >> 1)
      {
        case 0:
          DUMPBITS(3)
          t = k & 7;
          DUMPBITS(t)
          s->mode = LENS;
          break;
        case 1:
          {
            uInt bl, bd;
            inflate_huft *tl, *td;

            inflate_trees_fixed(&bl, &bd, &tl, &td);
            s->sub.decode.codes = inflate_codes_new(bl, bd, tl, td, z);
            if (s->sub.decode.codes == Z_NULL)
            {
              r = Z_MEM_ERROR;
              LEAVE
            }
            s->sub.decode.tl = Z_NULL;
            s->sub.decode.td = Z_NULL;
          }
          DUMPBITS(3)
          s->mode = CODES;
          break;
        case 2:
          DUMPBITS(3)
          s->mode = TABLE;
          break;
        case 3:
          DUMPBITS(3)
          s->mode = BAD;
          z->msg = (char*)"invalid block type";
          r = Z_DATA_ERROR;
          LEAVE
      }
      break;
    case LENS:
      NEEDBITS(32)
      if ((((~b) >> 16) & 0xffff) != (b & 0xffff))
      {
        s->mode = BAD;
        z->msg = (char*)"invalid stored block lengths";
        r = Z_DATA_ERROR;
        LEAVE
      }
      s->sub.left = (uInt)b & 0xffff;
      b = k = 0;
      s->mode = s->sub.left ? STORED : (s->last ? DRY : TYPE);
      break;
    case STORED:
      if (n == 0)
        LEAVE
      NEEDOUT
      t = s->sub.left;
      if (t > n) t = n;
      if (t > m) t = m;
      zmemcpy(q, p, t);
      p += t;  n -= t;
      q += t;  m -= t;
      if ((s->sub.left -= t) != 0)
        break;
      s->mode = s->last ? DRY : TYPE;
      break;
    case TABLE:
      NEEDBITS(14)
      s->sub.trees.table = t = (uInt)b & 0x3fff;
      if ((t & 0x1f) > 29 || ((t >> 5) & 0x1f) > 29)
      {
        s->mode = BAD;
        z->msg = (char*)"too many length or distance symbols";
        r = Z_DATA_ERROR;
        LEAVE
      }
      t = 258 + (t & 0x1f) + ((t >> 5) & 0x1f);
      if (t < 19)
        t = 19;
      if ((s->sub.trees.blens = (uIntf*)ZALLOC(z, t, sizeof(uInt))) == Z_NULL)
      {
        r = Z_MEM_ERROR;
        LEAVE
      }
      DUMPBITS(14)
      s->sub.trees.index = 0;
      s->mode = BTREE;
    case BTREE:
      while (s->sub.trees.index < 4 + (s->sub.trees.table >> 10))
      {
        NEEDBITS(3)
        s->sub.trees.blens[border[s->sub.trees.index++]] = (uInt)b & 7;
        DUMPBITS(3)
      }
      while (s->sub.trees.index < 19)
        s->sub.trees.blens[border[s->sub.trees.index++]] = 0;
      s->sub.trees.bb = 7;
      t = inflate_trees_bits(s->sub.trees.blens, &s->sub.trees.bb,
                             &s->sub.trees.tb, z);
      if (t != Z_OK)
      {
        r = t;
        if (r == Z_DATA_ERROR)
          s->mode = BAD;
        LEAVE
      }
      s->sub.trees.index = 0;
      s->mode = DTREE;
    case DTREE:
      while (t = s->sub.trees.table,
             s->sub.trees.index < 258 + (t & 0x1f) + ((t >> 5) & 0x1f))
      {
        inflate_huft *h;
        uInt i, j, c;

        t = s->sub.trees.bb;
        NEEDBITS(t)
        h = s->sub.trees.tb + ((uInt)b & inflate_mask[t]);
        t = h->word.what.Bits;
        c = h->more.Base;
        if (c < 16)
        {
          DUMPBITS(t)
          s->sub.trees.blens[s->sub.trees.index++] = c;
        }
        else
        {
          i = c == 18 ? 7 : c - 14;
          j = c == 18 ? 11 : 3;
          NEEDBITS(t + i)
          DUMPBITS(t)
          j += (uInt)b & inflate_mask[i];
          DUMPBITS(i)
          i = s->sub.trees.index;
          t = s->sub.trees.table;
          if (i + j > 258 + (t & 0x1f) + ((t >> 5) & 0x1f) ||
              (c == 16 && i < 1))
          {
            s->mode = BAD;
            z->msg = (char*)"invalid bit length repeat";
            r = Z_DATA_ERROR;
            LEAVE
          }
          c = c == 16 ? s->sub.trees.blens[i - 1] : 0;
          do {
            s->sub.trees.blens[i++] = c;
          } while (--j);
          s->sub.trees.index = i;
        }
      }
      inflate_trees_free(s->sub.trees.tb, z);
      s->sub.trees.tb = Z_NULL;
      {
        uInt bl, bd;
        inflate_huft *tl, *td;
        inflate_codes_statef *c;

        bl = 9;
        bd = 6;
        t = s->sub.trees.table;
        t = inflate_trees_dynamic(257 + (t & 0x1f), 1 + ((t >> 5) & 0x1f),
                                  s->sub.trees.blens, &bl, &bd, &tl, &td, z);
        if (t != Z_OK)
        {
          if (t == (uInt)Z_DATA_ERROR)
            s->mode = BAD;
          r = t;
          LEAVE
        }
        if ((c = inflate_codes_new(bl, bd, tl, td, z)) == Z_NULL)
        {
          inflate_trees_free(td, z);
          inflate_trees_free(tl, z);
          r = Z_MEM_ERROR;
          LEAVE
        }
        ZFREE(z, s->sub.trees.blens);
        s->sub.decode.codes = c;
        s->sub.decode.tl = tl;
        s->sub.decode.td = td;
      }
      s->mode = CODES;
    case CODES:
      UPDATE
      if ((r = inflate_codes(s, z, r)) != Z_STREAM_END)
        return inflate_flush(s, z, r);
      r = Z_OK;
      inflate_codes_free(s->sub.decode.codes, z);
      inflate_trees_free(s->sub.decode.td, z);
      inflate_trees_free(s->sub.decode.tl, z);
      LOAD
      if (!s->last)
      {
        s->mode = TYPE;
        break;
      }
      if (k > 7)
      {
        k -= 8;
        n++;
        p--;
      }
      s->mode = DRY;
    case DRY:
      FLUSH
      if (s->read != s->write)
        LEAVE
      s->mode = DONE;
    case DONE:
      r = Z_STREAM_END;
      LEAVE
    case BAD:
      r = Z_DATA_ERROR;
      LEAVE
    default:
      r = Z_STREAM_ERROR;
      LEAVE
  }
}

/* WHAT IT DOES: Free a block decoder: reset it (handing back the check
 * value), then free its window and itself. */
/* @implements 0x8023FED8 tgr inflate_blocks_free */
int inflate_blocks_free(inflate_blocks_statef *s, z_streamp z, uLongf *c)
{
  inflate_blocks_reset(s, z, c);
  ZFREE(z, s->window);
  ZFREE(z, s);
  return Z_OK;
}

/* WHAT IT DOES: Preload the window with an n-byte dictionary. */
/* @implements 0x8023FF34 tgr inflate_set_dictionary */
void inflate_set_dictionary(inflate_blocks_statef *s, const Bytef *d, uInt n)
{
  zmemcpy((charf *)s->window, d, n);
  s->read = s->write = s->window + n;
}
