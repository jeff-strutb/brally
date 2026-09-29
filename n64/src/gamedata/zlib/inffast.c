/* inffast.c -- zlib 1.0.4's fast decoding loop (inffast.c), as the ROM has
 * it.
 *
 * zlib 1.0.4, Copyright (C) 1995-1996 Mark Adler.  For conditions of
 * distribution and use, see the notice in tgr/zinfl.h.  Altered from the
 * original: K&R header written as a prototype, traces removed.
 */
#include "tgr/zinfl.h"

/* -- declarations -- */
#define base more.Base
#define next more.Next
#define exop word.what.Exop
#define bits word.what.Bits

#define GRABBITS(j) {while(k<(j)){b|=((uLong)NEXTBYTE)<<k;k+=8;}}
#define UNGRAB {n+=(c=k>>3);p-=c;k&=7;}
/* -- end declarations -- */

/* WHAT IT DOES: Decode codes straight into the window while there are at
 * least 258 bytes of room and 10 of input, with no per-bit input checks:
 * literals, and length/distance copies from earlier in the window
 * (wrapping round its end); returns the block's end or a bad code.
 * RESIDUE (293): the ROM's version is the game's: it hangs when a distance
 * table link is 0x10001, and its frame is 0x50 larger; zlib's source as
 * written here is the starting point. */
/* @implements 0x80241FC0 tgr inflate_fast */
int inflate_fast(uInt bl, uInt bd, inflate_huft *tl, inflate_huft *td,
                 inflate_blocks_statef *s, z_streamp z)
{
  inflate_huft *t;
  uInt e;
  uLong b;
  uInt k;
  Bytef *p;
  uInt n;
  Bytef *q;
  uInt m;
  uInt ml;
  uInt md;
  uInt c;
  uInt d;
  Bytef *r;

  LOAD

  ml = inflate_mask[bl];
  md = inflate_mask[bd];

  do {
    GRABBITS(20)
    if ((e = (t = tl + ((uInt)b & ml))->exop) == 0)
    {
      DUMPBITS(t->bits)
      *q++ = (Byte)t->base;
      m--;
      continue;
    }
    do {
      DUMPBITS(t->bits)
      if (e & 16)
      {
        e &= 15;
        c = t->base + ((uInt)b & inflate_mask[e]);
        DUMPBITS(e)

        GRABBITS(15);
        e = (t = td + ((uInt)b & md))->exop;
        do {
          DUMPBITS(t->bits)
          if (e & 16)
          {
            e &= 15;
            GRABBITS(e)
            d = t->base + ((uInt)b & inflate_mask[e]);
            DUMPBITS(e)

            m -= c;
            if ((uInt)(q - s->window) >= d)
            {
              r = q - d;
              *q++ = *r++;  c--;
              *q++ = *r++;  c--;
            }
            else
            {
              e = d - (uInt)(q - s->window);
              r = s->end - e;
              if (c > e)
              {
                c -= e;
                do {
                  *q++ = *r++;
                } while (--e);
                r = s->window;
              }
            }
            do {
              *q++ = *r++;
            } while (--c);
            break;
          }
          else if ((e & 64) == 0)
            e = (t = t->next + ((uInt)b & inflate_mask[e]))->exop;
          else
          {
            z->msg = (char*)"invalid distance code";
            UNGRAB
            UPDATE
            return Z_DATA_ERROR;
          }
        } while (1);
        break;
      }
      if ((e & 64) == 0)
      {
        if ((e = (t = t->next + ((uInt)b & inflate_mask[e]))->exop) == 0)
        {
          DUMPBITS(t->bits)
          *q++ = (Byte)t->base;
          m--;
          break;
        }
      }
      else if (e & 32)
      {
        UNGRAB
        UPDATE
        return Z_STREAM_END;
      }
      else
      {
        z->msg = (char*)"invalid literal/length code";
        UNGRAB
        UPDATE
        return Z_DATA_ERROR;
      }
    } while (1);
  } while (m >= 258 && n >= 10);

  UNGRAB
  UPDATE
  return Z_OK;
}
