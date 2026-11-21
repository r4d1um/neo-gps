/*
 * neo_gps_time - local time from the GPS time (UTC) with time zone and daylight saving time.
 *
 *  - integer-only, no math.h, no libc time.h, no time zone database
 *  - rules as in the POSIX TZ string (e.g. "CET-1CEST,M3.5.0,M10.5.0/3" for Berlin),
 *    so practically every time zone in the world works
 *  - only linked when you actually use the functions
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef NEO_GPS_TIME_H
#define NEO_GPS_TIME_H

#include "neo_gps.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Time zone with DST rule "month, week, weekday" (POSIX "Mm.w.d/time") */
typedef struct {
    int16_t std_offset;     /* minutes east of UTC in standard time (Berlin: +60)     */
    int16_t dst_offset;     /* minutes in daylight time (Berlin: +120); = std_offset: none */
    uint8_t start_month;    /* DST start: month 1..12                                  */
    uint8_t start_week;     /*   week 1..4, 5 = last                                  */
    uint8_t start_wday;     /*   weekday 0 = Sunday                                   */
    int16_t start_min;      /*   time in minutes, local standard time (Berlin 02:00)  */
    uint8_t end_month;      /* DST end                                                */
    uint8_t end_week;
    uint8_t end_wday;
    int16_t end_min;        /*   time in minutes, local daylight time (Berlin 03:00)  */
} neo_tz_t;

/* Ready-made time zones (as initialiser: static const neo_tz_t tz = NEO_TZ_BERLIN;) */
#define NEO_TZ_UTC       {   0,   0, 0, 0, 0,   0,  0, 0, 0,   0 }
#define NEO_TZ_BERLIN    {  60, 120, 3, 5, 0, 120, 10, 5, 0, 180 }  /* CET-1CEST,M3.5.0,M10.5.0/3 */
#define NEO_TZ_CET       NEO_TZ_BERLIN                              /* also Vienna, Zurich, Paris ... */
#define NEO_TZ_LONDON    {   0,  60, 3, 5, 0,  60, 10, 5, 0, 120 }  /* GMT0BST,M3.5.0/1,M10.5.0   */
#define NEO_TZ_HELSINKI  { 120, 180, 3, 5, 0, 180, 10, 5, 0, 240 }  /* EET-2EEST,M3.5.0/3,M10.5.0/4 */

/* Local time */
typedef struct {
    neo_datetime_t t;       /* local date + time (ms taken from the GPS time)          */
    uint8_t  weekday;       /* 0 = Sunday ... 6 = Saturday                            */
    uint8_t  is_dst;        /* 1 = daylight saving time                              */
    int16_t  offset_min;    /* current offset from UTC in minutes (Berlin 60/120)     */
} neo_local_time_t;

/* Convert GPS time (UTC) to local time. Returns 0 if date/time are not valid
 * (yet) - then *out is unchanged. */
int neo_gps_local_time(const neo_gps_data_t *d, const neo_tz_t *tz, neo_local_time_t *out);

/* Parse a POSIX TZ string, e.g. "CET-1CEST,M3.5.0,M10.5.0/3", "EST5EDT,M3.2.0,M11.1.0",
 * "AEST-10AEDT,M10.1.0,M4.1.0/3", "<+03>-3". Only rules in the format Mm.w.d.
 * Returns 1 = ok, 0 = format not supported (tz stays unchanged). */
int neo_tz_parse(neo_tz_t *tz, const char *posix);

/* --- Building blocks --------------------------------------------------- */

/* UTC date/time -> Unix time (seconds since 1970). 0 if invalid. */
uint32_t neo_gps_unix_time(const neo_gps_data_t *d);

/* Unix time (UTC) -> local Unix time (seconds), optionally with offset/DST flag */
uint32_t neo_tz_utc_to_local(const neo_tz_t *tz, uint32_t utc, int16_t *offset_min, uint8_t *is_dst);

/* Seconds since 1970 -> date/time (ms = 0), returns the weekday (0 = Sunday) */
uint8_t neo_time_from_unix(uint32_t t, neo_datetime_t *out);

/* Date -> Unix time 00:00:00 of that day */
uint32_t neo_time_to_unix(uint16_t year, uint8_t month, uint8_t day);

#ifdef __cplusplus
}
#endif

#endif /* NEO_GPS_TIME_H */
