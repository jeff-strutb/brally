/* sim.h -- the race's simulation (the game's racing/ and driving/ code, transcribed
 * over fx): the cars' rigid bodies, their tyres and drive, collision with the track
 * and each other, the ground under them, where they are on the track.
 *
 * The records keep the game's: a car is a body (the chassis) with four wheel bodies
 * and a pool of force records linked into lists, as BrCarPhysInit builds it; the
 * offsets in the comments are the game's (in BrCar, and in a body: the 0x208-byte
 * record at car + 0x148 and at each wheel), so host/simcheck.c can load a car from
 * the cartridge's memory and compare it field by field. */
#ifndef SIM_H
#define SIM_H
#include "fx.h"

typedef struct { fx pos[3], vel[3], q[4], omega[3], qdot[4]; } RbState;   /* 0x44 */

typedef struct Force {                  /* BrRbForce (0x20 bytes) */
    struct Force *next;
    int frame;                          /* 0: world axes, 1: the body's */
    fx f[3];                            /* 0x08 */
    fx at[3];                           /* 0x14  where (body axes) */
} Force;

typedef struct Plane {                  /* a track triangle's collision plane (BrCrPlane) */
    fx n[3], d;
    const fx *v0, *v1, *v2;             /* its corners */
    uint16_t tri;
    uint8_t surface;                    /* 0x1E: the triangle's surface bits & 7 */
    uint8_t pad;
    /* bounds the game does not keep: the loops over a cell's triangles pass by those that
       cannot meet what they test (a sphere about the corners, the box of their x and y,
       each a little generous), so the answers stay the game's and the work falls */
    fx c[3], r;
    fx xmin, xmax, ymin, ymax;
    int16_t bx[6];                      /* the corners' box in eighths, a little wide: x, y, z each
                                           low then high (sim_cell_pick's) */
} Plane;

typedef struct Body {                   /* a rigid body (0x208 bytes in the game) */
    struct Body *sub[4];                /* 0x04  the chassis' wheels */
    Force *forces;                      /* 0x18  its force list (the chassis: the loads first) */
    int kind;                           /* 0x1C  1: a box; 2: a wheel (no inverse inertia) */
    fx w, h, d;                         /* 0x20  box size */
    fx mass;                            /* 0x2C */
    fx I[3][3];                         /* 0x30 */
    fx Iinv[3][3];                      /* 0x54 */
    RbState st;                         /* 0x78  (a wheel: pos is its mount, pos[2] its travel) */
    fx m[4][4];                         /* 0xBC */
    fx force[3];                        /* 0xFC  summed this step, then the acceleration */
    fx torque[3];                       /* 0x108 */
    RbState stA;                        /* 0x114 */
    RbState stB;                        /* 0x158 */
    const Plane *hit;                   /* 0x19C  a wheel: the ground plane under it */
    uint8_t surface;                    /* 0x1A0  a wheel: what it is on */
    fx hitN[4];                         /* 0x1A4  that plane's normal and constant */
    int x1b4;                           /* 0x1B4  a wheel: frames on the ground (to 100) */
    fx spring;                          /* 0x1B8 */
    fx damper;                          /* 0x1BC */
    fx steer;                           /* 0x1C0  a wheel: steering angle; the chassis: 0.174 */
    fx spin;                            /* 0x1C4  a wheel: spin rate */
    fx inertia;                         /* 0x1C8  a wheel: 0.5 */
    fx drive;                           /* 0x1CC  a wheel: drive torque */
    fx brake;                           /* 0x1D0  a wheel: brake torque */
    fx angle;                           /* 0x1D4  a wheel: display angle, degrees; the chassis: roll */
    fx depth;                           /* 0x1D8  a wheel: minus the drop to the ground */
    fx box[4];                          /* 0x1DC  the chassis box for collision: x, y, z, z offset */
    fx hitN2[3];                        /* 0x1EC  the last hard contact's normal */
    int stuck;                          /* 0x1F8 */
    uint8_t hitSize;                    /* 0x1FC */
    uint8_t tyres;                      /* 0x1FD */
    uint8_t hitPeak;                    /* 0x1FF */
    uint8_t idle;                       /* 0x200  frames without a contact (to 40) */
    uint8_t landed;                     /* 0x203  0x80: a wheel has just landed */
    uint8_t slide;                      /* 0x204  0x80 while sliding */
} Body;

typedef struct Pad {                    /* the pad record as a car reads it (0x15C bytes) */
    uint32_t flags;                     /* 0x00  0x10000 accelerate, 0x20000 brake, 0x40000 handbrake,
                                           0x100000/0x200000 gear up/down, 0x10 A ... */
    fx throttle;                        /* 0x1C */
    fx steer;                           /* 0x20  -1..1 */
    uint8_t kind;                       /* 0x25  4: a steering wheel */
} Pad;

typedef struct Car {                    /* BrCar (0x2090 bytes) */
    fx mtx0[4][4];                      /* 0x000  the body's draw matrix */
    fx wheelMtx[4][4][4];               /* 0x040 */
    int slot;                           /* 0x140 */
    Body body;                          /* 0x148 */
    Body wb[4];                         /* 0x350, 0x558, 0x760, 0x968: wheel[0], [2], [1], [3] */
    Force force[16];                    /* 0xB70 .. 0xD50 */
    fx xdf0;                            /* 0xDF0  the steering angle */
    fx xdf4;                            /* 0xDF4  engine speed */
    fx ratio[7];                        /* 0xDF8  the gears' ratios */
    fx torque[4];                       /* 0xE14  the torque curve: rpm^3, rpm^2, rpm, 1 */
    fx torqueK[4];                      /* the fixed build's: the same curve in thousands of rpm (the
                                           loader scales the game's coefficients: x1e9, x1e6, x1e3, x1) */
    fx xe24;                            /* 0xE24  wheel spin to engine speed */
    int gears;                          /* 0xE28 */
    int frontDrive;                     /* 0xE2C */
    int automatic;                      /* 0xE30 */
    int xe34;                           /* 0xE34 */
    fx xe38;                            /* 0xE38  drive force */
    fx xe3c;                            /* 0xE3C  brake force */
    int gear;                           /* 0xE40 */
    fx gripR;                           /* 0xE44 */
    uint8_t slipR;                      /* 0xE48 */
    fx gripF;                           /* 0xE4C */
    uint8_t slipF;                      /* 0xE50 */
    int8_t slew;                        /* 0xE51  the steering's last turn back through the centre */
    int firstFrame;                     /* 0xE54  the tyre passes skip the first frame */
    int handling;                       /* 0xE68  the steering mode (0..2) */
    uint32_t linkFlags;                 /* (0xED0)->0x68  bits 0-1: out of the race */
    int fly;                            /* 0xF4C  the debug fly mode */
    fx xfa8;                            /* 0xFA8  distance back: the ranking key */
    int xfac;                           /* 0xFAC */
    int groundHits;                     /* 0xFD4 */
    fx speedMph;                        /* 0xFE4  for the gauges */
    fx flyRate[4];                      /* 0x1DD4  x, y rotation rates, speed, z rate */
    uint16_t nearIds[32];               /* 0x1FC0  what the ground ray met */
    int gotHit;                         /* 0x2000 */
    uint16_t farIds[32];                /* 0x2004 */
    int farCount;                       /* 0x2044 */
    fx groundDist;                      /* 0x2048 */
    int groundFace;                     /* 0x204C */
    uint8_t sndHitA;                    /* 0x346  a pending hit sound's loudness */
    uint8_t hitAge;                     /* 0x349  frames since the last car-to-car hit */
    uint8_t fade;                       /* 0x2063  colour[3]: 2 while it fades in or out (no collision) */
    int camMode;                        /* 0xF48 */
    int cam;                            /* 0x1DE8  the camera in use (cams[cam]) */
    struct { fx mtx[4][4], fov; } cams[4]; /* 0x1DF0  rows: forward, side, up, position */
    struct { fx mtx[4][4], fov; } cam4; /* 0x1F44 */
    fx camSpeed, camSpin, x1f90;        /* 0x1F88 */
    fx camTarget[3];                    /* 0x1F94 */
    fx camPosA[3];                      /* 0x1FA0 */
    fx x1fac;                           /* 0x1FAC */
    fx camPosB[3];                      /* 0x1FB4 */
    fx camView[3];                      /* the model's lens offsets (model + 0xB0) */
    Pad *pad;                           /* 0x2074 */
    fx mount[4];                        /* the model's wheel mounts: rear x, y, front x, y */
} Car;

/* the track (the loaded track's header at 0x80025C00): its triangles, their planes, the
   64 by 64 grid of 32-unit cells listing them */
typedef struct {
    int nverts, ntris;
    const fx (*verts)[3];
    const uint16_t (*tris)[4];          /* three corners and the surface id */
    const uint8_t *surf;                /* per triangle: surface bits */
    const Plane *planes;                /* per triangle (BrCollGridCellAcquire's, made once) */
    const uint16_t *cellStart;          /* 4097: each cell's first entry in cellTris */
    const uint16_t *cellTris;           /* triangle numbers */
    const uint16_t *triggers;           /* zero-terminated lists */
    const uint16_t *triTrigger;         /* per triangle: its list */
    fx x28, x2c, x38, x3c;              /* the track's extents */
    fx defNormFar[3], defNormNear[3];   /* D_8028B318, D_8028B324: the ground ray's default normals */
} Track;

typedef struct {                        /* a grid cell's triangles (BrCollGridCellAcquire's slot): their
                                           planes are g_track->planes[tris[i]] */
    const uint16_t *tris;
    int n;
    int key;                            /* the cache's key for the cell: one per 32-unit square */
} SimCell;

/* the globals the simulation reads */
typedef struct {
    int walkBack;                       /* D_802A4A30: the cell walked backwards this frame */
    fx dt;                              /* D_8028AAD8 */
    int weather;                        /* D_8028C800 */
    int mode;                           /* D_8026FF18 */
    int players;                        /* D_8026FF08 */
    int ncars;
    Car *cars[4];
    fx lens;                            /* D_8028AAC0: the lens scale */
    int replay;                         /* D_80270788 */
    int views;                          /* D_8028AB0C */
    int camPushed;                      /* D_8028B710 */
    int camHold, camHold2;              /* D_8028B7F8, D_8028B7FC */
    int viewCar;                        /* D_8031B2C8[0].car: the car the view follows */
} World;

extern const Track *g_track;
extern World g_world;
extern fx g_grip[72];            /* D_802A4A38: grip, then the lateral speeds above which it falls and
                                     below which it is full, each by weather row (8) and surface */

void sim_car_link(Car *car);
void sim_plane_bounds(Plane *p);
void BrCamChaseStep(Car *car);
extern void (*sim_camera)(Car *car);                       /* the pointers BrCarPhysInit sets */
void BrCarPhysTick(Car *car);
void BrCarPhysStep(Car *car);
void BrCarDriveInput(Car *car);
void BrCarBuildMatrices(Car *car);
int BrGroundRay(fx *pos, fx *norm, const fx *eye, uint16_t *nearIds, int *gotHit, uint16_t *farIds,
                int *farCount, fx *dist, int *face);

/* geometry */
void BrMat4RotateVec(fx out[3], fx m[4][4], const fx v[3]);
void BrMat4RotateVecT(fx out[3], fx m[4][4], const fx v[3]);
void BrMat3MulVecRows(fx out[3], fx m[4][4], const fx v[3]);
void BrMat3MulVec(fx out[3], fx m[3][3], const fx v[3]);
void BrMat3Transpose(fx t[3][3], fx c[3][3], fx m[4][4]);
void BrMat3FromMat4T(fx t[3][3], fx m[4][4]);
void BrMat3FromMat4(fx out[3][3], fx m[4][4]);
void BrMat3Skew(fx out[3][3], const fx v[3]);
void BrMat3Mul(fx out[3][3], fx a[3][3], fx b[3][3]);
void BrMat3Sub(fx out[3][3], fx a[3][3], fx b[3][3]);
void BrMat3Solve(fx out[3], fx m[3][3], const fx v[3]);
void BrMat4InvertScaled(fx m[4][4], fx out[4][4], const fx s[3]);
void BrQuatToMat(fx m[4][4], const RbState *s);
void sim_car_wheels(Car *car);
void sim_tick_phase(Car *car, int k);   /* BrCarPhysTick's phase k (0..3) */
void sim_tick_input(Car *car);
void sim_step_forces(Car *car);
void sim_step_collide(Car *car);
void sim_step_ground(Car *car);
void sim_tick_after(Car *car);
int sim_cell_pick(const SimCell *c, const int32_t q[6], uint16_t *out);   /* the cell's entries whose box meets q */
void sim_pick_box(int32_t q[6], const fx *p, fx r);
int sim_tri_box(fx v[3][3], fx nrm[3], fx m[4][4], const Plane *p);   /* a triangle against the box, in 32 bits */
int sim_tri_contains(const Plane *t, const fx *p);
fx sim_probe_last(Body *w, const Plane *p, const fx *world, const fx *dir);
fx sim_probe_walk(Body *w, const SimCell *cell, const uint16_t *pick, int npick, const fx *world, const fx *dir);
void sim_pick_seg(int32_t q[6], const fx *a, const fx *b, fx pad);   /* q: a to b's box, pad wider */   /* q: p's box r wide each way, in eighths */          /* the wheels' matrices (BrCarBuildMatrices' rest) */
void BrVec4Normalise(fx v[4]);
void BrVec3NormaliseF(fx v[3]);
void BrRbQuatDerivative(RbState *b);
void BrRbStateStep(RbState *out, const RbState *in, fx dt);
void guRotateF(fx m[4][4], fx a, fx x, fx y, fx z);
void guMtxCatF(fx m[4][4], fx n[4][4], fx r[4][4]);
fx BrAtan2(fx x, fx y);
/* a tick's length: the game's 1/30 s; the fixed-point builds' can be longer (g_sim_dt, the
   GBA's physics running fewer ticks a second) */
#ifdef FX_FLOAT
#define SIM_DT FX(0.033333335f)
#define SIM_DTK FX(1.0f)
#else
extern fx g_sim_dt, g_sim_dtk;             /* the tick's length, and that in the game's ticks */
#define SIM_DT g_sim_dt
#define SIM_DTK g_sim_dtk
#endif

/* the collision's substeps a tick: the game's four, two on the GBA (gba/ builds define
   SIM_SUBSTEPS; the host checks keep the game's) */
#ifndef SIM_SUBSTEPS
#define SIM_SUBSTEPS 4
#endif

/* SIM_OWN: a helper kept a function of its own (not folded into its caller), so the GBA's
   link can place it (gba/place.txt) */
#define SIM_OWN __attribute__((noinline))

/* SIM_PROF: the cycles of each stage of a tick, summed into g_simprof (gba/race.c) */
#ifdef SIM_PROF
extern uint32_t g_simprof[24];
uint32_t sim_clock(void);
#define SP_BEGIN uint32_t sp_t_ = sim_clock()
#define SP(k) do { uint32_t n_ = sim_clock(); g_simprof[k] += n_ - sp_t_; sp_t_ = n_; } while (0)
#else
#define SP_BEGIN
#define SP(k) ((void)0)
#endif

#endif
