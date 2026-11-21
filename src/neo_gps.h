/*
 * neo_gps - small, fast C library for u-blox NEO-6M / NEO-7M / NEO-M8N
 *
 *  - plain C99, no dynamic memory, no printf/scanf
 *  - byte-wise streaming parser for NMEA *and* UBX (mixed in the same stream)
 *  - integer-only (fixed-point) arithmetic in the parser -> fast even on AVR/Cortex-M0
 *  - hardware-independent: you supply the received bytes and a send function
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef NEO_GPS_H
#define NEO_GPS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEO_GPS_VERSION "1.2.4"

/* ------------------------------------------------------------------------- */
/* Compile-time configuration (override before the #include or with -D)     */
/* ------------------------------------------------------------------------- */

#ifndef NEO_GPS_ENABLE_NMEA
#define NEO_GPS_ENABLE_NMEA 1        /* NMEA parser (RMC, GGA, GSA, VTG, GSV) */
#endif
#ifndef NEO_GPS_ENABLE_UBX
#define NEO_GPS_ENABLE_UBX 1         /* UBX parser + configuration commands  */
#endif
#ifndef NEO_GPS_UBX_MAX_PAYLOAD
#define NEO_GPS_UBX_MAX_PAYLOAD 92   /* NAV-PVT (M8) = 92 bytes; longer messages are stored
                                        truncated but fully checked */
#endif
#ifndef NEO_GPS_UBX_LEN_LIMIT
#define NEO_GPS_UBX_LEN_LIMIT 1024   /* larger length fields are treated as garbage */
#endif
#ifndef NEO_GPS_ENABLE_FILTER
#define NEO_GPS_ENABLE_FILTER 1      /* jump/quality filter for the position */
#endif
#ifndef NEO_GPS_NMEA_FIELD_MAX
#define NEO_GPS_NMEA_FIELD_MAX 15    /* max. characters per parsed NMEA field */
#endif

/* ------------------------------------------------------------------------- */
/* Types                                                                     */
/* ------------------------------------------------------------------------- */

/* Fix type, identical to u-blox gpsFix/fixType */
typedef enum {
    NEO_FIX_NONE      = 0,
    NEO_FIX_DR        = 1,  /* dead reckoning only */
    NEO_FIX_2D        = 2,
    NEO_FIX_3D        = 3,
    NEO_FIX_GNSS_DR   = 4,
    NEO_FIX_TIME_ONLY = 5
} neo_fix_t;

typedef enum {
    NEO_MODEL_UNKNOWN = 0,
    NEO_MODEL_NEO6,         /* u-blox 6  (NEO-6M-0-001)  - no NAV-PVT, max 5 Hz   */
    NEO_MODEL_NEO7,         /* u-blox 7  (NEO-7M-0-000)  - NAV-PVT (84 B), max 10 Hz */
    NEO_MODEL_M8            /* u-blox M8 (NEO-M8N-0-01)  - NAV-PVT (92 B), max 10 Hz */
} neo_model_t;

/* Return value of neo_gps_feed(): which message has just been completed */
typedef enum {
    NEO_EV_NONE = 0,
    NEO_EV_NMEA_RMC,
    NEO_EV_NMEA_GGA,
    NEO_EV_NMEA_GSA,
    NEO_EV_NMEA_VTG,
    NEO_EV_NMEA_GSV,
    NEO_EV_UBX_NAV_PVT,
    NEO_EV_UBX_NAV_POSLLH,
    NEO_EV_UBX_NAV_VELNED,
    NEO_EV_UBX_NAV_TIMEUTC,
    NEO_EV_UBX_NAV_SOL,
    NEO_EV_UBX_NAV_DOP,
    NEO_EV_UBX_ACK,
    NEO_EV_UBX_NAK,
    NEO_EV_UBX_MON_VER,
    NEO_EV_UBX_OTHER,       /* valid UBX message without a built-in handler */
    NEO_EV_CHECKSUM_ERROR
} neo_event_t;

/* Bits in data.updated - what has changed since the last neo_gps_updates() */
#define NEO_UPD_POSITION  0x0001u
#define NEO_UPD_ALTITUDE  0x0002u
#define NEO_UPD_TIME      0x0004u
#define NEO_UPD_DATE      0x0008u
#define NEO_UPD_SPEED     0x0010u
#define NEO_UPD_COURSE    0x0020u
#define NEO_UPD_FIX       0x0040u
#define NEO_UPD_DOP       0x0080u
#define NEO_UPD_SATS      0x0100u
#define NEO_UPD_ACCURACY  0x0200u   /* UBX only */
#define NEO_UPD_VELNED    0x0400u   /* UBX only */
#define NEO_UPD_REJECTED  0x0800u   /* the filter rejected a position (see reject_reason) */

/* Reason for the last rejected position (data.reject_reason) */
typedef enum {
    NEO_REJECT_NONE = 0,
    NEO_REJECT_FIX,         /* fix type too poor             */
    NEO_REJECT_SATS,        /* too few satellites            */
    NEO_REJECT_HDOP,        /* HDOP too high                 */
    NEO_REJECT_ACCURACY,    /* h_acc too high (UBX only)     */
    NEO_REJECT_JUMP         /* implausible position jump     */
} neo_reject_t;

typedef struct {
    uint16_t year;          /* e.g. 2026 */
    uint8_t  month, day;
    uint8_t  hour, minute, second;
    uint16_t ms;
} neo_datetime_t;

/* All values as integers - lossless and without float costs */
typedef struct {
    int32_t  lat;           /* latitude  in 1e-7 degrees (481173230 = 48.1173230 N) */
    int32_t  lon;           /* longitude in 1e-7 degrees (negative = west)     */
    int32_t  alt_msl;       /* altitude above mean sea level in mm             */
    int32_t  geoid_sep;     /* Ellipsoid - MSL in mm                            */
    uint32_t speed;         /* speed over ground in mm/s                       */
    int32_t  course;        /* course over ground in 1e-5 degrees (0..36000000) */
    int32_t  vel_n, vel_e, vel_d; /* NED velocity mm/s (UBX only)               */
    uint32_t h_acc, v_acc;  /* accuracy estimate in mm (UBX only)              */
    uint32_t s_acc;         /* speed accuracy mm/s (UBX only)                  */
    uint32_t itow;          /* GPS time of week in ms (UBX only)               */
    neo_datetime_t time;    /* UTC                                              */
    uint16_t hdop, vdop, pdop; /* in 0.01 (101 = 1.01)                          */
    uint8_t  fix;           /* neo_fix_t                                        */
    uint8_t  valid;         /* 1 = position valid                              */
    uint8_t  time_valid;    /* 1 = time valid                                  */
    uint8_t  date_valid;    /* 1 = date valid                                  */
    uint8_t  num_sv;        /* satellites used in the solution                 */
    uint8_t  sats_in_view;  /* satellites in view (from GSV)                   */
    uint8_t  quality;       /* GGA quality: 0 no fix, 1 GPS, 2 DGPS, 6 DR      */
    uint16_t updated;       /* NEO_UPD_* Bits                                   */
#if NEO_GPS_ENABLE_FILTER
    /* With the filter active, lat/lon are the filtered position; raw values here: */
    int32_t  raw_lat, raw_lon;
    uint16_t rejected;      /* number of rejected positions (wraps around)    */
    uint8_t  reject_reason; /* neo_reject_t of the last rejected position     */
#endif
} neo_gps_data_t;

#if NEO_GPS_ENABLE_FILTER
/* Filter settings. Any limit = 0 disables that check. */
typedef struct {
    uint8_t  min_fix;       /* e.g. NEO_FIX_3D (ignored if no GSA/UBX fix type arrives) */
    uint8_t  min_sv;        /* min. satellites in the solution                  */
    uint16_t max_hdop;      /* in 0.01 (400 = 4.0)                               */
    uint32_t max_h_acc;     /* mm, UBX only                                      */
    uint32_t max_accel;     /* mm/s^2 - allowed acceleration for the jump test; 0 = no jump test */
    uint32_t margin;        /* mm  - fixed margin in the jump test (+ 2 x h_acc)  */
    uint8_t  reanchor;      /* take over the new position after this many jumps in a row */
    uint8_t  smooth_shift;  /* smoothing at standstill: new share 1/2^n (2 = 25 %), 0 = off */
    uint16_t static_speed;  /* mm/s - below this counts as "standstill" (smoothing on) */
} neo_filter_cfg_t;

typedef enum {
    NEO_FILTER_OFF = 0,
    NEO_FILTER_PEDESTRIAN,
    NEO_FILTER_AUTOMOTIVE,
    NEO_FILTER_AIRBORNE,    /* drone, aircraft                            */
    NEO_FILTER_STATIONARY   /* fixed mount: strong smoothing, hardly any movement */
} neo_filter_preset_t;
#endif

/* Send callback: writes len bytes to the module's UART (the module's RX pin) */
typedef void (*neo_write_fn)(void *user, const uint8_t *data, uint16_t len);

/* Optional callback for every valid UBX message (extensions without touching the
 * library). len = stored length (max. NEO_GPS_UBX_MAX_PAYLOAD). */
typedef void (*neo_ubx_handler_fn)(void *user, uint8_t cls, uint8_t id,
                                   const uint8_t *payload, uint16_t len);

typedef enum { NEO_ACK_NONE = 0, NEO_ACK_PENDING, NEO_ACK_OK, NEO_ACK_NAK } neo_ack_t;

#define NEO_GPS_TIME_UNKNOWN 0xFFFFFFFFUL   /* internal */

struct neo_gps;
typedef uint8_t (*neo_pos_filter_fn)(struct neo_gps *gps, int32_t lat, int32_t lon,
                                     uint32_t t_ms, uint32_t period_ms);

/* Complete state - no malloc, simply create it as a global/static variable. */
typedef struct neo_gps {
    neo_gps_data_t data;

    /* Statistics */
    uint32_t msgs_ok;
    uint32_t msgs_err;

    /* --- internal --- */
    neo_write_fn       write;
    neo_ubx_handler_fn ubx_handler;
    void              *user;
    uint8_t  model;          /* neo_model_t */
    uint8_t  state;
    uint8_t  ack_state, ack_cls, ack_id;
#if NEO_GPS_ENABLE_NMEA
    uint8_t  n_chk, n_rx_chk, n_field, n_flen, n_type, n_talker, n_len;
    char     n_buf[NEO_GPS_NMEA_FIELD_MAX + 1];
    uint8_t  gsv_view[4];
    struct {
        uint32_t time_raw, date_raw, speed_raw;
        int32_t  lat, lon, alt, sep, course;
        uint16_t hdop, pdop, vdop, have;
        uint8_t  num_sv, quality, nav_mode, status, mode, in_view;
    } n;
#endif
#if NEO_GPS_ENABLE_FILTER
    neo_pos_filter_fn f_fn;          /* NULL = filter off */
    neo_filter_cfg_t f_cfg;
    int32_t  f_lat, f_lon;          /* reference = last accepted position      */
    uint32_t f_t, f_speed;          /* its time (ms) and speed                 */
    uint32_t f_epoch;               /* time of the last evaluated epoch        */
    uint8_t  f_have, f_epoch_ok, f_epoch_set, f_jumps;
#endif
#if NEO_GPS_ENABLE_UBX
    uint8_t  u_cls, u_id, u_cka, u_ckb;
    uint16_t u_len, u_idx;
    uint8_t  u_buf[NEO_GPS_UBX_MAX_PAYLOAD];
#endif
} neo_gps_t;

/* ------------------------------------------------------------------------- */
/* Core API                                                                  */
/* ------------------------------------------------------------------------- */

/* Initialises the struct. write may be NULL (receive only). */
void neo_gps_init(neo_gps_t *gps, neo_write_fn write, void *user);

/* Process one received byte. Returns the event when a message has been received
 * completely and validly, otherwise NEO_EV_NONE.
 * Can be called directly from a UART ISR (constant, very short run time). */
neo_event_t neo_gps_feed(neo_gps_t *gps, uint8_t byte);

/* Process several bytes. Returns the number of valid messages. */
uint16_t neo_gps_feed_buf(neo_gps_t *gps, const uint8_t *buf, uint16_t len);

/* Returns the update bits since the last call and resets them. */
uint16_t neo_gps_updates(neo_gps_t *gps);

#if NEO_GPS_ENABLE_FILTER
/* Position filter against GPS jumps (off by default).
 * Checks: fix type, satellites, HDOP, h_acc and a jump test that compares the distance to
 * the last good position with the module's (Doppler) speed.
 * At standstill the position is also smoothed. Integer-only, runs once per epoch.
 * Result: data.lat/lon = filtered, data.raw_lat/raw_lon = raw. */
void neo_gps_filter_preset(neo_gps_t *gps, neo_filter_preset_t preset);
void neo_gps_filter_config(neo_gps_t *gps, const neo_filter_cfg_t *cfg);
void neo_gps_filter_reset(neo_gps_t *gps);   /* forget the reference (e.g. after moving elsewhere) */
#endif

/* Convenient access */
static inline const neo_gps_data_t *neo_gps_data(const neo_gps_t *gps) { return &gps->data; }
static inline int neo_gps_has_fix(const neo_gps_t *gps) { return gps->data.valid != 0; }
static inline neo_model_t neo_gps_model(const neo_gps_t *gps) { return (neo_model_t)gps->model; }
static inline void neo_gps_set_model(neo_gps_t *gps, neo_model_t m) { gps->model = (uint8_t)m; }
static inline void neo_gps_set_ubx_handler(neo_gps_t *gps, neo_ubx_handler_fn fn) { gps->ubx_handler = fn; }

/* Float convenience (note: on AVR double is only 32 bit -> ~1 m resolution) */
static inline double neo_gps_lat_deg(const neo_gps_t *gps) { return (double)gps->data.lat * 1e-7; }
static inline double neo_gps_lon_deg(const neo_gps_t *gps) { return (double)gps->data.lon * 1e-7; }
static inline float  neo_gps_alt_m(const neo_gps_t *gps)   { return (float)gps->data.alt_msl * 1e-3f; }
static inline float  neo_gps_speed_kmh(const neo_gps_t *gps) { return (float)gps->data.speed * 0.0036f; }
static inline float  neo_gps_course_deg(const neo_gps_t *gps) { return (float)gps->data.course * 1e-5f; }

/* ------------------------------------------------------------------------- */
/* UBX configuration (all commands work on NEO-6M, NEO-7M, NEO-M8N)        */
/* All functions send immediately and do not block. Acknowledgement via     */
/* neo_gps_ack_status().                                                     */
/* ------------------------------------------------------------------------- */
#if NEO_GPS_ENABLE_UBX

#define NEO_PROTO_UBX   0x01u
#define NEO_PROTO_NMEA  0x02u

typedef enum {
    NEO_DYN_PORTABLE   = 0,
    NEO_DYN_STATIONARY = 2,
    NEO_DYN_PEDESTRIAN = 3,
    NEO_DYN_AUTOMOTIVE = 4,
    NEO_DYN_SEA        = 5,
    NEO_DYN_AIR_1G     = 6,
    NEO_DYN_AIR_2G     = 7,
    NEO_DYN_AIR_4G     = 8
} neo_dyn_model_t;

typedef enum {
    NEO_OUT_NMEA_DEFAULT = 0, /* factory default: GGA GLL GSA GSV RMC VTG       */
    NEO_OUT_NMEA_MINIMAL,     /* only RMC + GGA + GSA (saves ~70 % data rate)  */
    NEO_OUT_UBX               /* NMEA off, binary UBX (most efficient mode)    */
} neo_output_t;

typedef enum { NEO_START_HOT = 0, NEO_START_WARM, NEO_START_COLD } neo_start_t;

/* Send any UBX message */
void neo_gps_ubx_send(neo_gps_t *gps, uint8_t cls, uint8_t id,
                      const uint8_t *payload, uint16_t len);
/* Poll request (empty payload) */
static inline void neo_gps_ubx_poll(neo_gps_t *gps, uint8_t cls, uint8_t id)
{ neo_gps_ubx_send(gps, cls, id, NULL, 0); }

/* Query MON-VER -> sets the model automatically (NEO_EV_UBX_MON_VER) */
void neo_gps_detect_model(neo_gps_t *gps);

/* The module's UART1: baud rate + output protocols (NEO_PROTO_*).
 * Afterwards switch your own UART to the new baud rate! */
void neo_gps_cfg_port(neo_gps_t *gps, uint32_t baud, uint8_t out_proto);

/* Measurement rate in ms (1000 = 1 Hz, 200 = 5 Hz, 100 = 10 Hz). NEO-6: max 5 Hz. */
void neo_gps_cfg_rate(neo_gps_t *gps, uint16_t meas_ms);

/* Output rate of one message (0 = off, 1 = every measurement, n = every n-th) */
void neo_gps_cfg_msg(neo_gps_t *gps, uint8_t cls, uint8_t id, uint8_t rate);

/* Ready-made output profiles (several CFG-MSG in a row).
 * NEO_OUT_UBX needs the model to be known (neo_gps_detect_model or _set_model):
 * NEO-6 gets POSLLH+VELNED+TIMEUTC+SOL+DOP, NEO-7/M8 gets NAV-PVT+DOP. */
void neo_gps_cfg_output(neo_gps_t *gps, neo_output_t out);

/* Dynamic model (CFG-NAV5) */
void neo_gps_cfg_dynmodel(neo_gps_t *gps, neo_dyn_model_t model);

/* Power save mode (CFG-RXM): 0 = full power, 1 = power save */
void neo_gps_cfg_power_save(neo_gps_t *gps, uint8_t enable);

/* Save the current configuration permanently (BBR/flash/EEPROM) */
void neo_gps_cfg_save(neo_gps_t *gps);

/* Load + save factory settings */
void neo_gps_cfg_factory_reset(neo_gps_t *gps);

/* GNSS restart (hot/warm/cold) */
void neo_gps_reset(neo_gps_t *gps, neo_start_t type);

/* Status of the last CFG message sent */
neo_ack_t neo_gps_ack_status(const neo_gps_t *gps);

#endif /* NEO_GPS_ENABLE_UBX */

#ifdef __cplusplus
}
#endif

#endif /* NEO_GPS_H */
