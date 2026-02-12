#!/bin/bash
# run_strong_tests.sh - Run all strong verification tests for k=9 or k=10
#
# Usage:
#   ./tests/run_strong_tests.sh [k]     # default k=9
#   ./tests/run_strong_tests.sh 9       # k=9 (full suite)
#   ./tests/run_strong_tests.sh 10      # k=10 (idempotent + isomorphism + orbits only)
#
# Run from the repo root directory.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_DIR"

K=${1:-9}

PASS=0
FAIL=0
SKIP=0
TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1"; }
skip() { SKIP=$((SKIP+1)); TOTAL=$((TOTAL+1)); echo "  SKIP: $1"; }

echo "=== Strong Verification Tests for k=$K ==="
echo ""

# Build test binaries if needed
if [ ! -f tests/test-idempotent ]; then
    echo "Building strong test binaries..."
    make strong-tests
    echo ""
fi

# Check prerequisites
if [ ! -f "canon_maps/canon_list${K}.txt" ]; then
    echo "ERROR: canon_maps/canon_list${K}.txt not found."
    echo "  Generate it first with: make k${K}"
    exit 1
fi

# ---------- Test 1: Canonical Idempotency ----------
echo "Test 1: Canonical Idempotency (k=$K)"
if [ -f tests/test-idempotent ]; then
    if ./tests/test-idempotent "$K"; then
        pass "Canonical idempotency (k=$K)"
    else
        fail "Canonical idempotency (k=$K)"
    fi
else
    skip "test-idempotent not built"
fi
echo ""

# ---------- Test 2: Isomorphism Invariance ----------
echo "Test 2: Isomorphism Invariance (k=$K)"
if [ -f tests/test-isomorphism-invariant ]; then
    if [ "$K" -le 9 ]; then
        TRIALS=1000000
    else
        TRIALS=100000
    fi
    if ./tests/test-isomorphism-invariant "$K" "$TRIALS"; then
        pass "Isomorphism invariance (k=$K, $TRIALS trials)"
    else
        fail "Isomorphism invariance (k=$K, $TRIALS trials)"
    fi
else
    skip "test-isomorphism-invariant not built"
fi
echo ""

# ---------- Test 3: Canonical Consistency ----------
echo "Test 3: Canonical Consistency via NautyCanonical (k=$K)"
if [ -f tests/test-canonical-minimum ]; then
    # For k=9: test all canonicals x 100 permutations (~27s)
    # For k=10: test 10000 random canonicals x 100 permutations
    if [ "$K" -le 9 ]; then
        # No M/P args: defaults to all canonicals, 100 permutations each
        if ./tests/test-canonical-minimum "$K"; then
            pass "Canonical consistency (k=$K, all canonicals x 100 perms)"
        else
            fail "Canonical consistency (k=$K)"
        fi
    else
        if ./tests/test-canonical-minimum "$K" 10000 100; then
            pass "Canonical consistency (k=$K, 10000 canonicals x 100 perms)"
        else
            fail "Canonical consistency (k=$K)"
        fi
    fi
else
    skip "test-canonical-minimum not built"
fi
echo ""

# ---------- Test 4: Subgraph Consistency (Chain of Trust) ----------
# Only for k=9 (reduces to k=8 flat table); skip for k=10.
echo "Test 4: Subgraph Consistency / Chain of Trust (k=$K)"
if [ "$K" -gt 9 ]; then
    skip "Subgraph consistency test only supported for k<=9 (k=9->k=8 reduction)"
elif [ ! -f "canon_maps/canon_map8.bin" ]; then
    skip "canon_maps/canon_map8.bin not found (needed for k=8 trusted lookup)"
elif [ ! -f blant-large-k ]; then
    skip "blant-large-k not built"
elif [ ! -f tests/test-subgraph-consistency ]; then
    skip "test-subgraph-consistency not built"
else
    if ./tests/test-subgraph-consistency "$K" 50000 networks/syeast.el; then
        pass "Subgraph consistency k=$K->k=$((K-1)) (correlation > 0.90)"
    else
        fail "Subgraph consistency k=$K->k=$((K-1))"
    fi
fi
echo ""

# ---------- Test 5: Orbit-Degree Consistency ----------
echo "Test 5: Orbit-Degree Consistency (k=$K)"
if [ ! -f "canon_maps/orbit_map${K}.txt" ]; then
    skip "orbit_map${K}.txt not found"
elif [ -f tests/test-orbit-degrees ]; then
    if ./tests/test-orbit-degrees "$K"; then
        pass "Orbit-degree consistency (k=$K)"
    else
        fail "Orbit-degree consistency (k=$K)"
    fi
else
    skip "test-orbit-degrees not built"
fi
echo ""

# ---------- Summary ----------
echo "=============================="
echo "Strong Test Results (k=$K): $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total"
if [ $FAIL -gt 0 ]; then
    echo "OVERALL: FAIL"
    exit 1
else
    echo "OVERALL: PASS"
    exit 0
fi
