#!/usr/bin/env bash
# Reproducible local verification of the Phase 0 exit gate.
# Configures, builds and tests all four configurations in order.
set -euo pipefail

cd "$(dirname "$0")/../.."

: "${CXX:=g++}"
: "${CC:=gcc}"
export CXX CC

for preset in debug development release server; do
    echo "=== ${preset}: configure ==="
    cmake --preset "${preset}"
    echo "=== ${preset}: build ==="
    cmake --build --preset "${preset}" --parallel
    echo "=== ${preset}: test ==="
    ctest --preset "${preset}" --output-on-failure
done

echo "All four configurations built and tested successfully."
