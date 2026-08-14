/* pageprobe72.c -- report slice6_72.h's view of the page layout.
 * See test_pagemodel.c for why this is a separate translation unit. */
#include "slice6_72.h"
#include <stddef.h>

const size_t g_p72_size   = sizeof(BrUiPage_);
const size_t g_p72_cCtl   = offsetof(BrUiPage_, cCtl);
const size_t g_p72_apCtl  = offsetof(BrUiPage_, apCtl);
const size_t g_p72_fX     = offsetof(BrUiPage_, fX);
const size_t g_p72_fY     = offsetof(BrUiPage_, fY);
const size_t g_p72_pOwner = offsetof(BrUiPage_, pOwner);
const size_t g_p72_cSel   = offsetof(BrUiPage_, cSel);
