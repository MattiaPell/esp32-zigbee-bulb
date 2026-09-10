#!/usr/bin/env bash
# Host-side unit tests (Linux/macOS/CI). Needs a C++17 compiler; override the
# compiler with CXX (e.g. CXX=clang++ test/run.sh).
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

cxx="${CXX:-g++}"
out="$(mktemp -d)/host_tests"

# shellcheck disable=SC2046
"$cxx" -std=gnu++17 -Wall -Wextra -Itest/shim -I. test/*.cpp -o "$out"
"$out"
