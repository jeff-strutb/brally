/* br_cartick.c -- racing.
 * Per-car, per-frame bookkeeping that the driver step 0x100623E0 runs on
 * every car between the physics and the gate machine: the lap and air
 * clocks (0x1006E9E0), the two on-screen message countdowns (0x1006EA70)
 * and the 64x64 grid cell the car occupies (0x1006EBC0).  br_racestep.h
 * lists all three under BR_RS_HOLE_SKID beside 0x1006EB00, which is in
 * br_wrongway.c and writes the message slots these count down.
 */
#ifdef BR_MATCHING_BUILD

extern volatile float g_f6C2CFC;  /* 0x106E9D8C  frame dt, seconds; VOLATILE:
                                   * it is what makes VC5 load it FIRST
                                   * (`fld dt; fadd member`) -- see the
                                   * BrCarTickClocks note.                  */
extern int   g_br0AA010;      /* 0x100A9360  race mode                   */
extern int   BrG_6C7CB8;      /* 0x106EED48                              */
int BrFtolTrunc(float f);     /* 0x10018990  int(f), a real cdecl callee */
void __fastcall FUN_1006e5c0(int pCar);   /* 0x1006E5C0                  */

/* The car record is not typed in this lane; these name the fields used. */
#define CAR_I(p, off)  (*(int *)((p) + (off)))
#define CAR_F(p, off)  (*(float *)((p) + (off)))
#define CAR_B(p, off)  (*(unsigned char *)((p) + (off)))

/* WHAT IT DOES: advance a car's clocks by one frame, unless its body is
 * flagged (bits 0-1 of +0x68 on the physics record) as not running.  In race
 * mode 3 only the lap clock (+0xFB0) runs; otherwise both the lap clock and
 * the total clock (+0xFEC) run, and in modes 1 and 6 a countdown (+0xFF0)
 * also ticks down, stopping at zero. */
/* @implements 0x1006E9E0 glide BrCarTickClocks */
void __fastcall BrCarTickClocks(int pCar)
{
    if ((CAR_B(CAR_I(pCar, 0xf00), 0x68) & 3) == 0) {
        if (g_br0AA010 == 3) {
            CAR_F(pCar, 0xfb0) += g_f6C2CFC;
            return;
        }
        CAR_F(pCar, 0xfec) += g_f6C2CFC;
        CAR_F(pCar, 0xfb0) += g_f6C2CFC;
        if (g_br0AA010 == 1 || g_br0AA010 == 6) {
            /* Written out, not `-=`: on a volatile operand `-=` loads the
             * volatile first and emits fsubr; this form keeps `fld x; fsub dt`. */
            CAR_F(pCar, 0xff0) = CAR_F(pCar, 0xff0) - g_f6C2CFC;
            if (CAR_F(pCar, 0xff0) < 0.0f)
                CAR_F(pCar, 0xff0) = 0.0f;
        }
    }
}

/* WHAT IT DOES: count down a car's on-screen message.  The first slot
 * (+0xFFC id, +0x1000 seconds left) takes priority; only when it is idle
 * does the second (+0x1004, +0x1008) tick.  When a countdown runs out both
 * the timer and the id are cleared, which takes the message off screen. */
/* @implements 0x1006EA70 glide BrCarTickMessages */
void __fastcall BrCarTickMessages(int pCar)
{
    if (CAR_F(pCar, 0x1000) != 0.0f) {
        CAR_F(pCar, 0x1000) = CAR_F(pCar, 0x1000) - g_f6C2CFC;  /* not -=: volatile */
        if (CAR_F(pCar, 0x1000) <= 0.0f) {
            CAR_F(pCar, 0x1000) = 0.0f;
            CAR_I(pCar, 0xffc) = 0;
        }
    }
    else if (CAR_F(pCar, 0x1008) != 0.0f) {
        CAR_F(pCar, 0x1008) = CAR_F(pCar, 0x1008) - g_f6C2CFC;  /* not -=: volatile */
        if (CAR_F(pCar, 0x1008) <= 0.0f) {
            CAR_F(pCar, 0x1008) = 0.0f;
            CAR_I(pCar, 0x1004) = 0;
        }
    }
}

/* WHAT IT DOES: work out which cell of a 64x64 grid (32 units a side) the
 * car's position falls in, store the two coordinates as bytes at +0x29BC/BD
 * clamped to 63, and hand the car to 0x1006E5C0, which uses that cell.
 * Skipped entirely while the global at 0x106EED48 is off. */
/* @implements 0x1006EBC0 glide BrCarTickGridCell */
void __fastcall BrCarTickGridCell(int pCar)
{
    if (BrG_6C7CB8 != 0) {
        float fx = CAR_F(pCar, 0x30) * 0.03125f;
        float fy = CAR_F(pCar, 0x34) * 0.03125f;

        CAR_B(pCar, 0x29bc) = (unsigned char)BrFtolTrunc(fx);
        CAR_B(pCar, 0x29bd) = (unsigned char)BrFtolTrunc(fy);
        if (CAR_B(pCar, 0x29bc) >= 0x40)
            CAR_B(pCar, 0x29bc) = 0x3f;
        if (CAR_B(pCar, 0x29bd) >= 0x40)
            CAR_B(pCar, 0x29bd) = 0x3f;
        FUN_1006e5c0(pCar);
    }
}

#endif /* BR_MATCHING_BUILD */
