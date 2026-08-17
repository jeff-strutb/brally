/* br_cfgfile.h -- "BossRally.cfg", the settings file, and the function that
 * reads it: Glide 0x10063060 / D3D 0x10069FF0.
 *
 * RESPONSIBILITY: settings -- what the player chose and what the machine is.
 * This is the file the whole of that folder exists for: it is the ONE thing
 * the game reads before it has a window, and everything the options screens
 * change ends up in it.
 *
 * WHO CALLS IT
 *
 *   RallyMain, 0x1001CD0D:   mov ecx, 0x10B71290
 *   RallyMain, 0x1001CD12:   call 0x10063060
 *
 * so it is __thiscall with `this` = 0x10B71290 and ONE stack argument, the
 * path, pushed back at 0x1001CCC8.  That path is built by the inlined
 * strcpy/strcat at 0x1001CCB5..0x1001CD0D as
 *
 *     0x10B72F48 = <basedir at 0x10B73540> + "BossRally.cfg" (0x100A9914)
 *
 * and <basedir> is br_basedir.c's, so with that module in place the path is
 * the install directory's, not a bare relative name.
 *
 * WHAT `this` IS -- 0x10B71290 IS g_BrCtrlCfg, NOT A FOURTH THING
 *
 * Established three independent ways, because this address is modelled twice
 * elsewhere in the tree under names that hide what it is:
 *
 *   1. 0x10062AD0 is `mov ecx, 0x10B71290; jmp 0x10062B00` -- the static
 *      constructor for the global -- and 0x10062AE0 registers 0x10062AF0
 *      (`mov ecx, 0x10B71290; jmp 0x10008D60`) as its atexit destructor.
 *      That is byte-for-byte D3D's 0x10069A60/0x10069A70 pair for
 *      0x10B4DF30, which slice3_42.h already names `g_BrCtrlCfg`.
 *   2. Every offset 0x10063060 touches -- 0x2A0, 0x2A8, 0x2B4(0x104),
 *      0x3B8(0x400), 0x7B8..0x870, and four 0xA8 blocks at 0, 0xA8, 0x150,
 *      0x1F8 -- is a BrCtrlCfg field at that exact offset.
 *   3. The command-line parser 0x10007F40 writes 0x10B71530 and 0x10B71534
 *      as a PAIR (0x100080E5/0x100080F3 and again at 0x100085A8/0x100085B6),
 *      and 0x10B71530 == 0x10B71290 + 0x2A0 == BrCtrlCfg::active while
 *      0x10B71534 == +0x2A4 == BrCtrlCfg::pActive.  It stores one of
 *      0x10B71290 / 0x10B71338 / 0x10B713E0 / 0x10B71488 into the latter --
 *      i.e. `&profile[0..3]`, since those are base + 0/0xA8/0x150/0x1F8.
 *
 * TWO EXISTING MODELS OF THIS OBJECT, both recorded rather than edited here
 * (they belong to other modules; see the report):
 *
 *   - br_appstart.h reads 0x10B71290/0x10B71338/0x10B713E0/0x10B71488 as
 *     "four objects ... stride 0xA8" and 0x10B71534 as a separate global
 *     `g_iBrCfgInputDevice`.  They are one object and its pActive field.
 *   - slice4_53.c's BrCfgSave1006A4A0 is the WRITER of this same file
 *     (Glide 0x100634B0 / D3D 0x1006A4A0) and takes `void *pThis`, walking
 *     it with raw byte offsets.  Those offsets are 32-bit-layout offsets, so
 *     it cannot be handed a host BrCtrlCfg -- everything past +0x2A4 shifts
 *     when the pointer widens.  Its field table and this module's agree
 *     field-for-field; they were derived independently, from the writer and
 *     from the reader, and match, which is the strongest evidence the format
 *     below is right.
 *
 * THE FILE
 *
 * Fixed layout, 0x878 == 2168 bytes, no padding and no alignment holes.
 * Every row below is stated TWICE in the binary -- by the reader's fread
 * sequence and by the writer's fwrite sequence -- and the two agree.
 *
 *   file    size   object   field
 *   0x000   4      --       magic "RCfg" (0x100B4C20).  FOUR bytes; the
 *                           writer emits strlen("RCfg") and no terminator.
 *   0x004   4      --       version, must equal 2.  The writer copies it
 *                           from the dword at 0x10077A2C, which is 2.
 *   0x008   4      0x2A8    f2A8
 *   0x00C   4      0x2AC    f2AC
 *   0x010   4      0x2B0    f2B0
 *   0x014   0x104  0x2B4    f2B4[0x41]
 *   0x118   0x400  0x3B8    f3B8[0x100]
 *   0x518   4      0x7B8    f7B8      (ctor 640)
 *   0x51C   4      0x7BC    f7BC      (ctor 480)
 *   0x520   4      0x7C0    f7C0
 *   0x524   4      0x7C4    f7C4
 *   0x528   0x10   0x7C8    f7C8[4]
 *   0x538   4      0x7D8    f7D8
 *   0x53C   4      0x7DC    f7DC
 *   0x540   4      0x7E0    f7E0
 *   0x544   4      0x7E4    f7E4
 *   0x548   4      0x7E8    f7E8
 *   0x54C   4      0x7EC    f7EC
 *   0x550   4      0x7F0    f7F0
 *   0x554   4      0x7F4    f7F4
 *   0x558   4      0x7F8    f7F8
 *   0x55C   4      0x7FC    f7FC
 *   0x560   4      0x800    f800
 *   0x564   4      0x804    f804
 *   0x568   4      0x808    f808
 *   0x56C   4      0x80C    f80C
 *   0x570   0x20   0x810    f810[8]
 *   0x590   0x40   0x830    f830[16]
 *   0x5D0   4      0x870    f870
 *   0x5D4   4      0x2A0    active
 *   0x5D8   0xA8   0x000    profile[0]
 *   0x680   0xA8   0x0A8    profile[1]
 *   0x728   0xA8   0x150    profile[2]
 *   0x7D0   0xA8   0x1F8    profile[3]
 *
 * 0x7D0 + 0xA8 == 0x878.  The covered object bytes are 0x870 of the object's
 * 0x874: the ONE hole is +0x2A4, pActive, which is neither written nor read.
 * That is not an oversight in the format -- a pointer has no meaning in a
 * file -- but it is the cause of the second bug below.
 *
 * THE ORDERING ODDITY is real and is in both directions: the four profile
 * blocks that occupy [0, 0x2A0) come LAST, after everything from 0x2A8 up,
 * and `active` (+0x2A0) goes out immediately before them.
 *
 * WHAT THE FUNCTION DOES
 *
 *   fp = fopen(path, "rb")          mode string 0x1007B0E0 == "rb"
 *   fp == NULL -> return 0          nothing is opened, nothing is touched
 *   BrCtrlCfgCtor(&tmp)             0x10062B00, on a 0x874 stack temporary
 *   BrCtrlCfgCopy(&tmp, this)       0x10062E50, i.e. tmp = *this
 *   read the 34 fields above, IN PLACE, straight into *this
 *   any check fails -> the failure arm
 *   fclose(fp); return 1
 *
 * Each read is `fread(p, size, 1, fp)` with the result compared to 1, so a
 * field is all-or-nothing: a file one byte short of a field's end fails on
 * that field and no later field is touched.  There is no length check, no
 * EOF check after the last field, and no seek: trailing bytes are ignored.
 *
 * BUG 1 -- THE FAILURE ARM SAVES THE OLD SETTINGS AND THEN DISCARDS THEM
 *
 *   0x1006343D  *this = tmp                (0x10062E50, the restore)
 *   0x1006344A  fclose(fp)
 *   0x10063455  BrCtrlCfgInit(this)        (0x10062D00)
 *
 * BrCtrlCfgInit writes EVERY field of the object -- all four profiles,
 * active, pActive, and every named field from 0x2A8 to 0x870 -- so the
 * restore on the line above is dead.  The whole point of the temporary is
 * defeated: a truncated or corrupt BossRally.cfg does not leave the player's
 * settings alone, it RESETS THEM TO DEFAULTS.  Reproduced, both calls, in
 * the original's order.
 *
 * BUG 2 -- pActive IS NOT REBUILT FROM THE LOADED `active`
 *
 * The success arm reads +0x2A0 out of the file and never touches +0x2A4, and
 * unlike 0x10062E50 it does not rebuild the pointer.  So after a successful
 * load the object can be self-inconsistent: `active` is the file's, pActive
 * is whatever the command line left it as.  This is very likely deliberate
 * rather than a slip -- 0x10007F40's `ReadJoystick=` runs BEFORE the load and
 * sets both, and leaving pActive alone is exactly what makes a command-line
 * device choice survive the config file.  Either way it is preserved.
 *
 * THE SEH FRAME IS NOT MODELLED, AND THAT IS A FINDING
 *
 * 0x10063060 installs an fs:[0] frame with handler 0x1007662B and moves a
 * state dword at [R-4] from -1 to 0 once the temporary is constructed and
 * back to -1 before each return.  That machinery exists for exactly one
 * purpose: to destroy the stack temporary if an exception unwinds through
 * the function.  The destructor it would call is 0x10008D60, which is ONE
 * byte -- `c3`, a bare `ret`.  So the frame has no observable effect and
 * there is nothing to transcribe.  (Both normal returns call that same
 * destructor explicitly, at 0x10063469 and 0x1006348B.)
 *
 * THE ESP TRACE, because a displacement here means nothing without it
 *
 *   entry                    esp = R      [R] = return address
 *   push -1                        R-4    EH state
 *   push 0x1007662B                R-8    handler
 *   push fs:[0]                    R-0xC  previous link;  fs:[0] = esp
 *   mov eax,[esp+0x10]                    == [R+4] == arg1, the path
 *   sub esp,0x87C                  R-0x888
 *   push ebx/ebp/esi/edi           R-0x898
 *   ... fopen(path, "rb") ...      back to R-0x898 after `add esp,8`
 *
 * Call that settled value B = R-0x898.  Then, and ONLY then, do the
 * displacements resolve:
 *
 *   B+0x010   the 4-byte version local
 *   B+0x014   the 4-byte magic local
 *   B+0x018   the BrCtrlCfg temporary, 0x874 bytes, ending at B+0x88C
 *   B+0x88C   saved fs:[0]      -- what 0x10063495 reloads
 *   B+0x890   handler
 *   B+0x894   EH state          -- what 0x1006345E/0x10063480 set to -1
 *   B+0x898   return address
 *   B+0x89C   arg1
 *
 * 0x10 + 0x874 + 4 + 4 == 0x87C exactly, so the frame is fully accounted for
 * and the temporary is exactly one BrCtrlCfg with nothing else in it.
 *
 * TWO DISPLACEMENTS THAT LOOK LIKE DIFFERENT OBJECTS AND ARE NOT.  At
 * 0x1006309E the constructor gets `lea ecx,[esp+0x18]`; at 0x100630A8 the
 * assignment gets `lea ecx,[esp+0x1C]`.  A `push ebx` sits between them, so
 * esp has dropped by 4 and BOTH are B+0x18 -- the SAME temporary.  Read as
 * two objects, this function grows a second 0x874 local that does not exist.
 *
 * PORTABILITY
 *
 * The original freads raw dwords into the live object, which works only
 * because it is 32-bit little-endian x86 and BrCtrlCfg is POD there.  It is
 * not POD here: pActive is a host pointer, so every offset past +0x2A4 moves
 * and sizeof(BrCtrlCfg) is not 0x874.  This module therefore decodes
 * FIELD BY FIELD, byte-wise little-endian, and never overlays anything on
 * the image.  One fread per field is kept, so the all-or-nothing failure
 * boundary is bit-for-bit the original's.
 */
#ifndef BR_CFGFILE_H
#define BR_CFGFILE_H

#include <stddef.h>
#include <stdint.h>

#include "slice3_42.h"    /* BrCtrlCfg, BrCtrlProfile, BrCtrlCfgInit/Ctor/Copy
                           * -- CANONICAL.  This module defines no type. */

#ifdef __cplusplus
extern "C" {
#endif

/* The literals, read out of BRGlide.dll rather than assumed.
 * The BR_CFG_ prefix is already taken four times over (slice5_60.h,
 * slice2_23.h, br_appstart.h), so these are BR_CTRLCFG_. */
#define BR_CTRLCFG_MAGIC        "RCfg"          /* 0x100B4C20            */
#define BR_CTRLCFG_MAGIC_SIZE   4               /* strlen, at runtime    */
#define BR_CTRLCFG_VERSION      2u              /* dword at 0x10077A2C   */
#define BR_CTRLCFG_MODE_READ    "rb"            /* 0x1007B0E0            */
#define BR_CTRLCFG_MODE_WRITE   "wb"            /* 0x1007B600            */
#define BR_CTRLCFG_FILENAME     "BossRally.cfg" /* 0x100A9914            */

/* 8 bytes of header plus 0x870 of the 0x874-byte object: everything except
 * pActive at +0x2A4.  Spelled out so the arithmetic is checkable. */
#define BR_CTRLCFG_HEADER_SIZE  (BR_CTRLCFG_MAGIC_SIZE + 4)
#define BR_CTRLCFG_BODY_SIZE    0x870
#define BR_CTRLCFG_FILE_SIZE    (BR_CTRLCFG_HEADER_SIZE + BR_CTRLCFG_BODY_SIZE)

/* Glide 0x10063060 / D3D 0x10069FF0.
 *
 * Returns 1 if every field was read and both header checks passed, 0
 * otherwise -- which is the original's eax exactly, including the fopen
 * failure at 0x10063098.
 *
 * On failure the object is left holding BrCtrlCfgInit's defaults, NOT its
 * previous contents; see BUG 1 above.  The one exception is a failed fopen,
 * which returns before the object is touched at all.
 *
 * On success pActive is NOT updated to match the loaded `active`; see BUG 2.
 */
int32_t BrCtrlCfgReadFile(BrCtrlCfg *pThis, const char *pszPath);

/* The file LAYOUT, as bytes.  This is not a transcription of anything: the
 * game's writer is Glide 0x100634B0 / D3D 0x1006A4A0 and is already ported,
 * as slice4_53.c's BrCfgSave1006A4A0, which stays where it is.  This exists
 * so the format can be expressed once against a typed BrCtrlCfg -- which the
 * writer cannot be handed on a 64-bit host -- and so the reader can be
 * tested against images it did not itself produce.
 *
 * Writes BR_CTRLCFG_FILE_SIZE bytes and returns that count, or -1 if pOut is
 * NULL, pIn is NULL, or cbOut is too small.  pIn->pActive is not consulted.
 */
int BrCtrlCfgFileEncode(unsigned char *pOut, size_t cbOut,
                        const BrCtrlCfg *pIn);

#ifdef __cplusplus
}
#endif

#endif /* BR_CFGFILE_H */
