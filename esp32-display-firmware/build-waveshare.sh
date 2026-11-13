#!/bin/sh
set -eu
cd "$(dirname "$0")"
# Install the explicitly pinned Arduino 3 toolchain before the pre-build hook.
# The classic WROOM environments use Arduino 2 in the same PlatformIO cache.
pio pkg install -e waveshare_c6_147
pio run -e waveshare_c6_147 "$@"
