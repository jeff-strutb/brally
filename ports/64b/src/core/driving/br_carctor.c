/* br_carctor.c -- 0x1005BCC0, the car/entity constructor.
 *
 * thiscall on the car.  Four wheel rigid bodies, two contact chains, the
 * live BrRbState at car+0x1DC.  Matching build only.
 *
 * RESIDUE 2026-09-12: 1872/1909 B, 466/471 insns, REGNORM 7+12, A4 clean.
 * First size change is the 1.0f stores (orig rematerialises imm32 at
 * +0x60c/+0xa24 while a 0x3f800000 register is live) and five `lea`
 * of the wheel-body pointers at +0x168 -- orig re-leas after the last
 * BuildMatrix because those addresses were passed in eax (caller-saved);
 * ours keep them in a callee-saved register from InitInertia.  Record
 * pointer is re-derived per field (the BrInputPoll load idiom).
 */

#include <stdint.h>
#include "slice3_41.h"
#include "br_match.h"

/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */
/* 64-bit core: declared once, in br_globals.h or its struct's header */

/* 64-bit core: declared once, by its definition's header */
/* 64-bit core: declared once, by its definition's header */
/* BrX100746E0: prototype in br_funcs.h */
/* BrPodNop: prototype in br_funcs.h */


/* 2026-09-13: the 1.0f web. The original holds 0x3f800000 in ebp for the
 * +0x1f4 and +0x400 stores only, then gives ebp to pRF (= p + 0x800) and
 * writes +0x818 / +0x60c / +0xa24 and the four BrX100746E0 pushes as
 * immediates; ours keeps the constant in edi for the whole function and pRF
 * never gets a callee-saved register. DEAD: typing the late stores (or all
 * seven) as `((*(float *)((uint8_t *)((p)) + ((o))))) = 1.0f`, and `pRF[6] = 1.0f` -- VC5 folds a float
 * constant store into the live int register web every time (1235 each). */
/* WHAT IT DOES: initialise one car's physics bodies, wheel states and the
 * two contact-point chains.  Four rigid bodies (chassis plus three more
 * wheel/axle slots) get their inertia filled and a pose matrix built from
 * a default state; the car-data record at +0x29C4 supplies the wheel
 * positions.  Two linked lists of contact nodes are wired up, four debug
 * prints dump the wheel points, and a couple of mode bytes are cleared. */
/* @t4-pass 0x1005BCC0 1 2026-09-20 probes 12 bytes 1871 insns 466 regions 17 rows 31 census yes  (1.0f web: k1 int-const naming folds like the imm; DEAD with the 09-13 float-typing attempts) */
/* @t4-pass 0x1005BCC0 2 2026-09-20 probes 10 bytes 1871 insns 466 regions 17 rows 31 census no   (pRF[] indexing of the +0x800 block does not pin pRF callee-saved; wheel-body re-lea residue unmoved) */
/* @t3 0x1005BCC0 2026-09-20 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 1871/1909 insns 466/471 rows 18+13 regions 17 oracle EQUIVALENT
 * @t3-effort passes 2 zero-movement 1 2
 * Residue is register allocation only: the 1.0f web (orig parks 0x3f800000 in
 * ebp then recycles it as pRF, ours keeps it live and re-materialises the late
 * stores as immediates) and the five wheel-body pointer re-leas at +0x168
 * (orig re-computes p+0x370.. after the calls clobber caller-saved eax; ours
 * keeps them callee-saved).  A5 oracle EQUIVALENT is the completeness proof
 * (rule 12).  Do not reopen before the end-grind. */
/* @implements 0x1005BCC0 glide BrSub10062C50 */
void BR_THISCALL1 BrSub10062C50(void *pCar)
{
    BrDriverCar *p = (uint8_t *)pCar;
    float *pLF;
    float *pRF;

    ((*(uint32_t *)&p->f0E84)) = 1;
    ((*(uint32_t *)&p->f0E20)) = 0;
    ((*(uint32_t *)&p->fE70)) = 0;
    ((*(uint32_t *)&p->f0E24)) = 0;
    if (((BR_LP64_PTR_AS_INT(p->pModel))) != 0) {
    uint32_t k2 = 0x40000000;           /* 2.0f, orig edi */

    ((*(uint32_t *)&p->aBody[0].rb.mode)) = 1;
    ((*(uint32_t *)&p->aBody[0].rb.mass)) = 0x447a0000;          /* 1000.0f */
    ((*(uint32_t *)&p->aBody[0].rb.dim[0])) = 0x40600000;          /* 3.5f */
    ((*(uint32_t *)&p->aBody[0].rb.dim[1])) = k2;
    ((*(uint32_t *)&p->aBody[0].rb.dim[2])) = 0x3fc00000;          /* 1.5f */
    BrRbInitInertia(&p->aBody[0].rb.f00);

    ((*(uint32_t *)&p->aBody[0].rb.st.pos.x)) = 0;
    ((*(uint32_t *)&p->aBody[0].rb.st.pos.y)) = 0;
    ((*(uint32_t *)&p->aBody[0].rb.st.pos.z)) = k2;
    ((*(uint32_t *)&p->aBody[0].rb.st.vel.x)) = 0;
    ((*(uint32_t *)&p->aBody[0].rb.st.vel.y)) = 0;
    ((*(uint32_t *)&p->aBody[0].rb.st.vel.z)) = 0;
    ((*(uint32_t *)&p->aBody[0].rb.st.quat.f00)) = 0x3f800000;
    ((*(uint32_t *)&p->aBody[0].rb.st.quat.f04)) = 0;
    ((*(uint32_t *)&p->aBody[0].rb.st.quat.f08)) = 0;
    ((*(uint32_t *)&p->aBody[0].rb.st.quat.f0C)) = 0;
    ((*(uint32_t *)&p->aBody[0].rb.st.angVel.x)) = 0;
    ((*(uint32_t *)&p->aBody[0].rb.st.angVel.y)) = 0;
    ((*(uint32_t *)&p->aBody[0].rb.st.angVel.z)) = 0;
    BrRbBuildMatrix(&p->aBody[0].rb.m.m[0], &p->aBody[0].rb.st.pos.x);

    ((p->aBody[0].rb.f1B8)) = (DAT_10077864 - (float)(int32_t)((*(uint32_t *)&p->fE94)) * _DAT_1007788c)
                   * _DAT_10077890;
    ((*(uint32_t *)&p->aBody[0].rb.f1BC)) = 0xc53b8000;          /* -3000.0f */
    ((*(uint32_t *)&p->aBody[1].rb.f1D8)) = 0;
    p->aBody[1].rb.pPlane = (struct BrCollPlane *)(intptr_t)(0);
    ((*(uint32_t *)&p->aBody[1].rb.f1C4)) = 0;
    ((*(uint32_t *)&p->aBody[1].rb.mode)) = 2;
    ((*(uint32_t *)&p->aBody[1].rb.mass)) = 0;
    ((*(uint32_t *)&p->aBody[1].rb.dim[0])) = 0;
    ((*(uint32_t *)&p->aBody[1].rb.dim[1])) = 0;
    ((*(uint32_t *)&p->aBody[1].rb.dim[2])) = 0;
    BrRbInitInertia(&p->aBody[1].rb.f00);

    pLF = &p->aBody[1].rb.st.pos.x;
    *pLF = *(float *)(((((char *)p->pModel))) + 0x80f8);
    ((*(uint32_t *)&p->aBody[1].rb.st.pos.y)) = *(uint32_t *)(((((char *)p->pModel))) + 0x80fc);
    ((*(uint32_t *)&p->aBody[1].rb.st.pos.z)) = 0xbdcccccd;          /* -0.1f */
    ((*(uint32_t *)&p->aBody[1].rb.st.vel.x)) = 0;
    ((*(uint32_t *)&p->aBody[1].rb.st.vel.y)) = 0;
    ((*(uint32_t *)&p->aBody[1].rb.st.vel.z)) = 0;
    ((*(uint32_t *)&p->aBody[1].rb.st.quat.f00)) = 0x3f800000;
    ((*(uint32_t *)&p->aBody[1].rb.st.quat.f04)) = 0;
    ((*(uint32_t *)&p->aBody[1].rb.st.quat.f08)) = 0;
    ((*(uint32_t *)&p->aBody[1].rb.st.quat.f0C)) = 0;
    ((*(uint32_t *)&p->aBody[1].rb.st.angVel.x)) = 0;
    ((*(uint32_t *)&p->aBody[1].rb.st.angVel.y)) = 0;
    ((*(uint32_t *)&p->aBody[1].rb.st.angVel.z)) = 0;
    BrRbBuildMatrix(&p->aBody[1].rb.m.m[0], &p->aBody[1].rb.st.pos.x);

    ((*(uint32_t *)&p->aBody[3].rb.f1D8)) = 0;
    p->aBody[3].rb.pPlane = (struct BrCollPlane *)(intptr_t)(0);
    ((*(uint32_t *)&p->aBody[3].rb.f1C4)) = 0;
    ((*(uint32_t *)&p->aBody[3].rb.mode)) = 2;
    ((*(uint32_t *)&p->aBody[3].rb.mass)) = 0;
    ((*(uint32_t *)&p->aBody[3].rb.dim[0])) = 0;
    ((*(uint32_t *)&p->aBody[3].rb.dim[1])) = 0;
    ((*(uint32_t *)&p->aBody[3].rb.dim[2])) = 0;
    BrRbInitInertia(&p->aBody[3].rb.f00);

    pRF = &p->aBody[3].rb.st.pos.x;
    *pRF = *(float *)(((((char *)p->pModel))) + 0x80f8);
    ((p->aBody[3].rb.st.pos.y)) = -*(float *)(((((char *)p->pModel))) + 0x80fc);
    ((*(uint32_t *)&p->aBody[3].rb.st.pos.z)) = 0xbdcccccd;
    ((*(uint32_t *)&p->aBody[3].rb.st.vel.x)) = 0;
    ((*(uint32_t *)&p->aBody[3].rb.st.vel.y)) = 0;
    ((*(uint32_t *)&p->aBody[3].rb.st.vel.z)) = 0;
    ((*(uint32_t *)&p->aBody[3].rb.st.quat.f00)) = 0x3f800000;
    ((*(uint32_t *)&p->aBody[3].rb.st.quat.f04)) = 0;
    ((*(uint32_t *)&p->aBody[3].rb.st.quat.f08)) = 0;
    ((*(uint32_t *)&p->aBody[3].rb.st.quat.f0C)) = 0;
    ((*(uint32_t *)&p->aBody[3].rb.st.angVel.x)) = 0;
    ((*(uint32_t *)&p->aBody[3].rb.st.angVel.y)) = 0;
    ((*(uint32_t *)&p->aBody[3].rb.st.angVel.z)) = 0;
    BrRbBuildMatrix(&p->aBody[3].rb.m.m[0], &p->aBody[3].rb.st.pos.x);

    ((*(uint32_t *)&p->aBody[2].rb.f1D8)) = 0;
    p->aBody[2].rb.pPlane = (struct BrCollPlane *)(intptr_t)(0);
    ((*(uint32_t *)&p->aBody[2].rb.f1C4)) = 0;
    ((*(uint32_t *)&p->aBody[2].rb.mode)) = 2;
    ((*(uint32_t *)&p->aBody[2].rb.mass)) = 0;
    ((*(uint32_t *)&p->aBody[2].rb.dim[0])) = 0;
    ((*(uint32_t *)&p->aBody[2].rb.dim[1])) = 0;
    ((*(uint32_t *)&p->aBody[2].rb.dim[2])) = 0;
    BrRbInitInertia(&p->aBody[2].rb.f00);

    ((*(uint32_t *)&p->aBody[2].rb.st.pos.x)) = *(uint32_t *)(((((char *)p->pModel))) + 0x80ec);
    ((*(uint32_t *)&p->aBody[2].rb.st.pos.y)) = *(uint32_t *)(((((char *)p->pModel))) + 0x80f0);
    ((*(uint32_t *)&p->aBody[2].rb.st.pos.z)) = 0xbdcccccd;
    ((*(uint32_t *)&p->aBody[2].rb.st.vel.x)) = 0;
    ((*(uint32_t *)&p->aBody[2].rb.st.vel.y)) = 0;
    ((*(uint32_t *)&p->aBody[2].rb.st.vel.z)) = 0;
    ((*(uint32_t *)&p->aBody[2].rb.st.quat.f00)) = 0x3f800000;
    ((*(uint32_t *)&p->aBody[2].rb.st.quat.f04)) = 0;
    ((*(uint32_t *)&p->aBody[2].rb.st.quat.f08)) = 0;
    ((*(uint32_t *)&p->aBody[2].rb.st.quat.f0C)) = 0;
    ((*(uint32_t *)&p->aBody[2].rb.st.angVel.x)) = 0;
    ((*(uint32_t *)&p->aBody[2].rb.st.angVel.y)) = 0;
    ((*(uint32_t *)&p->aBody[2].rb.st.angVel.z)) = 0;
    BrRbBuildMatrix(&p->aBody[2].rb.m.m[0], &p->aBody[2].rb.st.pos.x);

    ((*(uint32_t *)&p->aBody[4].rb.f1D8)) = 0;
    p->aBody[4].rb.pPlane = (struct BrCollPlane *)(intptr_t)(0);
    ((*(uint32_t *)&p->aBody[4].rb.f1C4)) = 0;
    ((*(uint32_t *)&p->aBody[4].rb.mode)) = 2;
    ((*(uint32_t *)&p->aBody[4].rb.mass)) = 0;
    ((*(uint32_t *)&p->aBody[4].rb.dim[0])) = 0;
    ((*(uint32_t *)&p->aBody[4].rb.dim[1])) = 0;
    ((*(uint32_t *)&p->aBody[4].rb.dim[2])) = 0;
    BrRbInitInertia(&p->aBody[4].rb.f00);

    ((*(uint32_t *)&p->aBody[4].rb.st.pos.x)) = *(uint32_t *)(((((char *)p->pModel))) + 0x80ec);
    ((p->aBody[4].rb.st.pos.y)) = -*(float *)(((((char *)p->pModel))) + 0x80f0);
    ((*(uint32_t *)&p->aBody[4].rb.st.pos.z)) = 0xbdcccccd;
    ((*(uint32_t *)&p->aBody[4].rb.st.vel.x)) = 0;
    ((*(uint32_t *)&p->aBody[4].rb.st.vel.y)) = 0;
    ((*(uint32_t *)&p->aBody[4].rb.st.vel.z)) = 0;
    ((*(uint32_t *)&p->aBody[4].rb.st.quat.f00)) = 0x3f800000;
    ((*(uint32_t *)&p->aBody[4].rb.st.quat.f04)) = 0;
    ((*(uint32_t *)&p->aBody[4].rb.st.quat.f08)) = 0;
    ((*(uint32_t *)&p->aBody[4].rb.st.quat.f0C)) = 0;
    ((*(uint32_t *)&p->aBody[4].rb.st.angVel.x)) = 0;
    ((*(uint32_t *)&p->aBody[4].rb.st.angVel.y)) = 0;
    ((*(uint32_t *)&p->aBody[4].rb.st.angVel.z)) = 0;
    BrRbBuildMatrix(&p->aBody[4].rb.m.m[0], &p->aBody[4].rb.st.pos.x);

    {
        uint8_t *pA = (uint8_t *)&p->aBody[1].rb.f00;
        uint8_t *pB = (uint8_t *)&p->aBody[3].rb.f00;
        uint8_t *pC = (uint8_t *)&p->aBody[2].rb.f00;
        uint8_t *pD = (uint8_t *)&p->aBody[4].rb.f00;
        p->aBody[0].rb.child[0] = (void *)pA;
        p->aBody[0].rb.child[1] = (void *)pB;
        p->aBody[0].rb.child[2] = (void *)pC;
        p->aBody[0].rb.child[3] = (void *)pD;
    }

    /* the force list: aForce[i] is the original's record at 0xBA0 + 0x20*i */
    BrX100746E0(&p->aForce[0], 0, 0, 0, 0xbfc00000u, 0xbf800000u, 0, 1);
    BrX100746E0(&p->aForce[2], 0, 0, 0, 0xbfc00000u, 0x3f800000u, 0, 1);
    BrX100746E0(&p->aForce[1], 0, 0, 0, 0x3fc00000u, 0xbf800000u, 0, 1);
    BrX100746E0(&p->aForce[3], 0, 0, 0, 0x3fc00000u, 0x3f800000u, 0, 1);
    BrX100746E0(&p->aForce[8], 0, 0, 0xc6716b00u, 0, 0, 0, 0);
    BrX100746E0(&p->aForce[9], 0, 0, 0xc2ce028fu, 0, 0, 0, 0);
    BrX100746E0(&p->aForce[10], 0, 0, 0xc2ce028fu, 0, 0, 0, 0);

    p->aForce[0].pNext = &p->aForce[2];
    p->aForce[2].pNext = &p->aForce[1];
    p->aForce[1].pNext = &p->aForce[3];
    p->aBody[0].rb.pForces = &p->aForce[0];
    p->aForce[3].pNext = &p->aForce[8];
    p->aForce[8].pNext = 0;

    BrX100746E0(&p->aForce[12], 0, 0, 0, 0, 0, 0, 1);
    BrX100746E0(&p->aForce[14], 0, 0, 0, 0, 0, 0, 1);
    BrX100746E0(&p->aForce[13], 0, 0, 0, 0, 0, 0, 1);
    BrX100746E0(&p->aForce[15], 0, 0, 0, 0, 0, 0, 1);

    p->aBody[1].rb.pForces = &p->aForce[12];
    p->aBody[2].rb.pForces = &p->aForce[13];
    p->aBody[4].rb.pForces = &p->aForce[15];
    p->aForce[12].pNext = &p->aForce[10];
    p->aForce[13].pNext = &p->aForce[9];
    p->aForce[15].pNext = &p->aForce[9];
    p->aForce[9].pNext = 0;
    p->aBody[3].rb.pForces = &p->aForce[14];
    p->aForce[14].pNext = &p->aForce[10];
    p->aForce[10].pNext = 0;

    BrX100746E0(&p->aForce[4], 0, 0, 0, 0xbfc00000u, 0xbf800000u, 0, 1);
    BrX100746E0(&p->aForce[6], 0, 0, 0, 0xbfc00000u, 0x3f800000u, 0, 1);
    BrX100746E0(&p->aForce[5], 0, 0, 0, 0x3fc00000u, 0xbf800000u, 0, 1);
    BrX100746E0(&p->aForce[7], 0, 0, 0, 0x3fc00000u, 0x3f800000u, 0, 1);
    BrX100746E0(&p->aForce[11], 0, 0, 0, 0, 0, 0, 0);

    p->aForce[4].pNext = &p->aForce[6];
    p->aForce[6].pNext = &p->aForce[5];
    p->aForce[5].pNext = &p->aForce[7];
    p->aForce[7].pNext = &p->aForce[11];
    p->aForce[11].pNext = 0;

    ((*(uint8_t *)((uint8_t *)((p)) + ((0xe81))))) = 0;
    ((*(uint32_t *)&p->f0E7C)) = 0;
    ((*(uint32_t *)&p->f0E74)) = 0;
    ((*(uint8_t *)((uint8_t *)((p)) + ((0x361))))) = ((*(uint8_t *)((uint8_t *)((p)) + ((0xe90)))));

    BrPodNop();
    BrPodNop();
    BrPodNop();
    BrPodNop();
    }

    ((*(uint8_t *)((uint8_t *)((p)) + ((0xe80))))) = 0;
    ((*(uint8_t *)((uint8_t *)((p)) + ((0xe78))))) = 0;
}

