/* body.h -- the car's rigid bodies (0x208 bytes each on the N64): the car
 * record (tgr/car.h) holds five in a row from 0x148, the car's own and its
 * four wheels', and sixteen force records after them.  One definition for
 * every part of the physics; where the parts named a field differently, the
 * names share it (an unnamed union).
 */
#ifndef TGR_BODY_H
#define TGR_BODY_H

#include "tgr/vec.h"

/* A rigid-body state (0x44 bytes): position, velocity, orientation and its
 * rate. */
typedef struct BrRbState {
    BrVec3 pos;                 /* 0x00 */
    BrVec3 vel;                 /* 0x0C */
    float q[4];                 /* 0x18  orientation */
    BrVec3 angVel;              /* 0x28 */
    char pad34[0x10];
} BrRbState;

/* A force applied to a body (0x20 bytes on the N64), linked. */
typedef struct BrRbForce {
    struct BrRbForce *next;     /* 0x00 */
    int frame;                  /* 0x04  0: world axes, 1: body axes */
    float f[3];                 /* 0x08 */
    float at[3];                /* 0x14  where, in body axes */
} BrRbForce;

/* One wheel's contact record (0x14 bytes on the N64), linked. */
typedef struct BrTyreLoad {
    struct BrTyreLoad *next;    /* 0x00 */
    int x4;
    float x8;                   /* 0x08 */
    float xc;                   /* 0x0C */
    float load;                 /* 0x10  the wheel's load */
} BrTyreLoad;

typedef struct BrRbBody {
    int x0;                     /* 0x000 */
    union {                     /* 0x004  the bodies attached to it (the car's wheels) */
        struct BrRbBody *sub[4];
        struct BrRbBody *child[4];
    };
    char pad014[0x18 - 0x14];
    union {                     /* 0x018  what acts on it, linked */
        BrRbForce *forces;
        BrTyreLoad *loads;
    };
    union {                     /* 0x01C  0, 1: box; 2: a wheel, fixed (no inverse inertia) */
        int kind;
        int shape;
    };
    float w, h, d;              /* 0x020  box size */
    float mass;                 /* 0x02C */
    float I[3][3];              /* 0x030  inertia tensor */
    float Iinv[3][3];           /* 0x054  its inverse */
    union {                     /* 0x078  its state */
        BrRbState st;
        struct {
            float pos[3];       /* 0x078  (on a wheel: where it hangs off the body) */
            float vel[3];       /* 0x084 */
            float q[4];         /* 0x090 */
            union {
                float angVel[3];    /* 0x0A0 */
                float omega[3];
            };
            union {
                float qdot[4];      /* 0x0AC */
                float padac[4];
            };
        };
        float f78[3];
        struct {
            float mount[2];
            float x80;          /* 0x080  on a wheel: x1d8 held to [-0.4, 0] */
        };
        char state[0x44];
    };
    union {                     /* 0x0BC  orientation and position */
        float m[4][4];
        float mtx[4][4];
    };
    float force[3];             /* 0x0FC  accumulated this step */
    float torque[3];            /* 0x108 */
    union {                     /* 0x114  the saved state */
        BrRbState stA;
        float stAf[0x44 / 4];
        struct {
            float pad114[10];
            float angVelA[3];   /* 0x13C */
        };
    };
    union {                     /* 0x158  the state being stepped */
        BrRbState stB;
        float cur[0x44 / 4];
    };
    void *hit;                  /* 0x19C  on a wheel: the ground plane under it, 0 for none */
    union {                     /* 0x1A0  on a wheel: the surface under it */
        unsigned char surface;
        unsigned char x1a0;
    };
    char pad1a1[0x1A4 - 0x1A1];
    union {                     /* 0x1A4  that plane's normal and constant */
        struct {
            float n[3];
            float d2;
        };
        float hitN[4];
    };
    union {                     /* 0x1B4  on a wheel: it is on the ground (contacts) */
        int x1b4;
        int contact;
        int f1B4;
    };
    union {                     /* 0x1B8  spring load per unit of compression squared */
        float x1b8;
        float spring;
    };
    union {                     /* 0x1BC  load per unit of upward speed */
        float x1bc;
        float damper;
    };
    union {                     /* 0x1C0  on a wheel: steering angle */
        float steer;
        float x1c0;
    };
    union {                     /* 0x1C4  on a wheel: spin rate */
        float spin;
        float x1c4;
    };
    union {                     /* 0x1C8  on a wheel */
        float inertia;
        float x1c8;
    };
    union {                     /* 0x1CC  on a wheel: drive torque */
        float drive;
        float x1cc;
    };
    union {                     /* 0x1D0  on a wheel: brake torque */
        float brake;
        float x1d0;
    };
    union {                     /* 0x1D4  on a wheel: display angle, degrees; on the car: the visual roll */
        float angle;
        float x1d4;
    };
    float x1d8;                 /* 0x1D8 */
    union {                     /* 0x1DC */
        struct {
            float f1DC, f1E0, f1E4, f1E8;
        };
        float x1dc[4];
    };
    union {                     /* 0x1EC  the last hard contact's plane normal (the HUD arrow's aim) */
        float hitDir[3];
        BrVec3 x1ec;
    };
    int stuck;                  /* 0x1F8  frames left before it counts as stuck; negative: off the track */
    unsigned char hitSize;      /* 0x1FC  how hard it was (0 when under 10); a pending HUD arrow */
    unsigned char tyres;        /* 0x1FD  on the car: the tyre compound, 1-3 */
    unsigned char x1fe;         /* 0x1FE  pending one-shot sound: loudness, 0 none */
    unsigned char hitPeak;      /* 0x1FF  the hardest since it was cleared, 128-255 */
    unsigned char idle;         /* 0x200  frames with no contact (to 40) */
    unsigned char x201;         /* 0x201  frames since the last car-to-car hit */
    unsigned char x202;         /* 0x202 */
    unsigned char x203;         /* 0x203  0x80: a wheel has just landed */
    unsigned char slide;        /* 0x204  on the car: 0x80 while sliding; the surface impact level */
    char pad205[0x208 - 0x205];
} BrRbBody;

#endif
