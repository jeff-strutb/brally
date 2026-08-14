/* slice1_10.h -- force-feedback teardown, decompiled from BRD3D.dll.
 *
 * Agent 10's packet covered 14 functions in 0x10079550-0x10086A10. Thirteen
 * of them turned out to be statically linked MSVC CRT (see slice1_10.c for
 * the address-by-address identification and the evidence for each). Exactly
 * one is game code:
 *
 *   0x10079550  BrFfbShutdown
 *
 * WHAT IT IS
 * ----------
 * The subsystem was identified from data reachable only from its sibling
 * initialiser at 0x100791D0 (same module, 0x100791D0-0x100795D0):
 *
 *   - error strings "Error: IDirectInputDevice::SetProperty(DIPH_RANGE)
 *     FAILED" (0x100BD528) and "...(DIPH_WORD) FAILED" (0x100BD4EC)
 *   - two 16-byte GUIDs passed to vtable slot 18 (+0x48):
 *       0x100907D0 = {13541C27-8E33-11D0-9AD0-00A0C9A06E35} = GUID_Spring
 *       0x10090780 = {13541C22-8E33-11D0-9AD0-00A0C9A06E35} = GUID_Square
 *   - two DIEFFECT descriptors built inline with dwSize = 0x34 (= sizeof
 *     DIEFFECT), dwFlags = 0x12 (DIEFF_OBJECTOFFSETS|DIEFF_CARTESIAN),
 *     dwDuration = 0xFFFFFFFF (INFINITE), and cbTypeSpecificParams of 0x30
 *     (two DICONDITIONs, i.e. a two-axis spring) and 0x10 (one DIPERIODIC).
 *   - a DIPROPDWORD (dwSize 0x14, dwHeaderSize 0x10, dwHow 0 = DIPH_DEVICE,
 *     dwData 0) sent to property 9 = DIPROP_AUTOCENTER, i.e. autocentre off.
 *
 * So: a DirectInput force-feedback wheel, one IDirectInputDevice2 plus two
 * IDirectInputEffects. Slot 18 (+0x48) is IDirectInputDevice2::CreateEffect,
 * which is what pins the interface version.
 *
 * WHAT IT DOES
 * ------------
 * Nested-init teardown. A counter is incremented by 0x100791D0 and only the
 * transition to 1 initialises; here it is decremented and only the transition
 * to 0 tears down. See the gotchas on BrFfbShutdown below -- the counter is
 * NOT symmetric with the initialiser.
 */
#ifndef SLICE1_10_H
#define SLICE1_10_H

typedef struct BrDiObj BrDiObj;

/* A COM method taking only `this`. Both methods this file calls are of this
 * shape (`push obj; call [vtbl+n]` with nothing else pushed). The original
 * discards both return values; the types differ in COM (Release returns
 * ULONG, Unacquire returns HRESULT) but neither is examined, so one signature
 * covers both.
 *
 * NOTE: what is preserved from the original is the vtable SLOT INDEX, not the
 * byte offset -- on a 64-bit host these fields are 8 bytes apart, so slot 8 is
 * at +0x40, not +0x20. Every comment below gives the original x86 offset. */
typedef long (*BrDiMethod0)(BrDiObj *pThis);

/* Only the two slots this translation unit calls are typed. The rest are
 * deliberately left as `void *` rather than given plausible-looking
 * signatures we would not be reproducing faithfully. */
typedef struct BrDiVtbl {
    void        *pfnSlot0;      /* +0x00  QueryInterface */
    void        *pfnSlot1;      /* +0x04  AddRef */
    BrDiMethod0  pfnRelease;    /* +0x08  Release */
    void        *pfnSlot3;      /* +0x0C */
    void        *pfnSlot4;      /* +0x10 */
    void        *pfnSlot5;      /* +0x14 */
    void        *pfnSlot6;      /* +0x18  IDirectInputDevice::SetProperty */
    void        *pfnSlot7;      /* +0x1C  IDirectInputDevice::Acquire */
    BrDiMethod0  pfnUnacquire;  /* +0x20  IDirectInputDevice::Unacquire */
} BrDiVtbl;

struct BrDiObj {
    const BrDiVtbl *pVtbl;      /* +0x00 -- the original does `mov ecx,[eax]` */
};

/* The four globals the routine owns, in ascending address order.
 *
 * An effect pointer being non-NULL does not imply the device is: the
 * initialiser's failure paths release and NULL the device while leaving the
 * counter raised, so this struct can legitimately hold a raised count and a
 * NULL device. Each field is tested independently, which is why. */
typedef struct BrFfb {
    BrDiObj *pDevice;        /* 0x118ABDD4  IDirectInputDevice2 */
    BrDiObj *pEffectSpring;  /* 0x118ABDEC  created with GUID_Spring */
    BrDiObj *pEffectSquare;  /* 0x118ABDFC  created with GUID_Square */
    int      initCount;      /* 0x118ABE00  nested-init depth */
} BrFfb;

/* 0x10079550  release the force-feedback objects when the last user leaves.
 *
 *   initCount--;
 *   if (initCount < 0) { initCount = 0; return; }   <-- clamp, NO teardown
 *   if (initCount != 0) return;
 *   release square effect, then spring effect, then unacquire+release device
 *
 * GOTCHAS, all verified against the original:
 *
 * 1. THE UNDERFLOW CLAMP DOES NOT TEAR DOWN. Calling this with initCount
 *    already 0 sets the count to -1, notices the sign, writes 0 back and
 *    RETURNS -- it does not fall through to the release code. So a stray
 *    extra shutdown is silently swallowed rather than double-releasing.
 *
 * 2. THE COUNTER IS NOT SYMMETRIC WITH THE INITIALISER. 0x100791D0 checks a
 *    separate enable flag (0x10B4E1D0) BEFORE it increments, and returns
 *    early without incrementing when force feedback is off. This function has
 *    no such check and always decrements. Clamp (1) is what stops the counter
 *    going permanently negative on a no-FFB machine; it is load-bearing, not
 *    defensive padding.
 *
 * 3. RELEASE ORDER IS FIXED: effects before device, and among the effects
 *    0x118ABDFC (square) before 0x118ABDEC (spring) -- the reverse of the
 *    order the initialiser creates them in. Effects hold a reference on the
 *    device, so the effects-first half of this matters.
 *
 * 4. THE DEVICE POINTER IS RE-READ BETWEEN Unacquire AND Release. The
 *    original reloads the global at 0x100795B2 instead of reusing the
 *    register, and does NOT re-test it for NULL. Reproduced exactly: if a
 *    caller's Unacquire clears pDevice, this dereferences NULL, just as the
 *    original would. Not fixed -- see the DEVIATION policy in CONTRACT.md;
 *    silently adding a guard would hide a real difference.
 *
 * 5. Each of the three pointers is NULL-tested and NULLed independently, so
 *    partially constructed state tears down cleanly and a second full
 *    teardown is a no-op.
 *
 * Passing pFfb == NULL is not something the original can express (its state
 * is four globals); this port asserts nothing and will fault, matching the
 * cost of the original's unconditional loads. */
void BrFfbShutdown(BrFfb *pFfb);

#endif /* SLICE1_10_H */
