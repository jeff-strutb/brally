/* @implements 0x10041CC0 glide BrPhaseDtor_10048870
 * @cpp_kind dtor
 * @cpp_symbol ??1Phase32@@QAE@XZ
 *
 * 63 B. Plain (non-deleting) dtor: vtbl reset to 0x100776C8, then
 * `delete` of the two owned members at +0xC0/+0xC4 (slot-0 scalar-deleting
 * vcalls with flag 1), each pointer zeroed after. No EH frame.
 */
class PhaseMember {
public:
    virtual ~PhaseMember();
};

class Phase32 {
public:
    virtual ~Phase32();
    char pad[0xBC];
    PhaseMember *pC0;   /* +0xC0 */
    PhaseMember *pC4;   /* +0xC4 */
};

Phase32::~Phase32()
{
    delete pC0;
    pC0 = 0;
    delete pC4;
    pC4 = 0;
}
