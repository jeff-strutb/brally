/* @implements 0x10058D00 glide BrBoundsInsert_10058D00
 * @cpp_kind method
 * @cpp_symbol ?Insert@BoundsNode@@QAEXPAV1@@Z
 *
 * Thiscall, one stack arg (`ret 4`), 53 B. Walk the +0x10 chain from
 * `this` looking for the first node the new one does NOT fit inside
 * (0x10058CC0 BrBoundsFits, a non-virtual member): link in front of that
 * node, or append at the end of the chain.
 *
 * The two exits share the `p->f10 = node` store -- VC5 tail-merges them,
 * which is why the early-return arm costs nothing.
 */
#ifdef BR_MATCHING_BUILD
#define _CRTIMP __declspec(dllimport)
#endif

class BoundsNode {
public:
    char        pad[0x10];
    BoundsNode *f10;        /* +0x10 -- next in chain */

    int  Fits(BoundsNode *pOther);      /* 0x10058CC0 */
    void Insert(BoundsNode *pNode);
};

typedef char chk_f10[(unsigned)&((BoundsNode *)0)->f10 == 0x10 ? 1 : -1];

void BoundsNode::Insert(BoundsNode *pNode)
{
    BoundsNode *p = this;

    while (p->Fits(pNode) == 0) {
        if (p->f10 == 0) {
            p->f10 = pNode;
            return;
        }
        p = p->f10;
    }

    pNode->f10 = p->f10;
    p->f10 = pNode;
}
