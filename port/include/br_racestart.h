/* br_racestart.h -- RESPONSIBILITY: the rules of a race.  Glide 0x100628B0
 * (D3D 0x10069840, byte-identical, `config/shared.csv` class `shared`), the
 * function that turns the loading screen into a running race.
 *
 * WHERE IT SITS.  State 3 of the top-level machine (br_boot.h, 0x1001CDD0) is
 * exactly three things:
 *
 *      0x1001CDEC   0x10063970(3, ...)          the fade, four globals
 *      0x1001CDFB   0x1006C990("loading.img",0) br_imgblit.h
 *      0x1001CE03   0x100628B0()                THIS
 *      0x1001CE0D   [0x105CCBBC] = 1            -> state 1 -> state 2
 *
 * So this is the last thing that happens before the game starts running, and
 * it is where the race is actually assembled.  It takes no arguments and
 * returns nothing: everything it does is to globals and through callees.
 *
 * WHAT IT DOES, in order, off the listing.  The interesting part is that the
 * two `while (!load(...))` loops are SYNCHRONOUS -- state 3 does not return
 * until they finish, which is why there is a loading screen to look at.
 *
 *   0x100628B0   [0x106EC760] = [0x10B71A68];  [0x106E9A34] = [0x10B71A6C]
 *   0x100628C8   0x100703A0()
 *   0x100628DC   g_brRaceTick = (g_brCfgGameMode == 4)
 *   0x100628E2   [0x118EEED8] = 0;  [0x105CCB80] = 0
 *   0x100628EE   0x1002DEC3(); 0x1002E334(); 0x1002E2E3()
 *   0x100628FD   0x10062870()                 the ten car records
 *   0x10062902   0x1002E136()
 *   0x10062908   BrCursorPairSet(0)           0x100182F0
 *   0x1006291A   if (mode == 1 || mode == 6)  three 0xFFFF words, +0xF2 FIRST
 *   0x10062950   0x10062850(1)                entrant count, and a null step
 *   0x1006295D   BrGameStepSet(0x10008D60)    again -- redundant
 *   0x10062965   [0x106ED6DC] = 0;  [0x10B71288] = 0
 *   0x10062971   g_brRaceNCar = 2
 *   0x10062980   g_brRaceNEntrant = 1;  g_brCarPhysWeather = 1
 *   0x1006298C   [0x104ABB20] = 0;  [0x104ABB24] = 0
 *   0x1006299C   BrGameStepSet(0x10019A70)    THE RACE STEP GOES IN HERE
 *   0x100629A4   [0x105BC8D8] = 8
 *   0x100629AE   0x1002F6C0(); 0x10008D60();  while (!0x1006A080(0, 1)) ;
 *   0x100629D4   0x1002F6C0(); 0x10008D60();  while (!0x1006A080(2, 1)) ;
 *   0x100629FC   0x10059E00()                 the volume tables
 *   0x10062A01   if (mode == 0) BrSelLookup()
 *                if (mode == 0 || mode == 2) g_brRaceNEntrant = 1
 *   0x10062A1C   g_brCarPhysWeather = g_brCfgChosenWeather
 *                four equipment dwords into *0x10AF2094 -- see below
 *   0x10062A77   four calls to the folded empty function 0x10008D60
 *   0x10062AB5   0x1002E32F()
 *
 * THE FUNCTION-POINTER SLOT IS THE POINT.  br_gamestep.h records that the
 * race is not a phase -- it is whatever 0x106E79F4 points at, and 0x1002E317
 * is the setter.  This function is where 0x10019A70 gets installed, and it
 * installs the NULL step (0x10008D60) twice first, so the slot is briefly
 * cleared and then loaded.  Nothing between the two runs a frame.
 *
 * FOUR OF THE FIVE CAR-EQUIPMENT SLOTS ARE NAMED HERE, which is new.
 * slice5_60.h already knows that something writes five dwords at +0xF8 of
 * the object reached through 0x10ACED34 / Glide 0x10AF2094, and declined to
 * name them (`g_BrCarEquipTarget`, BR_CAR_EQUIP_OFF 0xF8, count 5).  This
 * function writes four of those five from the config globals br_appstart.h
 * already owns, which pins their meaning:
 *
 *      +0xF8   g_brCfgHandlingType     0x1007B320   HandlingType=
 *      +0xFC   g_brCfgTransmission     0x1007B324   TransmissionType=
 *      +0x100  g_brCfgTireType         0x1007B32C   TireType=
 *      +0x104  g_brCfgSuspensionType   0x1007B328   SuspensionType=
 *      +0x108  (not written here -- slice5_60's fifth)
 *
 * Note that the last two are CROSSED: the lower source address (0x1007B328)
 * goes to the higher offset.  That is the original at 0x10062A5B/0x10062A6B
 * and it is not a transcription slip.
 *
 * The same object takes three 0xFFFF words at +0xF0, +0xF2, +0xF4 when the
 * mode is 1 or 6, stored +0xF2 first.  They sit immediately below the
 * equipment block.
 *
 * THIS IS THE SAME OBJECT slice5_60.h reaches through
 * `g_BrCarEquipTarget(index)`, at index 0 -- the original loads the bare
 * pointer with no `0x2B68 * index` term.  Wire pfnEquipRecord and
 * g_BrCarEquipTarget to ONE body; CONVENTIONS.md's aliased-storage rule is
 * about exactly this, and 0x10ACED34 has already collected four descriptions
 * in four headers.  This one deliberately does not coin a fifth name for it.
 *
 * THREE STRUCTURE SIZES, STATED BY THE BINARY ITSELF.  The four tail calls
 * all land on 0x10008D60, the one-byte `ret` -- MSVC folds identical empty
 * functions, so these were four DIFFERENT empty functions at link time, not
 * four calls to one.  Three are printf-shaped and their format strings are
 * still in .data:
 *
 *      0x100B3884  "sizeof(UltraCarHeader)=%d\n"   0x15F88   == 89992
 *      0x100B3870  "sizeof(Vehicle)=%d\n"          0x2B68
 *      0x100B385C  "sizeof(Enemy)=%d\n"            0x80
 *
 * which independently confirms two strides CONVENTIONS.md already carries
 * (the 0x2B68 entity record, and 89992 as the stride of the array at
 * 0x100C12A0) and names the third: 0x80 is br_racestep.h's `BrDriver`, the
 * 0x10AF07F8 array.  The fourth call is not printf-shaped -- its arguments
 * are 0x80025C00, the N64 address .trk file offset 0 maps to, and
 * 0x10B25798, a .bss buffer.  What it was is not established; that it did
 * nothing in the shipped build is.
 *
 * FOUR OF THE CALLEES ARE EMPTY IN THIS BUILD.  0x1002E136, 0x1002E2E3,
 * 0x1002E32F and 0x1002E334 are each five bytes -- `push ebp ; mov ebp,esp ;
 * pop ebp ; ret`.  They are still ops entries here, because their POSITION in
 * the sequence is real information and a later build (or the N64 side) may
 * have bodies for them.  Their being empty is why a NULL hook for them costs
 * nothing, and BrRaceStartSkipped says which were reached unwired anyway.
 *
 * WHY THE CALLEES ARE AN OPS STRUCT.  Same reason br_bootinit.h gives: nine
 * of them are unported and the ported ones sit in modules with large link
 * closures, so calling them directly would drag most of the tree behind a
 * 525-byte function.  Each entry names the address it stands for and, where
 * the function is already ported, which symbol to wire it to.  A NULL entry
 * is SKIPPED and COUNTED; nothing here invents a result.  BrGameStepSet is
 * the one exception -- br_gamestep.o has no undefined symbols at all, so it
 * is called for real.
 */
#ifndef BR_RACESTART_H
#define BR_RACESTART_H

#include <stdint.h>

#include "br_gamestep.h"   /* BrGameStepFn, BrGameStepSet -- 0x1002E317 */

/* ------------------------------------------------------------------ *
 * The literals, so no caller re-types them.
 * ------------------------------------------------------------------ */

/* 0x100628D4.  g_brRaceTick is seeded with `mode == 4`. */
#define BR_RACESTART_TICK_MODE      4

/* 0x1006291A / 0x1006291E.  The two modes that get the 0xFFFF words. */
#define BR_RACESTART_FFFF_MODE_A    1
#define BR_RACESTART_FFFF_MODE_B    6

/* 0x1006292E / 0x1006293B / 0x10062948, in the order the original stores. */
#define BR_RACESTART_OFF_FFFF_1     0xF2
#define BR_RACESTART_OFF_FFFF_2     0xF0
#define BR_RACESTART_OFF_FFFF_3     0xF4

/* The equipment block.  slice5_60.h calls its base BR_CAR_EQUIP_OFF. */
#define BR_RACESTART_OFF_HANDLING     0xF8
#define BR_RACESTART_OFF_TRANSMISSION 0xFC
#define BR_RACESTART_OFF_TIRE         0x100
#define BR_RACESTART_OFF_SUSPENSION   0x104

/* 0x10062971 and 0x1006297B/0x10062986 -- three literals. */
#define BR_RACESTART_NCAR           2
#define BR_RACESTART_NENTRANT       1
#define BR_RACESTART_WEATHER_INIT   1

/* 0x100629A4. */
#define BR_RACESTART_5BC8D8         8

/* 0x1002F6C0's only instruction. */
#define BR_RACESTART_6EECC8_VALUE   0x80096400u

/* The two load phases, 0x100629B8 and 0x100629DE.  Second argument 1 both
 * times; only the first differs. */
#define BR_RACESTART_LOAD_PHASE_A   0
#define BR_RACESTART_LOAD_PHASE_B   2
#define BR_RACESTART_LOAD_ARG2      1

/* The four folded-empty tail calls, 0x10062A77 .. 0x10062AAD.  Argument 1 is
 * the format string's address for the last three. */
#define BR_RACESTART_TRACE_A1_1     0x80025C00u  /* .trk maps here          */
#define BR_RACESTART_TRACE_A2_1     0x10B25798u  /* a .bss buffer           */
#define BR_RACESTART_TRACE_A1_2     0x100B3884u  /* "sizeof(UltraCarHeader)"*/
#define BR_RACESTART_TRACE_A2_2     0x15F88u     /* 89992                   */
#define BR_RACESTART_TRACE_A1_3     0x100B3870u  /* "sizeof(Vehicle)"       */
#define BR_RACESTART_TRACE_A2_3     0x2B68u
#define BR_RACESTART_TRACE_A1_4     0x100B385Cu  /* "sizeof(Enemy)"         */
#define BR_RACESTART_TRACE_A2_4     0x80u

/* ------------------------------------------------------------------ *
 * The globals nothing else in this port names.  Every one was grepped
 * across port/ before being given storage here (CONVENTIONS.md, "Aliased
 * storage"); none had an owner.  Names are POSITIONAL because their meaning
 * is not established -- ARCHITECTURE.md's warning about naming from
 * inference applies, and a positional name is honest about that.
 * ------------------------------------------------------------------ */
extern int32_t g_brRace6EC760;   /* 0x106EC760 <- 0x10B71A68 */
extern int32_t g_brRace6E9A34;   /* 0x106E9A34 <- 0x10B71A6C */
extern int32_t g_brRaceB71A68;   /* 0x10B71A68, the source   */
extern int32_t g_brRaceB71A6C;   /* 0x10B71A6C, the source   */
extern int32_t g_brRace18EEED8;  /* 0x118EEED8 = 0           */
extern int32_t g_brRace5CCB80;   /* 0x105CCB80 = 0           */
extern int32_t g_brRace6ED6DC;   /* 0x106ED6DC = 0           */
extern int32_t g_brRaceB71288;   /* 0x10B71288 = 0           */
extern int32_t g_brRace4ABB20;   /* 0x104ABB20 = 0           */
extern int32_t g_brRace4ABB24;   /* 0x104ABB24 = 0           */
extern int32_t g_brRace5BC8D8;   /* 0x105BC8D8 = 8           */

/* 0x106EECC8, written only by 0x1002F6C0.  0x80096400 is an N64 KSEG0
 * address; what it addresses is NOT established here. */
extern uint32_t g_brRace6EECC8;

/* ------------------------------------------------------------------ *
 * Two callees small enough to transcribe rather than hook.
 * ------------------------------------------------------------------ */

/* Glide 0x1002F6C0, eleven bytes: one store and a `ret`. */
void BrRaceSub1002F6C0(void);

/* Glide 0x10062850, twenty-three bytes:
 *      g_brRaceNEntrant = n;  BrGameStepSet(pfnNullStep);
 * The step it installs is the folded empty function 0x10008D60, which
 * br_gamestep.h already models as BR_GAMESTEP_NULL.  It is a parameter
 * because a host cannot store an original address in a function pointer. */
void BrRaceEntrantCountSet(int32_t n, BrGameStepFn pfnNullStep);

/* ------------------------------------------------------------------ *
 * The frontier.
 * ------------------------------------------------------------------ */
typedef enum BrRaceStartStep {
    BR_RACESTART_100703A0 = 0,  /* 0x100703A0, 37 B, clean target          */
    BR_RACESTART_1002DEC3,      /* 0x1002DEC3, 627 B, clean target         */
    BR_RACESTART_1002E334,      /* 0x1002E334, EMPTY in this build         */
    BR_RACESTART_1002E2E3,      /* 0x1002E2E3, EMPTY in this build         */
    BR_RACESTART_10062870,      /* 0x10062870, the ten car records         */
    BR_RACESTART_1002E136,      /* 0x1002E136, EMPTY in this build         */
    BR_RACESTART_CURSORPAIR,    /* 0x100182F0 == slice1_05.c BrCursorPairSet
                                 * (D3D 0x1002B280).  ALREADY PORTED.      */
    BR_RACESTART_EQUIPRECORD,   /* *0x10AF2094 -- wire to the same body as
                                 * slice5_60.h's g_BrCarEquipTarget        */
    BR_RACESTART_LOADSTEP,      /* 0x1006A080, 641 B, returns "done"       */
    BR_RACESTART_10059E00,      /* 0x10059E00 == slice6_76.c BrSub10060D90
                                 * (D3D 0x10060D90).  ALREADY PORTED.      */
    BR_RACESTART_SELLOOKUP,     /* 0x1001C9D0 == slice1_05.c BrSelLookup
                                 * (D3D 0x1002F460).  ALREADY PORTED.      */
    BR_RACESTART_TRACE,         /* 0x10008D60, the folded empty function   */
    BR_RACESTART_1002E32F,      /* 0x1002E32F, EMPTY in this build         */
    BR_RACESTART_NSTEPS
} BrRaceStartStep;

typedef struct BrRaceStartOps {
    void    (*pfn100703A0)(void *pUser);
    void    (*pfn1002DEC3)(void *pUser);
    void    (*pfn1002E334)(void *pUser);
    void    (*pfn1002E2E3)(void *pUser);
    void    (*pfn10062870)(void *pUser);
    void    (*pfn1002E136)(void *pUser);

    void    (*pfnCursorPairSet)(void *pUser, int32_t v);  /* wire 0x100182F0 */

    /* The 0x200-byte record *0x10AF2094 points at, as raw bytes.  Raw
     * because it is written to disc verbatim (br_save.h, payload at file
     * +0x008), so its byte layout is externally visible and CONVENTIONS.md
     * forbids overlaying a struct on it. NULL means "not loaded yet", which
     * is a state slice2_24.h says the original genuinely has -- the stores
     * are then skipped and counted rather than aimed at address zero. */
    unsigned char *(*pfnEquipRecord)(void *pUser);

    /* 0x1006A080.  Returns non-zero when that phase has finished; the
     * original spins on it.  A hook that never returns non-zero would hang
     * exactly as the original would, so the port bounds the spin -- see
     * BR_RACESTART_LOAD_SPINS. */
    int32_t (*pfnLoadStep)(void *pUser, int32_t phase, int32_t a2);

    void    (*pfn10059E00)(void *pUser);   /* wire BrSub10060D90 */
    void    (*pfnSelLookup)(void *pUser);  /* wire BrSelLookup   */

    /* 0x10008D60 with two 32-bit arguments, four times. */
    void    (*pfnTrace)(void *pUser, uint32_t a1, uint32_t a2);

    void    (*pfn1002E32F)(void *pUser);

    void     *pUser;
} BrRaceStartOps;

/* DEVIATION: the original's `while (!load(...)) ;` has no bound.  A NULL
 * hook, or one that never reports done, would spin for ever inside a test.
 * The port gives up after this many iterations and counts it
 * (BrRaceStartSpun).  With a hook that answers, nothing changes. */
#define BR_RACESTART_LOAD_SPINS  1000

/* ------------------------------------------------------------------ *
 * Glide 0x100628B0 / D3D 0x10069840.
 *
 * `pfnRaceStep` is the body standing for 0x10019A70 -- br_racestep.c's
 * BrRaceStepInit -- and `pfnNullStep` the one standing for 0x10008D60.
 * They are parameters rather than direct calls for the reason br_gamestep.h
 * gives: the slot is a host function pointer and the original's addresses
 * cannot be stored in it.  Register them with BrGameStepRegister if a test
 * wants BrGameStepId to name what ended up installed.
 * ------------------------------------------------------------------ */
void BrRaceStart(const BrRaceStartOps *pOps,
                 BrGameStepFn pfnRaceStep, BrGameStepFn pfnNullStep);

int32_t BrRaceStartSkipped(BrRaceStartStep step);
int32_t BrRaceStartSpun(void);          /* how many spins hit the bound */
void    BrRaceStartResetForTest(void);

#endif /* BR_RACESTART_H */
