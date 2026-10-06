/* WHAT IT DOES: reset an entity to its starting state -- squares up its
 * matrices and reinitialises each animation frame. */
/* @implements 0x1006FD90 glide BrEntReset
 * @cpp_kind method
 * @cpp_symbol ?Reset@Ent6FD90@@QAEXXZ
 *
 * 368 B thiscall, no stack args. Five matrix resets + five frame resets
 * (frame 4 skipped) with the sixth-pi constant hoisted into a register,
 * SetVel(0,0,0) through the zero register, then the record unpack:
 * a 7-dword memcpy, five dwords, three sign-extended bytes, four dwords,
 * and the record pointer cleared before the last store.
 */
extern "C" void *memcpy(void *, const void *, unsigned int);

class Mat4M {
public:
    void SetLastColumn();       /* thiscall, defined elsewhere */
    float m[16];
};

struct EntFrame {
    Mat4M m;                    /* +0x00 */
    float f40;                  /* +0x40 */
};

struct EntRec {
    char pad0[0x96];
    char b96;                   /* +0x96, signed */
    char b97;                   /* +0x97, signed */
    char a98[0x1C];             /* +0x98 */
    int  dB4, dB8, dBC, dC0, dC4;   /* +0xB4 */
    int  dC8, dCC, dD0, dD4;    /* +0xC8 */
    char bD8;                   /* +0xD8, signed */
};

class Ent6FD90 {
public:
    Mat4M    mats[5];           /* +0x0000 */
    char     pad140[0x200];
    int      f340[4];           /* +0x0340 */
    char     pad350[0xAD8];
    int      fE28[7];           /* +0x0E28 */
    int      fE44, fE48, fE4C, fE50, fE54;  /* +0x0E44 */
    int      fE58, fE5C, fE60, fE64;        /* +0x0E58 */
    char     padE68[0x34];
    int      fE9C;              /* +0x0E9C */
    char     padEA0[0xEC];
    int      fF8C, fF90;        /* +0x0F8C */
    char     padF94[0x17A0];
    EntFrame *p2734;            /* +0x2734 */
    int      f2738;             /* +0x2738 */
    EntFrame aFrames[6];        /* +0x273C */
    char     pad28D4[0xF0];
    EntRec  *pRec;              /* +0x29C4 */
    void     SetVel(float x, float y, float z);
    void     Reset();
};

typedef char chk_frames[(unsigned)&((Ent6FD90 *)0)->aFrames == 0x273C ? 1 : -1];
typedef char chk_rec[(unsigned)&((Ent6FD90 *)0)->pRec == 0x29C4 ? 1 : -1];

void Ent6FD90::Reset()
{
    EntRec   *r;
    EntFrame *f0;
    EntFrame *f;

    mats[0].SetLastColumn();

    f0 = &aFrames[0];
    f0->m.SetLastColumn();
    f0->f40 = 0.5235987901687622f;
    f = &aFrames[1];
    f->m.SetLastColumn();
    f->f40 = 0.5235987901687622f;
    f = &aFrames[2];
    f->m.SetLastColumn();
    f->f40 = 0.5235987901687622f;
    f = &aFrames[3];
    f->m.SetLastColumn();
    f->f40 = 0.5235987901687622f;
    /* aFrames[4] deliberately skipped */
    f = &aFrames[5];
    f->m.SetLastColumn();
    f->f40 = 0.5235987901687622f;

    p2734 = f0;

    mats[1].SetLastColumn();
    mats[2].SetLastColumn();
    mats[3].SetLastColumn();
    mats[4].SetLastColumn();

    SetVel(0.0f, 0.0f, 0.0f);

    fF8C  = 0;
    fF90  = 0;
    f2738 = 0;

    r = pRec;
    memcpy(fE28, r->a98, 0x1C);
    fE44 = r->dB4;
    fE48 = r->dB8;
    fE4C = r->dBC;
    fE50 = r->dC0;
    fE54 = r->dC4;
    fE58 = r->bD8;
    fE60 = fE9C;
    fE5C = r->b96;
    f340[0] = r->dC8;
    f340[1] = r->dCC;
    f340[2] = r->dD0;
    f340[3] = r->dD4;
    {
        int v = r->b97;
        pRec = 0;
        fE64 = v;
    }
}
