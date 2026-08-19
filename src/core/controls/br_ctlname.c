/* br_ctlname.c -- see br_ctlname.h.
 *
 * RESPONSIBILITY: reading what the player is doing -- specifically, naming
 * the things that can be bound.
 */
#include "br_ctlname.h"

#include "slice4_52.h"   /* BrStrGet, D3D 0x10074030 == Glide 0x1006D280 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ==========================================================================
 * Storage this module owns
 * ========================================================================== */

BrCfgRec g_aBrCtlNameJoy[BR_CTLNAME_JOY_COUNT];       /* Glide 0x10B71C70 */
BrCfgRec g_aBrCtlNameMouse[BR_CTLNAME_MOUSE_COUNT];   /* Glide 0x10B71B08 */

/* Glide 0x100B3B40, 120 records of 0x24, transcribed byte for byte out of
 * BRGlide.dll's .data.  Every name's tail is zero-filled in the image and no
 * name exceeds 13 characters.
 *
 * These are DirectInput DIK_* scan codes and the names are the game's own
 * spellings -- "KEYPAD *", "APP MENU", a bare "\\" for DIK_BACKSLASH. */
const BrCfgRec g_aBrCtlNameKey[BR_CTLNAME_KEY_COUNT] = {
    { 0x01, "ESCAPE" },
    { 0x02, "1" },
    { 0x03, "2" },
    { 0x04, "3" },
    { 0x05, "4" },
    { 0x06, "5" },
    { 0x07, "6" },
    { 0x08, "7" },
    { 0x09, "8" },
    { 0x0A, "9" },
    { 0x0B, "0" },
    { 0x0C, "-" },
    { 0x0D, "=" },
    { 0x0E, "BACKSPACE" },
    { 0x0F, "TAB" },
    { 0x10, "Q" },
    { 0x11, "W" },
    { 0x12, "E" },
    { 0x13, "R" },
    { 0x14, "T" },
    { 0x15, "Y" },
    { 0x16, "U" },
    { 0x17, "I" },
    { 0x18, "O" },
    { 0x19, "P" },
    { 0x1A, "[" },
    { 0x1B, "]" },
    { 0x1C, "ENTER" },
    { 0x1D, "LEFT CONTROL" },
    { 0x1E, "A" },
    { 0x1F, "S" },
    { 0x20, "D" },
    { 0x21, "F" },
    { 0x22, "G" },
    { 0x23, "H" },
    { 0x24, "J" },
    { 0x25, "K" },
    { 0x26, "L" },
    { 0x27, ";" },
    { 0x28, "'" },
    { 0x29, "`" },
    { 0x2A, "LEFT SHIFT" },
    { 0x2B, "\\" },
    { 0x2C, "Z" },
    { 0x2D, "X" },
    { 0x2E, "C" },
    { 0x2F, "V" },
    { 0x30, "B" },
    { 0x31, "N" },
    { 0x32, "M" },
    { 0x33, "," },
    { 0x34, "." },
    { 0x35, "/" },
    { 0x36, "RIGHT SHIFT" },
    { 0x37, "*" },
    { 0x38, "LEFT ALT" },
    { 0x39, "SPACE" },
    { 0x3A, "CAPS LOCK" },
    { 0x3B, "F1" },
    { 0x3C, "F2" },
    { 0x3D, "F3" },
    { 0x3E, "F4" },
    { 0x3F, "F5" },
    { 0x40, "F6" },
    { 0x41, "F7" },
    { 0x42, "F8" },
    { 0x43, "F9" },
    { 0x44, "F10" },
    { 0x45, "NUMLOCK" },
    { 0x46, "SCROLL LOCK" },
    { 0x47, "NUMPAD 7" },
    { 0x48, "NUMPAD 8" },
    { 0x49, "NUMPAD 9" },
    { 0x4A, "NUMPAD -" },
    { 0x4B, "NUMPAD 4" },
    { 0x4C, "NUMPAD 5" },
    { 0x4D, "NUMPAD 6" },
    { 0x4E, "NUMPAD +" },
    { 0x4F, "NUMPAD 1" },
    { 0x50, "NUMPAD 2" },
    { 0x51, "NUMPAD 3" },
    { 0x52, "NUMPAD 0" },
    { 0x53, "NUMPAD ." },
    { 0x57, "F11" },
    { 0x58, "F12" },
    { 0x64, "F13" },
    { 0x65, "F14" },
    { 0x66, "F15" },
    { 0x70, "KANA" },
    { 0x79, "CONVERT" },
    { 0x7B, "NOCONVERT" },
    { 0x7D, "YEN" },
    { 0x8D, "NUMPAD =" },
    { 0x90, "CIRCUMFLEX" },
    { 0x91, "AT" },
    { 0x92, ":" },
    { 0x93, "_" },
    { 0x94, "KANJI" },
    { 0x95, "STOP" },
    { 0x96, "AX" },
    { 0x97, "UNLABELED" },
    { 0x9C, "NUMPAD ENTER" },
    { 0x9D, "RIGHT CONTROL" },
    { 0xB3, "NUMPAD ," },
    { 0xB5, "/" },
    { 0xB7, "SYSRQ" },
    { 0xB8, "RIGHT ALT" },
    { 0xC7, "KEYPAD HOME" },
    { 0xC8, "UP ARROW" },
    { 0xC9, "KEYPAD PGUP" },
    { 0xCB, "LEFT ARROW" },
    { 0xCD, "RIGHT ARROW" },
    { 0xCF, "KEYPAD END" },
    { 0xD0, "DOWN ARROW" },
    { 0xD1, "KEYPAD PGDN" },
    { 0xD2, "KEYPAD INSERT" },
    { 0xD3, "KEYPAD DELETE" },
    { 0xDB, "LEFT WINDOWS" },
    { 0xDC, "RIGHT WINDOWS" },
    { 0xDD, "APP MENU" },
};

/* ==========================================================================
 * 0x10058AF0 -- build the joystick and mouse name tables
 * ========================================================================== */

/* The original's `call [sprintf]` with a RUNTIME format string, which is what
 * BrStrGet returns.  Wrapped rather than called inline so that the non-literal
 * format does not trip -Wformat-security; the behaviour, including the absence
 * of any bound on the 32-byte destination, is the original's.
 *
 * Both call sites are here: 0x10058B36 passes one integer argument and
 * 0x10058B94 passes none. */
static void ctlname_sprintf(char *pszDst, const char *pszFmt, ...)
{
    va_list ap;
    va_start(ap, pszFmt);
    vsprintf(pszDst, pszFmt, ap);
    va_end(ap);
}

/* The tail shared by both arms of both loops.  `code` is the value already
 * stored in the record; the select is the original's
 *     add eax, -base ; cmp eax, 5 ; ja skip ; jmp [table + eax*4]
 * so it is UNSIGNED and a code below the base wraps to a huge number and is
 * skipped rather than indexing backwards. */
static void ctlname_axis(BrCfgRec *pRec, uint32_t code, uint32_t base)
{
    uint32_t sel = code - base;

    if (sel <= (uint32_t)(BR_CTLNAME_STR_AXIS_COUNT - 1)) {
        ctlname_sprintf(pRec->szText,
                        BrStrGet((int)(BR_CTLNAME_STR_AXIS_FIRST + sel)));
    }
    /* else: the record keeps its key and whatever text the clear left. */
}

void BrCtlNameInit(void)
{
    int32_t i;

    /* 0x10058AF4 and 0x10058B02.  DWORD counts; deliberately short of both
     * tables -- br_ctlname.h says why that is kept. */
    memset(g_aBrCtlNameMouse, 0, BR_CTLNAME_MOUSE_CLEAR);
    memset(g_aBrCtlNameJoy,   0, BR_CTLNAME_JOY_CLEAR);

    /* ---- 0x10058B16 .. 0x10058BA3 -- the MOUSE table, 10 records ------- *
     * esi starts at &mouse[0].szText and the record's key is written
     * through [esi-4].  The button/axis split is a comparison of esi
     * against 0x10B71B9C, i.e. index 4. */
    for (i = 0; i < BR_CTLNAME_MOUSE_COUNT; i++) {
        if (i < BR_CTLNAME_MOUSE_BUTTONS) {
            /* 0x10058B23.  The `push ebx` at 0x10058B23 is the sprintf
             * VARARG and sits UNDER the string id pushed at 0x10058B24 --
             * the `add esp,4` at 0x10058B31 pops only the id, so the three
             * words the `add esp,0xc` at 0x10058B38 removes are
             * (buffer, format, index).  Tracing that is the only way to see
             * this is a one-argument sprintf and not a two-argument one. */
            g_aBrCtlNameMouse[i].key = (uint32_t)i;
            ctlname_sprintf(g_aBrCtlNameMouse[i].szText,
                            BrStrGet(BR_CTLNAME_STR_BUTTON), (int)i);
        } else {
            /* 0x10058B3D.  `mov al,bl / sub al,0x7E` into a one-byte local
             * that is then read back as a DWORD and masked with 0xFF -- so
             * the three high bytes of that read are uninitialised and
             * discarded, and the value is (unsigned char)(i - 0x7E). */
            uint32_t code = (uint32_t)(uint8_t)(i - 0x7E);

            g_aBrCtlNameMouse[i].key = code;
            ctlname_axis(&g_aBrCtlNameMouse[i], code,
                         BR_CTLNAME_MOUSE_AXIS_BASE);
        }
    }

    /* ---- 0x10058BAB .. 0x10058C2C -- the JOYSTICK table, 134 records --- *
     * The same shape with a different split (index 128) and a different
     * axis code base. */
    for (i = 0; i < BR_CTLNAME_JOY_COUNT; i++) {
        if (i < BR_CTLNAME_JOY_BUTTONS) {
            g_aBrCtlNameJoy[i].key = (uint32_t)i;
            ctlname_sprintf(g_aBrCtlNameJoy[i].szText,
                            BrStrGet(BR_CTLNAME_STR_BUTTON), (int)i);
        } else {
            /* 0x10058BD2.  `mov eax,ebx / and eax,0xFF` -- no offset here,
             * which is the whole of the difference from the mouse arm. */
            uint32_t code = (uint32_t)(uint8_t)i;

            g_aBrCtlNameJoy[i].key = code;
            ctlname_axis(&g_aBrCtlNameJoy[i], code, BR_CTLNAME_JOY_AXIS_BASE);
        }
    }
}

/* ==========================================================================
 * The three tables as slice2_23.c wants them
 * ========================================================================== */

const BrCfgTables *BrCtlNameTables(void)
{
    static const BrCfgTables T = {
        g_aBrCtlNameKey,     /* kind 0     */
        g_aBrCtlNameJoy,     /* kinds 1, 2 */
        g_aBrCtlNameMouse    /* kind 3     */
    };
    return &T;
}
