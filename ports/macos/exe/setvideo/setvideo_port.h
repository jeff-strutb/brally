/* Declarations for the ported half of SetVideo.exe.
 *
 * The structs and prototypes below mirror the ones the matched translation
 * units under src/exe/setvideo/ declare for themselves; this header exists
 * for the port driver, not for them. NOT byte-matched.
 */
#ifndef BR_PORT_SETVIDEO_H
#define BR_PORT_SETVIDEO_H

#include <stdio.h>

typedef struct ObjList {
    int    n;
    int   *rgi;
    char **rgsz;
} ObjList;

typedef struct INI {
    ObjList *list;
    int      index;
} INI;

typedef struct Section {
    INI *pini;
    int  index;
} Section;

typedef struct Sel {
    int saved;
    int index;
    int method;
} Sel;

typedef struct CHKFile {
    FILE *fp;
    char *name;
} CHKFile;

/* --- byte-matched functions, compiled from src/exe/setvideo/ ------------ */

void      FreeObjList(ObjList *p);                          /* 0x00401000 */
CHKFile  *CHK_FReadOpen(char *path);                        /* 0x00401050 */
char     *CHK_FGets(char *buf, int n, CHKFile *p);          /* 0x00401150 */
CHKFile  *CHK_FWriteOpen(char *path, char *mode);           /* 0x00401230 */
void      CHK_FPutS(char *s, CHKFile *p);                   /* 0x00401310 */
void      CHK_FClose(CHKFile *p);                           /* 0x00401370 */
int       CHK_FileExists(char *path);                       /* 0x00401400 */
void     *CHK_AllocateMemory(unsigned int size, char *what);/* 0x00401470 */
Section  *SetSubstituteDir(INI *pini, char *name);          /* 0x004014D0 */
Section  *FindFirstSection(INI *pini);                      /* 0x00401560 */
Section  *FindNextSection(Section *p);                      /* 0x004015B0 */
void      CHK_FreeMemory(void *p);                          /* 0x00401600 */
int       CountSections(INI *pini);                         /* 0x00401610 */
char     *GetObj(Section *p);                               /* 0x00401650 */
INI      *BindSection(Section *p);                          /* 0x00401680 */
char     *NextObj(INI *p);                                  /* 0x004016A0 */
INI      *ReadINI(char *path);                              /* 0x004016D0 */
void      FreeINI(INI *p);                                  /* 0x00401720 */
ObjList  *ReadList(char *path);                             /* 0x00401740 */
void      ResetIncludeStack(void);                          /* 0x00401910 */
FILE     *ReadListLine(char *buf, int n, FILE *fp);         /* 0x00401920 */
int       IncludeStackEmpty(void);                          /* 0x00401AC0 */
void      PushInclude(void *f);                             /* 0x00401AD0 */
void     *PopInclude(void);                                 /* 0x00401AF0 */
char      GetCommentChar(void);                             /* 0x00401B10 */
void      SetCommentChar(char c);                           /* 0x00401B20 */
Section  *FollowUse(INI *pini, char *name);                 /* 0x00402360 */
char     *GetIniValue(Section *psec, char *key);            /* 0x004023B0 */
char     *GetSectionNameByIndex(int idx, INI *pini);        /* 0x00402CE0 */

/* --- globals the matched code links against ---------------------------- */

extern int   gChkVerbose;
extern char  gCommentChar;
extern int   gIncludeDepth;
extern void *gIncludeStack[];
extern char  gLineBuf[];

extern char  s_rt[];
extern char  s_ReadListOpenErr[];
extern char  s_PRJ[];
extern char  s_rgiObj[];
extern char  s_rgszObj[];
extern char  s_szObji[];

int fputs_fp(FILE *fp, char *s);

/* --- port-side state (WinMain's globals) -------------------------------- */

extern char  gInstallDir[];
extern char  gIniPath[];
extern INI  *gINI;
extern int   gSectionCount;
extern Sel   gSel;
extern int   gD3DAlphaCompare;
extern int   gD3DDrawCarShadow;
extern int   gD3DInvSrcAlpha;
extern int   gD3DClearZBuffer;

#endif /* BR_PORT_SETVIDEO_H */
