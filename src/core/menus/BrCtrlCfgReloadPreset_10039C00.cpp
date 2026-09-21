/* WHAT IT DOES: re-applies the currently selected control preset (0..3) to
 * the control-config object -- one call per preset value, written out as a
 * four-way switch; any other selector value does nothing.  Always reports
 * success. */
/* @implements 0x10039C00 glide BrCtrlCfgReloadPreset
 * @cpp_kind free
 * @cpp_symbol ?BrCtrlCfgReloadPreset@@YAHXZ
 *
 * cdecl, no args, `ret`, 89 B, no EH.  A four-way jump table over the preset
 * selector; each live arm loads the control-config object into ecx and calls
 * the preset loader (0x10062B10) with the preset number.
 *
 * This is a C++-lane function: every arm is `push imm8; mov ecx,0x10B71290;
 * call 0x10062B10` -- the argument is pushed BEFORE ecx is set, and it is a
 * plain int immediate.  A C __fastcall twin cannot order the push before the
 * ecx setup, and its only way to keep the second arg off edx is a struct-
 * typed wrapper, which is a copy (`mov eax,N; push eax`), not an immediate --
 * +31 bytes over the original.  A real thiscall member call pushes the int
 * constant directly.  Same tell as BrVt8A70CallPair (src/core/cpp/0x10008A70).
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

/* 0x10062B10 -- __thiscall member, one int arg: `push n; mov ecx,obj; call`. */
class CtrlCfg {
public:
    void LoadPreset(int n);
};

extern "C" {
CtrlCfg g_brCtrlCfgObj;     /* 0x10B71290 */
int     g_brCtrlPreset;     /* 0x10AC5D64 */
}

int BrCtrlCfgReloadPreset(void)
{
    switch (g_brCtrlPreset) {
    case 0: g_brCtrlCfgObj.LoadPreset(0); return 1;
    case 1: g_brCtrlCfgObj.LoadPreset(1); return 1;
    case 2: g_brCtrlCfgObj.LoadPreset(2); return 1;
    case 3: g_brCtrlCfgObj.LoadPreset(3); break;
    }
    return 1;
}
