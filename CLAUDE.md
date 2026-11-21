# Regeln für dieses Repo (öffentlich, MIT)

Erster Schritt in jeder Session: `tools/session-setup.sh` (installiert nur, was fehlt; Toolchains aus r4d1um/setup).

Nur die Library `neo_gps` (C99, u-blox NEO-6M/7M/M8N), sonst nichts.

## Struktur

```
src/                    neo_gps (Parser, UBX), _filter (Sprungfilter), _time (Ortszeit), _util (Entfernung/Peilung)
tests/                  test_neo_gps.c (Host-Tests, Bench mit NEO_BENCH=1), link_mcu.c (Firmware-Linktest)
examples/               linux, arduino, stm32_hal, simulation (make sim)
Makefile                test, dist, dist-check, package, print-version
CMakeLists.txt          CMake + ESP-IDF-Komponente
library.json/.properties  PlatformIO / Arduino (Version muss zu src/neo_gps.h passen: make version-check)
.github/workflows/      ci.yml (Tests, alle Ziele, Release v<VERSION>), privcheck.yml
tools/session-setup.sh  Toolchains (nur öffentliche aus r4d1um/setup)
tools/privcheck/        Go-Check auf private Inhalte (go.mod im Root gilt nur dafür)
```

## Regeln

- Version in `src/neo_gps.h`, `library.json`, `library.properties` und CHANGELOG gleich erhöhen; das Mergen auf main
  macht Tag und Release `v<VERSION>` (Sessions können keine Tags pushen).
- Schnittstellen nur erweitern, nie umbenannt (SemVer).
- Nichts Privates: `go run ./tools/privcheck .` muss sauber sein; keine echten Orte, Hosts, IPs, Namen.
- Neue Ziele: Toolchain zuerst nach r4d1um/setup, dann hier in `tools/session-setup.sh` und der README-Tabelle.

## Entscheidungen

- 2026-11-21: aus r4d1um/iot-lib-public (`c/neo_gps`) in dieses eigene Repo umgezogen, ohne Git-Historie; ab 1.2.1
  Releases `v<version>` hier, die alten Releases `c/neo_gps/v*` bleiben in iot-lib-public.
