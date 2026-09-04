/* WHAT IT DOES: construct a menu control in place -- the compiler-emitted
 * constructor for the control class, clearing its boxes and list. */
/* @implements 0x10040D10 glide BrCtl
 * @cpp_kind dtor
 * @cpp_symbol ??1BrCtl@@UAE@XZ
 *
 * 0x10040D10 is the virtual destructor of the 0x1E214-byte UI control
 * (ctor is 0x10040B10, 478 B; scalar-deleting dtor is 0x10040CF0).
 * Body, from the orig bytes — not a ctor:
 *
 *   mov eax, fs:[0]; push -1; push handler; push eax; mov fs:[0], esp
 *   push esi; mov esi, ecx
 *   mov [esi], vtable            ; 0x10077680 (reloc)
 *   lea ecx, [esi+0x3838]
 *   mov [esp+0xc], 0             ; trylevel = 0
 *   call ~TextList               ; 0x10054710 BrObj54710Dtor (reloc)
 *   push ~TextBox; push 3; add esi, 0x2B5C; push 0x438; push esi
 *   mov [esp+0x1c], -1           ; trylevel = -1
 *   call __ehvec_dtor            ; 0x100746C0 (reloc)
 *   mov ecx, [esp+4]; pop esi; mov fs:[0], ecx; add esp, 0xc; ret
 *
 * Layout forced by those displacements (not relocs — they must be exact):
 *
 *   +0x0000  vptr
 *   +0x2B5C  TextBox boxes[3]     ; each 0x438, dtor 0x10053EE0
 *   +0x3838  TextList list        ; dtor 0x10054710
 *
 * Element / list dtors are DECLARED not defined so the compiler cannot
 * inline them and must emit the call / __ehvec_dtor. Compile /GX.
 *
 * Unwind data (FuncInfo magic 0x19930520, maxState=1, nTryBlocks=0,
 * action = __ehvec_dtor of boxes) lives in .rdata, not in these 97 bytes.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

enum {
    kVptr       = 4,
    kBox        = 0x438,
    kCount      = 3,
    kBoxesAt    = 0x2B5C,
    kListAt     = 0x3838,
    kPad0       = kBoxesAt - kVptr,
    kPad1       = kListAt - (kBoxesAt + kCount * kBox)
};

class TextBox {
    char _[kBox];
public:
    ~TextBox();
};

class TextList {
public:
    ~TextList();
};

class BrCtl {
public:
    virtual ~BrCtl();
    char pad0[kPad0];
    TextBox boxes[kCount];
    char pad1[kPad1];
    TextList list;
};

typedef char chk_box[sizeof(TextBox) == kBox ? 1 : -1];
typedef char chk_pad0[kPad0 == 0x2B58 ? 1 : -1];
typedef char chk_pad1[kPad1 == 0x34 ? 1 : -1];
typedef char chk_boxes[(unsigned)&((BrCtl *)0)->boxes == kBoxesAt ? 1 : -1];
typedef char chk_list[(unsigned)&((BrCtl *)0)->list == kListAt ? 1 : -1];

BrCtl::~BrCtl() {}
