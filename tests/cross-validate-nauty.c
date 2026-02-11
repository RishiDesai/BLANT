/*
 * cross-validate-nauty.c
 *
 * Exhaustive cross-validation of nauty-based canonical labeling against
 * BLANT's existing flat lookup tables for k=3 through k=8.
 *
 * For every Gint from 0 to 2^(k*(k-1)/2)-1:
 *   1. ordinal_flat = _K[Gint]               (from the mmap'd flat table)
 *   2. canonical_gint = NautyCanonical(tg, k, perm)
 *   3. ordinal_nauty = _K[canonical_gint]     (same flat table)
 *   4. Assert ordinal_flat == ordinal_nauty
 *   5. Verify perm: applying perm to canonical graph must reconstruct original
 *
 * Usage: cross-validate-nauty <k>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "blant.h"
#include "blant-utils.h"
#include "nauty-canonical.h"

/* These globals are needed by SetGlobalCanonMaps and blant infrastructure */
unsigned int _k;
unsigned int _Bk;
Gordinal_type _numCanon, _numConnectedCanon;
Gordinal_type *_K = NULL;
char _canonNumEdges[MAX_CANONICALS];
double _totalStarMotifs;
Gint_type _canonList[MAX_CANONICALS];
SET *_connectedCanonicals;
Gint_type _numOrbits, _orbitList[MAX_CANONICALS][MAX_K], _alphaList[MAX_CANONICALS];
Gordinal_type _orbitCanonMapping[MAX_ORBITS];
char _orbitCanonNodeMapping[MAX_ORBITS];
int _connectedOrbits[MAX_ORBITS];
int _numConnectedOrbits;
int _orca_orbit_mapping[58];
enum OutputMode _outputMode = undef;
double *_graphletDegreeVector[MAX_CANONICALS];
double *_orbitDegreeVector[MAX_ORBITS];
int _outputMapping[MAX_CANONICALS], _canonNumStarMotifs[MAX_CANONICALS];

/* Stub declarations needed to link without full blant */
typedef unsigned char kperm[3];
kperm *Permutations = NULL;

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <k>\n", argv[0]);
        return 1;
    }

    _k = atoi(argv[1]);
    if (_k < 3 || _k > 8) {
        fprintf(stderr, "Error: k must be between 3 and 8 for cross-validation\n");
        return 1;
    }

    /* Set up BLANT directories and load canon maps */
    _BLANT_DIR = ".";
    _CANON_DIR = "canon_maps";

    char BUF[BUFSIZ];

    /* Load canon_list and orbit_map */
    _connectedCanonicals = canonListPopulate(BUF, _canonList, _k, _canonNumEdges);
    _numCanon = _connectedCanonicals->maxElem;
    _numConnectedCanon = SetCardinality(_connectedCanonicals);
    _numOrbits = orbitListPopulate(BUF, _orbitList, _orbitCanonMapping, _orbitCanonNodeMapping, _numCanon, _k);

    /* Load flat canon_map (mmap) */
    _K = (Gordinal_type *)mapCanonMap(BUF, _K, _k);
    if (!_K) {
        fprintf(stderr, "Error: could not mmap canon_map%d.bin\n", _k);
        return 1;
    }

    /* Compute total number of entries */
    unsigned long long total = 1ULL << (_k * (_k - 1) / 2);

    fprintf(stderr, "Cross-validating nauty vs flat table for k=%d (%llu entries, %u canonicals)\n",
            _k, total, (unsigned)_numCanon);

    clock_t start = clock();
    unsigned long long validated = 0;
    unsigned long long perm_errors = 0;

    TINY_GRAPH *tg = TinyGraphAlloc(_k);
    TINY_GRAPH *canon_tg = TinyGraphAlloc(_k);
    TINY_GRAPH *rebuilt = TinyGraphAlloc(_k);
    unsigned char perm[MAX_K];

    for (unsigned long long gint = 0; gint < total; gint++) {
        /* 1. Look up ordinal in flat table */
        Gordinal_type ordinal_flat = _K[gint];

        /* 2. Reconstruct TINY_GRAPH and compute nauty canonical */
        Int2TinyGraph(tg, (Gint_type)gint);
        Gint_type canonical_gint = NautyCanonical(tg, _k, perm);

        /* 3. Look up nauty's canonical gint in the flat table */
        Gordinal_type ordinal_nauty = _K[canonical_gint];

        /* 4. Assert ordinals match */
        if (ordinal_flat != ordinal_nauty) {
            fprintf(stderr, "FAIL: Gint=%llu ordinal_flat=%u ordinal_nauty=%u canonical_gint=%u\n",
                    gint, (unsigned)ordinal_flat, (unsigned)ordinal_nauty, (unsigned)canonical_gint);
            return 1;
        }

        /* 5. Verify permutation: apply perm (can2non) to canonical graph -> original */
        Int2TinyGraph(canon_tg, canonical_gint);
        TinyGraphEdgesAllDelete(rebuilt);
        rebuilt->n = _k;
        int i, j;
        for (i = 0; i < _k; i++)
            for (j = i + 1; j < _k; j++)
                if (TinyGraphAreConnected(canon_tg, i, j))
                    TinyGraphConnect(rebuilt, perm[i], perm[j]);

        Gint_type rebuilt_gint = TinyGraph2Int(rebuilt, _k);
        if (rebuilt_gint != (Gint_type)gint) {
            fprintf(stderr, "FAIL perm: Gint=%llu rebuilt=%u (canon=%u perm=",
                    gint, (unsigned)rebuilt_gint, (unsigned)canonical_gint);
            for (i = 0; i < (int)_k; i++) fprintf(stderr, "%d", perm[i]);
            fprintf(stderr, ")\n");
            return 1;
        }

        validated++;

        /* Progress every 1M entries */
        if (validated % 1000000 == 0) {
            double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
            fprintf(stderr, "  ... validated %llu / %llu (%.1f%%, %.1fs elapsed)\n",
                    validated, total, 100.0 * validated / total, elapsed);
        }
    }

    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("PASS: all %llu entries validated for k=%d (%.1fs)\n", validated, _k, elapsed);

    TinyGraphFree(tg);
    TinyGraphFree(canon_tg);
    TinyGraphFree(rebuilt);

    return 0;
}
