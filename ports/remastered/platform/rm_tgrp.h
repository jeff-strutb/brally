/* rm_tgrp.h: Top Gear Rally built a second time, from Japan's cartridge (the
 * Remaster Options' Menus: TGR (PAL/Japan)), its entry points named tgrp_*
 * so the two libraries link side by side (link.sh forces this header into
 * every file of that build).  The first build keeps tgr_*; the host calls
 * the one whose menus are in front (main.c, mux.c, options.c). */
#ifndef RM_TGRP_H
#define RM_TGRP_H
#define tgr_main            tgrp_main
#define tgr_race_engine     tgrp_race_engine
#define tgr_menus_engine    tgrp_menus_engine
#define tgr_other_engine    tgrp_other_engine
#define tgr_boot_to_menu    tgrp_boot_to_menu
#define tgr_flag            tgrp_flag
#define tgr_flag_set        tgrp_flag_set
#define tgr_view_driving    tgrp_view_driving
#define tgr_touch_tap       tgrp_touch_tap
#define tgr_touch_swipe     tgrp_touch_swipe
#define tgr_touch_press     tgrp_touch_press
#define tgr_pointer         tgrp_pointer
#define tgr_pointer_wanted  tgrp_pointer_wanted
#define tgr_audio_lowpass   tgrp_audio_lowpass
#define tgr_race_speed      tgrp_race_speed
#define tgr_music_mute      tgrp_music_mute
#define TGR_ROMDATA_NAME    "romdata-jpn.bin"   /* its data file (platform/os/romdata.c) */
#define TGR_ROMDATA_ENV     "TGRP_ROMDATA"
#endif
