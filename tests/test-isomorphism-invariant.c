/*
 * test-isomorphism-invariant.c
 *
 * For N random trials:
 *   1. Pick a random Gint in [0, 2^(k*(k-1)/2) - 1]
 *   2. Build the TINY_GRAPH
 *   3. Compute canonical_gint_1 = NautyCanonical(tg, k, perm)
 *   4. Generate a random permutation of nodes 0..k-1
 *   5. Apply the permutation to the TINY_GRAPH (remap edges)
 *   6. Compute canonical_gint_2 = NautyCanonical(permuted_tg, k, perm2)
 *   7. Assert canonical_gint_1 == canonical_gint_2
 *
 * Usage: test-isomorphism-invariant <k> <num_trials>
 * Default: k=9 1000000 trials, k=10 100000 trials.
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

/* Fisher-Yates shuffle for a permutation array */
static void random_permutation(unsigned char *p, int n)
{
    int i;
    for (i = 0; i < n; i++)
        p[i] = (unsigned char)i;
    for (i = n - 1; i > 0; i--) {
        int j = (int)(drand48() * (i + 1));
        if (j > i) j = i; /* clamp */
        unsigned char tmp = p[i];
        p[i] = p[j];
        p[j] = tmp;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <k> [num_trials]\n", argv[0]);
        return 1;
    }

    int k = atoi(argv[1]);
    if (k < 3 || k > MAX_K) {
        fprintf(stderr, "Error: k must be between 3 and %d\n", MAX_K);
        return 1;
    }

    unsigned long num_trials;
    if (argc >= 3) {
        num_trials = strtoul(argv[2], NULL, 10);
    } else {
        num_trials = (k <= 9) ? 1000000 : 100000;
    }

    /* Fixed seed for reproducibility */
    srand48(42);

    unsigned long long max_gint = 1ULL << (k * (k - 1) / 2);

    fprintf(stderr,
        "Testing isomorphism invariance for k=%d (%lu trials, Gint space 0..%llu)\n",
        k, num_trials, max_gint - 1);

    clock_t start = clock();
    TINY_GRAPH *tg = TinyGraphAlloc(k);
    TINY_GRAPH *permuted = TinyGraphAlloc(k);
    unsigned char perm_canon[MAX_K], perm_canon2[MAX_K];
    unsigned char sigma[MAX_K]; /* random permutation */

    unsigned long trial;
    for (trial = 0; trial < num_trials; trial++) {
        /* Pick a random Gint */
        Gint_type gint;
        if (max_gint <= (unsigned long long)RAND_MAX) {
            gint = (Gint_type)(drand48() * max_gint);
        } else {
            /* For large Gint spaces (k=10: 2^45), combine two random values */
            gint = (Gint_type)(
                ((unsigned long long)(lrand48()) << 30) |
                ((unsigned long long)(lrand48()))
            ) % max_gint;
        }

        /* Build TINY_GRAPH from Gint */
        Int2TinyGraph(tg, gint);

        /* Compute canonical form */
        Gint_type canon1 = NautyCanonical(tg, k, perm_canon);

        /* Generate a random permutation */
        random_permutation(sigma, k);

        /* Apply permutation: permuted graph has edge (sigma[i], sigma[j])
           whenever original has edge (i, j) */
        TinyGraphEdgesAllDelete(permuted);
        permuted->n = k;
        int i, j;
        for (i = 0; i < k; i++)
            for (j = i + 1; j < k; j++)
                if (TinyGraphAreConnected(tg, i, j))
                    TinyGraphConnect(permuted, sigma[i], sigma[j]);

        /* Compute canonical of the permuted graph */
        Gint_type canon2 = NautyCanonical(permuted, k, perm_canon2);

        /* Isomorphic graphs must have the same canonical form */
        if (canon1 != canon2) {
            fprintf(stderr,
                "FAIL: trial %lu: Gint=" GINT_FMT " canon1=" GINT_FMT
                " canon2=" GINT_FMT " (sigma=",
                trial, gint, canon1, canon2);
            for (i = 0; i < k; i++) fprintf(stderr, "%d", sigma[i]);
            fprintf(stderr, ")\n");
            return 1;
        }

        if ((trial + 1) % 200000 == 0) {
            double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
            fprintf(stderr, "  ... %lu / %lu trials (%.1f%%, %.1fs)\n",
                    trial + 1, num_trials,
                    100.0 * (trial + 1) / num_trials, elapsed);
        }
    }

    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
    printf("PASS: all %lu isomorphism invariance trials passed for k=%d (%.1fs)\n",
           num_trials, k, elapsed);

    TinyGraphFree(tg);
    TinyGraphFree(permuted);

    return 0;
}
