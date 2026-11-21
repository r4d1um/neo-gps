# Changelog neo_gps

Versions follow SemVer. Interfaces are only extended, never renamed.
Every version is published as release `v<version>` in
[r4d1um/neo-gps](https://github.com/r4d1um/neo-gps/releases)
(up to 1.2.0: `c/neo_gps/v<version>` in r4d1um/iot-lib-public).

## 1.2.3

- README: disclaimer (no warranty, not for safety-critical navigation on its own).
- Simulation page loads no web fonts from Google any more (system fonts only), so it can be hosted
  publicly without sending visitors' IP addresses to a third party. Code and API unchanged.

## 1.2.2

- Everything in English: README, CHANGELOG, code comments, test and example output, workflows.
  Code and API unchanged.

## 1.2.1

- Own repo [r4d1um/neo-gps](https://github.com/r4d1um/neo-gps), releases now as `v<version>`.
  Code and API unchanged; the releases up to 1.2.0 stay in r4d1um/iot-lib-public.

## 1.2.0

- Local time with time zone and daylight saving time (`neo_gps_time.c/.h`), Unix time.
- Example: terrain simulation for area and slope with four receivers.
- 2026-11-11: moved to `iot-lib-public/c/neo_gps` (before: `r4d1um/lib`, earlier
  `r4d1um/gps-library-neo`). Reason: public, versioned releases instead of committed
  binaries. The history was not carried over because it contains private author data.
  Code and API are unchanged.

## 1.1.0

- Jump filter against GPS jumps (`neo_gps_filter.c`).

## 1.0.0

- First version: streaming parser for NMEA and UBX without malloc and without float,
  UBX configuration, model detection for NEO-6M, NEO-7M and NEO-M8N.
