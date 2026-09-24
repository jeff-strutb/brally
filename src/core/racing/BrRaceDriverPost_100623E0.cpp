/* WHAT IT DOES: the per-driver end-of-step bookkeeping.  If the car left a
 * skid this step it kicks the force-feedback wheel one way or the other by
 * the skid's heading, drops a ripple for a long enough skid and clears it;
 * then it runs the car's clocks, messages, wrong-way check, the driver's
 * checkpoint gate and the car's grid cell, rebuilds the world velocity from
 * the last two positions, refreshes the car's heading and counts down its
 * hold timer.  Nothing happens while the race is paused. */
/* @implements 0x100623E0 glide BrRaceDriverPost
 * @cpp_symbol ?m_100623E0@Driver_623E0@@QAEXXZ
 *
 * A __thiscall on the driver whose callees are __thiscall on the car
 * (0x1000C4E0 the ripple, 0x1006E9E0/EA70/EB00/EBC0) and on the driver
 * (0x1005FF00): C++.  Members are VA-named so every thiscall maps to its
 * address.  Source facts that carry the bytes:
 *   - the wheel kick is an if/else with the `<= 180` arm (-1) first: VC5
 *     tail-merges the two calls into `push -1 / jmp / push 1 / call`, where
 *     a ternary argument becomes setcc arithmetic;
 *   - the skid length is read into an unsigned-char local and passed as
 *     (short) -- the original's `movzx ax, al`;
 *   - pCar is re-read through `this` for every statement (it is a member). */

struct BrVec3 { float x, y, z; };

class Car_623E0 {
public:
    void m_1000C4E0(const float *pDir, short mag);   /* the ripple */
    void m_1006E9E0();
    void m_1006EA70();
    void m_1006EB00();
    void m_1006EBC0();

    char          pad000[0x30];
    BrVec3        pos;          /* +0x030 */
    char          pad03c[0x350 - 0x3c];
    float         skid[2];      /* +0x350 */
    char          pad358[8];
    unsigned char skidLen;      /* +0x360 */
    char          pad361[0xF04 - 0x361];
    int           fF04;         /* +0xF04 */
    char          padF08[0xF80 - 0xF08];
    BrVec3        posPrev;      /* +0xF80 */
    char          padF8C[0x1024 - 0xF8C];
    BrVec3        vel;          /* +0x1024 */
    char          pad1030[0x2718 - 0x1030];
    float         f2718;        /* +0x2718 */
    char          pad271C[0x2734 - 0x271C];
    float        *p2734;        /* +0x2734 */
};

class Driver_623E0 {
public:
    void m_1005FF00();
    void m_100623E0();

    char       pad[0x60];
    Car_623E0 *pCar;            /* +0x60 */
};

extern "C" {
extern int   g_brRacePaused;    /* 0x105CCB5C */
extern float g_brRaceStepDt;    /* 0x106E9D8C */
float BrAtan2(float x, float y);
void  BrFfbSetDirection(int dir);
void  BrVec3Sub(BrVec3 *pOut, const BrVec3 *pA, const BrVec3 *pB);
void  BrVec3ScaleBy(BrVec3 *pV, float s);
}

void Driver_623E0::m_100623E0()
{
    if (g_brRacePaused != 0 || pCar == 0)
        return;

    if (pCar->skidLen != 0) {
        if ((int)(BrAtan2(pCar->skid[0], pCar->skid[1]) * 57.29578f) <= 180)
            BrFfbSetDirection(-1);
        else
            BrFfbSetDirection(1);
        {
            unsigned char n = pCar->skidLen;
            if (n >= 10)
                pCar->m_1000C4E0(pCar->skid, (short)n);
        }
        pCar->skidLen = 0;
    }
    pCar->m_1006E9E0();
    pCar->m_1006EA70();
    pCar->m_1006EB00();
    m_1005FF00();
    pCar->m_1006EBC0();

    BrVec3Sub(&pCar->vel, &pCar->pos, &pCar->posPrev);
    BrVec3ScaleBy(&pCar->vel, 1.0f / g_brRaceStepDt);
    pCar->f2718 = BrAtan2(pCar->p2734[0], pCar->p2734[1]);

    if (pCar->fF04 != 0)
        pCar->fF04--;
}
