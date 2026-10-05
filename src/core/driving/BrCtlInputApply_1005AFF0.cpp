#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern "C" void BrMat4MulVec3(float *pOut, float *pM, float *pV);

struct BrCtlPad {
  unsigned flags;                  /* 0x00 */
  unsigned char _04[0x1c - 0x04];
  float throttle;                  /* 0x1c */
  float steer;                     /* 0x20 */
  void m_1002F640(unsigned mask);  /* BrBitLatchTake */
};

struct BrCtlState {
  unsigned char _00[0x68];
  unsigned char flags68;           /* 0x68 */
};

struct BrCar {
  unsigned char _0000[0x01e8];
  float vel[3];                    /* 0x01e8 */
  unsigned char _01f4[0x0220 - 0x01f4];
  float mat[16];                   /* 0x0220 */
  unsigned char _0260[0x0740 - 0x0260];
  float wheelSpin;                 /* 0x0740 */
  unsigned char _0744[0x0e20 - 0x0744];
  float steer;                     /* 0x0e20 */
  float rpm;                       /* 0x0e24 */
  float ratio[7];                  /* 0x0e28 */
  float torque[4];                 /* 0x0e44 */
  float finalDrive;                /* 0x0e54 */
  int gearTop;                     /* 0x0e58 */
  unsigned char _0e5c[4];
  int autoBox;                     /* 0x0e60 */
  unsigned char _0e64[4];
  float force;                     /* 0x0e68 */
  float brake;                     /* 0x0e6c */
  int gear;                        /* 0x0e70 */
  unsigned char _0e74[0x0e81 - 0x0e74];
  signed char steerDir;            /* 0x0e81 */
  unsigned char _0e82[0x0e98 - 0x0e82];
  int handling;                    /* 0x0e98 */
  unsigned char _0e9c[0x0f00 - 0x0e9c];
  BrCtlState *pState;              /* 0x0f00 */
  unsigned char _0f04[0x0ff4 - 0x0f04];
  float dist;                      /* 0x0ff4 */
  int rubber;                      /* 0x0ff8 */
  unsigned char _0ffc[0x1030 - 0x0ffc];
  float speed;                     /* 0x1030 */
  unsigned char _1034[0x29c0 - 0x1034];
  BrCtlPad *pPad;                  /* 0x29c0 */
  void CtlInputApply();               /* BrCtlInputApply */
};

extern "C" {
extern unsigned short *DAT_10b71534;
extern int DAT_100b2e6c;
extern float DAT_100b2e70;
extern float DAT_100b2e74;
extern float DAT_100b2e78;
extern short DAT_10ac67cc;
extern int DAT_100a9360;
extern int DAT_100b2f00;
extern int DAT_10af0858[];
}

static inline float BrClampf(float f, float lo, float hi)
{
  if (f < lo)
    f = lo;
  if (f > hi)
    f = hi;
  return f;
}

#define BR_SGN(x) ((x) == 0.0f ? 0.0f : ((x) > 0.0f ? 1.0f : -1.0f))
#define BR_ABS(x) ((x) < 0.0f ? -(x) : (x))

/* WHAT IT DOES: apply this frame's player input to one car. Deadzones the
 * stick, picks handling coefficients for the controller mode, slews steering
 * toward the stick (or a speed-shaped curve when a digital button is held),
 * then steps gear, engine force and the throttle slew. */
/* @t3 0x1005AFF0 2026-10-05 -- CERTIFIED COMPLETE, NOT BYTE-EXACT.
 * @t3-measure bytes 3212/3210 insns 816/783 rows 79+112 regions 25 oracle EQUIVALENT
 * @t3-effort passes 6 zero-movement 3 6
 * Residue: switch cases 1 and 2 end in the same statement
 * (`steer = d * x * -0.08726647f`). The original keeps one copy (case 1
 * jumps into case 2's); ours keeps both: VC5 cross-jumps the pair, then
 * FUN_0043d16c copies the shared block back because its size estimate is
 * 20 bytes, at the 20-byte limit. Everything else is byte-exact. */
/* @implements 0x1005AFF0 glide BrCtlInputApply
 * @cpp_kind method
 * @cpp_symbol ?CtlInputApply@BrCar@@QAEXXZ
 *
 * 3210 B thiscall (car in ecx). Levers read from the original:
 *   - every constant is a literal from the unit's pool (pool order is
 *     source order); `a + 0.07f` is emitted as `fsub [-0.07]`;
 *   - the speed copy is `volatile`: one read, the clamp in registers, one
 *     store that is otherwise dead (the original keeps it);
 *   - three scratch floats share the frame by reference count: x (also the
 *     button-path scale and the upper steer bound), v (also the button-path
 *     magnitude, the lower bound and the previous rpm), and a third slot
 *     shared by rate, the per-case signs, hi and the second throttle copy;
 *   - the per-case signs are block locals (16 references as one variable
 *     would sort them ahead of x and v);
 *   - the pad is C++ (`m_1002F640` is thiscall, no edx);
 *   - windows.h and the CRT headers come first: the commutative operand
 *     orders (slew add, rpm product) follow the front end's symbol ids;
 *   - throttle copies are their own statements inside the key test.
 *
 * @t4-pass 0x1005AFF0 5 2026-10-05 probes 43 bytes 3212 insns 816 regions 25 rows 191 census yes  (hand transcription from the original: literals, frame, volatile clamp, sign scoping, C++ pad, header prefix, throttle statements)
 * @t4-pass 0x1005AFF0 6 2026-10-05 probes 24 bytes 3212 insns 816 regions 25 rows 191 census no  (case 1/2 shared tail: goto, block scope, double/cast curve, symbol-id wrap before d, pragma optimize letters, /G3-/G5, SP3 C2; duplicator estimate stays 20)
 */
void BrCar::CtlInputApply()
{
  float in;
  float k, t;
  float x, v, rate, vec[3];
  volatile float spd;
  float hi, lo, z, z2;
  float bonus, best, acc, mul, r, lim, d;
  short turn;
  int gear, n;
  int *q;

  in = this->pPad->steer;
  if ((DAT_10b71534[0] & 0x8000) == 0 && (DAT_10b71534[3] & 0x8000) == 0) {
    if (in > 0.0f) {
      x = in - 0.07f;
      if (x < 0.0f)
        x = 0.0f;
    } else {
      x = in + 0.07f;
      if (x > 0.0f)
        x = 0.0f;
    }
    spd = this->speed;
    BrMat4MulVec3(vec, this->mat, this->vel);
    v = vec[0];
    if (v < 10.0f)
      v = 10.0f;
    if (v > 70.0f)
      v = 70.0f;
    spd = BrClampf(spd, 140.0f, 340.0f) - 140.0f;
    switch (this->handling) {
    case 0:
      DAT_100b2e78 = 10.0f;
      DAT_100b2e70 = 14.0f;
      DAT_100b2e74 = 0.008f;
      k = 6.0f;
      break;
    case 1:
      DAT_100b2e78 = 10.0f;
      DAT_100b2e70 = 14.0f;
      DAT_100b2e74 = 0.01f;
      k = 5.0f;
      break;
    case 2:
      DAT_100b2e78 = 10.0f;
      DAT_100b2e70 = 14.0f;
      DAT_100b2e74 = 0.013f;
      k = 4.0f;
      break;
    case 3:
      DAT_100b2e78 = 10.0f;
      DAT_100b2e70 = 14.0f;
      DAT_100b2e74 = 0.017f;
      k = 3.0f;
      break;
    case 4:
      DAT_100b2e78 = 10.0f;
      DAT_100b2e70 = 14.0f;
      DAT_100b2e74 = 0.022f;
      k = 3.0f;
      break;
    case 5:
      DAT_100b2e78 = 10.0f;
      DAT_100b2e70 = 14.0f;
      DAT_100b2e74 = 0.03f;
      k = 3.0f;
      break;
    case 6:
      DAT_100b2e78 = 10.0f;
      DAT_100b2e70 = 14.0f;
      DAT_100b2e74 = 0.05f;
      k = 3.0f;
      break;
    default:
      DAT_100b2e70 = 17.0f;
      DAT_100b2e74 = 0.07f;
      DAT_100b2e78 = 14.0f;
      k = 16.0f;
      break;
    }
    rate = DAT_100b2e74;
    DAT_10ac67cc = 0;
    v = DAT_100b2e70 - DAT_100b2e78 * (v * 0.011111111f);
    if (BR_ABS(x) < 0.001f)
      t = 0.0f;
    else if (x < -0.75f)
      t = v * 0.017453292f;
    else if (x > 0.75f)
      t = v * -0.017453292f;
    else
      t = -(x * (k * 0.017453292f));
    x = v * 0.017453292f;
    if (t > x)
      t = x;
    v = v * -0.017453292f;
    if (t < v)
      t = v;
    if (BR_SGN(t) != BR_SGN(this->steer) ||
        (this->steer > 0.0f && t < this->steer) ||
        (this->steer < 0.0f && t > this->steer))
      turn = 1;
    else
      turn = 0;
    if (this->steer == 0.0f) {
      turn = 0;
      this->steerDir = 0;
    }
    if (t < this->steer && this->steerDir < 0)
      turn = 1;
    if (t > this->steer && this->steerDir > 0)
      turn = 1;
    if (turn != 0) {
      if (t < this->steer)
        this->steerDir = -1;
      else
        this->steerDir = 1;
      rate = 1.0f;
      if (BR_SGN(this->steer) != BR_SGN(t))
        t = 0.0f;
    } else {
      this->steerDir = 0;
    }
    if (BR_ABS(this->steer - t) < rate)
      this->steer = t;
    else if (t < this->steer)
      this->steer = this->steer - rate;
    else
      this->steer = this->steer + rate;
  } else {
    v = BR_ABS(in);
    x = 1.0f;
    if (DAT_100b2e6c != 0) {
      if (this->speed <= 50.0f)
        x = 1.0f;
      else if (this->speed >= 100.0f)
        x = 0.95f;
      else
        x = 1.0f - (this->speed - 50.0f) * 0.001f;
    }
    switch (this->handling) {
    case 0: {
      float s = BR_SGN(in);
      d = ((float)pow(11.0, v) - 1.0f) * s * 0.1f;
      this->steer = d * x * -0.07853982f;
      }
      break;
    case 1: {
      float s = BR_SGN(in);
      d = ((float)pow(11.0, v) - 1.0f) * s * 0.1f;
      this->steer = d * x * -0.08726647f;
      }
      break;
    case 2: {
      float s = BR_SGN(in);
      d = ((float)pow(2.0, v) - 1.0f) * s;
      this->steer = d * x * -0.08726647f;
      }
      break;
    case 3: {
      float s = BR_SGN(in);
      d = ((float)pow(2.0, v) - 1.0f) * s;
      this->steer = d * x * -0.09599311f;
      }
      break;
    case 4:
      this->steer = x * in * -0.08726647f;
      break;
    case 5:
      this->steer = x * in * -0.10471976f;
      break;
    case 6:
      this->steer = x * in * -0.12217305f;
      break;
    default:
      t = BR_SGN(in);
      d = t * (in * in);
      this->steer = d * x * -0.1308997f;
      break;
    }
  }

  v = this->rpm;
  if (this->rpm < 800.0f)
    this->rpm = 800.0f;
  if (this->autoBox != 0) {
    lo = 4000.0f;
    hi = 6000.0f;
    if ((DAT_10b71534[6] & 0x8000) != 0) {
      z = this->pPad->throttle;
      if (z > 0.4f) {
        hi = 4000.0f - z * -2000.0f;
        lo = hi - 2000.0f;
      }
    }
    gear = this->gear;
    if (gear > 1 && lo > this->rpm)
      this->gear = gear - 1;
    else if (gear < this->gearTop && this->rpm > hi &&
             (this->pPad->flags & 0x20000) == 0)
      this->gear = gear + 1;
  } else {
    if (this->gear > 0 && (this->pPad->flags & 0x200000) != 0) {
      this->pPad->m_1002F640(0x200000);
      this->gear = this->gear - 1;
    } else if (this->gear < this->gearTop && (this->pPad->flags & 0x100000) != 0 &&
               (this->pPad->flags & 0x20000) == 0) {
      this->pPad->m_1002F640(0x100000);
      this->gear = this->gear + 1;
    }
  }

  bonus = 0.0f;
  if (DAT_100a9360 == 1) {
    if (this->rubber != 0) {
      best = 0.0f;
      for (n = 0; n < DAT_100b2f00; n++) {
        q = &DAT_10af0858[n * 0x20];
        if (*q != 0) {
          if (best < *(float *)(*q + 0xff4))
            best = *(float *)(*q + 0xff4);
        } else if (best < *(float *)(q - 4)) {
          best = *(float *)(q - 4);
        }
      }
      bonus = BR_ABS(best - this->dist) * 0.5f - 18.0f;
      if (bonus < 0.0f)
        bonus = 0.0f;
      else if (bonus > 30.0f)
        bonus = 30.0f;
    }
  } else if (DAT_100a9360 == 6) {
    if (this->rubber == 0)
      bonus = 0.0f;
    else if (this->rubber == 1)
      bonus = 20.0f;
    else
      bonus = 30.0f;
  }
  acc = ((this->torque[0] * this->rpm + this->torque[1]) * this->rpm + this->torque[2]) *
        this->rpm + this->torque[3] + bonus;
  if (this->autoBox == 0)
    acc = acc * 1.03f;
  if ((this->pPad->flags & 0x10000) == 0)
    acc = 0.0f;
  mul = 7.0f;
  if ((DAT_10b71534[6] & 0x8000) != 0) {
    z2 = this->pPad->throttle;
    if (z2 > 0.0f)
      mul = z2 * 7.0f;
  }
  this->force = mul * acc;
  if ((this->pState->flags68 & 1) != 0)
    this->gear = 0;
  else if (this->gear == 0)
    this->gear = 1;
  gear = this->gear;
  if (gear != 0) {
    if ((this->pPad->flags & 0x20000) != 0)
      this->rpm = this->ratio[1] * this->finalDrive * (this->wheelSpin * 0.15915494f) * -60.0f;
    else
      this->rpm = this->ratio[gear] * this->finalDrive * (this->wheelSpin * 0.15915494f) * -60.0f;
  } else {
    if ((this->pPad->flags & 0x10000) != 0)
      this->rpm = this->rpm + 300.0f;
    else
      this->rpm = this->rpm - 200.0f;
    this->force = 0.0f;
  }
  if (this->rpm < 0.0f)
    this->rpm = -this->rpm;
  if (this->rpm < 900.0f)
    this->rpm = 900.0f;
  if (this->rpm > 8000.0f)
    this->rpm = 8000.0f;
  if ((DAT_10b71534[6] & 0x8000) != 0 && gear == 0) {
    lim = this->pPad->throttle * 8000.0f;
    if (lim < 900.0f)
      lim = 900.0f;
    if (lim < this->rpm)
      this->rpm = lim;
  }
  d = this->rpm - v;
  if (BR_ABS(d) > 400.0f)
    d = BR_SGN(d) * 400.0f;
  this->brake = 0.0f;
  this->rpm = d + v;
  if ((this->pPad->flags & 0x40000) != 0)
    this->brake = -140000.0f;
}
