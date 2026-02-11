#!/bin/bash
# test_k10.sh - Comprehensive tests for BLANT k=10 extension
# Run from the repo root: bash tests/test_k10.sh
# Prerequisite: k=10 reference files must be generated first
#   (canon_maps/canon_list10.txt, canon_maps/orbit_map10.txt, alpha_list_*10.txt)
set -e

PASS=0
FAIL=0
SKIP=0
TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  FAIL: $1"; }
skip() { SKIP=$((SKIP+1)); TOTAL=$((TOTAL+1)); echo "  SKIP: $1"; }

echo "=== BLANT k=10 Extension Tests ==="
echo ""

# Check prerequisites
if [ ! -f canon_maps/canon_list10.txt ]; then
    echo "ERROR: canon_maps/canon_list10.txt not found. Generate it first with:"
    echo "  make generate-canon-list geng && ./geng 10 2>/dev/null | ./generate-canon-list 10 canon_maps/canon-ordinal-to-signature10.txt > canon_maps/canon_list10.txt"
    exit 1
fi
if [ ! -f canon_maps/orbit_map10.txt ]; then
    echo "ERROR: canon_maps/orbit_map10.txt not found. Generate it first with:"
    echo "  make generate-orbit-map && ./generate-orbit-map 10 canon_maps/canon_list10.txt > canon_maps/orbit_map10.txt"
    exit 1
fi
if [ ! -f blant-large-k ]; then
    echo "ERROR: blant-large-k not found. Build it first with: make blant-large-k"
    exit 1
fi

# Test 1: Mathematical constants - canon_list10.txt
echo "Test 1: canon_list10.txt mathematical constants"
HEADER=$(head -1 canon_maps/canon_list10.txt)
LINES=$(wc -l < canon_maps/canon_list10.txt | tr -d ' ')
if [ "$HEADER" = "12005168" ]; then
    pass "canon_list10.txt header says 12005168"
else
    fail "canon_list10.txt header: expected 12005168, got $HEADER"
fi
if [ "$LINES" = "12005169" ]; then
    pass "canon_list10.txt has 12005169 lines (header + 12005168 data)"
else
    fail "canon_list10.txt line count: expected 12005169, got $LINES"
fi

# Test 2: orbit_map10.txt mathematical constants
echo "Test 2: orbit_map10.txt mathematical constants"
ORBIT_HEADER=$(head -1 canon_maps/orbit_map10.txt)
ORBIT_LINES=$(wc -l < canon_maps/orbit_map10.txt | tr -d ' ')
if [ "$ORBIT_HEADER" = "113743760" ]; then
    pass "orbit_map10.txt header says 113743760 orbits"
else
    fail "orbit_map10.txt header: expected 113743760, got $ORBIT_HEADER"
fi
if [ "$ORBIT_LINES" = "12005169" ]; then
    pass "orbit_map10.txt has 12005169 lines (header + 12005168 data)"
else
    fail "orbit_map10.txt line count: expected 12005169, got $ORBIT_LINES"
fi

# Test 3: Connected canonical count (OEIS A001349 for n=10 = 11,716,571)
echo "Test 3: Connected canonical count"
CONNECTED=$(awk -F'\t' 'NR>1{split($2,a," "); if(a[1]==1) c++} END{print c}' canon_maps/canon_list10.txt)
if [ "$CONNECTED" = "11716571" ]; then
    pass "11716571 connected canonicals (matches OEIS A001349)"
else
    fail "Connected count: expected 11716571, got $CONNECTED"
fi

# Test 4: geng cross-check (OEIS A000088 for n=10 = 12,005,168)
echo "Test 4: geng produces exactly 12005168 graphs for k=10"
if [ -x ./geng ]; then
    GENG_COUNT=$(./geng 10 2>/dev/null | wc -l | tr -d ' ')
    if [ "$GENG_COUNT" = "12005168" ]; then
        pass "geng 10 produces 12005168 graphs"
    else
        fail "geng 10: expected 12005168, got $GENG_COUNT"
    fi
else
    skip "geng not found"
fi

# Test 5: blant-large-k can sample k=10 graphlets (with -R to skip alpha files)
echo "Test 5: k=10 sampling produces valid output"
OUTPUT=$(timeout 120 ./blant-large-k -sNBE -k10 -mi -R -n100 networks/syeast.el 2>/dev/null)
if [ $? -eq 0 ] && [ -n "$OUTPUT" ]; then
    pass "blant-large-k -k10 produces output"
else
    fail "blant-large-k -k10 failed to produce output"
fi

# Test 5b: All ordinals in valid range [0, 12005167]
BAD=$(echo "$OUTPUT" | awk '{if ($1 < 0 || $1 >= 12005168) {print "BAD:", $0; exit 1}}' 2>&1)
if [ $? -eq 0 ]; then
    pass "All 100 k=10 ordinals in range [0, 12005167]"
else
    fail "Invalid ordinal found: $BAD"
fi

# Test 5c: All sampled graphlets have k=10 nodes in the output (11 columns)
COLS=$(echo "$OUTPUT" | head -1 | awk '{print NF}')
if [ "$COLS" = "11" ]; then
    pass "k=10 output has 11 columns (ordinal + 10 nodes)"
else
    fail "k=10 output columns: expected 11, got $COLS"
fi

# Test 6: k=9 regression (blant-large-k with MAX_K=10 still works for k=9)
echo "Test 6: k=9 regression with MAX_K=10 build"
if [ -f canon_maps/canon_list9.txt ] && [ -f canon_maps/orbit_map9.txt ]; then
    K9_OUTPUT=$(timeout 30 ./blant-large-k -sNBE -k9 -mi -n1000 networks/syeast.el 2>/dev/null)
    if [ $? -eq 0 ] && [ -n "$K9_OUTPUT" ]; then
        pass "k=9 sampling still works with MAX_K=10 build"
    else
        fail "k=9 sampling failed with MAX_K=10 build"
    fi
    K9_COLS=$(echo "$K9_OUTPUT" | head -1 | awk '{print NF}')
    if [ "$K9_COLS" = "10" ]; then
        pass "k=9 output still has 10 columns"
    else
        fail "k=9 output columns: expected 10, got $K9_COLS"
    fi
    K9_BAD=$(echo "$K9_OUTPUT" | awk '{if ($1 < 0 || $1 >= 274668) {print "BAD"; exit 1}}' 2>&1)
    if [ $? -eq 0 ]; then
        pass "k=9 ordinals still in range [0, 274667]"
    else
        fail "k=9 ordinal out of range"
    fi
else
    skip "k=9 reference files not found"
fi

# Test 7: k<=8 regression (blant-large-k matches regular blant for k=5)
echo "Test 7: k<=8 regression"
if [ -f canon_maps/canon_map5.bin ]; then
    ./blant -sNBE -k5 -n100 -mf -r42 -t1 networks/syeast.el > /tmp/blant_k5.txt 2>/dev/null
    timeout 10 ./blant-large-k -sNBE -k5 -n100 -mf -r42 -t1 networks/syeast.el > /tmp/blant_large_k5.txt 2>/dev/null
    if diff -q /tmp/blant_k5.txt /tmp/blant_large_k5.txt > /dev/null 2>&1; then
        pass "k=5 regression (byte-identical)"
    else
        fail "k=5 regression (outputs differ)"
    fi
else
    skip "k=5 canon_map5.bin not found for regression test"
fi

# Test 8: Regular blant build unaffected
echo "Test 8: Regular blant build unaffected"
if [ -f blant ] && [ -f blant-sanity ] && [ -f canon_maps/canon_map5.bin ]; then
    ./blant -q -s MCMC -mi -n100000 -k5 networks/syeast.el 2>/dev/null | sort -n | ./blant-sanity 5 100000 networks/syeast.el 2>/dev/null
    if [ $? -eq 0 ]; then
        pass "Regular blant k=5 MCMC sanity test passes"
    else
        fail "Regular blant k=5 MCMC sanity test failed"
    fi
else
    skip "Regular blant or blant-sanity or k=5 maps not available"
fi

# Test 9: Diversity check (k=10 sampling hits multiple distinct canonicals)
echo "Test 9: k=10 canonical diversity"
DISTINCT=$(echo "$OUTPUT" | awk '{print $1}' | sort -nu | wc -l | tr -d ' ')
if [ "$DISTINCT" -gt 10 ]; then
    pass "k=10 sampling hits $DISTINCT distinct canonicals (diversity check)"
else
    fail "Only $DISTINCT distinct k=10 canonicals sampled (diversity too low)"
fi

echo ""
echo "=== Results: $PASS passed, $FAIL failed, $SKIP skipped, $TOTAL total ==="
if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
