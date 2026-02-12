/*
 * test-orbit-degrees.c
 *
 * For every canonical in canon_list{k}.txt and its orbit assignments
 * in orbit_map{k}.txt:
 *   - Reconstruct the TINY_GRAPH from the canonical Gint
 *   - Compute the degree of each node
 *   - Verify that nodes assigned to the same orbit have the same degree
 *
 * This is a necessary condition for correct orbit assignments: nodes in the
 * same orbit must be automorphically equivalent, which implies same degree.
 *
 * Usage: test-orbit-degrees <k>
 *
 * Compiled with -DTINY_SET_SIZE=16 -DMAX_K=10.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "blant.h"

/* Compute degree of node v in TINY_GRAPH with k nodes */
static int tiny_degree(TINY_GRAPH *tg, int k, int v)
{
    int deg = 0, j;
    for (j = 0; j < k; j++)
        if (j != v && TinyGraphAreConnected(tg, v, j))
            deg++;
    return deg;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <k>\n", argv[0]);
        return 1;
    }

    int k = atoi(argv[1]);
    if (k < 3 || k > MAX_K) {
        fprintf(stderr, "Error: k must be between 3 and %d\n", MAX_K);
        return 1;
    }

    _BLANT_DIR = ".";
    _CANON_DIR = "canon_maps";

    /* Allocate and populate canon list */
    _canonList = (Gint_type *) calloc(MAX_CANONICALS, sizeof(Gint_type));
    _canonNumEdges = (char *) calloc(MAX_CANONICALS, sizeof(char));
    if (!_canonList || !_canonNumEdges) {
        fprintf(stderr, "Error: failed to allocate canon arrays\n");
        return 1;
    }

    char BUF[BUFSIZ];
    _connectedCanonicals = canonListPopulate(BUF, _canonList, k, _canonNumEdges);
    _numCanon = _connectedCanonicals->maxElem;

    /* Read orbit_map directly (avoid allocating the full orbit infrastructure).
     * Format: first line = numOrbits, then one line per canonical with k orbit IDs.
     */
    sprintf(BUF, "%s/%s/orbit_map%d.txt", _BLANT_DIR, _CANON_DIR, k);
    FILE *fp = fopen(BUF, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s\n", BUF);
        return 1;
    }

    Gint_type numOrbits;
    if (1 != fscanf(fp, GINT_FMT, &numOrbits)) {
        fprintf(stderr, "Error: failed to read numOrbits from orbit_map\n");
        return 1;
    }

    fprintf(stderr,
        "Testing orbit-degree consistency for k=%d (%u canonicals, " GINT_FMT " orbits)\n",
        k, (unsigned)_numCanon, numOrbits);

    clock_t start = clock();
    TINY_GRAPH *tg = TinyGraphAlloc(k);
    Gordinal_type violations = 0;
    Gordinal_type checked = 0;

    Gordinal_type c;
    for (c = 0; c < _numCanon; c++) {
        /* Read k orbit IDs for this canonical */
        Gint_type orbit_ids[MAX_K];
        int j;
        for (j = 0; j < k; j++) {
            if (1 != fscanf(fp, GINT_FMT, &orbit_ids[j])) {
                fprintf(stderr, "Error: failed to read orbit ID for canonical %u node %d\n",
                        (unsigned)c, j);
                return 1;
            }
        }

        /* Build TINY_GRAPH from canonical Gint */
        Int2TinyGraph(tg, _canonList[c]);

        /* Compute degree of each node */
        int degrees[MAX_K];
        for (j = 0; j < k; j++)
            degrees[j] = tiny_degree(tg, k, j);

        /* Verify: nodes with the same orbit ID must have the same degree */
        int i;
        for (i = 0; i < k; i++) {
            for (j = i + 1; j < k; j++) {
                if (orbit_ids[i] == orbit_ids[j] && degrees[i] != degrees[j]) {
                    if (violations < 10) {
                        fprintf(stderr,
                            "FAIL: canonical %u (Gint=" GINT_FMT "): "
                            "nodes %d and %d share orbit " GINT_FMT
                            " but degrees differ (%d vs %d)\n",
                            (unsigned)c, _canonList[c], i, j,
                            orbit_ids[i], degrees[i], degrees[j]);
                    }
                    violations++;
                }
            }
        }

        checked++;
        if (checked % 100000 == 0) {
            double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
            fprintf(stderr, "  ... checked %u / %u (%.1f%%, %.1fs)\n",
                    (unsigned)checked, (unsigned)_numCanon,
                    100.0 * checked / _numCanon, elapsed);
        }
    }

    fclose(fp);

    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;

    if (violations > 0) {
        fprintf(stderr, "FAIL: %u orbit-degree violations found for k=%d\n",
                (unsigned)violations, k);
        return 1;
    }

    printf("PASS: all %u canonicals have consistent orbit-degree assignments for k=%d (%.1fs)\n",
           (unsigned)checked, k, elapsed);

    TinyGraphFree(tg);
    free(_canonList);
    free(_canonNumEdges);

    return 0;
}
