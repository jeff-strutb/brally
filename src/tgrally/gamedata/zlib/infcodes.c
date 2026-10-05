/* infcodes.c -- zlib 1.0.4's literal/length and distance decoding
 * (infcodes.c), as the ROM has it.
 *
 * zlib 1.0.4, Copyright (C) 1995-1996 Mark Adler.  For conditions of
 * distribution and use, see the notice in tgr/zinfl.h.  Altered from the
 * original: K&R headers written as prototypes, traces removed.
 */
#include "tgr/zinfl.h"

/* -- declarations -- */
#define base more.Base
#define next more.Next
#define exop word.what.Exop
#define bits word.what.Bits

struct inflate_codes_state {
  enum {
      START,
      LEN,
      LENEXT,
      DIST,
      DISTEXT,
      COPY,
      LIT,
      WASH,
      END,
      BADCODE}
    mode;
  uInt len;
  union {
    struct {
      inflate_huft *tree;
      uInt need;
    } code;
    uInt lit;
    struct {
      uInt get;
      uInt dist;
    } copy;
  } sub;
  Byte lbits;
  Byte dbits;
  inflate_huft *ltree;
  inflate_huft *dtree;
};
/* -- end declarations -- */

/* WHAT IT DOES: Allocate a code decoder for the given literal/length and
 * distance trees (bl and bd bits looked up per step). */
/* @implements 0x80241590 tgr inflate_codes_new */
inflate_codes_statef *inflate_codes_new(uInt bl, uInt bd, inflate_huft *tl, inflate_huft *td,
                                        z_streamp z)
{
  inflate_codes_statef *c;

  if ((c = (inflate_codes_statef *)
       ZALLOC(z,1,sizeof(struct inflate_codes_state))) != Z_NULL)
  {
    c->mode = START;
    c->lbits = (Byte)bl;
    c->dbits = (Byte)bd;
    c->ltree = tl;
    c->dtree = td;
  }
  return c;
}

/* WHAT IT DOES: Decode a compressed block's literal/length and distance
 * codes into the window until the input or output runs out or the block
 * ends, handing long runs to inflate_fast; the state carries over. */
/* @implements 0x80241600 tgr inflate_codes */
int inflate_codes(inflate_blocks_statef *s, z_streamp z, int r)
{
  uInt j;
  inflate_huft *t;
  uInt e;
  uLong b;
  uInt k;
  Bytef *p;
  uInt n;
  Bytef *q;
  uInt m;
  Bytef *f;
  inflate_codes_statef *c = s->sub.decode.codes;

  LOAD

  while (1) switch (c->mode)
  {
    case START:
      if (m >= 258 && n >= 10)
      {
        UPDATE
        r = inflate_fast(c->lbits, c->dbits, c->ltree, c->dtree, s, z);
        LOAD
        if (r != Z_OK)
        {
          c->mode = r == Z_STREAM_END ? WASH : BADCODE;
          break;
        }
      }
      c->sub.code.need = c->lbits;
      c->sub.code.tree = c->ltree;
      c->mode = LEN;
    case LEN:
      j = c->sub.code.need;
      NEEDBITS(j)
      t = c->sub.code.tree + ((uInt)b & inflate_mask[j]);
      DUMPBITS(t->bits)
      e = (uInt)(t->exop);
      if (e == 0)
      {
        c->sub.lit = t->base;
        c->mode = LIT;
        break;
      }
      if (e & 16)
      {
        c->sub.copy.get = e & 15;
        c->len = t->base;
        c->mode = LENEXT;
        break;
      }
      if ((e & 64) == 0)
      {
        c->sub.code.need = e;
        c->sub.code.tree = t->next;
        break;
      }
      if (e & 32)
      {
        c->mode = WASH;
        break;
      }
      c->mode = BADCODE;
      z->msg = (char*)"invalid literal/length code";
      r = Z_DATA_ERROR;
      LEAVE
    case LENEXT:
      j = c->sub.copy.get;
      NEEDBITS(j)
      c->len += (uInt)b & inflate_mask[j];
      DUMPBITS(j)
      c->sub.code.need = c->dbits;
      c->sub.code.tree = c->dtree;
      c->mode = DIST;
    case DIST:
      j = c->sub.code.need;
      NEEDBITS(j)
      t = c->sub.code.tree + ((uInt)b & inflate_mask[j]);
      DUMPBITS(t->bits)
      e = (uInt)(t->exop);
      if (e & 16)
      {
        c->sub.copy.get = e & 15;
        c->sub.copy.dist = t->base;
        c->mode = DISTEXT;
        break;
      }
      if ((e & 64) == 0)
      {
        c->sub.code.need = e;
        c->sub.code.tree = t->next;
        break;
      }
      c->mode = BADCODE;
      z->msg = (char*)"invalid distance code";
      r = Z_DATA_ERROR;
      LEAVE
    case DISTEXT:
      j = c->sub.copy.get;
      NEEDBITS(j)
      c->sub.copy.dist += (uInt)b & inflate_mask[j];
      DUMPBITS(j)
      c->mode = COPY;
    case COPY:
      f = (uInt)(q - s->window) < c->sub.copy.dist ?
          s->end - (c->sub.copy.dist - (q - s->window)) :
          q - c->sub.copy.dist;
      while (c->len)
      {
        NEEDOUT
        OUTBYTE(*f++)
        if (f == s->end)
          f = s->window;
        c->len--;
      }
      c->mode = START;
      break;
    case LIT:
      NEEDOUT
      OUTBYTE(c->sub.lit)
      c->mode = START;
      break;
    case WASH:
      FLUSH
      if (s->read != s->write)
        LEAVE
      c->mode = END;
    case END:
      r = Z_STREAM_END;
      LEAVE
    case BADCODE:
      r = Z_DATA_ERROR;
      LEAVE
    default:
      r = Z_STREAM_ERROR;
      LEAVE
  }
}

/* WHAT IT DOES: Free a code decoder. */
/* @implements 0x80241F88 tgr inflate_codes_free */
void inflate_codes_free(inflate_codes_statef *c, z_streamp z)
{
  ZFREE(z, c);
}
