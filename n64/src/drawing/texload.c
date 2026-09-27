/* texload.c -- preparing RSP/RDP inputs: matrices scaled into the
 * fixed-point range, and texture loads
 */
#include "tgr/common.h"

/* -- declarations -- */
int sprintf(char *buf, char *fmt, ...);
void BrFatal(char *msg);
/* -- end declarations -- */

/* WHAT IT DOES: Scale a 4x4 matrix so the largest magnitude among its
 * rotation rows fits the RSP's fixed-point range (just under 2). */
/* @implements 0x802174B4 tgr BrMat4FitRange */
void BrMat4FitRange(float m[16])
{
  float *p;
  float max;
  float min;

  min = max = 0.0f;
  for (p = m; p < m + 12; p++) {
    if (*p < 0.0f) {
      if (*p < min) {
        min = *p;
      }
    } else if (max < *p) {
      max = *p;
    }
  }
  min = -min;
  if (max < min) {
    max = min;
  }
  max = 1.9997559f / (max + 0.375f);
  m[0] *= max;
  m[1] *= max;
  m[2] *= max;
  m[3] *= max;
  m[4] *= max;
  m[5] *= max;
  m[6] *= max;
  m[7] *= max;
  m[8] *= max;
  m[9] *= max;
  m[10] *= max;
  m[11] *= max;
  m[12] *= max;
  m[13] *= max;
  m[14] *= max;
  m[15] *= max;
}

/* WHAT IT DOES: For a texture v texels wide (1 to 1024), give the number
 * of address bits the RDP needs to wrap it (log2 rounded up) and a full
 * 16-bit mask; a larger size is a fatal error. */
/* @implements 0x80217614 tgr BrTexSizeBits */
void BrTexSizeBits(unsigned int v, int *mask, int *bits)
{
  char msg[40];

  v--;
  if (v >> 8) {
    if (v >> 10) {
      sprintf(msg, "ERROR: unhandled texture size: %d", v);
      BrFatal(msg);
    } else if (v >> 9) {
      *bits = 10;
    } else {
      *bits = 9;
    }
  } else if (v & 0xf0) {
    if (v & 0xc0) {
      if (v & 0x80) {
        *bits = 8;
      } else {
        *bits = 7;
      }
    } else if (v & 0xe0) {
      *bits = 6;
    } else {
      *bits = 5;
    }
  } else if (v & 0xfc) {
    if (v & 0xf8) {
      *bits = 4;
    } else {
      *bits = 3;
    }
  } else if (v & 0xfe) {
    *bits = 2;
  } else if (v) {
    *bits = 1;
  } else {
    *bits = 0;
  }
  *mask = 0xffff;
}
