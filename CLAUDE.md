# Rules for this repo (public, MIT)

Toolchains: Ubuntu packages, the apt-get line in the README ("Building it yourself"), the same ones CI installs.

Only the `neo_gps` library (C99, u-blox NEO-6M/7M/M8N), nothing else.

## Map

```
src/                    neo_gps (parser, UBX), _filter (jump filter), _time (local time), _util (distance/bearing)
tests/                  test_neo_gps.c (host tests, bench with NEO_BENCH=1), link_mcu.c (firmware link test)
examples/               linux, arduino, stm32_hal, simulation (make sim)
Makefile                test, dist, dist-check, package, print-version
CMakeLists.txt          CMake + ESP-IDF component
library.json/.properties  PlatformIO / Arduino (version must match src/neo_gps.h: make version-check)
.github/workflows/      ci.yml (tests, all targets, release v<VERSION>)
```

## Rules

- **English only**: code, comments, docs, CHANGELOG, workflows, test and example output, commit messages, PRs, issues.
  No German anywhere in this repo.
- Raise the version in `src/neo_gps.h`, `library.json`, `library.properties` and CHANGELOG together; merging to main
  creates tag and release `v<VERSION>` (sessions can't push tags).
- Interfaces are only extended, never renamed (SemVer).
- Nothing private: no real places, hosts, IPs, names, email addresses or credentials. There is no automatic check,
  so review every change for this before pushing.
- **Self-contained**: no dependency on any of the owner's helper repos (kit, setup, lib, r4setup …). Only standard tooling:
  GitHub Actions from the marketplace and distro packages.
- New targets: add the Ubuntu package to `ci.yml` and the README apt-get line, and a row to the README table.

## Decisions

- 2026-11-21: moved from r4d1um/iot-lib-public (`c/neo_gps`) into this repo of its own, without git history; from 1.2.1
  releases are `v<version>` here, the old `c/neo_gps/v*` releases stay in iot-lib-public.
- 2026-11-21: English only (1.2.2 translated everything).
- 2026-11-21: toolchains from Ubuntu packages via awalsh128/cache-apt-pkgs-action instead of r4d1um/setup (1.2.4):
  an open-source repo must build without the owner's private infrastructure.
- 2026-11-21: tools/privcheck (and its go.mod and workflow) removed: the repo is a C library only; privacy is
  checked by review before pushing.
