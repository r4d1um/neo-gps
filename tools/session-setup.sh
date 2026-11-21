#!/bin/sh
# Installs the toolchains for this repo from r4d1um/setup (only what is missing).
#   tools/session-setup.sh                 all toolchains below
#   tools/session-setup.sh avr-gcc         only the ones named
# Public toolchains only: the CI token of this public repo can't read setup-private.
set -eu
R4SETUP_REF=v1.3.0
[ "$#" -gt 0 ] || set -- avr-gcc arm-none-eabi aarch64-linux-gnu arm-linux-gnueabihf i686-linux-gnu
curl -fsSL "https://raw.githubusercontent.com/r4d1um/setup/$R4SETUP_REF/bootstrap.sh" \
  | R4SETUP_REF=$R4SETUP_REF sh -s -- "$@"
