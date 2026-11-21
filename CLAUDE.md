# Rules for this repo (public, MIT)

First step in every session: `tools/session-setup.sh` (installs only what is missing; toolchains from r4d1um/setup).

Only the `neo_gps` library (C99, u-blox NEO-6M/7M/M8N), nothing else.

## Map

```
src/                    neo_gps (parser, UBX), _filter (jump filter), _time (local time), _util (distance/bearing)
tests/                  test_neo_gps.c (host tests, bench with NEO_BENCH=1), link_mcu.c (firmware link test)
examples/               linux, arduino, stm32_hal, simulation (make sim)
Makefile                test, dist, dist-check, package, print-version
CMakeLists.txt          CMake + ESP-IDF component
library.json/.properties  PlatformIO / Arduino (version must match src/neo_gps.h: make version-check)
.github/workflows/      ci.yml (tests, all targets, release v<VERSION>), privcheck.yml
tools/session-setup.sh  toolchains (public ones from r4d1um/setup only)
tools/privcheck/        Go check for private content (the root go.mod is only for this)
```

## Rules

- **English only**: code, comments, docs, CHANGELOG, workflows, test and example output, commit messages, PRs, issues.
  No German anywhere in this repo.
- Raise the version in `src/neo_gps.h`, `library.json`, `library.properties` and CHANGELOG together; merging to main
  creates tag and release `v<VERSION>` (sessions can't push tags).
- Interfaces are only extended, never renamed (SemVer).
- Nothing private: `go run ./tools/privcheck .` must be clean; no real places, hosts, IPs or names.
- New targets: toolchain first into r4d1um/setup, then here in `tools/session-setup.sh` and the README table.

## Decisions

- 2026-11-21: moved from r4d1um/iot-lib-public (`c/neo_gps`) into this repo of its own, without git history; from 1.2.1
  releases are `v<version>` here, the old `c/neo_gps/v*` releases stay in iot-lib-public.
- 2026-11-21: English only (1.2.2 translated everything).
