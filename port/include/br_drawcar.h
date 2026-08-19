/* br_drawcar.h -- build the display list for ONE vehicle.
 *
 * RESPONSIBILITY: drawing/ -- turn geometry and images into pixels.
 *
 * WHAT THESE TWO FUNCTIONS ARE, AND HOW THAT WAS ESTABLISHED
 * ======================================================================
 * Neither has a name, a string, or a direct caller in any named root.  Both
 * were identified from three independent lines of evidence.
 *
 * 1. THEY WRITE F3D COMMANDS.  Both consist mostly of the same eight-byte
 *    idiom repeated -- read the cursor at BRGlide 0x106E7710, store w0 and
 *    w1, bump it by 8.  0x1000A110 does it 142 times, 0x10009C10 does it
 *    ~35 times per pass.  Every w0 decodes as a stock F3D/RDP opcode in
 *    byte 3, which CONVENTIONS.md already establishes is where this build
 *    keeps the opcode (the loader byte-swaps into host order):
 *
 *      0x01 G_MTX     0x03 G_MOVEMEM  0x06 G_DL      0xB6 clear geom mode
 *      0xB7 set geom  0xB9 othermode_L 0xBA othermode_H 0xBB G_TEXTURE
 *      0xBC G_MOVEWORD 0xBD G_POPMTX  0xDC texture  0xE6/E7/E8 syncs
 *      0xF0/F2/F5 tile 0xF8 fog colour 0xFA prim colour 0xFB env colour
 *
 * 2. THE ARGUMENT COMES FROM THE FRAME DRIVER.  0x10011FA0 (4500 bytes,
 *    unported) calls 0x1000EAF0 then walks a per-view table at
 *    `view + 0x60`, stride 0x80, `g_0B2F00` entries, and hands each entry
 *    to 0x1000A110 -- twice, once for the entries whose byte at +0x29AF is
 *    NOT 2 and again for the ones where it IS.  0x29AF is therefore a draw
 *    CLASS and the two loops are an opaque pass and a translucent pass.
 *    The pointer it hands over has stride 0x2B68 -- the entity/car record
 *    stride this project already knows -- confirmed independently inside
 *    0x1000EAF0, which clears fields on a `add eax, 0x2b68` loop.
 *
 * 3. THE FIELDS IT TOUCHES ARE A CAR.  +0x00 is a BrMat4 (it is handed
 *    straight to BrMat4Mul), +0x30 is that matrix's translation row and is
 *    used as a POSITION against the camera object's +0x30, +0x40..+0x13F
 *    are FOUR more 0x40-byte matrices which 0x10009C10 walks one per pass,
 *    and +0x140 is the index this project already knows as the player
 *    index (slice3_42.h's BR_S42_CAR_OFF_INDEX).  +0x29B4 and +0x29C0 are
 *    slice1_09.h's BR_ENTITY_OFF_BANK and BR_ENTITY_OFF_AUX at exactly the
 *    offsets that header records.  Four matrices attached to a car, each
 *    drawing the same display list, are the four WHEELS.
 *
 * So:
 *
 *   0x1000A110  draw one vehicle.  Picks a detail level from how far the
 *               camera is, sets fog, lighting, culling and the colour
 *               combiner for it, then emits its body, its glass, its
 *               reflection and its wheels.
 *   0x10009C10  the four wheels of one vehicle.
 *
 * WHAT UNBLOCKED 0x10009C10
 * ======================================================================
 * port/src/slice2_13.c deliberately skipped this function under its D3D
 * address 0x1000C6E0, and said why: "one of them (0x1002F900) with sixteen
 * stack arguments whose meaning is not established anywhere in this slice".
 * It is established now, and by two routes that agree.  0x1002F900 was
 * transcribed as BrRdpSetCombineLERP in slice1_05.c, and reading its Glide
 * twin 0x1001CF90 cold reproduces slice1_05.h's field table bit for bit --
 * a0[23:20] c0[19:15] Aa0[14:12] Ac0[11:9] a1[8:5] c1[4:0] in w0, and the
 * ten-field chain in w1.  The token mappers 0x1001D150/0x1001D180 confirm
 * it from the other end: token 0 maps to 7 in the alpha mux and 31 in the
 * colour mux, which are exactly G_ACMUX_0 and G_CCMUX_0, and token 1 maps
 * to 6 in both, which is G_?CMUX_1.  The game's own tokens 0x3E8+n map to
 * n, so 0x3E9 is TEXEL0, 0x3EC is PRIMITIVE and 0x3ED is SHADE.
 *
 * WHAT IS NOT CLAIMED HERE, AND WHY
 * ======================================================================
 * 0x1000A110 itself is NOT claimed by this module.  It is 7,577 bytes and
 * this pass read all of it -- the structure is recorded in br_drawcar.c
 * under BrCarDrawPlan -- but a partial transcription under a whole-function
 * `@implements` is the exact failure CONVENTIONS.md records ("one claim in
 * this tree covered 2% of an 11 KB function and had to be withdrawn").  The
 * map is the deliverable; the claim is not made.
 *
 * THE RECORD IS A VIEW, NOT AN OVERLAY
 * ======================================================================
 * The original reaches every field by byte offset inside one 0x2B68 record.
 * Two of those fields -- +0x29C0 and +0x29C4 -- are POINTERS four bytes
 * apart, so on LP64 they cannot both sit at their original offsets, and
 * CONVENTIONS.md forbids overlaying a struct on a foreign layout anyway.
 * BrCarView therefore names the fields this code reads and comments each
 * with its original offset.  Every offset below was read out of the
 * disassembly, not inferred from a neighbour.
 */
#ifndef BR_DRAWCAR_H
#define BR_DRAWCAR_H

#include <stdint.h>

#include "br_mat.h"     /* BrMat4, BrMat4Scale (0x1002A7A0 == d3d 0x100310F0) */
#include "br_vec.h"     /* BrVec3 and the 0x1003Axxx cluster                  */

#define BR_CAR_MAX           16       /* BR_CARDATA_CARS; the flag-array bound */

/* Combiner tokens, as the game spells them.  0x1001D180/0x1001D150 map
 * 0x3E8 + n to n, and 0 to G_CCMUX_0 / G_ACMUX_0. */
#define TK_ZERO       0
#define TK_ONE        1        /* G_?CMUX_1; 0x1002FAF0/AC0 map token 1 -> 6 */
#define TK_COMBINED   0x3E8
#define TK_TEXEL0     0x3E9
#define TK_TEXEL1_A   0x3EB
#define TK_PRIMITIVE  0x3EC
#define TK_SHADE      0x3ED

/* --------------------------------------------------------------------
 * The car, as this code reads it.
 * -------------------------------------------------------------------- */
typedef struct BrCarView {
    BrMat4   mtx;            /* +0x0000  world transform; +0x30 is its
                              *          translation row and doubles as the
                              *          car's position                     */
    BrMat4   aWheel[4];      /* +0x0040  one per wheel, 0x40 apart          */
    int32_t  iCar;           /* +0x0140  player/car index                   */
    void    *p0168;          /* +0x0168  four guard pointers -- all four    */
    void    *p016C;          /* +0x016C  must be non-NULL or the whole      */
    void    *p0170;          /* +0x0170  function returns without emitting  */
    void    *p0174;          /* +0x0174  anything                           */
    float    f0E68;          /* +0x0E68  sign selects one of two lists      */
    void    *p0F00;          /* +0x0F00  has an int at its own +0x64        */
    int32_t  i0F00_64;       /* the int at p0F00 + 0x64                     */
    void    *p0F08;          /* +0x0F08  fifth guard pointer                */
    int32_t  i2714;          /* +0x2714  0 or 1; indexes a class table      */
    int32_t  f2734IsSelf;    /* +0x2734 == self+0x273C or self+0x2890       */
    float    f2718;          /* +0x2718  read off the PLAYER's record       */
    uint16_t u290C;          /* +0x290C  index into the 84-byte records at
                              *          0x106EED38                         */
    void    *p294C;          /* +0x294C  non-NULL enables the +0x290C test  */
    uint8_t  bKind;          /* +0x29AF  draw class; 2 is the translucent
                              *          pass the frame driver splits out   */
    float    fAlpha;         /* +0x29B0  0..1, scaled by 255 into the fog
                              *          and prim colours when bKind == 2   */
    int32_t  i29B4;          /* +0x29B4  slice1_09.h BR_ENTITY_OFF_BANK     */
    uint32_t u29C0Flags;     /* the dword at +0x29C0's target               */
    void    *pModel;         /* +0x29C4  the car's model record             */
} BrCarView;

/* --------------------------------------------------------------------
 * The model record (BrCarView::pModel, the original's 0x106EA398).
 * Only the fields these two functions read.
 * -------------------------------------------------------------------- */
typedef struct BrModelLod {
    uint32_t dlBody;         /* +0x04 of the 40-byte per-detail record  */
    uint32_t dlGlass;        /* +0x08                                    */
    uint32_t dlUnder;        /* +0x10                                    */
    uint32_t dlExtraA;       /* +0x18                                    */
    uint32_t dlExtraB;       /* +0x1C                                    */
} BrModelLod;

typedef struct BrModelView {
    int32_t     cost;        /* +0x8000  added to the 0x106E86AC total   */
    const void *pTexRecs;    /* +0x8014  BR_TEXREC_STRIDE array          */
    uint8_t     iTexRec;     /* +0x811B  index into it                   */
    uint32_t    aHilite[5];  /* +0x0080..+0x0090, five display lists the
                              *          0x118ED1BC hook is handed        */
    BrModelLod  aLod[3];     /* +0x8020 + 40*lod                         */
    uint32_t    dlWheel;     /* +0x80BC                                  */
    uint32_t    dlWheelAlt;  /* +0x80C4                                  */
} BrModelView;

/* --------------------------------------------------------------------
 * The globals 0x10009C10 reads, gathered so they are visible.
 * Every one is unmodelled anywhere else in this tree (checked by grepping
 * the ADDRESS, not the name, across port/include and port/src).
 * -------------------------------------------------------------------- */
extern uint32_t g_BrDrawRenderMode;   /* 0x10273644  render-mode low word */
extern int32_t  g_BrDrawFogAlpha;     /* 0x102735FC  __ftol(alpha * 255)  */
extern int32_t  g_BrDrawWheelAlt;     /* 0x106ED6B0  picks dlWheelAlt     */
extern BrMat4   g_BrDrawScale;        /* 0x106E7930  the 1/255 scale mtx  */
extern BrMat4   g_BrDrawWorld;        /* 0x10273570  scale * car          */
extern BrMat4   g_BrDrawView;         /* 0x106E9A38  the view matrix      */
extern BrMat4   g_BrDrawCombined;     /* 0x106E78F0  world * view         */
extern int32_t  g_BrDrawClass[BR_CAR_MAX];  /* 0x10273520  per-car LOD       */
extern uint8_t  g_BrDrawLights[24 * BR_CAR_MAX]; /* 0x102733A0 light copies */
extern uint32_t g_BrDrawModeBase;     /* 0x10273640  render mode base       */
extern int32_t  g_BrDrawSuppress;     /* 0x10273304  suppress underside     */
extern int32_t  g_BrDrawLodFloor;     /* 0x106ED6C0  minimum LOD level      */
extern BrVec3   g_BrDrawDir0;         /* 0x10273390  light direction 0      */
extern BrVec3   g_BrDrawDir1;         /* 0x10273560  light direction 1      */
extern int32_t  g_BrDrawReflectEnable; /* 0x10B7153C BSS 0                  */
extern int32_t  g_BrDrawReflectFlag;  /* 0x100A5B40  .data = 1              */
extern void    *g_BrDrawTrackFlags;   /* 0x106EED38  ptr to 84-byte recs    */
extern const void *g_BrDrawTexBlob;   /* 0x100A5C88  palette blob           */
extern void   (*g_BrDrawModelDlHook)(uint32_t, uint32_t); /* 0x118ED1BC    */
extern uint8_t  g_BrDrawByte80;       /* 0x106B7C80  light colour byte      */
extern uint8_t  g_BrDrawByte78;       /* 0x106B7C78  env colour byte        */

/* The display-list cursor is slice2_18's BrG_6C0680 -- 0x106C0680 in the
 * D3D build IS 0x106E7710 in Glide.  It is not modelled again here; see
 * CONVENTIONS.md's aliased-storage section for why that matters. */

/* The two pool allocators 0x10009C10 uses, as a seam.  Both are already
 * transcribed under their D3D addresses (0x10069490 in slice5_62.c), but
 * they return a HOST pointer and the display list needs the original's
 * 32-bit number, so the two views are bridged here rather than a third
 * model of the pool being invented.  Unset, every entry is a FRONTIER: it
 * returns "no matrix" and counts the reach.  Nothing downstream is told a
 * matrix was allocated when none was. */
typedef struct BrDrawCarHooks {
    BrMat4  *(*pfnMtxAlloc)(void);            /* 0x10062500 */
    uint32_t (*pfnDlAddr)(const BrMat4 *pM);  /* host pointer -> DL address */
} BrDrawCarHooks;

void BrDrawCarSetHooks(const BrDrawCarHooks *pHooks);
int32_t BrDrawCarFrontierHits(void);          /* reaches with no hook set   */
void    BrDrawCarFrontierReset(void);

/* 0x10029E50 -- copy sixteen dwords.  Argument order is (SOURCE,
 * DESTINATION), which is the original's and the opposite of memcpy. */
void BrGuMtxStore(const void *pSrc, void *pDst);

/* 0x1002A9F2 -- empty in the shipped build; see the site. */
void BrGuMtxHookNop(const BrMat4 *pM);

/* 0x10009C10 -- emit the four wheels of one car. */
void BrCarDrawWheels(const BrCarView *pCar, const BrModelView *pModel);

/* --------------------------------------------------------------------
 * The car VISIBILITY pass (0x10009FC0) and its shared state.
 *
 * ARCHITECTURE NOTE: BrCarView above is a REPACKED view, byte-accurate
 * only through +0x140, used by the read-only draw functions. The
 * visibility pass instead WRITES back into the car record (the fog factor
 * at +0x2730) and into the two per-car flag arrays below, which a repack
 * cannot model -- so it reaches the record by raw byte offset, the way the
 * original does. These offsets are the access convention the cull/emit
 * functions share.
 * -------------------------------------------------------------------- */
#define BR_CAR_OFF_MTX       0x0000u  /* BrMat4 world transform             */
#define BR_CAR_OFF_ROW2      0x0020u  /* BrVec3 mtx.m[2]; the headlight basis */
#define BR_CAR_OFF_POS       0x0030u  /* BrVec3 position == mtx.m[3]         */
#define BR_CAR_OFF_AWHEEL    0x0040u  /* 4 x BrMat4 wheel transforms         */
#define BR_CAR_OFF_ICAR      0x0140u  /* int32 car index, 0..BR_CAR_MAX-1    */
#define BR_CAR_OFF_P0168     0x0168u  /* ptr; guard -- must be non-NULL       */
#define BR_CAR_OFF_P016C     0x016Cu  /* ptr; guard                           */
#define BR_CAR_OFF_P0170     0x0170u  /* ptr; guard                           */
#define BR_CAR_OFF_P0174     0x0174u  /* ptr; guard                           */
#define BR_CAR_OFF_F0E68     0x0E68u  /* float; sign selects model DL variant */
#define BR_CAR_OFF_P0F00     0x0F00u  /* ptr; has int at own +0x64            */
#define BR_CAR_OFF_GUARD     0x0F08u  /* ptr; NULL => the pass does nothing  */
#define BR_CAR_OFF_I2714     0x2714u  /* int32; 0/1 written by +0x290C gate   */
#define BR_CAR_OFF_F2718     0x2718u  /* float; G_SETTILESIZE scale factor    */
#define BR_CAR_OFF_FOG       0x2730u  /* float; written = fog at the position */
#define BR_CAR_OFF_ACTIVECAM 0x2734u  /* ptr; player: == +0x273C or +0x2890  */
#define BR_CAR_OFF_CAMA      0x273Cu  /* a cam frame inside the record        */
#define BR_CAR_OFF_CAMSLOT   0x27C4u  /* body pass: player culls if the active
                                       * camera object equals record+0x27C4  */
#define BR_CAR_OFF_CAMB      0x2890u  /* the other cam frame                  */
#define BR_CAR_OFF_U290C     0x290Cu  /* uint16; index into 84-byte records   */
#define BR_CAR_OFF_P294C     0x294Cu  /* ptr; non-NULL enables +0x290C test   */
#define BR_CAR_OFF_KIND      0x29AFu  /* draw class; 2 == translucent pass    */
#define BR_CAR_OFF_ALPHA     0x29B0u  /* float; 0..1 fog/prim alpha           */
#define BR_CAR_OFF_I29B4     0x29B4u  /* int32; entity bank                   */
#define BR_CAR_OFF_U29C0     0x29C0u  /* ptr; aux flags at target dword       */
#define BR_CAR_OFF_MODEL     0x29C4u  /* ptr; the car's model record          */

/* The model record, by raw offset (the body pass reaches it through the
 * scratch global BrG_6C3308, the same way 0x1000A110 does). */
#define BR_MODEL_OFF_TEXRECS 0x8014u  /* ptr; BrGfxEmitTexCmd's record array   */
#define BR_MODEL_OFF_BODYDL  0x802Cu  /* uint32 body geometry display list     */

/* Per-car visibility flags the pass writes and the two draw passes read.
 * Indexed by the car's +0x140. Opaque = set only for non-class-2 cars that
 * pass; Any = set for every car that passes (or the player). */
extern int32_t g_BrCarVisOpaque[BR_CAR_MAX];   /* 0x10273648 (d3d 0x10277E60) */
extern int32_t g_BrCarVisAny[BR_CAR_MAX];      /* 0x10273350 (d3d 0x10277B68) */

/* Per-car pooled-matrix display-list addresses, one slot per car, filled by
 * 0x1000A110's prologue (still on the frontier) and read by BrCarDrawBody as
 * the G_MTX / G_MOVEMEM payloads.  Zero until 0x1000A110 runs -- the same
 * "count the reach, invent no address" contract as BrDrawCarHooks. */
extern uint32_t g_BrCarMtxSlot[BR_CAR_MAX];    /* 0x102735B0  model matrix    */
extern uint32_t g_BrCarLightSlot[BR_CAR_MAX];  /* 0x10273600  lighting matrix */

/* 0x10009FC0 -- per-car pre-draw pass: compute the fog factor and decide
 * whether the car is visible this frame, setting the two flags above. pCar
 * is the raw 0x2B68 car record. */
void BrCarVisibilityUpdate(void *pCar);

/* 0x1000BEB0 -- emit ONE opaque car's body display list, and (for non-player
 * cars) accumulate its headlight glare into the two per-frame scene
 * accumulators g_4B16A0 / g_4B16AC.  pCar is the raw 0x2B68 car record. */
void BrCarDrawBody(void *pCar);

/* 0x1000A110 -- emit one vehicle's complete display list (body, underside,
 * glass, detail, reflection, wheels). pCar is the raw 0x2B68 car record.
 * lodBias is added to the distance-derived detail level.
 *
 * NOT CLAIMED (@implements withheld): three internal TODO blocks remain
 * (light-dir, specular, post-detail) and the command stream has not been
 * validated against an independent source.  See br_drawcar.c. */
void BrCarDrawVehicle(void *pCar, int32_t lodBias);

#endif /* BR_DRAWCAR_H */
