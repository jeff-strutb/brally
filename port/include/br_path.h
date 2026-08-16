/* br_path.h -- 0x10008B90. See br_path.c for why it is not in slice6_78. */
#ifndef BR_PATH_H
#define BR_PATH_H

/* 0x10008B90 -- copy the basename of `pszSrc` into `pszDst`.
 *
 * `pStream` is the original's __thiscall `this`. Both call sites set ecx and
 * the body never reads it; kept so the signature matches slice2_12.h's
 * declaration rather than quietly disagreeing with it.
 *
 * GOTCHA, preserved: the LAST character is never examined as a separator, so a
 * trailing backslash is kept -- "dir\\" yields "dir\\", not "". strrchr would
 * differ on exactly that input.
 *
 * DEVIATION: on an empty source the original scans BACKWARDS from pszSrc-1
 * through unrelated memory until it meets a 0x5C byte. This copies the empty
 * string instead. That cannot be reproduced safely and is not worth trying. */
void BrPodWriterMakeName(void *pStream, const char *pszSrc, char *pszDst);

#endif /* BR_PATH_H */
