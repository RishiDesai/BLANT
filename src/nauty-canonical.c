#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "nauty-canonical.h"
#include "nauty.h"

/*
 * NautyCanonical: compute the canonical form of a TINY_GRAPH using nauty.
 *
 * Convention: BLANT uses PERMS_CAN2NON=1, meaning perm[i] = "canonical position i
 * came from original node perm[i]". nauty's lab[] has the same meaning:
 * lab[i] = "original node lab[i] goes to canonical position i".
 * So perm[i] = lab[i] gives the can2non mapping.
 *
 * We build the canonical TINY_GRAPH by applying the inverse mapping: for each
 * edge (u,v) in the original graph, we add edge (inv_lab[u], inv_lab[v]) in the
 * canonical graph, where inv_lab[original_node] = canonical_position.
 */
Gint_type NautyCanonical(TINY_GRAPH *tg, int k, unsigned char perm[])
{
    int m = SETWORDSNEEDED(k);

    /* Allocate nauty workspace on the stack (k <= ~16, so this is fine) */
    DYNALLSTAT(graph, g, g_sz);
    DYNALLSTAT(graph, cg, cg_sz);
    DYNALLSTAT(int, lab, lab_sz);
    DYNALLSTAT(int, ptn, ptn_sz);
    DYNALLSTAT(int, orbits, orbits_sz);

    DYNALLOC2(graph, g, g_sz, k, m, "malloc g");
    DYNALLOC2(graph, cg, cg_sz, k, m, "malloc cg");
    DYNALLOC1(int, lab, lab_sz, k, "malloc lab");
    DYNALLOC1(int, ptn, ptn_sz, k, "malloc ptn");
    DYNALLOC1(int, orbits, orbits_sz, k, "malloc orbits");

    /* Convert TINY_GRAPH to nauty dense graph format */
    EMPTYGRAPH(g, m, k);
    int i, j;
    for (i = 0; i < k; i++)
        for (j = i + 1; j < k; j++)
            if (TinyGraphAreConnected(tg, i, j))
                ADDONEEDGE(g, i, j, m);

    /* Run nauty */
    DEFAULTOPTIONS_GRAPH(options);
    options.getcanon = TRUE;
    statsblk stats;

    densenauty(g, lab, ptn, orbits, &options, &stats, m, k, cg);

    /*
     * lab[i] = "original node that goes to canonical position i"
     * We need inv_lab: inv_lab[original_node] = canonical_position
     * to build the canonical TINY_GRAPH.
     */
    unsigned char inv_lab[k];
    for (i = 0; i < k; i++)
        inv_lab[lab[i]] = (unsigned char)i;

    /* Build canonical TINY_GRAPH by remapping edges */
    TINY_GRAPH *canon_tg = TinyGraphAlloc(k);
    for (i = 0; i < k; i++)
        for (j = i + 1; j < k; j++)
            if (TinyGraphAreConnected(tg, i, j))
                TinyGraphConnect(canon_tg, inv_lab[i], inv_lab[j]);

    Gint_type canon_gint = TinyGraph2Int(canon_tg, k);
    TinyGraphFree(canon_tg);

    /* Fill perm[] with the can2non mapping: perm[i] = lab[i] */
    for (i = 0; i < k; i++)
        perm[i] = (unsigned char)lab[i];

    /* Free nauty dynamic allocations */
    DYNFREE(g, g_sz);
    DYNFREE(cg, cg_sz);
    DYNFREE(lab, lab_sz);
    DYNFREE(ptn, ptn_sz);
    DYNFREE(orbits, orbits_sz);

    return canon_gint;
}

#ifdef NAUTY_CANONICAL_TEST
/*
 * Standalone test: takes Gint and k on command line, reconstructs TINY_GRAPH,
 * runs NautyCanonical, prints canonical Gint and permutation.
 */
int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <Gint> <k>\n", argv[0]);
        return 1;
    }

    Gint_type gint = (Gint_type)atol(argv[1]);
    int k = atoi(argv[2]);
    assert(k >= 3 && k <= 8);

    TINY_GRAPH *tg = TinyGraphAlloc(k);
    Int2TinyGraph(tg, gint);

    unsigned char perm[k];
    Gint_type canon = NautyCanonical(tg, k, perm);

    printf("Input Gint: %u\n", (unsigned)gint);
    printf("Canon Gint: %u\n", (unsigned)canon);
    printf("Perm (can2non):");
    int i;
    for (i = 0; i < k; i++)
        printf(" %d", perm[i]);
    printf("\n");

    /* Verify: applying perm to canonical graph should give original graph */
    TINY_GRAPH *canon_tg = TinyGraphAlloc(k);
    Int2TinyGraph(canon_tg, canon);

    /* Apply perm (can2non): edge (ci, cj) in canonical -> edge (perm[ci], perm[cj]) in original */
    TINY_GRAPH *rebuilt = TinyGraphAlloc(k);
    for (i = 0; i < k; i++)
        for (int j = i + 1; j < k; j++)
            if (TinyGraphAreConnected(canon_tg, i, j))
                TinyGraphConnect(rebuilt, perm[i], perm[j]);

    Gint_type rebuilt_gint = TinyGraph2Int(rebuilt, k);
    printf("Rebuilt Gint: %u (should match input %u)\n", (unsigned)rebuilt_gint, (unsigned)gint);
    assert(rebuilt_gint == gint);

    TinyGraphFree(tg);
    TinyGraphFree(canon_tg);
    TinyGraphFree(rebuilt);

    printf("OK\n");
    return 0;
}
#endif
