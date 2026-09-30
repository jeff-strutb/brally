/* car.m -- the Remastered player car's seam into the display list (port code).
 *
 * While the Remastered renderer is on (host_fx.m, the ~ key) and a
 * replacement model is installed (host_car.m), every car given the ES (model
 * 1: the player's default and the Quick Race opponent) is drawn as that model
 * instead of its .rca display lists.  Everything else
 * about the car is still the game's: BrCarDrawVehicle runs as the original,
 * with the model record's body, glass, detail and reflection lists pointed at
 * an empty list and its wheel list cleared (which skips BrCarDrawWheels), so
 * the matrices, lighting slots, LOD class and every other side effect happen
 * exactly as before.  If it emitted anything -- it returns early for the
 * in-car cameras -- one marker command follows in the list:
 *
 *     BC000008 CA5Ennnn     G_MOVEWORD index 8, a no-op in BrGbiMoveWord
 *
 * nnnn names the car's transforms as they were when the list was built
 * (host_car.m keeps them), and the marker draws the model when the list
 * runs, in the list's order and under whichever view's projection is current
 * then (main view or the rear-view mirror).  Without this file the marker is
 * never emitted, and the original handler treats it as the no-op it is.
 */
#include <stdlib.h>
#include <string.h>
#include "w2c_native.h"

int hcar_enabled(void);
int hcar_record(u32 car);
void hcar_draw(int slot);
void hfx_car_seen(u32 car);
u32 hmem_alloc(u32 n, int zero);

#define LIST_CURSOR   0x106E7710u    /* DAT_106e7710, the list write cursor      */
#define VIEW_CAR      0x106E9D88u    /* BrG_6C2CF8, the car the current view follows */
#define CAR_MODEL     0x29C4u        /* car -> its .rca model record            */
#define CAR_MODELIDX  0x29A8u        /* the model it was given (0 CE, 1 ES, ...) */
#define REMASTERED_MODEL 1u          /* the ES: the model host_car.m has */
#define MARK_W0       0xBC000008u
#define MARK_TAG      0xCA5E0000u

/* the model record's display lists the replacement stands in for, per LOD */
static const u32 BODY_LISTS[] = { 0x8024, 0x8028, 0x8030, 0x8038, 0x803C };
#define LOD_STRIDE 0x28u
#define WHEEL_LIST 0x80BCu

static u32 g_empty;                  /* an 8-byte G_ENDDL in game memory */

/* WHY: the seam.  The original draws the car from its .rca lists; with the
 * Remastered model on, the same call runs with those lists emptied and a
 * marker queued where the car belongs in the list. */
/* @replaces 0x1000A110 BrCarDrawVehicle */
void n_BrCarDrawVehicle(u32 car, u32 lodBias)
{
    u32 model, saved[3][5], wheel, before, after;
    int lod, i, slot;
    hfx_car_seen(car);                  /* host_fx.m: every car's headlights */
    if (!hcar_enabled() || W_LD(u32, car, CAR_MODELIDX) != REMASTERED_MODEL || !(model = W_LD(u32, car, CAR_MODEL))) {
        W_ORIG_BrCarDrawVehicle(car, lodBias);
        return;
    }
    if (!g_empty) {
        g_empty = hmem_alloc(8, 1);
        W_ST(u32, g_empty, 0, 0xB8000000u);
        W_ST(u32, g_empty, 4, 0);
    }
    for (lod = 0; lod < 3; lod++)
        for (i = 0; i < 5; i++) {
            u32 a = BODY_LISTS[i] + (u32)lod * LOD_STRIDE;
            saved[lod][i] = W_LD(u32, model, a);
            if (saved[lod][i]) W_ST(u32, model, a, g_empty);
        }
    wheel = W_LD(u32, model, WHEEL_LIST);
    W_ST(u32, model, WHEEL_LIST, 0);
    before = W_LD(u32, LIST_CURSOR, 0);
    W_ORIG_BrCarDrawVehicle(car, lodBias);
    after = W_LD(u32, LIST_CURSOR, 0);
    for (lod = 0; lod < 3; lod++)
        for (i = 0; i < 5; i++)
            W_ST(u32, model, BODY_LISTS[i] + (u32)lod * LOD_STRIDE, saved[lod][i]);
    W_ST(u32, model, WHEEL_LIST, wheel);
    if (after == before || (slot = hcar_record(car)) < 0)
        return;
    W_ST(u32, after, 0, MARK_W0);
    W_ST(u32, after, 4, MARK_TAG | (u32)slot);
    W_ST(u32, LIST_CURSOR, 0, after + 8);
}

/* WHY: the marker's handler; every other G_MOVEWORD is the original's. */
/* @replaces 0x100239C0 BrGbiMoveWord */
u32 n_BrGbiMoveWord(u32 p)
{
    u32 w1 = W_LD(u32, p, 4);
    if (W_LD(u32, p, 0) == MARK_W0 && (w1 & 0xFFFF0000u) == MARK_TAG) {
        hcar_draw((int)(w1 & 0xFFFFu));
        return p + 8;
    }
    return W_ORIG_BrGbiMoveWord(p);
}
