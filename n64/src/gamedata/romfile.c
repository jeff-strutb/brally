/* romfile.c -- loading a ROM file into memory someone else allocates
 */
#include "tgr/common.h"

/* -- declarations -- */
typedef struct BrRomFile {
  int start;                    /* 0x00  ROM range */
  int end;                      /* 0x04 */
  void *data;                   /* 0x08  where it was loaded */
} BrRomFile;
void BrRomRead(void *dst, int rom, int len);
unsigned int BrRomReadSize(int rom);
void func_8021CD30(void *dst, int rom, void *stream);
/* -- end declarations -- */

/* WHAT IT DOES: Load a ROM file as it is: allocate its length with the
 * given allocator and copy it in. */
/* @implements 0x8023DF00 tgr BrRomFileLoad */
void BrRomFileLoad(BrRomFile *f, void *(*alloc)(int size))
{
  f->data = alloc(f->end - f->start);
  BrRomRead(f->data, f->start, f->end - f->start);
}

/* WHAT IT DOES: Load a compressed ROM file: allocate its unpacked size with
 * the given allocator and unpack it there. */
/* @implements 0x8023DF4C tgr BrRomFileUnpack */
void BrRomFileUnpack(BrRomFile *f, void *(*alloc)(int size))
{
  f->data = alloc(BrRomReadSize(f->start));
  func_8021CD30(f->data, f->start, 0);
}
