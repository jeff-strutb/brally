/* br_trace.c -- DIAGNOSTIC execution tracer (not part of the match).
 *
 * BrDiagTrace(va) appends the caller function's VA to brally.log.  __stdcall,
 * so the 19-byte trace stub `pushad; pushfd; push <VA>; call BrDiagTrace;
 * popfd; popad; jmp <annex body>` leaves the stack exactly as a direct call
 * (callee pops its arg) and is register/flag-transparent -- the trace is
 * invisible to the real body.
 *
 * The image builder places these bodies in the .t3x annex and rewrites each
 * traced function's original slot to that stub.  CRT calls are declared
 * dllimport so they become indirect `call [__imp__X]` (resolved to the IAT
 * slot); every string is built on the stack CHARACTER BY CHARACTER (a string
 * literal would land in .rdata, for which the fixed image has no room and
 * which the .text-only annex cannot carry); fclose flushes each line so a
 * crash keeps the trace.
 *
 * Each trace function ALSO installs a top-level unhandled-exception filter
 * (BrGlCrashFilter) on every call.  SetUnhandledExceptionFilter is not in
 * BRGlide's import table, so it is resolved at run time through the imported
 * GetModuleHandleA/GetProcAddress.  When a later page fault goes unhandled the
 * OS calls BrGlCrashFilter, which dumps the faulting EIP + registers to
 * brally.log and terminates -- so a crash self-reports its EXACT address even
 * when 86box swallows the Windows fault dialog.  Re-installing on every trace
 * call is idempotent and needs no guard variable (which the .text-only annex
 * could not hold).  The install block is a macro so it inlines identically
 * into both sinks; the ONLY cross-symbol reference the annex must resolve is
 * the DIR32 &BrGlCrashFilter, which the image builder patches from the annex.
 */
__declspec(dllimport) void *__cdecl   fopen(const char *, const char *);
__declspec(dllimport) int   __cdecl   fprintf(void *, const char *, ...);
__declspec(dllimport) int   __cdecl   fclose(void *);
__declspec(dllimport) unsigned __cdecl fwrite(const void *, unsigned, unsigned, void *);
__declspec(dllimport) void *__stdcall GetModuleHandleA(const char *);
__declspec(dllimport) void *__stdcall GetProcAddress(void *, const char *);

typedef long(__stdcall *BR_FILTER)(void *);
typedef BR_FILTER(__stdcall *BR_SETFILTER)(BR_FILTER);

/* Top-level exception filter.  pExc is EXCEPTION_POINTERS*:
 *   +0x00 EXCEPTION_RECORD*   +0x04 CONTEXT*
 * EXCEPTION_RECORD: +0x00 code, +0x0C address, +0x18 info[1] (AV target).
 * CONTEXT (x86): Edi 0x9C Esi 0xA0 Ebx 0xA4 Edx 0xA8 Ecx 0xAC Eax 0xB0
 *                Ebp 0xB4 Eip 0xB8 Esp 0xC4.
 * Logs "C <code> <eip> <faultaddr> <eax ebx ecx edx esi edi ebp esp>" and
 * returns EXCEPTION_EXECUTE_HANDLER (1): log, then let the process die. */
long __stdcall BrGlCrashFilter(void *pExc)
{
    unsigned int *rec = *(unsigned int **)((char *)pExc + 0);
    unsigned int *ctx = *(unsigned int **)((char *)pExc + 4);
    char  nm[12];
    char  md[2];
    char  fmt[48];
    void *fp;
    int   p;
    int   j;

    nm[0]='b'; nm[1]='r'; nm[2]='a'; nm[3]='l'; nm[4]='l'; nm[5]='y';
    nm[6]='.'; nm[7]='l'; nm[8]='o'; nm[9]='g'; nm[10]=0;
    md[0]='a'; md[1]=0;
    /* build "C %x %x %x %x %x %x %x %x %x %x %x %x\n" with no literal */
    p = 0;
    fmt[p++] = 'C';
    for (j = 0; j < 12; j++) { fmt[p++]=' '; fmt[p++]='%'; fmt[p++]='x'; }
    fmt[p++] = '\n';
    fmt[p]   = 0;

    fp = fopen(nm, md);
    if (fp != (void *)0) {
        char sfmt[40];
        unsigned int g_carcount = *(volatile unsigned int *)0x100b2f04u;
        unsigned int g_nentrant = *(volatile unsigned int *)0x100b3858u;
        unsigned int g_226a4c   = *(volatile unsigned int *)0x10226a4cu;
        unsigned int g_mode     = *(volatile unsigned int *)0x100a9360u;
        unsigned int g_rec1208  = *(volatile unsigned int *)0x10af3bccu; /* ent 0x10af1208 + 0x29c4 */
        int q;
        fprintf(fp, fmt,
                rec[0], rec[3], rec[6],
                ctx[0xB8 / 4],
                ctx[0xB0 / 4], ctx[0xA4 / 4], ctx[0xAC / 4], ctx[0xA8 / 4],
                ctx[0xA0 / 4], ctx[0x9C / 4], ctx[0xB4 / 4], ctx[0xC4 / 4]);
        /* second line: race-state globals -- "S carcount nentrant 226a4c mode rec\n" */
        q = 0;
        sfmt[q++] = 'S';
        { int w; for (w = 0; w < 5; w++) { sfmt[q++]=' '; sfmt[q++]='%'; sfmt[q++]='x'; } }
        sfmt[q++] = '\n'; sfmt[q] = 0;
        fprintf(fp, sfmt, g_carcount, g_nentrant, g_226a4c, g_mode, g_rec1208);
        {   /* "D <guard> <rec0.model> <rec1.model> <recsrc> <ndriver>" --
             * why car 0's model is null: guard skips BrCarSlotSetup, or the
             * model loaded into a different record slot (stride 0x15f88). */
            char dfmt[24];
            unsigned int guard   = *(volatile unsigned int *)0x10af2090u; /* car0 [esi-0x80] */
            unsigned int rec0mdl = *(volatile unsigned int *)0x100c4de4u; /* rec[0]+0x8014 */
            unsigned int rec1mdl = *(volatile unsigned int *)0x100dad6cu; /* rec[1]+0x8014 */
            unsigned int recsrc  = *(volatile unsigned int *)0x10af1348u; /* ent+0x140 */
            unsigned int ndriver = *(volatile unsigned int *)0x100b2f00u;
            int w2, q2 = 0;
            dfmt[q2++]='D';
            for (w2 = 0; w2 < 5; w2++) { dfmt[q2++]=' '; dfmt[q2++]='%'; dfmt[q2++]='x'; }
            dfmt[q2++]='\n'; dfmt[q2]=0;
            fprintf(fp, dfmt, guard, rec0mdl, rec1mdl, recsrc, ndriver);
        }
        /* third line: raw stack dump "K <esp+0> <esp+4> ..." (32 dwords).
         * Reading up the stack from esp never faults (esp is valid), unlike an
         * ebp-chain walk; the return addresses are the .text-range values.
         * Kept BEFORE fclose so the whole record flushes together. */
        {
            char sp[4];
            unsigned int esp = ctx[0xC4 / 4];
            int i;
            sp[0]=' '; sp[1]='%'; sp[2]='x'; sp[3]=0;
            { char kk[2]; kk[0]='K'; kk[1]=0; fprintf(fp, kk); }
            for (i = 0; i < 32; i++)
                fprintf(fp, sp, *(volatile unsigned int *)(esp + i * 4));
            { char nl[2]; nl[0]='\n'; nl[1]=0; fprintf(fp, nl); }
        }
        fclose(fp);
        /* fclose only reaches Win98's VCACHE; the process is about to die, so
         * force the file to the (emulated) disk with FlushFileBuffers, else the
         * final C/S line is lost on an immediate crash.  CreateFileA/
         * FlushFileBuffers/CloseHandle are not imported -> resolve at run time. */
        {
            typedef void *(__stdcall *PCREATE)(const char *, unsigned, unsigned,
                                               void *, unsigned, unsigned, void *);
            typedef int (__stdcall *PFLUSH)(void *);
            typedef int (__stdcall *PCLOSE)(void *);
            char k32[16], cf[12], ff[20], ch[12];
            void *hk, *h;
            PCREATE pcreate; PFLUSH pflush; PCLOSE pclose;
            k32[0]='K';k32[1]='E';k32[2]='R';k32[3]='N';k32[4]='E';k32[5]='L';
            k32[6]='3';k32[7]='2';k32[8]='.';k32[9]='d';k32[10]='l';k32[11]='l';k32[12]=0;
            cf[0]='C';cf[1]='r';cf[2]='e';cf[3]='a';cf[4]='t';cf[5]='e';
            cf[6]='F';cf[7]='i';cf[8]='l';cf[9]='e';cf[10]='A';cf[11]=0;
            ff[0]='F';ff[1]='l';ff[2]='u';ff[3]='s';ff[4]='h';ff[5]='F';ff[6]='i';
            ff[7]='l';ff[8]='e';ff[9]='B';ff[10]='u';ff[11]='f';ff[12]='f';
            ff[13]='e';ff[14]='r';ff[15]='s';ff[16]=0;
            ch[0]='C';ch[1]='l';ch[2]='o';ch[3]='s';ch[4]='e';ch[5]='H';
            ch[6]='a';ch[7]='n';ch[8]='d';ch[9]='l';ch[10]='e';ch[11]=0;
            hk = GetModuleHandleA(k32);
            if (hk != (void *)0) {
                pcreate = (PCREATE)GetProcAddress(hk, cf);
                pflush  = (PFLUSH)GetProcAddress(hk, ff);
                pclose  = (PCLOSE)GetProcAddress(hk, ch);
                if (pcreate && pflush && pclose) {
                    /* GENERIC_WRITE, share R|W, OPEN_EXISTING */
                    h = pcreate(nm, 0x40000000u, 3u, (void *)0, 3u, 0u, (void *)0);
                    if (h != (void *)0 && h != (void *)0xffffffffu) {
                        pflush(h);
                        pclose(h);
                    }
                }
            }
        }
    }
    /* Crash-time memory snapshot to memsnap.bin: the per-slot views/cars are
     * populated only DURING the frame, so capturing here (at the fault) records
     * the state that actually produced the crash.  Windows: [u32 base][u32 len]
     * [bytes].  Force to disk with CreateFileA+FlushFileBuffers (process dying). */
    {
        unsigned sbase[4], slen[4];
        char  snm[12], smd[3];
        void *sfp;
        int   si;
        sbase[0]=0x1007b000u; slen[0]=0x00045000u;
        sbase[1]=0x10470000u; slen[1]=0x00050000u;   /* active-slot cars+views */
        sbase[2]=0x105a0000u; slen[2]=0x00160000u;
        sbase[3]=0x10a80000u; slen[3]=0x000b0000u;
        snm[0]='m'; snm[1]='e'; snm[2]='m'; snm[3]='s'; snm[4]='n'; snm[5]='a';
        snm[6]='p'; snm[7]='.'; snm[8]='b'; snm[9]='i'; snm[10]='n'; snm[11]=0;
        smd[0]='w'; smd[1]='b'; smd[2]=0;
        sfp = fopen(snm, smd);
        if (sfp != (void *)0) {
            for (si = 0; si < 4; si++) {
                fwrite(&sbase[si], 4, 1, sfp);
                fwrite(&slen[si], 4, 1, sfp);
                fwrite((const void *)sbase[si], 1, slen[si], sfp);
            }
            fclose(sfp);
            {
                typedef void *(__stdcall *PCREATE)(const char *, unsigned,
                    unsigned, void *, unsigned, unsigned, void *);
                typedef int (__stdcall *PFLUSH)(void *);
                typedef int (__stdcall *PCLOSE)(void *);
                char k32[13], cf[12], ff[17], ch[12];
                void *hk, *h;
                PCREATE pcreate; PFLUSH pflush; PCLOSE pclose;
                k32[0]='K';k32[1]='E';k32[2]='R';k32[3]='N';k32[4]='E';k32[5]='L';
                k32[6]='3';k32[7]='2';k32[8]='.';k32[9]='d';k32[10]='l';k32[11]='l';k32[12]=0;
                cf[0]='C';cf[1]='r';cf[2]='e';cf[3]='a';cf[4]='t';cf[5]='e';
                cf[6]='F';cf[7]='i';cf[8]='l';cf[9]='e';cf[10]='A';cf[11]=0;
                ff[0]='F';ff[1]='l';ff[2]='u';ff[3]='s';ff[4]='h';ff[5]='F';ff[6]='i';
                ff[7]='l';ff[8]='e';ff[9]='B';ff[10]='u';ff[11]='f';ff[12]='f';
                ff[13]='e';ff[14]='r';ff[15]='s';ff[16]=0;
                ch[0]='C';ch[1]='l';ch[2]='o';ch[3]='s';ch[4]='e';ch[5]='H';
                ch[6]='a';ch[7]='n';ch[8]='d';ch[9]='l';ch[10]='e';ch[11]=0;
                hk = GetModuleHandleA(k32);
                if (hk != (void *)0) {
                    pcreate = (PCREATE)GetProcAddress(hk, cf);
                    pflush  = (PFLUSH)GetProcAddress(hk, ff);
                    pclose  = (PCLOSE)GetProcAddress(hk, ch);
                    if (pcreate && pflush && pclose) {
                        h = pcreate(snm, 0x40000000u, 3u, (void *)0, 3u, 0u, (void *)0);
                        if (h != (void *)0 && h != (void *)0xffffffffu) {
                            pflush(h); pclose(h);
                        }
                    }
                }
            }
        }
    }
    return 1;   /* EXCEPTION_EXECUTE_HANDLER */
}

/* Resolve KERNEL32!SetUnhandledExceptionFilter at run time and install the
 * crash filter.  Idempotent; inlined into every sink.  Strings built on the
 * stack char-by-char (no .rdata). */
#define BR_INSTALL_CRASH_FILTER()                                            \
    do {                                                                     \
        char  _k32[16];                                                      \
        char  _suf[32];                                                      \
        void *_hk;                                                           \
        BR_SETFILTER _setf;                                                  \
        _k32[0]='K'; _k32[1]='E'; _k32[2]='R'; _k32[3]='N'; _k32[4]='E';     \
        _k32[5]='L'; _k32[6]='3'; _k32[7]='2'; _k32[8]='.'; _k32[9]='d';     \
        _k32[10]='l'; _k32[11]='l'; _k32[12]=0;                              \
        _suf[0]='S';  _suf[1]='e';  _suf[2]='t';  _suf[3]='U';  _suf[4]='n'; \
        _suf[5]='h';  _suf[6]='a';  _suf[7]='n';  _suf[8]='d';  _suf[9]='l'; \
        _suf[10]='e'; _suf[11]='d'; _suf[12]='E'; _suf[13]='x';             \
        _suf[14]='c'; _suf[15]='e'; _suf[16]='p'; _suf[17]='t';            \
        _suf[18]='i'; _suf[19]='o'; _suf[20]='n'; _suf[21]='F';            \
        _suf[22]='i'; _suf[23]='l'; _suf[24]='t'; _suf[25]='e';            \
        _suf[26]='r'; _suf[27]=0;                                            \
        _hk = GetModuleHandleA(_k32);                                        \
        if (_hk != (void *)0) {                                              \
            _setf = (BR_SETFILTER)GetProcAddress(_hk, _suf);                 \
            if (_setf != (BR_SETFILTER)0)                                    \
                _setf(BrGlCrashFilter);                                      \
        }                                                                    \
    } while (0)

/* BrDiagTrace(va, frame): frame = the stub's pushad frame.  Layout of pushad
 * (low->high): edi esi ebp esp ebx edx ecx eax, so frame[5]=edx frame[6]=ecx;
 * the traced function's own esp is frame+0x20, so its stack args are
 * frame[9],[10],[11],[12].  Logging ecx/edx captures __fastcall/__thiscall
 * args; the stack slots capture __cdecl/__stdcall args.  One line per call:
 *   <va> <ecx> <edx> <arg0> <arg1> <arg2> <arg3> */
void __stdcall BrDiagTrace(unsigned int va, unsigned int *frame)
{
    char  nm[12];
    char  md[2];
    char  fmt[32];
    void *fp;
    unsigned int ecx = frame[6];
    unsigned int edx = frame[5];
    unsigned int a0 = frame[9], a1 = frame[10], a2 = frame[11], a3 = frame[12];
    int p, j;

    BR_INSTALL_CRASH_FILTER();

    /* Golden capture (no crash): snapshot at BrFrameDraw entry, but only once
     * DAT_106ed520 (the active camera) is set -- a later frame, after per-slot
     * setup ran -- so the working state matches T3's crash-time snapshot.
     * Guarded by memsnap.bin's existence. */
    if (va == 0x10011fa0u && *(volatile unsigned int *)0x106ed520u != 0u) {
        unsigned sbase[4], slen[4];
        char  snm[12], smd[3];
        void *sfp;
        int   si;
        sbase[0]=0x1007b000u; slen[0]=0x00045000u;
        sbase[1]=0x10470000u; slen[1]=0x00050000u;
        sbase[2]=0x105a0000u; slen[2]=0x00160000u;
        sbase[3]=0x10a80000u; slen[3]=0x000b0000u;
        snm[0]='m'; snm[1]='e'; snm[2]='m'; snm[3]='s'; snm[4]='n'; snm[5]='a';
        snm[6]='p'; snm[7]='.'; snm[8]='b'; snm[9]='i'; snm[10]='n'; snm[11]=0;
        smd[0]='r'; smd[1]='b'; smd[2]=0;
        sfp = fopen(snm, smd);
        if (sfp != (void *)0) { fclose(sfp); }
        else {
            smd[0]='w'; sfp = fopen(snm, smd);
            if (sfp != (void *)0) {
                for (si = 0; si < 4; si++) {
                    fwrite(&sbase[si], 4, 1, sfp);
                    fwrite(&slen[si], 4, 1, sfp);
                    fwrite((const void *)sbase[si], 1, slen[si], sfp);
                }
                fclose(sfp);
            }
        }
    }

    nm[0]='b'; nm[1]='r'; nm[2]='a'; nm[3]='l'; nm[4]='l'; nm[5]='y';
    nm[6]='.'; nm[7]='l'; nm[8]='o'; nm[9]='g'; nm[10]=0;
    md[0]='a'; md[1]=0;
    /* "%x %x %x %x %x %x %x\n" -- 7 fields, no literal */
    p = 0;
    for (j = 0; j < 7; j++) { fmt[p++]='%'; fmt[p++]='x'; fmt[p++]=' '; }
    fmt[p - 1] = '\n'; fmt[p] = 0;

    fp = fopen(nm, md);
    if (fp != (void *)0) {
        fprintf(fp, fmt, va, ecx, edx, a0, a1, a2, a3);
        fclose(fp);
    }

    /* Race-gate probe: on every BrGlNavPoll (0x10059410 -- runs each pre-race
     * frame), dump the state-machine + the two "loading complete" flags whose
     * both-zero gates the pre-race(4)->racing(3) advance at 0x1001CE2E, plus
     * the fixed-timestep tick and the game-mode word.  Absolute reads compile
     * to `mov eax,[imm32]` (no reloc).  One line: G <5ccbbc> <ac5c5c> <abaa0>
     * <10226a44> <a9360>. */
    if (va == 0x10059410u) {
        char gfmt[40];
        void *gp;
        unsigned int st   = *(volatile unsigned int *)0x105ccbbcu;
        unsigned int page = *(volatile unsigned int *)0x10ac5c5cu;   /* g_AC5C5C */
        unsigned int rdy2 = *(volatile unsigned int *)0x100abaa0u;
        unsigned int tick = *(volatile unsigned int *)0x10226a44u;
        /* the stuck phase's vtable ptr + its done flag (+0x68) + parent
         * (g_5C60 0x10ac5c60) + the DIK space edge (0x39) so we can see the
         * class and whether the confirm is registering. Guard the deref. */
        unsigned int vtbl = (page >= 0x10000u) ? *(volatile unsigned int *)page : 0xBAD1u;
        unsigned int done = (page >= 0x10000u) ? *(volatile unsigned int *)(page + 0x68u) : 0xBAD2u;
        unsigned int par  = *(volatile unsigned int *)0x10ac5c60u;
        unsigned int spc  = (unsigned int)(*(volatile unsigned char *)0x118ee9d0u);  /* g_brInKeys? placeholder */
        int gp2 = 0, gj;
        gfmt[gp2++]='G'; gfmt[gp2++]=' ';
        for (gj = 0; gj < 8; gj++) { gfmt[gp2++]='%'; gfmt[gp2++]='x'; gfmt[gp2++]=' '; }
        gfmt[gp2 - 1] = '\n'; gfmt[gp2] = 0;
        gp = fopen(nm, md);
        if (gp != (void *)0) {
            fprintf(gp, gfmt, st, page, rdy2, tick, vtbl, done, par, spc);
            fclose(gp);
        }
    }
}

/* BrDiagTexState(va): like BrDiagTrace but also dumps the texture-table
 * count/capacity/pointer globals (0x10697a58 / 0x10697a5c / 0x106b7aa0) so we
 * can see the table state at br_tex3d_append entry.  Absolute reads compile to
 * `mov eax,[imm32]` (no reloc). */
void __stdcall BrDiagTexState(unsigned int va, unsigned int *frame)
{
    char  nm[12];
    char  md[2];
    char  fmt[16];
    void *fp;
    unsigned int cnt = *(volatile unsigned int *)0x10697a58u;
    unsigned int cap = *(volatile unsigned int *)0x10697a5cu;
    unsigned int ptr = *(volatile unsigned int *)0x106b7aa0u;

    (void)frame;
    BR_INSTALL_CRASH_FILTER();

    nm[0]='b'; nm[1]='r'; nm[2]='a'; nm[3]='l'; nm[4]='l'; nm[5]='y';
    nm[6]='.'; nm[7]='l'; nm[8]='o'; nm[9]='g'; nm[10]=0;
    md[0]='a'; md[1]=0;
    /* "T %x %x %x %x\n" */
    fmt[0]='T'; fmt[1]=' '; fmt[2]='%'; fmt[3]='x'; fmt[4]=' '; fmt[5]='%';
    fmt[6]='x'; fmt[7]=' '; fmt[8]='%'; fmt[9]='x'; fmt[10]=' '; fmt[11]='%';
    fmt[12]='x'; fmt[13]='\n'; fmt[14]=0;

    fp = fopen(nm, md);
    if (fp != (void *)0) {
        fprintf(fp, fmt, va, cnt, cap, ptr);
        fclose(fp);
    }
}
