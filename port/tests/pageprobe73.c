/* pageprobe73.c -- report slice6_73.h's view of the page layout.
 * See test_pagemodel.c for why this is a separate translation unit. */
#include "slice6_73.h"
#include <stddef.h>

const size_t g_p73_size   = sizeof(BrUiPage_);
const size_t g_p73_cCtl   = offsetof(BrUiPage_, cCtl);
const size_t g_p73_apCtl  = offsetof(BrUiPage_, apCtl);
const size_t g_p73_fX     = offsetof(BrUiPage_, fX);
const size_t g_p73_fY     = offsetof(BrUiPage_, fY);
const size_t g_p73_pOwner = offsetof(BrUiPage_, pOwner);
const size_t g_p73_cSel   = offsetof(BrUiPage_, cSel);
