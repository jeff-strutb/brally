#!/bin/sh
# Boss Rally port -- build. Requires only clang + macOS SDK.
set -e
mkdir -p build
CFLAGS="-std=c99 -Wall -Wextra -Wno-unused-parameter -g -D_DARWIN_C_SOURCE -Iport/include -Iport/src/gfx"
MFLAGS="-fobjc-arc -Wall -g -Iport/include -Iport/src/gfx"
FW="-framework Metal -framework Foundation -framework AppKit -framework QuartzCore"

clang $CFLAGS -c port/src/br_pod.c            -o build/br_pod.o
clang $CFLAGS -c port/src/br_img.c            -o build/br_img.o
clang $CFLAGS -c port/src/br_rca.c            -o build/br_rca.o
clang $CFLAGS -c port/src/br_n64tex.c        -o build/br_n64tex.o
clang $CFLAGS -c port/src/br_f3d.c            -o build/br_f3d.o
clang $CFLAGS -c port/src/br_vec.c            -o build/br_vec.o
clang $CFLAGS -c port/src/br_mat.c            -o build/br_mat.o
clang $CFLAGS -c port/src/br_span.c           -o build/br_span.o
clang $CFLAGS -c port/src/br_seg.c            -o build/br_seg.o
clang $CFLAGS -c port/src/br_pool.c           -o build/br_pool.o
clang $CFLAGS -c port/src/br_vecd.c           -o build/br_vecd.o
clang $CFLAGS -c port/src/br_slots.c          -o build/br_slots.o
clang $CFLAGS -c port/src/br_state.c          -o build/br_state.o
clang $CFLAGS -c port/src/br_obj.c            -o build/br_obj.o
clang $CFLAGS -c port/src/br_bits.c           -o build/br_bits.o
clang $CFLAGS -c port/src/br_uictl.c           -o build/br_uictl.o
clang $CFLAGS -c port/src/br_uivt.c            -o build/br_uivt.o
clang $MFLAGS -c port/src/gfx/metal/br_gfx_metal.m -o build/br_gfx_metal.o

clang $CFLAGS -Iport/tests -c port/tests/test_uictl.c -o build/test_uictl.o
clang $CFLAGS -Iport/tests -c port/tests/test_uivt.c -o build/test_uivt.o
clang $CFLAGS -Iport/tests -c port/tests/test_pod.c -o build/test_pod.o
clang $CFLAGS -Iport/tests -c port/tests/test_gfx.c -o build/test_gfx.o
clang $CFLAGS -Iport/tests -c port/tests/test_rca.c -o build/test_rca.o
clang $CFLAGS -Iport/tests -c port/tests/test_n64tex.c -o build/test_n64tex.o
clang $CFLAGS -Iport/tests -c port/tests/test_f3d.c -o build/test_f3d.o
clang $CFLAGS -Iport/tests -c port/tests/test_vec.c -o build/test_vec.o
clang $CFLAGS -Iport/tests -c port/tests/test_mat.c -o build/test_mat.o
clang $CFLAGS -Iport/tests -c port/tests/test_span.c -o build/test_span.o
clang $CFLAGS -Iport/tests -c port/tests/test_seg.c -o build/test_seg.o
clang $CFLAGS -Iport/tests -c port/tests/test_pool.c -o build/test_pool.o
clang $CFLAGS -Iport/tests -c port/tests/test_vecd.c -o build/test_vecd.o
clang $CFLAGS -Iport/tests -c port/tests/test_slots.c -o build/test_slots.o
clang $CFLAGS -Iport/tests -c port/tests/test_state.c -o build/test_state.o
clang $CFLAGS -Iport/tests -c port/tests/test_obj.c -o build/test_obj.o
clang $CFLAGS -Iport/tests -c port/tests/test_bits.c -o build/test_bits.o
clang $CFLAGS -c port/tools/brview.c   -o build/brview.o


# --- slice 1 pass modules ---
clang $CFLAGS -c port/src/slice1_01.c -o build/slice1_01.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice1_01.c -o build/test_slice1_01.o
clang $CFLAGS -c port/src/slice1_02.c -o build/slice1_02.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice1_02.c -o build/test_slice1_02.o
clang $CFLAGS -c port/src/slice1_03.c -o build/slice1_03.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice1_03.c -o build/test_slice1_03.o
clang $CFLAGS -c port/src/slice1_04.c -o build/slice1_04.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice1_04.c -o build/test_slice1_04.o
clang $CFLAGS -c port/src/slice1_05.c -o build/slice1_05.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice1_05.c -o build/test_slice1_05.o
clang $CFLAGS -c port/src/slice1_06.c -o build/slice1_06.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice1_06.c -o build/test_slice1_06.o
clang $CFLAGS -c port/src/slice1_07.c -o build/slice1_07.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice1_07.c -o build/test_slice1_07.o
clang $CFLAGS -c port/src/slice1_08.c -o build/slice1_08.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice1_08.c -o build/test_slice1_08.o
clang $CFLAGS -c port/src/slice1_09.c -o build/slice1_09.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice1_09.c -o build/test_slice1_09.o
clang $CFLAGS -c port/src/slice1_10.c -o build/slice1_10.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice1_10.c -o build/test_slice1_10.o

clang build/slice1_01.o build/test_slice1_01.o -lm -o build/test_slice1_01
clang build/slice1_02.o build/test_slice1_02.o -lm -o build/test_slice1_02
clang build/slice1_03.o build/test_slice1_03.o -lm -o build/test_slice1_03
clang build/slice1_04.o build/test_slice1_04.o -lm -o build/test_slice1_04
clang build/slice1_05.o build/test_slice1_05.o -lm -o build/test_slice1_05
clang build/slice1_06.o build/test_slice1_06.o build/br_vec.o -lm -o build/test_slice1_06
clang build/slice1_07.o build/test_slice1_07.o -lm -o build/test_slice1_07
clang build/slice1_08.o build/test_slice1_08.o -lm -o build/test_slice1_08
clang build/slice1_09.o build/test_slice1_09.o -lm -o build/test_slice1_09
clang build/slice1_10.o build/test_slice1_10.o -lm -o build/test_slice1_10


# --- slice 2 pass modules ---
clang $CFLAGS -c port/src/slice2_11.c -o build/slice2_11.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_11.c -o build/test_slice2_11.o
clang $CFLAGS -c port/src/slice2_12.c -o build/slice2_12.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_12.c -o build/test_slice2_12.o
clang $CFLAGS -c port/src/slice2_13.c -o build/slice2_13.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_13.c -o build/test_slice2_13.o
clang $CFLAGS -c port/src/slice2_14.c -o build/slice2_14.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_14.c -o build/test_slice2_14.o
clang $CFLAGS -c port/src/slice2_15.c -o build/slice2_15.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_15.c -o build/test_slice2_15.o
clang $CFLAGS -c port/src/slice2_16.c -o build/slice2_16.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_16.c -o build/test_slice2_16.o
clang $CFLAGS -c port/src/slice2_17.c -o build/slice2_17.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_17.c -o build/test_slice2_17.o
clang $CFLAGS -c port/src/slice2_18.c -o build/slice2_18.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_18.c -o build/test_slice2_18.o
clang $CFLAGS -c port/src/slice2_19.c -o build/slice2_19.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_19.c -o build/test_slice2_19.o
clang $CFLAGS -c port/src/slice2_20.c -o build/slice2_20.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_20.c -o build/test_slice2_20.o
clang $CFLAGS -c port/src/slice2_21.c -o build/slice2_21.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_21.c -o build/test_slice2_21.o
clang $CFLAGS -c port/src/slice2_22.c -o build/slice2_22.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_22.c -o build/test_slice2_22.o
clang $CFLAGS -c port/src/slice2_23.c -o build/slice2_23.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_23.c -o build/test_slice2_23.o
clang $CFLAGS -c port/src/slice2_24.c -o build/slice2_24.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_24.c -o build/test_slice2_24.o
clang $CFLAGS -c port/src/slice2_25.c -o build/slice2_25.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_25.c -o build/test_slice2_25.o
clang $CFLAGS -c port/src/slice2_26.c -o build/slice2_26.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice2_26.c -o build/test_slice2_26.o

clang build/slice2_11.o build/test_slice2_11.o -lm -o build/test_slice2_11
clang build/slice2_12.o build/test_slice2_12.o -lm -o build/test_slice2_12
clang build/slice2_13.o build/test_slice2_13.o -lm -o build/test_slice2_13
clang build/slice2_14.o build/test_slice2_14.o -lm -o build/test_slice2_14
clang build/slice2_15.o build/test_slice2_15.o -lm -o build/test_slice2_15
clang build/slice2_16.o build/test_slice2_16.o -lm -o build/test_slice2_16
clang build/slice2_17.o build/test_slice2_17.o -lm -o build/test_slice2_17
clang build/slice2_18.o build/test_slice2_18.o -lm -o build/test_slice2_18
clang build/slice2_19.o build/test_slice2_19.o -lm -o build/test_slice2_19
clang build/slice2_20.o build/test_slice2_20.o -lm -o build/test_slice2_20
clang build/slice2_21.o build/test_slice2_21.o -lm -o build/test_slice2_21
clang build/slice2_22.o build/test_slice2_22.o -lm -o build/test_slice2_22
clang build/slice2_23.o build/test_slice2_23.o -lm -o build/test_slice2_23
clang build/slice2_24.o build/test_slice2_24.o -lm -o build/test_slice2_24
clang build/slice2_25.o build/test_slice2_25.o -lm -o build/test_slice2_25
clang build/slice2_26.o build/test_slice2_26.o -lm -o build/test_slice2_26


# --- slice 3 pass modules ---
clang $CFLAGS -c port/src/slice3_31.c -o build/slice3_31.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice3_31.c -o build/test_slice3_31.o
clang $CFLAGS -c port/src/slice3_32.c -o build/slice3_32.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice3_32.c -o build/test_slice3_32.o
clang $CFLAGS -c port/src/slice3_33.c -o build/slice3_33.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice3_33.c -o build/test_slice3_33.o
clang $CFLAGS -c port/src/slice3_39.c -o build/slice3_39.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice3_39.c -o build/test_slice3_39.o
clang $CFLAGS -c port/src/slice3_40.c -o build/slice3_40.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice3_40.c -o build/test_slice3_40.o
clang $CFLAGS -c port/src/slice3_41.c -o build/slice3_41.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice3_41.c -o build/test_slice3_41.o
clang $CFLAGS -c port/src/slice3_42.c -o build/slice3_42.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice3_42.c -o build/test_slice3_42.o
clang $CFLAGS -c port/src/slice3_44.c -o build/slice3_44.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice3_44.c -o build/test_slice3_44.o
clang $CFLAGS -c port/src/slice3_45.c -o build/slice3_45.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice3_45.c -o build/test_slice3_45.o

clang build/slice3_31.o build/test_slice3_31.o -lm -o build/test_slice3_31
clang build/slice3_32.o build/test_slice3_32.o -lm -o build/test_slice3_32
clang build/slice3_33.o build/test_slice3_33.o -lm -o build/test_slice3_33
clang build/slice3_39.o build/test_slice3_39.o -lm -o build/test_slice3_39
clang build/slice3_40.o build/test_slice3_40.o -lm -o build/test_slice3_40
clang build/slice3_41.o build/test_slice3_41.o -lm -o build/test_slice3_41
clang build/slice3_42.o build/test_slice3_42.o -lm -o build/test_slice3_42
clang build/slice3_44.o build/test_slice3_44.o -lm -o build/test_slice3_44
clang build/slice3_45.o build/test_slice3_45.o -lm -o build/test_slice3_45


# --- slice 4: link-gap closure + CRT shim ---
clang $CFLAGS -c port/src/slice4_50.c -o build/slice4_50.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice4_50.c -o build/test_slice4_50.o
clang $CFLAGS -c port/src/slice4_51.c -o build/slice4_51.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice4_51.c -o build/test_slice4_51.o
clang $CFLAGS -c port/src/slice4_52.c -o build/slice4_52.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice4_52.c -o build/test_slice4_52.o
clang $CFLAGS -c port/src/slice4_53.c -o build/slice4_53.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice4_53.c -o build/test_slice4_53.o
clang $CFLAGS -c port/src/br_crt.c -o build/br_crt.o

clang build/slice4_50.o build/test_slice4_50.o -lm -o build/test_slice4_50
clang build/slice4_51.o build/test_slice4_51.o -lm -o build/test_slice4_51
clang build/slice4_52.o build/test_slice4_52.o -lm -o build/test_slice4_52
clang build/slice4_53.o build/test_slice4_53.o -lm -o build/test_slice4_53

# layout assertions -- plain C99; failures break the BUILD
clang -std=c99 -Wall -Wextra -Iport/include port/tests/test_layout.c build/br_pod.o -o build/test_layout

# br_ui.h -- the canonical page/control types. Header-only, like test_layout:
# most of its claims are compile-time assertions, so a bad element count breaks
# the BUILD rather than the run.
#
# test_pagemodel and its two probes used to be built here. They existed only to
# prove that slice6_72.h's and slice6_73.h's rival `struct BrUiPage_`
# definitions had not drifted apart -- a hand-maintained invariant no compiler
# could check. Both packets are on br_ui.h now, so there is one definition and
# nothing left to drift; the test's own comment said to delete it at exactly
# this point and not before.
clang -std=c99 -Wall -Wextra -Iport/include port/tests/test_br_ui.c -o build/test_br_ui


# --- slice 5: link-gap round 2 ---
clang $CFLAGS -c port/src/slice5_60.c -o build/slice5_60.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice5_60.c -o build/test_slice5_60.o
clang $CFLAGS -c port/src/slice5_61.c -o build/slice5_61.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice5_61.c -o build/test_slice5_61.o
clang $CFLAGS -c port/src/slice5_62.c -o build/slice5_62.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice5_62.c -o build/test_slice5_62.o
clang $CFLAGS -c port/src/slice5_63.c -o build/slice5_63.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice5_63.c -o build/test_slice5_63.o

clang build/slice5_60.o build/test_slice5_60.o -lm -o build/test_slice5_60
clang build/slice5_61.o build/test_slice5_61.o -lm -o build/test_slice5_61
clang build/slice5_62.o build/test_slice5_62.o -lm -o build/test_slice5_62
clang build/slice5_63.o build/test_slice5_63.o -lm -o build/test_slice5_63

clang $CFLAGS -c port/src/slice6_70.c -o build/slice6_70.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice6_70.c -o build/test_slice6_70.o
clang $CFLAGS -c port/src/slice6_71.c -o build/slice6_71.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice6_71.c -o build/test_slice6_71.o

clang $CFLAGS -c port/src/slice6_72.c -o build/slice6_72.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice6_72.c -o build/test_slice6_72.o

clang $CFLAGS -c port/src/slice6_73.c -o build/slice6_73.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice6_73.c -o build/test_slice6_73.o

clang $CFLAGS -c port/src/slice6_74.c -o build/slice6_74.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice6_74.c -o build/test_slice6_74.o

clang $CFLAGS -c port/src/slice6_76.c -o build/slice6_76.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice6_76.c -o build/test_slice6_76.o

clang $CFLAGS -c port/src/slice6_77.c -o build/slice6_77.o
clang $CFLAGS -Iport/tests -c port/tests/test_slice6_77.c -o build/test_slice6_77.o

clang build/slice6_70.o build/test_slice6_70.o -lm -o build/test_slice6_70
clang build/slice6_71.o build/test_slice6_71.o -lm -o build/test_slice6_71
clang build/slice6_72.o build/test_slice6_72.o -lm -o build/test_slice6_72
clang build/slice6_73.o build/test_slice6_73.o -lm -o build/test_slice6_73
clang build/slice6_74.o build/test_slice6_74.o -lm -o build/test_slice6_74
clang build/slice6_76.o build/test_slice6_76.o -lm -o build/test_slice6_76
# slice6_77 shares 0x100586A0's loop with br_slots.c, so the real br_slots.o
# is linked rather than faked; every other dependency is supplied by the test.
clang build/slice6_77.o build/br_slots.o build/test_slice6_77.o -lm -o build/test_slice6_77

clang $CFLAGS -c port/src/br_audio.c -o build/br_audio.o
clang $CFLAGS -Iport/tests -c port/tests/test_audio.c -o build/test_audio.o
clang build/br_audio.o build/test_audio.o -lm -o build/test_audio

clang build/br_uictl.o build/test_uictl.o -lm -o build/test_uictl
clang build/br_uivt.o build/br_uictl.o build/br_crt.o build/test_uivt.o -lm -o build/test_uivt
clang build/br_pod.o build/test_pod.o -o build/test_pod
clang -fobjc-arc build/br_img.o build/br_gfx_metal.o build/test_gfx.o $FW -o build/test_gfx
clang -fobjc-arc build/br_img.o build/br_gfx_metal.o build/brview.o  $FW -o build/brview
clang build/br_rca.o build/test_rca.o -o build/test_rca
clang build/br_n64tex.o build/test_n64tex.o -o build/test_n64tex
clang build/br_f3d.o build/test_f3d.o -o build/test_f3d
clang build/br_vec.o build/test_vec.o -lm -o build/test_vec
clang build/br_mat.o build/br_vec.o build/test_mat.o -lm -o build/test_mat
clang build/br_span.o build/test_span.o -o build/test_span
clang build/br_seg.o build/test_seg.o -o build/test_seg
clang build/br_pool.o build/test_pool.o -o build/test_pool
clang build/br_vecd.o build/test_vecd.o -lm -o build/test_vecd
clang build/br_slots.o build/test_slots.o -o build/test_slots
clang build/br_state.o build/test_state.o -o build/test_state
clang build/br_obj.o build/test_obj.o -o build/test_obj
clang build/br_bits.o build/test_bits.o -o build/test_bits
echo "built: slice1+2+3 modules + test_pod test_gfx test_rca test_n64tex test_f3d test_vec test_mat test_span test_seg test_pool test_vecd test_slots test_state test_obj test_bits brview"

# --- the host: links the whole ported core into one runnable binary ---------
# Unported functions are satisfied by port/host/br_stubs.c, so this links today
# and reports at exit which stubs the run actually reached. That report is the
# work queue: it is measured from a real boot rather than guessed.
# slice6_71 and slice6_73 each duplicate a function slice6_70 also defines.
# Their TESTS link each module alone and need the real names, so the plain
# objects above keep them. The host links all three at once, so it needs the
# renamed copies -- built here, into a subdirectory the host glob skips.
mkdir -p build/host
clang $CFLAGS -DBR_HOST_LINK -c port/src/slice3_32.c -o build/host/slice3_32.o
clang $CFLAGS -DBR_HOST_LINK -c port/src/slice6_71.c -o build/host/slice6_71.o
clang $CFLAGS -DBR_HOST_LINK -c port/src/slice6_73.c -o build/host/slice6_73.o

clang $CFLAGS -c port/host/br_wire71.c -o build/br_wire71.o
clang $CFLAGS -c port/host/br_wire72.c -o build/br_wire72.o
clang $CFLAGS -c port/host/br_wire75.c -o build/br_wire75.o
clang $CFLAGS -c port/host/br_wire77.c -o build/br_wire77.o
clang $CFLAGS -c port/host/br_stubs.c -o build/br_stubs.o
# real definitions for the cross-module data objects (was br_stubs' 1 MiB blocks)
clang $CFLAGS -c port/src/br_data.c -o build/br_data.o
clang $CFLAGS -Iport/tests -c port/tests/test_data.c -o build/test_data.o
clang build/br_data.o build/test_data.o -lm -o build/test_data
clang $CFLAGS -c port/host/brally.c   -o build/brally.o

HOSTOBJS=""
for o in build/*.o; do
  case "$o" in
    *test_*|*brview*|*br_gfx_metal*|*brally.o|*br_stubs.o|*br_wire7*.o) continue;;
    */slice3_32.o|*/slice6_71.o|*/slice6_73.o) continue;;   # host uses build/host/ copies
  esac
  HOSTOBJS="$HOSTOBJS $o"
done
clang build/brally.o build/br_stubs.o build/br_wire71.o build/br_wire72.o build/br_wire75.o build/br_wire77.o $HOSTOBJS \
      build/host/slice3_32.o build/host/slice6_71.o build/host/slice6_73.o \
      build/br_gfx_metal.o -lm $FW -o build/brally
echo "built: brally (host)"
