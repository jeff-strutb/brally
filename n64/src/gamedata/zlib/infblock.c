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
