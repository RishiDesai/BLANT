#!/bin/bash
# Cross-validation test runner for nauty canonical labeling vs BLANT flat tables.
#
# By default, runs k=3 through k=7 (instant to ~3 seconds).
# Pass --k8 to also run k=8 (~5-6 minutes, 268M entries).
#
# Usage:
#   ./tests/run_cross_validation.sh          # k=3 through k=7
#   ./tests/run_cross_validation.sh --k8     # k=3 through k=8

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_DIR"

# Build if needed
if [ ! -f tests/cross-validate-nauty ]; then
    echo "Building cross-validate-nauty..."
    make tests/cross-validate-nauty
fi

RUN_K8=0
for arg in "$@"; do
    case "$arg" in
        --k8) RUN_K8=1 ;;
        *) echo "Unknown argument: $arg"; echo "Usage: $0 [--k8]"; exit 1 ;;
    esac
done

MAX_K=7
if [ "$RUN_K8" -eq 1 ]; then
    MAX_K=8
fi

PASS=0
FAIL=0

for k in $(seq 3 $MAX_K); do
    # Check that the required canon_map binary exists
    if [ ! -f "canon_maps/canon_map${k}.bin" ]; then
        echo "SKIP k=$k: canon_maps/canon_map${k}.bin not found"
        continue
    fi

    echo "=== Cross-validating k=$k ==="
    if ./tests/cross-validate-nauty "$k"; then
        PASS=$((PASS + 1))
    else
        echo "FAIL: k=$k cross-validation failed!"
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "=============================="
echo "Results: $PASS passed, $FAIL failed"
if [ "$FAIL" -gt 0 ]; then
    echo "OVERALL: FAIL"
    exit 1
else
    echo "OVERALL: PASS"
    exit 0
fi
