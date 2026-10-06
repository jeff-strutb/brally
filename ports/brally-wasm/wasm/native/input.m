/* input.m -- game controllers (ports/brally-wasm/NATIVE_RENDERER.md 4.4).
 *
 * The game reads a joystick through DirectInput: it enumerates joystick
 * devices once, opens the first, sets its X and Y ranges, and polls a
 * DIJOYSTATE2 every frame; which axis or button drives which control is the
 * player's choice in the game's own Controls menu.  host_dx.c reports one
 * joystick and answers its polls from here, so any controller macOS knows
 * (GameController.framework: Xbox, PlayStation, MFi, Switch Pro) works
 * through the game's own binding, in the layout Windows gives an XInput pad
 * under DirectInput:
 *
 *   X, Y     left stick (Y down is positive, as DirectInput reports it)
 *   Z        right trigger minus left trigger
 *   buttons  0 A  1 B  2 X  3 Y  4 LB  5 RB  6 view  7 menu  8 L3  9 R3
 *            10 LT  11 RT (pressed past half)  12-15 d-pad up/right/down/left
 *   hat      the d-pad
 *
 * The first connected controller is read; controllers plugged in while the
 * game runs are picked up at the next poll.
 */
#import <GameController/GameController.h>

typedef struct {
    float x, y, z;                /* -1..1 */
    unsigned buttons;             /* bit n = button n */
    int pov;                      /* hundredths of a degree, -1 centred */
} hpad;

static GCExtendedGamepad *pad(void)
{
    static int init;
    GCController *c;
    if (!init) {
        init = 1;
        GCController.shouldMonitorBackgroundEvents = NO;
    }
    c = GCController.current;
    if (!c || !c.extendedGamepad)
        for (GCController *k in GCController.controllers)
            if (k.extendedGamepad) { c = k; break; }
    return c ? c.extendedGamepad : nil;
}

/* Called by host_dx.c when the game polls the joystick. 0: none connected.
 * BR_PADFAKE=x,y,z,buttons holds a fixed state instead (checks without a
 * controller attached). */
int hinput_pad(hpad *o)
{
    GCExtendedGamepad *g;
    int u, r, d, l;
    unsigned b = 0;
    const char *fake = getenv("BR_PADFAKE");
    memset(o, 0, sizeof *o);
    o->pov = -1;
    if (fake) {
        sscanf(fake, "%f,%f,%f,%x", &o->x, &o->y, &o->z, &o->buttons);
        return 1;
    }
    g = pad();
    if (!g) return 0;
    o->x = g.leftThumbstick.xAxis.value;
    o->y = -g.leftThumbstick.yAxis.value;
    o->z = g.rightTrigger.value - g.leftTrigger.value;
    if (g.buttonA.pressed) b |= 1u << 0;
    if (g.buttonB.pressed) b |= 1u << 1;
    if (g.buttonX.pressed) b |= 1u << 2;
    if (g.buttonY.pressed) b |= 1u << 3;
    if (g.leftShoulder.pressed) b |= 1u << 4;
    if (g.rightShoulder.pressed) b |= 1u << 5;
    if (g.buttonOptions.pressed) b |= 1u << 6;
    if (g.buttonMenu.pressed) b |= 1u << 7;
    if (g.leftThumbstickButton.pressed) b |= 1u << 8;
    if (g.rightThumbstickButton.pressed) b |= 1u << 9;
    if (g.leftTrigger.value > 0.5f) b |= 1u << 10;
    if (g.rightTrigger.value > 0.5f) b |= 1u << 11;
    u = g.dpad.up.pressed; r = g.dpad.right.pressed; d = g.dpad.down.pressed; l = g.dpad.left.pressed;
    if (u) b |= 1u << 12;
    if (r) b |= 1u << 13;
    if (d) b |= 1u << 14;
    if (l) b |= 1u << 15;
    o->buttons = b;
    if (u || r || d || l) {
        int dx = r - l, dy = u - d;                       /* up is 0, clockwise */
        static const int ang[3][3] = { { 22500, 27000, 31500 }, { 18000, -1, 0 }, { 13500, 9000, 4500 } };
        o->pov = ang[dx + 1][dy + 1];
    }
    return 1;
}
