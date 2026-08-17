/* br_volume.h -- what the game's CD-ROM scan is answered WITH on this host.
 *
 * ARCHITECTURAL CONCERN: platform / asset provenance.
 *
 * THIS IS A HOST SUBSTITUTE, NOT A TRANSCRIPTION, AND IT CARRIES NO
 * @implements. Nothing in br_volume.c is a rendering of the bytes at BRGlide
 * 0x1003EE90 / 0x100377A0 (D3D 0x10045A00 / 0x1003E100). Attaching either
 * address to these symbols would be a false claim -- the enumeration below
 * walks one directory, not twenty-four drive letters, and there is no
 * _chdrive, no GetDriveTypeA and no GetVolumeInformationA anywhere in it. What
 * IS carried over is stated line by line at the bottom of this comment.
 *
 * WHAT THE ORIGINAL DOES
 *
 * BRGlide 0x1003EE90 saves the current drive and working directory, then walks
 * drive numbers 3..26 ('C'..'Z'), handing each to 0x100377A0, and returns 1 the
 * moment one of them answers and 0 if none does. It restores the drive and cwd
 * on both paths.
 *
 *   0x1003EE9A   the result slot is initialised to 0
 *   0x1003EED1   esi = 3, the first drive number
 *   0x1003EF04   cmp esi, 0x1A / jle -- so 26 ('Z') IS tested
 *   0x1003EF0F   found -> 1
 *
 * 0x100377A0 is the per-drive test, and all five parts must hold:
 *
 *   0x100377BD   sprintf(root, "%C:\\", drive + 0x40)   fmt at 0x100ACAD0
 *   0x100377C7   _chdrive(drive)              == 0
 *   0x100377D9   GetDriveTypeA(NULL)          == 5      (DRIVE_CDROM)
 *   0x100377E9   _chdir("\\")                 == 0      literal at 0x100ACACC
 *   0x1003780D   GetVolumeInformationA(root, vol, 0x104, 0,0,0,0,0) != 0
 *   0x10037817   strcmp(vol, "Boss Rally")    == 0      literal at 0x1007B384
 *
 * The comparison at 0x10037823..0x10037852 is an inlined strcmp: byte for
 * byte, stopping at the NUL, and only an exact match reaches the `mov eax, 1`
 * at 0x10037854. It is CASE-SENSITIVE and it is not a prefix or substring
 * test. The bytes at 0x1007B384 are `Boss Rally\0`, read out of the image.
 *
 * WHY THAT CANNOT BE ASKED HERE, AND WHY THE ANSWER IS STILL REAL
 *
 * This decomp ships code only. The retail content stays with whoever owns a
 * copy: tools/extract_assets.sh pulls it out of the builder's own bin/cue into
 * testdata/, and none of it is committed. So at runtime there is never a
 * physical CD in a drive -- but the disc's contents ARE present, as files.
 *
 * The scan was previously stubbed to 0 in port/host/br_stubs.c, which made
 * BrPhaseActivate_10045900 report status 0xD and refuse to open Championship
 * even on a machine where every asset had been extracted successfully. That is
 * the original's correct behaviour for a bare machine and the WRONG answer
 * here, because here the disc's contents are on the disk.
 *
 * Nothing needs faking, because the disc supplies the exact bytes the strcmp
 * wants. The retail image's ISO 9660 volume identifier is the literal string
 * `Boss Rally` -- in the primary descriptor at lba 16 and again in the Joliet
 * supplementary descriptor at lba 18. tools/extract_iso.py reads that field and
 * tools/extract_assets.sh records it in testdata/assets.manifest.json alongside
 * the disc's fingerprint and the list of files extracted. This service reports
 * the extracted asset root as a volume carrying that RECORDED label, and the
 * comparison is then the game's own.
 *
 * THE MANIFEST IS THE PROVENANCE, AND THE FILE LIST IS PART OF IT
 *
 * A tree with nothing extracted must still refuse, and does: no manifest means
 * no volume. A manifest sitting alone in an otherwise empty tree also refuses,
 * because it is required to name at least one extracted file that is actually
 * present. That second rule is not paranoia -- port/src/audio/br_wireaudio.c
 * records what happened when a development pass dropped two synthesised FLACs
 * where real music should be and every log line thereafter read "2 track(s)".
 * The lesson generalises: a manifest that vouches for nothing must not vouch.
 *
 * WHAT IS TRANSCRIBED AND WHAT IS HOST
 *
 *   transcribed -- BR_VOLUME_WANT, the literal at 0x1007B384.
 *   transcribed -- BR_VOLUME_LABEL_MAX, the 0x104 pushed at 0x10037802.
 *   transcribed -- the comparison: strcmp == 0, case-sensitive, whole string.
 *   transcribed -- the shape of the answer: 1 as soon as one volume matches,
 *                  0 if none does. (0x1003EF0F against 0x1003EE9A.)
 *   HOST        -- the enumeration. Drives 3..26 become one asset root.
 *   HOST        -- the drive-type test. There is no drive to type.
 *   HOST        -- where a label comes from: a recorded field, not a live
 *                  GetVolumeInformationA call.
 */
#ifndef BR_VOLUME_H
#define BR_VOLUME_H

#include <stdint.h>

/* The buffer GetVolumeInformationA is given at 0x10037802 is 0x104 bytes. A
 * recorded label longer than that is truncated exactly as the original's would
 * have been, and a truncated label does not compare equal to "Boss Rally". */
#define BR_VOLUME_LABEL_MAX   0x104

/* 0x1007B384. Do not relax this to a substring or a case-insensitive match:
 * 0x10037823 compares bytes. */
#define BR_VOLUME_WANT        "Boss Rally"

/* Written by tools/extract_assets.sh, beside the assets it vouches for --
 * the same placement tools/extract_cdaudio.py uses for cdaudio.manifest.json. */
#define BR_VOLUME_MANIFEST    "assets.manifest.json"

/* The extracted asset root. testdata/ is what the rest of the host already
 * reads (br_wireaudio.h's testdata/sfx/, brally.c's testdata/images/). */
#define BR_VOLUME_ROOT_DEFAULT "testdata"
#define BR_VOLUME_ROOT_ENV     "BR_ASSET_DIR"

/* At most one volume exists today -- there is one asset root. The array shape
 * is kept because the ORIGINAL enumerates, and a caller that walks the list is
 * therefore writing against the same contract the game does. */
#define BR_VOLUME_MAX 4

/* Override the asset root. NULL restores $BR_ASSET_DIR, or the default. */
void        BrVolumeSetRoot(const char *pszRoot);
const char *BrVolumeRoot(void);

/* The enumeration. Both re-read the manifest on every call: the original walks
 * every drive on every call too, and a cache here would report a stale answer
 * to a run that extracted its assets while it was up. */
int         BrVolumeCount(void);

/* The label of volume `i`, or NULL if `i` is out of range. Never NULL for an
 * index below BrVolumeCount(). */
const char *BrVolumeLabel(int i);

/* Non-zero iff some volume's label compares EQUAL to pszLabel -- strcmp, the
 * game's test. */
int         BrVolumePresent(const char *pszLabel);

/* One line of plain English saying why the last enumeration found what it
 * found. For host reports; never a decision input. */
const char *BrVolumeWhy(void);

/* The game's own entry point, declared by slice3_31.h as well. Defined in
 * br_volume.c: non-zero iff a volume labelled BR_VOLUME_WANT is present. */
int32_t     BrExt_10045A00(void);

#endif /* BR_VOLUME_H */
