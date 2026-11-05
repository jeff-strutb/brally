/* progress.h: the app's one save (progress.c): what is open and the
 * championship as it stands, for Boss Rally and both Top Gear Rally libraries.
 * Each game's own memory and files follow it. */
#ifndef RM_PROGRESS_H
#define RM_PROGRESS_H

/* one player's cars, courses and weather, opened here and there alike: Top
 * Gear Rally's words (0-4 the courses, 5-9 mirrored), Boss Rally's (0-5, 6-11) */
void rm_progress_tgr(unsigned short *cars, unsigned short *tracks, unsigned short *weather);
void rm_progress_br(unsigned short *cars, unsigned short *tracks, unsigned short *weather);

/* the championship: a version that moves with every change, and the two
 * players' seasons in each game's shape (Top Gear Rally: its two blocks, one
 * after the other; Boss Rally: each block, 0x14C bytes).  put answers the
 * version it made */
int  rm_progress_season_ver(void);
int  rm_progress_season_put_tgr(const void *seasons);
void rm_progress_season_get_tgr(void *seasons);
int  rm_progress_season_put_br(const void *p0, const void *p1);
void rm_progress_season_get_br(void *p0, void *p1);

/* one player's best laps and totals in Boss Rally's block, kept best of both ways */
void rm_progress_records_br(void *block);

/* the saves made before there was one join it once, the first time they are
 * read, the season furthest on standing for the championship (si.c): after
 * that an erasure stays erased */
int  rm_progress_seeded(void);
void rm_progress_seed_tgr(const void *season);
void rm_progress_seed_done(void);

/* erased: counts them, so a game that sees it move starts afresh */
int  rm_progress_gen(void);
void rm_progress_erase(void);

#endif
