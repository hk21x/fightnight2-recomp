#!/usr/bin/env bash
# interp-dump.sh FILE [threshold]: analyse a smooth-motion dump (F11 in the game): build tools/interp_dump, run it
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .local/tools
[ .local/tools/interp_dump -nt tools/interp_dump.cpp ] || g++ -std=c++20 -O2 -I sdk/runtime -o .local/tools/interp_dump tools/interp_dump.cpp
exec .local/tools/interp_dump "$@"
