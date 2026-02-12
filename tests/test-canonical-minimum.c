/*
 * test-canonical-minimum.c
 *
 * Verifies NautyCanonical consistency: for M random canonicals and P random
 * permutations each, NautyCanonical(permuted_graph) must equal the original
 * canonical Gint.
 *
 * NOTE: BLANT's k<=8 flat tables use the "minimum Gint" convention, but
 * k=9/k=10 use nauty's canonical form (which is deterministic but not
 * necessarily the minimum Gint). This test verifies CONSISTENCY of nauty's
 * labeling, not minimality.
 *
 * Usage: test-canonical-minimum <k> [M] [P]
 *   k = graphlet size
 *   M = number of random canonicals to test (default: all)
 *   P = number of random permutations per canonical (default: 100)
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

/* Fisher-Yates shuffle */
static void random_permutation(unsigned char *p, int n)
{
    int i;
    for (i = 0; i < n; i++)
        p[i] = (unsigned char)i;
    for (i = n - 1; i > 0; i--) {
        int j = (int)(drand48() * (i + 1));
        if (j > i) j = i;
        unsigned char tmp = p[i];
        p[i] = p[j];
        p[j] = tmp;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <k> [M] [P]\n", argv[0]);
        fprintf(stderr, "  M = number of random canonicals (default: all)\n");
        fprintf(stderr, "  P = permutations per canonical (default: 100)\n");
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

    unsigned long M = (argc >= 3) ? strtoul(argv[2], NULL, 10) : (unsigned long)_numCanon;
    unsigned long P = (argc >= 4) ? strtoul(argv[3], NULL, 10) : 100;

    /* Clamp M to actual number of canonicals */
    if (M > (unsigned long)_numCanon) M = (unsigned long)_numCanon;

    int test_all = (M == (unsigned long)_numCanon);

    /* Fixed seed for reproducibility */
    srand48(42);

    fprintf(stderr,
        "Testing canonical consistency for k=%d (%lu canonicals x %lu permutations)\n",
        k, M, P);

    clock_t start = clock();
    TINY_GRAPH *tg = TinyGraphAlloc(k);
    TINY_GRAPH *permuted = TinyGraphAlloc(k);
    unsigned char sigma[MAX_K], perm_out[MAX_K];

    unsigned long total_checks = 0;
    unsigned long m;

    for (m = 0; m < M; m++) {
        /* Select canonical: sequential if testing all, random otherwise */
        Gordinal_type ordinal;
        if (test_all)
            ordinal = (Gordinal_type)m;
        else
            ordinal = (Gordinal_type)(drand48() * _numCanon);

        Gint_type canon_gint = _canonList[ordinal];

        /* Build canonical TINY_GRAPH */
        Int2TinyGraph(tg, canon_gint);

        unsigned long p;
        for (p = 0; p < P; p++) {
            /* Generate random permutation */
            random_permutation(sigma, k);

            /* Apply permutation */
            TinyGraphEdgesAllDelete(permuted);
            permuted->n = k;
            int i, j;
            for (i = 0; i < k; i++)
                for (j = i + 1; j < k; j++)
                    if (TinyGraphAreConnected(tg, i, j))
                        TinyGraphConnect(permuted, sigma[i], sigma[j]);

            /* NautyCanonical must return the same canonical Gint */
            Gint_type result = NautyCanonical(permuted, k, perm_out);

            if (result != canon_gint) {
                fprintf(stderr,
                    "FAIL: canonical ordinal %u (Gint=" GINT_FMT
                    "): permuted NautyCanonical returned " GINT_FMT
                    " (sigma=",
                    (unsigned)ordinal, canon_gint, result);
                for (i = 0; i < k; i++) fprintf(stderr, "%d", sigma[i]);
                fprintf(stderr, ")\n");
                return 1;
            }

            total_checks++;
        }

        if ((m + 1) % 10000 == 0) {
            double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
            fprintf(stderr, "  ... %lu / %lu canonicals (%lu checks, %.1fs)\n",
                    m + 1, M, total_checks, elapsed);
        }
    }

    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("PASS: %lu canonicals x %lu permutations = %lu consistency checks for k=%d (%.1fs)\n",
           M, P, total_checks, k, elapsed);

    TinyGraphFree(tg);
    TinyGraphFree(permuted);
    free(_canonList);
    free(_canonNumEdges);

    return 0;
}
