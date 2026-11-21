/*
 * neo_gps_util - optional helpers (distance, bearing). Needs math.h / -lm.
 * Only linked when you actually use the functions.
 * SPDX-License-Identifier: MIT
 */
#ifndef NEO_GPS_UTIL_H
#define NEO_GPS_UTIL_H

#include "neo_gps.h"
#include "neo_gps_time.h"   /* neo_gps_unix_time() now lives there */

#ifdef __cplusplus
extern "C" {
#endif

/* Distance in metres between two points (1e-7 degrees), haversine */
float neo_gps_distance_m(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2);

/* Fast approximation (equirectangular) - for distances < ~20 km, error < 0.1 % */
float neo_gps_distance_fast_m(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2);

/* Initial bearing from point 1 to point 2 in degrees (0..360) */
float neo_gps_bearing_deg(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2);

#ifdef __cplusplus
}
#endif

#endif /* NEO_GPS_UTIL_H */
