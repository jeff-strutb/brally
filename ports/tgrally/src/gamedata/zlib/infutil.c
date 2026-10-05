/* infutil.c -- zlib 1.0.4's inflate utilities (infutil.c), as the ROM has
 * it.
 *
 * zlib 1.0.4, Copyright (C) 1995-1996 Mark Adler.  For conditions of
 * distribution and use, see the notice in tgr/zinfl.h.  Altered from the
 * original: K&R header written as a prototype.
 */
#include "tgr/zinfl.h"

/* -- declarations -- */
/* -- end declarations -- */

/* And'ing with mask[n] masks the lower n bits */
uInt inflate_mask[17] = {
    0x0000,
    0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
    0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
};

/* WHAT IT DOES: Copy as much of the window's decoded bytes as the output
 * has room for, updating the counters and the check value, in two pieces
 * when the bytes wrap round the window's end. */
/* @implements 0x80242490 tgr inflate_flush */
int inflate_flush(inflate_blocks_statef *s, z_streamp z, int r)
{
  uInt n;
  Bytef *p;
  Bytef *q;

  p = z->next_out;
  q = s->read;

  n = (uInt)((q <= s->write ? s->write : s->end) - q);
  if (n > z->avail_out) n = z->avail_out;
  if (n && r == Z_BUF_ERROR) r = Z_OK;

  z->avail_out -= n;
  z->total_out += n;

  if (s->checkfn != Z_NULL)
    z->adler = s->check = (*s->checkfn)(s->check, q, n);

  zmemcpy(p, q, n);
  p += n;
  q += n;

  if (q == s->end)
  {
    q = s->window;
    if (s->write == s->end)
      s->write = s->window;

    n = (uInt)(s->write - q);
    if (n > z->avail_out) n = z->avail_out;
    if (n && r == Z_BUF_ERROR) r = Z_OK;

    z->avail_out -= n;
    z->total_out += n;

    if (s->checkfn != Z_NULL)
      z->adler = s->check = (*s->checkfn)(s->check, q, n);

    zmemcpy(p, q, n);
    p += n;
    q += n;
  }

  z->next_out = p;
  s->read = q;

  return r;
}
