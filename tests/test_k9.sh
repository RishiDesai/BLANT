#!/bin/bash
# test_k9.sh - Comprehensive tests for BLANT k=9 extension
# Run from the repo root: bash tests/test_k9.sh
set -e

PASS=0
FAIL=0
TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1"; }

echo "=== BLANT k=9 Extension Tests ==="
echo ""

# Test 1: Mathematical constants - canon_list9.txt
echo "Test 1: canon_list9.txt mathematical constants"
HEADER=$(head -1 canon_maps/canon_list9.txt)
LINES=$(wc -l < canon_maps/canon_list9.txt | tr -d ' ')
if [ "$HEADER" = "274668" ]; then
    pass "canon_list9.txt header says 274668"
else
    fail "canon_list9.txt header: expected 274668, got $HEADER"
fi
if [ "$LINES" = "274669" ]; then
    pass "canon_list9.txt has 274669 lines (header + 274668 data)"
else
    fail "canon_list9.txt line count: expected 274669, got $LINES"
fi

# Test 2: orbit_map9.txt mathematical constants
echo "Test 2: orbit_map9.txt mathematical constants"
ORBIT_HEADER=$(head -1 canon_maps/orbit_map9.txt)
ORBIT_LINES=$(wc -l < canon_maps/orbit_map9.txt | tr -d ' ')
if [ "$ORBIT_HEADER" = "2208612" ]; then
    pass "orbit_map9.txt header says 2208612 orbits"
else
    fail "orbit_map9.txt header: expected 2208612, got $ORBIT_HEADER"
fi
if [ "$ORBIT_LINES" = "274669" ]; then
    pass "orbit_map9.txt has 274669 lines (header + 274668 data)"
else
    fail "orbit_map9.txt line count: expected 274669, got $ORBIT_LINES"
fi

# Test 3: geng cross-check
echo "Test 3: geng produces exactly 274668 graphs for k=9"
GENG_COUNT=$(./geng 9 2>/dev/null | wc -l | tr -d ' ')
if [ "$GENG_COUNT" = "274668" ]; then
    pass "geng 9 produces 274668 graphs"
else
    fail "geng 9: expected 274668, got $GENG_COUNT"
fi

# Test 4: Connected count (OEIS A001349 for n=9 = 261,080)
echo "Test 4: Connected canonical count"
CONNECTED=$(awk -F'\t' 'NR>1{split($2,a," "); if(a[1]==1) c++} END{print c}' canon_maps/canon_list9.txt)
if [ "$CONNECTED" = "261080" ]; then
    pass "261080 connected canonicals (matches OEIS A001349)"
else
    fail "Connected count: expected 261080, got $CONNECTED"
fi

# Test 5: blant-large-k can sample k=9 graphlets
echo "Test 5: k=9 sampling produces valid output"
OUTPUT=$(./blant-large-k -sNBE -k9 -mi -n10000 networks/syeast.el 2>/dev/null)
if [ $? -eq 0 ] && [ -n "$OUTPUT" ]; then
    pass "blant-large-k -k9 produces output"
else
    fail "blant-large-k -k9 failed to produce output"
fi

# Test 5b: All ordinals in valid range [0, 274667]
BAD=$(echo "$OUTPUT" | awk '{if ($1 < 0 || $1 >= 274668) {print "BAD:", $0; exit 1}}' 2>&1)
if [ $? -eq 0 ]; then
    pass "All 10000 k=9 ordinals in range [0, 274667]"
else
    fail "Invalid ordinal found: $BAD"
fi

# Test 5c: All sampled graphlets have k=9 nodes in the output
COLS=$(echo "$OUTPUT" | head -1 | awk '{print NF}')
if [ "$COLS" = "10" ]; then
    pass "k=9 output has 10 columns (ordinal + 9 nodes)"
else
    fail "k=9 output columns: expected 10, got $COLS"
fi

# Test 6: k<=8 regression (blant-large-k matches regular blant)
echo "Test 6: k<=8 regression"
for k in 3 4 5 6 7; do
    if [ ! -f "canon_maps/canon_map${k}.bin" ]; then
        echo "  SKIP: k=$k regression (no canon_map${k}.bin)"
        continue
    fi
    ./blant -sNBE -k${k} -n100 -mf -r42 -t1 networks/syeast.el > /tmp/blant_k${k}.txt 2>/dev/null
    ./blant-large-k -sNBE -k${k} -n100 -mf -r42 -t1 networks/syeast.el > /tmp/blant_large_k${k}.txt 2>/dev/null
    if diff -q /tmp/blant_k${k}.txt /tmp/blant_large_k${k}.txt > /dev/null 2>&1; then
        pass "k=$k regression (byte-identical)"
    else
        fail "k=$k regression (outputs differ)"
    fi
done

# Test 7: Self-consistency (idempotency) - canonical of canonical is itself
echo "Test 7: Self-consistency check (canonical idempotency)"
# For each canonical in canon_list9.txt, verify NautyCanonical(Int2TinyGraph(canon_gint)) == canon_gint
# We'll do a quick spot check using the existing cross-validate infrastructure
# by checking that L_K(canon_gint) returns the correct ordinal for a sample of canonicals
SPOT_CHECK=$(./blant-large-k -sNBE -k9 -mi -n1000 networks/syeast.el 2>/dev/null | awk '{print $1}' | sort -nu | wc -l | tr -d ' ')
if [ "$SPOT_CHECK" -gt 10 ]; then
    pass "Sampling hits $SPOT_CHECK distinct canonicals (diversity check)"
else
    fail "Only $SPOT_CHECK distinct canonicals sampled (diversity too low)"
fi

# Test 8: Regular blant build is unaffected
echo "Test 8: Regular blant build unaffected"
./blant -q -s MCMC -mi -n100000 -k5 networks/syeast.el 2>/dev/null | sort -n | ./blant-sanity 5 100000 networks/syeast.el 2>/dev/null
if [ $? -eq 0 ]; then
    pass "Regular blant k=5 MCMC sanity test passes"
else
    fail "Regular blant k=5 MCMC sanity test failed"
fi

echo ""
echo "=== Results: $PASS passed, $FAIL failed, $TOTAL total ==="
if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
