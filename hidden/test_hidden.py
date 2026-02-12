"""
Hidden tests for BLANT k=9 extension.

These tests are NOT shown to the agent. They verify correctness using
different seeds, different networks, different sampling methods, and
stricter thresholds than the public tests.
"""

import os
import re
import subprocess

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

BLANT_DIR = os.environ.get("BLANT_DIR", "/app/src")
BLANT_BIN = os.path.join(BLANT_DIR, "blant")
LARGE_K_BIN = os.path.join(BLANT_DIR, "blant-large-k")
HIDDEN_DIR = os.environ.get("HIDDEN_DIR", os.path.join(BLANT_DIR, "hidden"))
BASELINES_DIR = os.path.join(HIDDEN_DIR, "baselines")
CANON_MAPS = os.path.join(BLANT_DIR, "canon_maps")
NETWORKS = os.path.join(BLANT_DIR, "networks")

EXPECTED_CANONICALS_K9 = 274668
EXPECTED_CONNECTED_K9 = 261080


def _run(cmd, cwd=None, timeout=300):
    if cwd is None:
        cwd = BLANT_DIR
    return subprocess.run(cmd, capture_output=True, text=True, cwd=cwd, timeout=timeout)


# =========================================================================
# Hidden Tier 1: Regression with different seed (not r42)
# =========================================================================

class TestHiddenRegression:
    """k<=8 regression using seed 714 (public tests use seed 949)."""

    def _regression_seed714(self, k):
        baseline_path = os.path.join(BASELINES_DIR, f"blant_k{k}_seed714.txt")
        with open(baseline_path) as f:
            baseline = f.read()

        result = _run(
            [LARGE_K_BIN, "-q", "-sNBE", "-r714", "-n100", f"-k{k}", "-t1", "-mf",
             os.path.join(NETWORKS, "syeast.el")],
            timeout=60,
        )
        assert result.returncode == 0, f"blant-large-k seed=714 k={k} failed: {result.stderr[-300:]}"
        assert result.stdout == baseline, \
            f"k={k} seed=714 regression failure: output differs from baseline"

    def test_regression_k5_seed714(self):
        self._regression_seed714(5)

    def test_regression_k6_seed714(self):
        self._regression_seed714(6)

    def test_regression_k7_seed714(self):
        self._regression_seed714(7)


# =========================================================================
# Hidden Tier 2: k=9 on a different network (HSapiens)
# =========================================================================

class TestHiddenDifferentNetwork:
    """k=9 sampling on HSapiens (public tests only use syeast)."""

    def test_k9_hsapiens_produces_output(self):
        """blant-large-k -k9 must work on HSapiens.el."""
        result = _run(
            [LARGE_K_BIN, "-sNBE", "-k9", "-mi", "-n200",
             os.path.join(NETWORKS, "HSapiens", "HSapiens.el")],
            timeout=120,
        )
        assert result.returncode == 0, f"k=9 HSapiens failed: {result.stderr[-300:]}"
        lines = result.stdout.strip().split("\n")
        assert len(lines) == 200, f"Expected 200 lines, got {len(lines)}"

    def test_k9_hsapiens_valid_ordinals(self):
        """All k=9 ordinals on HSapiens must be in [0, 274667]."""
        result = _run(
            [LARGE_K_BIN, "-sNBE", "-k9", "-mi", "-n1000",
             os.path.join(NETWORKS, "HSapiens", "HSapiens.el")],
            timeout=120,
        )
        assert result.returncode == 0
        for line in result.stdout.strip().split("\n"):
            ordinal = int(line.split()[0])
            assert 0 <= ordinal < EXPECTED_CANONICALS_K9

    def test_k9_hsapiens_diversity(self):
        """k=9 on the larger HSapiens network should hit many distinct canonicals."""
        result = _run(
            [LARGE_K_BIN, "-sNBE", "-k9", "-mi", "-n5000",
             os.path.join(NETWORKS, "HSapiens", "HSapiens.el")],
            timeout=120,
        )
        assert result.returncode == 0
        ordinals = set()
        for line in result.stdout.strip().split("\n"):
            ordinals.add(int(line.split()[0]))
        assert len(ordinals) > 200, \
            f"Only {len(ordinals)} distinct canonicals on HSapiens (expected > 200)"


# =========================================================================
# Hidden Tier 3: Different sampling methods at k=9
# =========================================================================

class TestHiddenSamplingMethods:
    """k=9 with EBE and MCMC (public tests only use NBE)."""

    def test_k9_ebe_produces_output(self):
        """EBE sampling at k=9 must produce valid output."""
        result = _run(
            [LARGE_K_BIN, "-sEBE", "-k9", "-mi", "-n200",
             os.path.join(NETWORKS, "syeast.el")],
            timeout=120,
        )
        assert result.returncode == 0, f"k=9 EBE failed: {result.stderr[-300:]}"
        lines = result.stdout.strip().split("\n")
        assert len(lines) == 200
        for line in lines:
            parts = line.split()
            assert len(parts) == 10, f"EBE k=9 wrong column count: {len(parts)}"
            ordinal = int(parts[0])
            assert 0 <= ordinal < EXPECTED_CANONICALS_K9

    def test_k9_mcmc_produces_output(self):
        """MCMC sampling at k=9 must produce valid output."""
        result = _run(
            [LARGE_K_BIN, "-sMCMC", "-k9", "-mi", "-n200",
             os.path.join(NETWORKS, "syeast.el")],
            timeout=120,
        )
        assert result.returncode == 0, f"k=9 MCMC failed: {result.stderr[-300:]}"
        lines = result.stdout.strip().split("\n")
        assert len(lines) == 200
        for line in lines:
            parts = line.split()
            assert len(parts) == 10
            ordinal = int(parts[0])
            assert 0 <= ordinal < EXPECTED_CANONICALS_K9

    def test_k9_ebe_frequency_mode(self):
        """EBE frequency mode at k=9 must produce one line per connected canonical."""
        result = _run(
            [LARGE_K_BIN, "-sEBE", "-k9", "-n10000", "-mf",
             os.path.join(NETWORKS, "syeast.el")],
            timeout=120,
        )
        assert result.returncode == 0, f"k=9 EBE freq failed: {result.stderr[-300:]}"
        lines = result.stdout.strip().split("\n")
        assert len(lines) == EXPECTED_CONNECTED_K9, \
            f"EBE freq: expected {EXPECTED_CONNECTED_K9} lines, got {len(lines)}"


# =========================================================================
# Hidden Tier 4: k=9 determinism
# =========================================================================

class TestHiddenDeterminism:
    """Seeded k=9 output must be deterministic across runs."""

    def test_k9_seeded_determinism(self):
        """Two runs with same seed must produce identical output."""
        cmd = [LARGE_K_BIN, "-q", "-sNBE", "-k9", "-r714", "-t1", "-mi", "-n200",
               os.path.join(NETWORKS, "syeast.el")]

        result1 = _run(cmd, timeout=120)
        assert result1.returncode == 0, f"Run 1 failed: {result1.stderr[-300:]}"

        result2 = _run(cmd, timeout=120)
        assert result2.returncode == 0, f"Run 2 failed: {result2.stderr[-300:]}"

        assert result1.stdout == result2.stdout, \
            "k=9 seeded output is non-deterministic (two runs differ)"

    def test_k9_determinism_matches_baseline(self):
        """Seeded k=9 output must match the pre-generated baseline."""
        baseline_path = os.path.join(BASELINES_DIR, "blant_k9_seed714_determinism.txt")
        if not os.path.isfile(baseline_path):
            return  # skip if baseline not available

        with open(baseline_path) as f:
            baseline = f.read()

        result = _run(
            [LARGE_K_BIN, "-q", "-sNBE", "-k9", "-r714", "-t1", "-mi", "-n200",
             os.path.join(NETWORKS, "syeast.el")],
            timeout=120,
        )
        assert result.returncode == 0
        assert result.stdout == baseline, \
            "k=9 seeded output does not match pre-generated baseline"


# =========================================================================
# Hidden Tier 5: Exhaustive k=8 cross-validation
# =========================================================================

class TestHiddenExhaustiveCrossValidation:
    """Exhaustive nauty vs flat table cross-validation for k=8 (268M entries).
    Public tests only go up to k=7."""

    def test_cross_validate_k8(self):
        """All 268,435,456 k=8 entries must match between nauty and flat table."""
        xv_bin = os.path.join(BLANT_DIR, "tests", "cross-validate-nauty")
        if not os.path.isfile(xv_bin):
            _run(["make", "tests/cross-validate-nauty"], timeout=120)

        assert os.path.isfile(xv_bin), "cross-validate-nauty not built"

        result = _run([xv_bin, "8"], timeout=600)
        assert result.returncode == 0, \
            f"k=8 cross-validation failed: {result.stderr[-500:]}"
        assert "PASS" in result.stdout, \
            f"k=8 cross-validation did not report PASS"


# =========================================================================
# Hidden Tier 6: Stricter subgraph consistency
# =========================================================================

class TestHiddenStricterSubgraph:
    """Subgraph consistency with more samples and stricter threshold."""

    def test_subgraph_correlation_strict(self):
        """k=9->k=8 subgraph decomposition with 50K samples, correlation > 0.90."""
        sub_bin = os.path.join(BLANT_DIR, "tests", "test-subgraph-consistency")
        if not os.path.isfile(sub_bin):
            _run(["make", "strong-tests"], timeout=120)

        if not os.path.isfile(sub_bin):
            return  # skip if not built

        result = _run(
            [sub_bin, "9", "50000", os.path.join(NETWORKS, "syeast.el")],
            timeout=300,
        )
        assert result.returncode == 0, \
            f"Subgraph consistency failed: {result.stderr[-500:]}"
        assert "PASS" in result.stdout

        match = re.search(r"correlation[=:]\s*([\d.]+)", result.stdout)
        if match:
            corr = float(match.group(1))
            assert corr > 0.90, \
                f"Correlation {corr:.4f} below strict threshold 0.90"


# =========================================================================
# Hidden Tier 7: Larger sample diversity + frequency sanity
# =========================================================================

class TestHiddenLargeSampleDiversity:
    """Verify k=9 output statistics with large samples."""

    def test_k9_large_diversity(self):
        """50K k=9 samples should hit > 500 distinct canonicals on syeast."""
        result = _run(
            [LARGE_K_BIN, "-sNBE", "-k9", "-mi", "-n50000",
             os.path.join(NETWORKS, "syeast.el")],
            timeout=180,
        )
        assert result.returncode == 0
        ordinals = set()
        for line in result.stdout.strip().split("\n"):
            ordinals.add(int(line.split()[0]))
        assert len(ordinals) > 500, \
            f"Only {len(ordinals)} distinct canonicals in 50K samples (expected > 500)"

    def test_k9_no_dominant_canonical(self):
        """No single canonical should account for more than 15% of samples."""
        result = _run(
            [LARGE_K_BIN, "-sNBE", "-k9", "-mi", "-n10000",
             os.path.join(NETWORKS, "syeast.el")],
            timeout=120,
        )
        assert result.returncode == 0
        counts = {}
        lines = result.stdout.strip().split("\n")
        for line in lines:
            ordinal = int(line.split()[0])
            counts[ordinal] = counts.get(ordinal, 0) + 1
        max_frac = max(counts.values()) / len(lines)
        assert max_frac < 0.15, \
            f"Dominant canonical has {max_frac:.1%} of samples (expected < 15%)"
