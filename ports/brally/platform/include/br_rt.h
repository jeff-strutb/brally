/* br_rt.h: the C runtime extras the game called (the MSVC runtime's), as
 * the port's own (platform/common/crt.c). */
#ifndef BR_RT_H
#define BR_RT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* The MSVC runtime's extras the game called, as the port's own: string case
 * and number formatting, and the DOS drive model (drives, the current
 * directory, wildcard listings) the platform maps onto the host's folders
 * (platform/common/crt.c). */
/* a 16-byte identifier (the game's DirectX-era GUIDs, kept as data) */
typedef struct BrGuid { uint32_t Data1; uint16_t Data2, Data3; uint8_t Data4[8]; } BrGuid;

#define BR_FIND_DIR   0x10        /* attrib: a folder */
#define BR_FIND_ARCH  0x20        /* attrib: a file */
typedef struct BrFindData {
    unsigned  attrib;               /* 0x10 a folder */
    int64_t   time_create;
    int64_t   time_access;
    int64_t   time_write;
    uint32_t  size;
    char      name[260];
} BrFindData;
intptr_t br_findfirst(const char *spec, BrFindData *fd);
int      br_findnext(intptr_t h, BrFindData *fd);
int      br_findclose(intptr_t h);
char    *br_itoa(int v, char *out, int radix);
char    *br_ltoa(long v, char *out, int radix);
char    *br_strupr(char *s);
char    *br_strlwr(char *s);
int      br_stricmp(const char *a, const char *b);
int      br_strnicmp(const char *a, const char *b, size_t n);
int      br_chdir(const char *path);
char    *br_getcwd(char *buf, int n);
int      br_getdrive(void);
int      br_chdrive(int drive);

#ifdef __cplusplus
}
#endif
#endif
