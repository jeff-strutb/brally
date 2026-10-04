/* br_podarc.h -- the POD archive reader object (0x424 bytes in the original).
 *
 * One object of this class lives at 0x10AC0810 (g_brModelMgr), constructed by
 * BrObj87Ctor and given the vtable at 0x10077150.  Every view of it in the
 * tree uses this one layout, so the field offsets agree at any pointer size.
 *
 *   original   field
 *   +0x000     vtable
 *   +0x004     sub04      empty helper sub-object (its methods ignore `this`)
 *   +0x008     magic[8]   first 8 bytes of the 16-byte file header
 *   +0x010     cEntries
 *   +0x014     offDir
 *   +0x018     aEntries   the directory, 76 bytes per entry (disk format)
 *   +0x01C     pFile
 *   +0x020     szName     the archive's path
 *   +0x420     cbDir
 *
 * Vtable (0x10077150), C entry points in brackets:
 *   0 scalar deleting destructor    [BrObj87A0DeleteDtor]
 *   1 MakeKey(name, key[64])        [BrCleanupName_100087D0]
 *   2 Find(name) -> index or -1     [BrKeyCacheFind]
 *   3 Lookup(name) -> index, fatal if missing          [M8930]
 *   4 Size(i)                                          [M8960]
 *   5 Read(i, dst)                                     [M8990]
 *   6 LoadInto(name, dst) -> dst                       [M8A90]
 *   7 ReadInto(i, dst) -> dst                          [M8A30]
 *   8 LoadNew(name) -> new buffer                      [BrVt8A70CallPair]
 *   9 ReadNew(i) -> new buffer                         [M89F0]
 */
#ifndef BR_PODARC_H
#define BR_PODARC_H

#include <stdio.h>
#include <stdint.h>

/* One directory entry, as stored on disk. */
typedef struct BrPodArcEntry {
    uint32_t offData;           /* +0x00 */
    uint32_t cbData;            /* +0x04 */
    uint8_t  flagA, flagB;      /* +0x08 */
    uint8_t  pad0A[2];          /* +0x0A */
    char     szName[64];        /* +0x0C  the key the search compares */
} BrPodArcEntry;

#define BR_PODARC_FIELDS                                                    \
    uint32_t       sub04;           /* +0x004 */                            \
    char           magic[8];        /* +0x008 */                            \
    int32_t        cEntries;        /* +0x010 */                            \
    int32_t        offDir;          /* +0x014 */                            \
    BrPodArcEntry *aEntries;        /* +0x018 */                            \
    FILE          *pFile;           /* +0x01C */                            \
    char           szName[0x400];   /* +0x020 */                            \
    uint32_t       cbDir;           /* +0x420 */

typedef struct BrPodArc {
    void *const *pVtbl;
    BR_PODARC_FIELDS
} BrPodArc;

#ifdef __cplusplus
/* The C++ methods' view: the vtable slots, then the same fields. */
class BrPodArcObj {
public:
    virtual void *Delete(unsigned char flags);
    virtual void  MakeKey(const char *src, char *dst);
    virtual int   Find(const char *name);
    virtual int   Lookup(const char *name);
    virtual int   Size(unsigned i);
    virtual void  Read(unsigned i, void *dst);
    virtual void *LoadInto(const char *name, void *dst);
    virtual void *ReadInto(unsigned i, void *dst);
    virtual void *LoadNew(const char *name);
    virtual void *ReadNew(unsigned i);
    BR_PODARC_FIELDS
};
#endif

#endif
