#ifndef _TIME_H
#define _TIME_H
#include <stddef.h>
typedef long time_t; typedef long clock_t;
struct tm { int tm_sec,tm_min,tm_hour,tm_mday,tm_mon,tm_year,tm_wday,tm_yday,tm_isdst; };
time_t time(time_t*); clock_t clock(void); struct tm *localtime(const time_t*);
#define CLOCKS_PER_SEC 1000
#endif
