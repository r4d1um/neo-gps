# neo_gps

Lean, fast C library for u-blox GPS modules

| Module | Chip | Max. rate | UBX navigation |
|---|---|---|---|
| GY-GPSV3-NEO | **NEO-M8N-0-01** (u-blox M8) | 10 Hz | NAV-PVT (92 bytes) |
| GY-GPSV3-NEO / HW-448 | **NEO-7M-0-000** (u-blox 7) | 10 Hz | NAV-PVT (84 bytes) |
| GY-NEO6MV2 and similar | **NEO-6M-0-001** (u-blox 6) | 5 Hz | POSLLH + VELNED + TIMEUTC + SOL |

All three have the same pins (VCC, RX, TX, GND) and start at **9600 baud, NMEA**.

## Why this library

- **Two files** (`neo_gps.c` + `neo_gps.h`), plain C99, no `malloc`, no `printf`/`scanf`.
- **Streaming parser**: every byte is processed immediately, no line buffer. NMEA and UBX may be mixed in the same stream.
- **No float in the parser**: coordinates in 1e-7 degrees, altitude in mm, speed in mm/s; lossless and fast even on AVR and Cortex-M0.
- **Values are only taken over when the checksum is correct**; broken or truncated sentences change nothing.
- **Optional jump filter** against multipath outliers and drifting while standing still, also integer-only.
- **Local time with time zone and daylight saving time** (e.g. Berlin CET/CEST), without a time zone database.
- **Hardware-independent**: you feed in bytes and pass a send function. Runs on Arduino, ESP32, STM32, RP2040, Linux …
- **UBX configuration** built in: baud rate, measurement rate, output profiles, dynamic model, power save, save, reset, automatic model detection.

| | ATmega328P | Cortex-M0 | Cortex-M4 |
|---|---|---|---|
| Flash (complete) | 6.9 KB | 4.6 KB | 4.4 KB |
| Flash (NMEA only, `NEO_GPS_ENABLE_UBX=0`) | 3.5 KB | – | – |
| + jump filter (only if used) | +3.7 KB | +1.3 KB | +1.1 KB |
| + local time (only if used; + TZ string parser) | +2.3 KB (+1.2 KB) | – | +0.8 KB (+0.6 KB) |
| RAM (`neo_gps_t`) | 324 bytes | 340 bytes | 340 bytes |

On a PC it parses about 190 MB/s of NMEA; at 115200 baud the module delivers at most 11.5 KB/s.

## Wiring

```
Module TX  ->  MCU RX
Module RX  ->  MCU TX     (with 5 V MCUs a voltage divider to 3.3 V is recommended)
VCC        ->  3.3 V or 5 V (the boards have a regulator)
GND        ->  GND
```

## Quick start

```c
#include "neo_gps.h"

static neo_gps_t gps;

static void uart_tx(void *user, const uint8_t *data, uint16_t len)
{
    my_uart_write(data, len);          /* your UART function */
}

int main(void)
{
    neo_gps_init(&gps, uart_tx, NULL); /* uart_tx may be NULL = receive only */

    for (;;) {
        while (my_uart_available())
            neo_gps_feed(&gps, my_uart_read());

        if (neo_gps_updates(&gps) & NEO_UPD_POSITION) {
            const neo_gps_data_t *d = neo_gps_data(&gps);
            /* d->lat, d->lon     1e-7 degrees (481173230 = 48.1173230°)
               d->alt_msl         mm
               d->speed           mm/s
               d->course          1e-5 degrees
               d->time.hour ...   UTC                                      */
        }
    }
}
```

Instead of polling you can also react to the return value of `neo_gps_feed()` (`NEO_EV_NMEA_RMC`, `NEO_EV_UBX_NAV_PVT`, …).

## Using it in a project

The library lives publicly in [`r4d1um/neo-gps`](https://github.com/r4d1um/neo-gps). GitHub Actions tests it, builds all targets once and publishes every version as release **`v<version>`** ([releases](https://github.com/r4d1um/neo-gps/releases)). Projects fetch the finished package instead of copying source code.

### Prebuilt binaries (preferred)

For each target there is `neo_gps-<version>-<target>.tar.gz` with `lib/libneo_gps.a` and `include/`. Checksums are in `SHA256SUMS` on the same release.

| Target | Toolchain (Ubuntu 24.04 package) | Flags | For |
|---|---|---|---|
| `linux-x86_64` | gcc 13.3 (`gcc`) | `-O2 -fPIC` | PC |
| `linux-i686` | gcc 13.3 (`gcc-i686-linux-gnu`) | `-O2 -fPIC` | 32-bit PC |
| `linux-aarch64` | gcc 13.3 (`gcc-aarch64-linux-gnu`) | `-O2 -fPIC` | Pi 3/4/5 with 64-bit OS |
| `linux-armv6hf` | gcc 13.3 (`gcc-arm-linux-gnueabihf`) | `-march=armv6+fp -mfpu=vfp -mfloat-abi=hard -marm` | every Pi with a 32-bit OS (including Pi 1 / Zero) |
| `avr-atmega328p` | avr-gcc 7.3.0 (`gcc-avr`) | `-Os -mmcu=atmega328p` | Arduino Uno/Nano/Pro Mini |
| `cortex-m0` | arm-none-eabi 13.2.rel1 (`gcc-arm-none-eabi`) | `-Os -mcpu=cortex-m0 -mthumb` | STM32F0 |
| `cortex-m0plus` | arm-none-eabi 13.2.rel1 | `-Os -mcpu=cortex-m0plus -mthumb` | RP2040 (Pico / Pico W) |
| `cortex-m3` | arm-none-eabi 13.2.rel1 | `-Os -mcpu=cortex-m3 -mthumb` | STM32F1 |
| `cortex-m4f` | arm-none-eabi 13.2.rel1 | `-Os -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16` | STM32F4 |
| `cortex-m33` | arm-none-eabi 13.2.rel1 | `-Os -mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16` | RP2350 (Pico 2) |

The Linux libraries only use basic libc/libm functions (no symbols newer than glibc 2.17), so they link on
Raspberry Pi OS, Debian, Ubuntu and other current distributions.

All MCU targets are built with `-ffunction-sections -fdata-sections`; link with `-Wl,--gc-sections` and the linker only pulls in what you use. The float ABI must match your project (`cortex-m4f`/`m33` are hard-float).

```sh
V=1.2.4 T=cortex-m0plus
curl -fsSLO https://github.com/r4d1um/neo-gps/releases/download/v$V/neo_gps-$V-$T.tar.gz
curl -fsSL  https://github.com/r4d1um/neo-gps/releases/download/v$V/SHA256SUMS | grep "$T.tar.gz" | sha256sum -c
tar -xzf neo_gps-$V-$T.tar.gz -C third_party/
# CMake: target_include_directories(app PRIVATE third_party/neo_gps-$V-$T/include)
#        target_link_libraries(app ${CMAKE_CURRENT_SOURCE_DIR}/third_party/neo_gps-$V-$T/lib/libneo_gps.a m)
# gcc:   -Ithird_party/neo_gps-$V-$T/include third_party/neo_gps-$V-$T/lib/libneo_gps.a -lm
```

### Source package (for ESP32/ESP8266, PlatformIO, Arduino, ESP-IDF)

There is no binary for Xtensa (ESP32/ESP8266) yet; there, and when you change compile switches such as `NEO_GPS_ENABLE_UBX=0`, use `neo_gps-<version>-src.tar.gz` from the same release:

| Environment | How |
|---|---|
| PlatformIO | `lib_deps = https://github.com/r4d1um/neo-gps/releases/download/v1.2.4/neo_gps-1.2.4-src.tar.gz` |
| Arduino IDE / arduino-cli | unpack the archive to `~/Arduino/libraries/neo_gps` |
| ESP-IDF | unpack the archive to `components/neo_gps` (`CMakeLists.txt` detects IDF) |
| CMake (Pico SDK, Zephyr, STM32CubeIDE) | unpack, `add_subdirectory(neo_gps-1.2.4-src)` + `target_link_libraries(app neo_gps)` |

Always pin the version. `neo_gps_util.c/.h` (distance, bearing) needs `math.h`; `neo_gps_time.c/.h` (local time, Unix time) does not.

### Building it yourself

On Debian/Ubuntu the cross toolchains are plain packages (the same ones CI uses):

```sh
sudo apt-get install --no-install-recommends gcc-avr avr-libc binutils-avr \
  gcc-arm-none-eabi libnewlib-arm-none-eabi libnewlib-dev binutils-arm-none-eabi \
  gcc-aarch64-linux-gnu libc6-dev-arm64-cross gcc-arm-linux-gnueabihf libc6-dev-armhf-cross \
  gcc-i686-linux-gnu libc6-dev-i386-cross cmake
```

Then `make test`, `make dist` (missing compilers are skipped, `DIST_STRICT=1` turns that into an error), `make dist-check` links tests and a firmware against the finished `.a` files, `make package` creates the release packages in `build/pkg/`.

## Recommended configuration

**Simple (NMEA, enough for almost everything):**

```c
neo_gps_cfg_output(&gps, NEO_OUT_NMEA_MINIMAL);   /* only RMC + GGA + GSA */
neo_gps_cfg_dynmodel(&gps, NEO_DYN_PEDESTRIAN);   /* or AUTOMOTIVE, AIR_1G ... */
```

**Most efficient (UBX binary, 115200 baud, 10 Hz):**

```c
neo_gps_detect_model(&gps);                        /* MON-VER, answer -> NEO_EV_UBX_MON_VER */
/* ... wait for the event ... */
neo_gps_cfg_port(&gps, 115200, NEO_PROTO_UBX);     /* then switch your own UART to 115200 */
neo_gps_cfg_output(&gps, NEO_OUT_UBX);             /* picks the right messages for the model */
neo_gps_cfg_rate(&gps, 100);                       /* 10 Hz (NEO-6M: 200 = 5 Hz) */
neo_gps_cfg_save(&gps);                            /* optionally save permanently */
```

In UBX mode you additionally get accuracy estimates (`h_acc`, `v_acc`, `s_acc` in mm and mm/s), the NED velocity (`vel_n/e/d`) and the GPS time of week (`itow`).

Note on `cfg_save`: the NEO-6M/7M boards save to battery-backed RAM or EEPROM. If the coin cell is empty, the module starts with factory settings again.

## Jump filter (GPS jumps)

Single positions often jump 20–200 m off, especially between buildings (multipath), under trees or right after the fix. The filter is off by default and is enabled with one line:

```c
neo_gps_filter_preset(&gps, NEO_FILTER_PEDESTRIAN);   /* AUTOMOTIVE, AIRBORNE, STATIONARY, OFF */
neo_gps_cfg_dynmodel(&gps, NEO_DYN_PEDESTRIAN);       /* matching model in the module (recommended) */
```

After that `data.lat`/`data.lon` are the filtered position; the raw values are in `data.raw_lat`/`data.raw_lon`. A rejected position sets `NEO_UPD_REJECTED` instead of `NEO_UPD_POSITION`; `data.reject_reason` says why, `data.rejected` counts them. With a rejected position the altitude is not taken over either.

How it works, once per measurement:

1. **Quality**: valid fix, fix type, number of satellites, HDOP and (in UBX mode) the module's accuracy estimate `h_acc`.
2. **Jump test**: the distance since the last good position is compared with what is physically possible: 1.5 × speed × time + max. acceleration × time² + margin + 2 × `h_acc`. The module measures speed via Doppler, independently of the position, so a jump is also caught while driving. After a longer gap (tunnel) the allowed distance grows automatically.
3. **Re-anchoring**: if several "jumps" in a row land at the same place, the old position was wrong. The filter then takes over the new one instead of staying stuck forever.
4. **Smoothing while standing still**: below the standstill speed the position is smoothed exponentially, so the typical drift of a few metres disappears. Nothing is smoothed while moving, so there is no lag.

| Preset | min. satellites | max. HDOP | max. `h_acc` | acceleration | margin | re-anchor | smoothing at standstill |
|---|---|---|---|---|---|---|---|
| `PEDESTRIAN` | 5 | 4.0 | 25 m | 3 m/s² | 10 m | after 5 | 1/4, below 0.3 m/s |
| `AUTOMOTIVE` | 5 | 5.0 | 30 m | 8 m/s² | 15 m | after 5 | 1/4, below 0.5 m/s |
| `AIRBORNE` | 6 | 3.0 | 20 m | 20 m/s² | 10 m | after 3 | 1/2, below 0.2 m/s |
| `STATIONARY` | 6 | 3.0 | 15 m | 1 m/s² | 5 m | after 10 | 1/16, below 1 m/s |

All presets require a 3D fix. Your own values go through `neo_gps_filter_config()`; any limit set to `0` disables that check:

```c
neo_filter_cfg_t f = {
    .min_fix = NEO_FIX_3D, .min_sv = 6, .max_hdop = 250, .max_h_acc = 8000,
    .max_accel = 4000, .margin = 5000, .reanchor = 5,
    .smooth_shift = 3, .static_speed = 300,
};
neo_gps_filter_config(&gps, &f);
neo_gps_filter_reset(&gps);   /* forget the reference, e.g. after being carried while switched off */
```

The filter lives in `neo_gps_filter.c` and is only linked when you call `neo_gps_filter_*`. With `NEO_GPS_ENABLE_FILTER=0` its fields in RAM go away as well.

## Local time (time zone + daylight saving time)

The GPS delivers UTC. `neo_gps_time.h` turns it into local time, daylight saving time included:

```c
#include "neo_gps_time.h"

static const neo_tz_t berlin = NEO_TZ_BERLIN;      /* CET / CEST */

neo_local_time_t lt;
if (neo_gps_local_time(neo_gps_data(&gps), &berlin, &lt)) {
    /* lt.t.year, .month, .day, .hour, .minute, .second, .ms   -> local time
       lt.weekday  0 = Sunday
       lt.is_dst   1 = daylight saving time (CEST)
       lt.offset_min  60 or 120                                           */
}
```

Berlin follows the EU rule: daylight saving time from the last Sunday in March at 02:00 CET to the last Sunday in October at 03:00 CEST. Built in are `NEO_TZ_BERLIN` (= `NEO_TZ_CET`, valid for all of Central Europe), `NEO_TZ_LONDON`, `NEO_TZ_HELSINKI` and `NEO_TZ_UTC`.

Any other time zone works via the usual POSIX TZ string, the same as on Linux or the ESP32:

```c
neo_tz_t tz;
neo_tz_parse(&tz, "EST5EDT,M3.2.0,M11.1.0");        /* New York */
neo_tz_parse(&tz, "AEST-10AEDT,M10.1.0,M4.1.0/3");  /* Sydney (DST across New Year) */
neo_tz_parse(&tz, "IST-5:30");                      /* India, no DST */
```

Rules in the format `Mm.w.d[/time]` are supported, which covers almost all zones. Time zone database names such as `Europe/Berlin` do not work; you find the matching POSIX string for any zone in `/usr/share/zoneinfo` (last line of the file) or in the common tables for ESP32/`setenv("TZ", …)`.

All calculations are integer-only and without libc `time.h`, valid for 1970–2105. The tests compare the conversion for eight zones, including half-hour DST, switching at "24:00" and negative switch times, against glibc over 1971–2105, to the minute around every switch.

Building blocks for your own use: `neo_gps_unix_time()`, `neo_tz_utc_to_local()`, `neo_time_from_unix()`, `neo_time_to_unix()`.

Note: right after a cold start the module does not know the current leap seconds yet and uses the value from its firmware. With older firmware (NEO-6M in particular) the UTC time can then be a few seconds off until the UTC parameters have been received, after about 12.5 minutes at most.

## API overview

### Core

| Function | |
|---|---|
| `neo_gps_init(gps, write, user)` | initialise |
| `neo_gps_feed(gps, byte)` | process one byte → `neo_event_t` (ISR-safe) |
| `neo_gps_feed_buf(gps, buf, len)` | process a buffer → number of valid messages |
| `neo_gps_updates(gps)` | `NEO_UPD_*` bits since the last call (resets them) |
| `neo_gps_data(gps)` | pointer to `neo_gps_data_t` |
| `neo_gps_has_fix(gps)` | position valid? |
| `neo_gps_lat_deg/lon_deg/alt_m/speed_kmh/course_deg` | float convenience functions |
| `neo_gps_set_ubx_handler(gps, fn)` | callback for every UBX message (your own extensions) |
| `neo_gps_filter_preset(gps, preset)` | jump filter on/off with a preset |
| `neo_gps_filter_config(gps, &cfg)` | jump filter with your own values |
| `neo_gps_filter_reset(gps)` | forget the filter reference |
| `neo_gps_local_time(data, &tz, &lt)` | local time with DST (`neo_gps_time.h`) |
| `neo_tz_parse(&tz, "CET-1CEST,…")` | time zone from a POSIX TZ string |

### Configuration (UBX)

| Function | UBX | |
|---|---|---|
| `neo_gps_detect_model` | MON-VER | detects NEO-6 / NEO-7 / M8 |
| `neo_gps_cfg_port(baud, proto)` | CFG-PRT | baud rate + output protocols |
| `neo_gps_cfg_rate(ms)` | CFG-RATE | measurement interval |
| `neo_gps_cfg_msg(cls, id, rate)` | CFG-MSG | single message on/off |
| `neo_gps_cfg_output(profile)` | CFG-MSG | `NMEA_DEFAULT`, `NMEA_MINIMAL`, `UBX` |
| `neo_gps_cfg_dynmodel(model)` | CFG-NAV5 | Portable, Stationary, Pedestrian, Automotive, Sea, Air |
| `neo_gps_cfg_power_save(on)` | CFG-RXM | power save mode |
| `neo_gps_cfg_save()` | CFG-CFG | save configuration |
| `neo_gps_cfg_factory_reset()` | CFG-CFG | factory settings |
| `neo_gps_reset(HOT/WARM/COLD)` | CFG-RST | GNSS restart |
| `neo_gps_ack_status()` | ACK | `PENDING`, `OK`, `NAK` of the last CFG message |
| `neo_gps_ubx_send / _ubx_poll` | – | any UBX message |

All commands are non-blocking. The acknowledgement arrives as `NEO_EV_UBX_ACK`/`NEO_EV_UBX_NAK` through `neo_gps_feed()`.

### Parsed messages

- **NMEA** (all talkers: GP, GN, GL, GA, GB): RMC, GGA, GSA, VTG, GSV (number of satellites in view)
- **UBX**: NAV-PVT, NAV-POSLLH, NAV-VELNED, NAV-TIMEUTC, NAV-SOL, NAV-DOP, ACK-ACK/NAK, MON-VER

### Compile-time options

| Macro | Default | |
|---|---|---|
| `NEO_GPS_ENABLE_NMEA` | 1 | NMEA parser |
| `NEO_GPS_ENABLE_UBX` | 1 | UBX parser + configuration |
| `NEO_GPS_ENABLE_FILTER` | 1 | jump filter (fields + `neo_gps_filter.c`) |
| `NEO_GPS_UBX_MAX_PAYLOAD` | 92 | buffer for the UBX payload |
| `NEO_GPS_NMEA_FIELD_MAX` | 15 | max. characters per NMEA field |

## Examples

- `examples/arduino/NeoGpsBasic`: NMEA, also runs on the Uno (SoftwareSerial)
- `examples/arduino/NeoGpsUbx10Hz`: UBX binary mode at 115200 baud and 5–10 Hz
- `examples/stm32_hal`: interrupt + ring buffer
- `examples/linux`: USB serial adapter or Raspberry Pi: `make linux && ./build/neo_gps_linux /dev/ttyUSB0 9600`
- `examples/simulation`: terrain simulation with 4 sensors (area and slope, 100 × 24 h through parser and jump filter): `make sim` writes `build/sim.json`

## Tests

```sh
make test     # 266 checks: NMEA, UBX, errors, resync, sending, model detection, filter, time zones
make bench    # throughput
```

CI additionally builds with sanitizers and for AVR and Cortex-M0 with `-Werror`.

## Disclaimer

neo_gps is provided as is, without warranty (see [LICENSE](LICENSE)). GPS positions can be wrong, jump or be
missing at any time, and the jump filter reduces but does not rule out bad positions. Do not use this library
on its own for safety-critical purposes such as aviation, marine or rescue navigation, or anything where a wrong
position can put people or property at risk.

`examples/simulation/index.html` loads nothing from third-party servers (no web fonts, no CDNs), so it can be
hosted publicly without passing visitors' data to anyone.

## License

MIT
