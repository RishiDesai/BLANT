/*
 * test-subgraph-consistency.c
 *
 * "Chain of Trust" test: verifies k=9 canonical labeling by reduction to
 * the trusted k=8 flat lookup table.
 *
 * Algorithm:
 *   1. Load the k=8 flat table (canon_map8.bin) for trusted ordinal lookup
 *   2. Load the network (e.g., syeast.el)
 *   3. Run blant-large-k at k=9 via popen(), collect N graphlet samples as node sets
 *   4. For each k=9 sample (9 nodes):
 *      - For each of the 9 nodes, remove it to get a k=8 subgraph
 *      - Build the k=8 TINY_GRAPH from the network's adjacency
 *      - Look up _K[TinyGraph2Int()] to get the trusted k=8 ordinal
 *      - Increment a k=8 frequency counter
 *   5. Run blant at k=8 via popen() to get a reference k=8 distribution
 *   6. Compare the two k=8 distributions via Pearson correlation
 *   7. Assert correlation > 0.90
 *
 * Usage: test-subgraph-consistency [k] [num_samples] [network_file]
 *   k           = graphlet size for decomposition (default: 9)
 *   num_samples = number of graphlet samples (default: 50000)
 *   network_file = edge list file (default: networks/syeast.el)
 *
 * Compiled with -DTINY_SET_SIZE=16 -DMAX_K=10.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>

#include "blant.h"
#include "graph.h"

/* Number of k=8 canonicals */
#define NUM_CANON_K8 12346

/* Compute Pearson correlation coefficient between two arrays */
static double pearson_correlation(const double *x, const double *y, int n)
{
    double sum_x = 0, sum_y = 0, sum_xy = 0;
    double sum_x2 = 0, sum_y2 = 0;
    int i;
    for (i = 0; i < n; i++) {
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        sum_x2 += x[i] * x[i];
        sum_y2 += y[i] * y[i];
    }
    double denom = sqrt((n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y));
    if (denom < 1e-12) return 0.0;
    return (n * sum_xy - sum_x * sum_y) / denom;
}

int main(int argc, char *argv[])
{
    int k = (argc >= 2) ? atoi(argv[1]) : 9;
    int num_samples = (argc >= 3) ? atoi(argv[2]) : 50000;
    const char *network = (argc >= 4) ? argv[3] : "networks/syeast.el";
    int k_sub = k - 1; /* subgraph size: k=9 -> k_sub=8 */

    if (k < 4 || k > MAX_K) {
        fprintf(stderr, "Error: k must be between 4 and %d\n", MAX_K);
        return 1;
    }
    if (k_sub > 8) {
        fprintf(stderr, "Error: subgraph consistency only works for k<=9 (reduces to k<=8 flat table)\n");
        return 1;
    }

    _BLANT_DIR = ".";
    _CANON_DIR = "canon_maps";

    fprintf(stderr, "=== Subgraph Consistency Test (Chain of Trust) ===\n");
    fprintf(stderr, "k=%d -> k=%d decomposition, %d samples, network=%s\n",
            k, k_sub, num_samples, network);

    /* ------- Step 1: Load k=8 flat table ------- */
    fprintf(stderr, "Loading k=%d flat table (canon_map%d.bin)...\n", k_sub, k_sub);
    char BUF[BUFSIZ];
    Gordinal_type *K_sub = mapCanonMap(BUF, NULL, k_sub);
    if (!K_sub) {
        fprintf(stderr, "Error: could not load canon_map%d.bin\n", k_sub);
        return 1;
    }

    /* Also load the k_sub canon list to know how many canonicals */
    _canonList = (Gint_type *) calloc(MAX_CANONICALS, sizeof(Gint_type));
    _canonNumEdges = (char *) calloc(MAX_CANONICALS, sizeof(char));
    _connectedCanonicals = canonListPopulate(BUF, _canonList, k_sub, _canonNumEdges);
    Gordinal_type numCanon_sub = _connectedCanonicals->maxElem;
    fprintf(stderr, "  k=%d has %u canonicals\n", k_sub, (unsigned)numCanon_sub);

    /* ------- Step 2: Load the network ------- */
    fprintf(stderr, "Loading network %s...\n", network);
    FILE *fp = fopen(network, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s\n", network);
        return 1;
    }
    GRAPH *G = GraphReadEdgeList(fp, true, false, false); /* sparse, no names, no weights */
    fclose(fp);
    fprintf(stderr, "  Network has %u nodes\n", G->n);

    /* ------- Step 3: Run blant-large-k at k=K to get node sets ------- */
    fprintf(stderr, "Running blant-large-k at k=%d for %d samples...\n", k, num_samples);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "./blant-large-k -sNBE -k%d -mi -n%d -r42 -t1 %s 2>/dev/null",
        k, num_samples, network);

    FILE *pipe_k = popen(cmd, "r");
    if (!pipe_k) {
        fprintf(stderr, "Error: failed to run: %s\n", cmd);
        return 1;
    }

    /* Allocate frequency arrays */
    double *freq_decomp = (double *) calloc(numCanon_sub, sizeof(double));
    double *freq_direct = (double *) calloc(numCanon_sub, sizeof(double));
    assert(freq_decomp && freq_direct);

    /* ------- Step 4: Parse k-samples and decompose into k-1 subgraphs ------- */
    TINY_GRAPH *tg_sub = TinyGraphAlloc(k_sub);
    char line[4096];
    int samples_parsed = 0;
    unsigned long decomp_total = 0;

    while (fgets(line, sizeof(line), pipe_k)) {
        /* Parse: ordinal node0 node1 ... node_{k-1} */
        int nodes[MAX_K];
        int ordinal;
        int nf = 0;

        char *tok = strtok(line, " \t\n");
        if (!tok) continue;
        ordinal = atoi(tok); /* k-ordinal (not used for decomposition) */
        (void)ordinal;

        int ni = 0;
        while ((tok = strtok(NULL, " \t\n")) && ni < k) {
            nodes[ni++] = atoi(tok);
        }
        if (ni != k) continue; /* skip malformed lines */

        /* For each of k subsets of size k-1 (remove one node at a time) */
        int removed;
        for (removed = 0; removed < k; removed++) {
            /* Build k-1 node subset */
            int subset[MAX_K];
            int si = 0, r;
            for (r = 0; r < k; r++) {
                if (r != removed)
                    subset[si++] = nodes[r];
            }
            assert(si == k_sub);

            /* Build TINY_GRAPH from network adjacency */
            TinyGraphEdgesAllDelete(tg_sub);
            tg_sub->n = k_sub;
            int a, b;
            for (a = 0; a < k_sub; a++)
                for (b = a + 1; b < k_sub; b++)
                    if (GraphAreConnected(G, subset[a], subset[b]))
                        TinyGraphConnect(tg_sub, a, b);

            /* Get k-1 Gint and look up trusted ordinal */
            Gint_type gint_sub = TinyGraph2Int(tg_sub, k_sub);
            Gordinal_type ord_sub = K_sub[gint_sub];
            assert(ord_sub < numCanon_sub);

            freq_decomp[ord_sub]++;
            decomp_total++;
        }

        samples_parsed++;
        if (samples_parsed % 10000 == 0) {
            fprintf(stderr, "  ... parsed %d / %d samples\n",
                    samples_parsed, num_samples);
        }
    }
    pclose(pipe_k);
    fprintf(stderr, "  Parsed %d k=%d samples, %lu k=%d subgraphs from decomposition\n",
            samples_parsed, k, decomp_total, k_sub);

    /* ------- Step 5: Run blant at k-1 for reference distribution ------- */
    fprintf(stderr, "Running blant-large-k at k=%d for reference distribution...\n", k_sub);
    snprintf(cmd, sizeof(cmd),
        "./blant-large-k -sNBE -k%d -mi -n%d -r42 -t1 %s 2>/dev/null",
        k_sub, num_samples, network);

    FILE *pipe_sub = popen(cmd, "r");
    if (!pipe_sub) {
        fprintf(stderr, "Error: failed to run: %s\n", cmd);
        return 1;
    }

    int ref_samples = 0;
    while (fgets(line, sizeof(line), pipe_sub)) {
        char *tok = strtok(line, " \t\n");
        if (!tok) continue;
        int ord = atoi(tok);
        if (ord >= 0 && ord < (int)numCanon_sub) {
            freq_direct[ord]++;
            ref_samples++;
        }
    }
    pclose(pipe_sub);
    fprintf(stderr, "  Parsed %d k=%d reference samples\n", ref_samples, k_sub);

    /* ------- Step 6: Compute Pearson correlation ------- */
    double corr = pearson_correlation(freq_decomp, freq_direct, (int)numCanon_sub);
    fprintf(stderr, "  Pearson correlation: %.6f\n", corr);

    /* Count how many canonicals were observed in both distributions */
    int nonzero_decomp = 0, nonzero_direct = 0, nonzero_both = 0;
    Gordinal_type i;
    for (i = 0; i < numCanon_sub; i++) {
        if (freq_decomp[i] > 0) nonzero_decomp++;
        if (freq_direct[i] > 0) nonzero_direct++;
        if (freq_decomp[i] > 0 && freq_direct[i] > 0) nonzero_both++;
    }
    fprintf(stderr, "  Nonzero canonicals: decomp=%d, direct=%d, both=%d (of %u)\n",
            nonzero_decomp, nonzero_direct, nonzero_both, (unsigned)numCanon_sub);

    /* ------- Step 7: Pass/fail ------- */
    TinyGraphFree(tg_sub);
    free(freq_decomp);
    free(freq_direct);
    free(_canonList);
    free(_canonNumEdges);

    if (corr > 0.90) {
        printf("PASS: subgraph consistency k=%d->k=%d correlation=%.4f (>0.90)\n",
               k, k_sub, corr);
        return 0;
    } else {
        fprintf(stderr, "FAIL: subgraph consistency k=%d->k=%d correlation=%.4f (<= 0.90)\n",
                k, k_sub, corr);
        return 1;
    }
}
