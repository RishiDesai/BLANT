/*
 * test-idempotent.c
 *
 * For every canonical in canon_list{k}.txt, verify:
 *   1. NautyCanonical(Int2TinyGraph(canonical_gint)) == canonical_gint
 *      (the canonical form of a canonical must be itself)
 *   2. The permutation returned is the identity (perm[i] == i)
 *
 * Usage: test-idempotent <k>
 * Passes for k=9 (274,668 canonicals) and k=10 (12,005,168 canonicals).
 *
 * Compiled with -DTINY_SET_SIZE=16 -DMAX_K=10.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "blant.h"
#include "nauty-canonical.h"

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

    fprintf(stderr, "Testing canonical idempotency for k=%d (%u canonicals)\n",
            k, (unsigned)_numCanon);

    clock_t start = clock();
    TINY_GRAPH *tg = TinyGraphAlloc(k);
    unsigned char perm[MAX_K];
    Gordinal_type checked = 0;

    Gordinal_type i;
    for (i = 0; i < _numCanon; i++) {
        /* Reconstruct TINY_GRAPH from canonical Gint */
        Int2TinyGraph(tg, _canonList[i]);

        /* Compute canonical form — should be identical to the input */
        Gint_type result = NautyCanonical(tg, k, perm);

        if (result != _canonList[i]) {
            fprintf(stderr,
                "FAIL: canonical ordinal %u: input Gint=" GINT_FMT
                " but NautyCanonical returned " GINT_FMT "\n",
                (unsigned)i, _canonList[i], result);
            return 1;
        }

        /* Verify permutation is identity */
        int j;
        for (j = 0; j < k; j++) {
            if (perm[j] != (unsigned char)j) {
                fprintf(stderr,
                    "FAIL: canonical ordinal %u: perm[%d]=%d (expected %d)\n",
                    (unsigned)i, j, perm[j], j);
                return 1;
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

    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("PASS: all %u canonicals are idempotent for k=%d (%.1fs)\n",
           (unsigned)checked, k, elapsed);

    TinyGraphFree(tg);
    free(_canonList);
    free(_canonNumEdges);

    return 0;
}
