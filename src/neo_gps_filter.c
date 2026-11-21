/*
 * neo_gps_filter - position filter against GPS jumps (part of neo_gps).
 * Separate file so the code is only linked when neo_gps_filter_* is used.
 * Pure 32-bit integer arithmetic, runs once per epoch (not per byte).
 * SPDX-License-Identifier: MIT
 */
#include "neo_gps.h"

#if NEO_GPS_ENABLE_FILTER

#define NO_TIME   NEO_GPS_TIME_UNKNOWN
#define HALF_TURN ((int32_t)1800000000L)   /* 180 degrees in 1e-7 degrees */

/* cos(lat) in Q15 without a table (Bhaskara approximation, error < 0.2 %):
 * cos(x) ~ (32400 - 4x^2) / (32400 + x^2), x in degrees; here with x in 0.01 degrees */
static uint32_t cos_lat_q15(int32_t lat)
{
    uint32_t c = (lat < 0 ? 0u - (uint32_t)lat : (uint32_t)lat) / 100000UL;   /* 0..9000 */
    uint32_t c2, num, den;
    if (c >= 9000u) return 0;
    c2  = c * c;
    num = (uint32_t)324000000UL - 4u * c2;
    den = ((uint32_t)324000000UL + c2) >> 15;
    return num / den;
}

/* Longitude difference a-b, normalised to -180..+180 degrees, without overflow (|a|,|b| <= 180 degrees) */
static int32_t dlon32(int32_t a, int32_t b)
{
    uint32_t d;
    if ((a >= 0) == (b >= 0)) return a - b;
    if (a >= 0) {
        d = (uint32_t)a + (0u - (uint32_t)b);                        /* a - b > 0 */
        return d > 1800000000UL ? -(int32_t)(3600000000UL - d) : (int32_t)d;
    }
    d = (uint32_t)b + (0u - (uint32_t)a);                            /* b - a > 0 */
    return d > 1800000000UL ? (int32_t)(3600000000UL - d) : -(int32_t)d;
}

static uint32_t abs32(int32_t v) { return v < 0 ? 0u - (uint32_t)v : (uint32_t)v; }

/* a * b / div, saturating instead of overflowing */
static uint32_t muldiv_sat(uint32_t a, uint32_t b, uint32_t div)
{
    if (b && a > 0xFFFFFFFFUL / b) {
        a /= div;                                    /* divide first, then check */
        return (b && a > 0xFFFFFFFFUL / b) ? 0xFFFFFFFFUL : a * b;
    }
    return a * b / div;
}

static uint32_t add_sat(uint32_t a, uint32_t b) { return (a + b < a) ? 0xFFFFFFFFUL : a + b; }

/* Is the new position further away than physically possible in dt_ms?
 * Pure 32-bit arithmetic; distance via "max + 3/8 min" (error < 7 %). */
static uint8_t is_jump(const neo_gps_t *g, int32_t lat, int32_t lon, uint32_t dt_ms)
{
    const neo_filter_cfg_t *c = &g->f_cfg;
    uint32_t dy = abs32(lat - g->f_lat);
    uint32_t ax = abs32(dlon32(lon, g->f_lon));
    uint32_t cq = cos_lat_q15(g->f_lat);
    uint32_t dx = (ax >> 15) * cq + (((ax & 0x7FFFu) * cq) >> 15);
    uint32_t hi = dx > dy ? dx : dy, lo = dx > dy ? dy : dx;
    uint32_t dist = hi + lo / 8u * 3u;                               /* in 1e-7 degrees */
    uint32_t v = g->data.speed > g->f_speed ? g->data.speed : g->f_speed;
    uint32_t dt = dt_ms > 600000UL ? 600000UL : dt_ms;
    uint32_t allow;

    /* allowed distance in mm:  1.5 * v * t  +  a * t^2  +  margin  +  2 * h_acc */
    allow = muldiv_sat(v + v / 2u, dt, 1000u);
    allow = add_sat(allow, muldiv_sat(muldiv_sat(c->max_accel, dt, 1000u), dt, 1000u));
    allow = add_sat(allow, c->margin);
    allow = add_sat(allow, g->data.h_acc);
    allow = add_sat(allow, g->data.h_acc);
    /* mm -> 1e-7 degrees (1e-7 degrees of latitude ~ 11.132 mm) */
    allow = allow / 11132u * 1000u + allow % 11132u * 1000u / 11132u;
    return dist > allow;
}

/* Decides on a new raw position. t = epoch time in ms (NO_TIME = unknown),
 * period = wrap-around period of t. Returns 1 = taken over (data.lat/lon set). */
static uint8_t filter_commit(neo_gps_t *g, int32_t lat, int32_t lon, uint32_t t, uint32_t period)
{
    neo_gps_data_t *d = &g->data;
    const neo_filter_cfg_t *c = &g->f_cfg;
    uint8_t why = NEO_REJECT_NONE, reanchor = 0;

    /* Second position message of the same epoch (e.g. RMC + GGA): don't count it twice.
     * Only re-check after a quality rejection (GGA brings fresh satellites/HDOP). */
    if (t != NO_TIME && g->f_epoch_set && t == g->f_epoch) {
        if (g->f_epoch_ok) { d->updated |= NEO_UPD_POSITION; return 1; }
        if (d->reject_reason == NEO_REJECT_JUMP) return 0;
    }

    if (!d->valid || lat > 900000000L || lat < -900000000L ||
        lon > 1800000000L || lon < -1800000000L)                     why = NEO_REJECT_FIX;
    else if (c->min_fix && d->fix != NEO_FIX_NONE && d->fix < c->min_fix) why = NEO_REJECT_FIX;
    else if (c->min_sv && d->num_sv && d->num_sv < c->min_sv)        why = NEO_REJECT_SATS;
    else if (c->max_hdop && d->hdop > c->max_hdop)                   why = NEO_REJECT_HDOP;
    else if (c->max_h_acc && d->h_acc > c->max_h_acc)                why = NEO_REJECT_ACCURACY;
    else if (g->f_have && c->max_accel && t != NO_TIME && g->f_t != NO_TIME) {
        uint32_t dt = (t >= g->f_t) ? t - g->f_t : t + period - g->f_t;
        if (is_jump(g, lat, lon, dt)) {
            if (c->reanchor && ++g->f_jumps >= c->reanchor) reanchor = 1;   /* the place really has changed */
            else why = NEO_REJECT_JUMP;
        }
    }

    if (t != NO_TIME) { g->f_epoch = t; g->f_epoch_set = 1; g->f_epoch_ok = (why == NEO_REJECT_NONE); }

    if (why) {
        d->reject_reason = why;
        d->rejected++;
        d->updated |= NEO_UPD_REJECTED;
        return 0;
    }

    /* Smoothing at standstill (against the typical drift of a few metres) */
    if (!reanchor && g->f_have && c->smooth_shift && d->speed <= c->static_speed) {
        int32_t dy = lat - g->f_lat;
        int32_t dx = dlon32(lon, g->f_lon);
        if (abs32(dy) < 10000000UL && abs32(dx) < 10000000UL) {   /* < ~1 km */
            int32_t div = (int32_t)1 << c->smooth_shift;
            int32_t nl = g->f_lon + dx / div;
            if (nl > HALF_TURN) nl = nl - HALF_TURN - HALF_TURN;        /* across the date line */
            else if (nl < -HALF_TURN) nl = nl + HALF_TURN + HALF_TURN;
            lat = g->f_lat + dy / div;
            lon = nl;
        }
    }

    g->f_jumps = 0;
    g->f_lat = lat; g->f_lon = lon;
    g->f_t = t;
    g->f_speed = d->speed;
    g->f_have = 1;
    d->lat = lat; d->lon = lon;
    d->updated |= NEO_UPD_POSITION;
    return 1;
}

void neo_gps_filter_reset(neo_gps_t *g)
{
    g->f_have = 0;
    g->f_epoch_set = 0;
    g->f_jumps = 0;
}

void neo_gps_filter_config(neo_gps_t *g, const neo_filter_cfg_t *cfg)
{
    g->f_cfg = *cfg;
    g->f_fn = filter_commit;
    neo_gps_filter_reset(g);
}

void neo_gps_filter_preset(neo_gps_t *g, neo_filter_preset_t preset)
{
    /* Assignments instead of const tables: on AVR const data would otherwise sit in RAM */
    neo_filter_cfg_t c;
    c.min_fix = NEO_FIX_3D;
    switch (preset) {
    case NEO_FILTER_PEDESTRIAN:
        c.min_sv = 5; c.max_hdop = 400; c.max_h_acc = 25000; c.max_accel = 3000;
        c.margin = 10000; c.reanchor = 5; c.smooth_shift = 2; c.static_speed = 300;
        break;
    case NEO_FILTER_AUTOMOTIVE:
        c.min_sv = 5; c.max_hdop = 500; c.max_h_acc = 30000; c.max_accel = 8000;
        c.margin = 15000; c.reanchor = 5; c.smooth_shift = 2; c.static_speed = 500;
        break;
    case NEO_FILTER_AIRBORNE:
        c.min_sv = 6; c.max_hdop = 300; c.max_h_acc = 20000; c.max_accel = 20000;
        c.margin = 10000; c.reanchor = 3; c.smooth_shift = 1; c.static_speed = 200;
        break;
    case NEO_FILTER_STATIONARY:
        c.min_sv = 6; c.max_hdop = 300; c.max_h_acc = 15000; c.max_accel = 1000;
        c.margin = 5000; c.reanchor = 10; c.smooth_shift = 4; c.static_speed = 1000;
        break;
    default:
        g->f_fn = 0;
        neo_gps_filter_reset(g);
        return;
    }
    neo_gps_filter_config(g, &c);
}

#endif /* NEO_GPS_ENABLE_FILTER */
