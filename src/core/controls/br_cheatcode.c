/* br_cheatcode.c -- controls: the typed-cheat-code scanner (0x10040A90).
 *
 * RESPONSIBILITY: watching the ring of recently typed characters for one of
 * the built-in code words and firing that word's handler.
 *
 * The two data structures are read straight out of the original:
 *   0x100ABE48  a NULL-terminated table of { handler, text } pairs.  Ghidra
 *               named the first text pointer `PTR_s_madeleine_100abe4c`, so
 *               "madeleine" is code word #1 and this is unambiguously the
 *               cheat table.
 *   0x10AC51D8  a 32-entry ring of 4-byte key records; only the first byte
 *               (the character) is read here.  0x10AC5DA0 is the ring's
 *               write position, and the scan reads BACKWARDS from it:
 *               index (i - len + pos) & 31 lines the code word's last
 *               character up with the most recently typed one.
 *
 * RESIDUE (T3a, size-exact 91/91, every block boundary on the original's
 * offsets -- loop top 0x19, inner top 0x29, call 0x43, reload 0x46, bottom
 * 0x4c).  ONE instruction short (37 vs 38), and it is a single trade:
 *
 *      orig    19  mov edi, esi          <- copy `s` into edi for the
 *                                           inlined strlen's repne scasb
 *              29  mov dl, [eax+esi]     <- and index the SAME esi
 *              2c  mov edi, eax / sub edi, ecx / add edi, ebp
 *      here    19  (nothing -- `s` already lives in edi)
 *              27  mov esi, [ebx]        <- so the indexed read reloads it
 *              2c  lea edi, [ebp+eax] / sub edi, ecx
 *
 * 2 + 2 + 2 == 2 + 4, which is why the byte count still lands exactly.  To
 * close it, `s` has to be allocated to a callee-saved register that is NOT
 * edi, and the index sum has to stay mov/sub/add instead of folding into a
 * lea.
 *
 * WHAT IS ALREADY BOUGHT, and must not be undone:
 *   - `pos` IS A LOCAL, read once before the loop and re-read after the
 *     handler call.  Reading g_brKeyRingPos inline in the index expression
 *     instead puts the load INSIDE the outer loop (90 bytes, whole block
 *     shifts).
 *   - the compare reads `e->text[i]`, NOT `s[i]`.  Spelling it `s[i]` makes
 *     VC5 strength-reduce the string walk into a second induction variable
 *     (`mov bl,[eax]` plus its own `inc`), which costs three instructions:
 *     REGNORM 6+3 instead of 3+4, in EVERY loop form tried.
 *
 * DEAD PROBES -- do not re-run:
 *   - index sums: (i-len)+pos, pos+(i-len), pos-len+i, i-(len-pos),
 *     (i+pos)-len, `% 32` instead of `& 0x1f`, and the same six with the
 *     partial sum broken out into a named temp (one statement or two).  VC5
 *     canonicalises the integer sum: all identical, byte for byte.
 *   - loop forms: for/while/do-while on `e->text`; a `char **` walking
 *     &entry.text with the handler at [-1] (the Ghidra reading); `s` from
 *     the bottom load vs re-read; `i != len` / `len > i` as the inner test;
 *     the inner loop hand-rotated to a guarded do-while (135 bytes).
 *   - types: unsigned i, unsigned/size_t len, `unsigned char *s`,
 *     `*(char *)(s+i)`, `*(char *)&ring[k]`, (int) casts on strlen.
 *   - declaration order of the locals, and swapping `e` / `s`.
 */
#ifdef BR_MATCHING_BUILD

/* The original binary is /MD: CRT calls resolve through the import table. */
#define _CRTIMP __declspec(dllimport)
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <mmsystem.h>

typedef void (*BrCheatFn)(void);

typedef struct {
    BrCheatFn  fn;
    char      *text;
} BrCheatEntry;

/* One typed key.  Only the character is looked at here; the other three
 * bytes are what makes the stride 4 in `cmp dl, [edi*4 + 0x10AC51D8]`. */
typedef struct {
    char ch;
    char pad[3];
} BrKeyRec;

extern BrCheatEntry g_aBrCheatCode[];   /* 0x100ABE48, NULL-terminated */
extern BrKeyRec     g_aBrKeyRing[32];   /* 0x10AC51D8 */
extern int          g_brKeyRingPos;     /* 0x10AC5DA0 */

/* WHAT IT DOES: checks whether the player has just finished typing one of
 * the cheat words.  For every entry in the code table it compares the word
 * against the characters most recently typed -- reading the key ring
 * backwards from the write position so the word's last letter is the last
 * key pressed -- and, on a full match, calls that word's handler.  An empty
 * code word matches trivially and always fires.  Scanning continues through
 * the whole table, so more than one word can trigger in a single call. */
/* @implements 0x10040A90 glide BrCheatCodeScan */
void BrCheatCodeScan(void)
{
    BrCheatEntry *e;
    char *s;
    int i, len, pos;

    s = g_aBrCheatCode[0].text;
    if (s != NULL) {
        e = g_aBrCheatCode;
        pos = g_brKeyRingPos;
        do {
            len = strlen(s);
            for (i = 0; i < len; i++) {
                if (e->text[i] != g_aBrKeyRing[(i - len + pos) & 0x1f].ch)
                    goto next;
            }
            e->fn();
            pos = g_brKeyRingPos;
        next:
            e++;
            s = e->text;
        } while (s != NULL);
    }
}

#endif /* BR_MATCHING_BUILD */
