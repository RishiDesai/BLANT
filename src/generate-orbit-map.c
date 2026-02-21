/*
 * generate-orbit-map.c
 *
 * Reads canon_list{k}.txt, and for each canonical graphlet, uses nauty's
 * densenauty() to compute the automorphism group orbits. Outputs orbit_map{k}.txt.
 *
 * Usage: ./generate-orbit-map k canon_maps/orbit_map{k}.txt [canon_maps/canon_list{k}.txt]
 *
 * The orbit_map format is:
 *   <numOrbits>
 *   <orbit_id_0> <orbit_id_1> ... <orbit_id_{k-1}>   (one line per canonical)
 *
 * Compile with -DTINY_SET_SIZE=16 -DMAX_K=10 for k>8 support.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "blant.h"
#include "tinygraph.h"
#include "nauty.h"

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <k> <output_file> [canon_list_file]\n", argv[0]);
        return 1;
    }

    int k = atoi(argv[1]);
    if (k < 3 || k > MAX_K) {
        fprintf(stderr, "Error: k must be between 3 and %d\n", MAX_K);
        return 1;
    }

    const char *output_file = argv[2];
    FILE *outfp = fopen(output_file, "w");
    if (!outfp) {
        fprintf(stderr, "Error: cannot open %s\n", output_file);
        return 1;
    }

    /* Open canon_list file */
    FILE *fp = NULL;
    if (argc >= 4) {
        fp = fopen(argv[3], "r");
        if (!fp) {
            fprintf(stderr, "Error: cannot open %s\n", argv[3]);
            fclose(outfp);
            return 1;
        }
    } else {
        char buf[256];
        snprintf(buf, sizeof(buf), "canon_maps/canon_list%d.txt", k);
        fp = fopen(buf, "r");
        if (!fp) {
            fprintf(stderr, "Error: cannot open %s\n", buf);
            fclose(outfp);
            return 1;
        }
    }

    /* Read number of canonicals */
    int numCanon;
    if (1 != fscanf(fp, "%d", &numCanon) || numCanon <= 0) {
        fprintf(stderr, "Error: failed to read numCanon from canon_list\n");
        return 1;
    }

    /* Allocate arrays for Gint values */
    Gint_type *gints = (Gint_type *)malloc(numCanon * sizeof(Gint_type));
    assert(gints);

    /* Read all canonical Gint values */
    int i;
    for (i = 0; i < numCanon; i++) {
        char line[2048];
        if (!fgets(line, sizeof(line), fp)) {
            /* First fgets may get remainder of header line */
            if (i == 0) { i--; continue; }
            fprintf(stderr, "Error: unexpected EOF at canonical %d\n", i);
            return 1;
        }
        /* Skip empty lines */
        if (line[0] == '\n' || line[0] == '\r') { i--; continue; }
        Gint_type gint;
        int connected, num_edges;
        if (sscanf(line, GINT_FMT "\t%d %d", &gint, &connected, &num_edges) < 3) {
            /* This might be the header line if i==0 */
            if (i == 0) {
                /* Try to parse as header */
                int hdr;
                if (sscanf(line, "%d", &hdr) == 1 && hdr == numCanon) {
                    i--; continue;
                }
            }
            fprintf(stderr, "Error: failed to parse canon_list line %d: %s", i, line);
            return 1;
        }
        gints[i] = gint;
    }

    fclose(fp);

    fprintf(stderr, "Read %d canonicals for k=%d\n", numCanon, k);

    /* nauty workspace */
    int m = SETWORDSNEEDED(k);

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

    /* Allocate output orbit IDs for each canonical */
    /* orbit_ids[canonical][node] = global orbit ID */
    int **orbit_ids = (int **)malloc(numCanon * sizeof(int *));
    for (i = 0; i < numCanon; i++)
        orbit_ids[i] = (int *)malloc(k * sizeof(int));

    int global_orbit_count = 0;

    TINY_GRAPH *tg = TinyGraphAlloc(k);

    for (i = 0; i < numCanon; i++) {
        /* Reconstruct TINY_GRAPH from Gint */
        Int2TinyGraph(tg, gints[i]);

        /* Convert to nauty dense graph */
        EMPTYGRAPH(g, m, k);
        int u, v;
        for (u = 0; u < k; u++)
            for (v = u + 1; v < k; v++)
                if (TinyGraphAreConnected(tg, u, v))
                    ADDONEEDGE(g, u, v, m);

        /* Run nauty to get orbits */
        DEFAULTOPTIONS_GRAPH(options);
        options.getcanon = TRUE;
        statsblk stats;

        densenauty(g, lab, ptn, orbits, &options, &stats, m, k, cg);

        /* orbits[j] = orbit representative for node j (nodes in the same orbit have the same value) */
        /* Map local orbit representatives to global orbit IDs */
        int local_to_global[k];
        int j;
        for (j = 0; j < k; j++) local_to_global[j] = -1;

        for (j = 0; j < k; j++) {
            int local_rep = orbits[j];
            if (local_to_global[local_rep] == -1) {
                local_to_global[local_rep] = global_orbit_count++;
            }
            orbit_ids[i][j] = local_to_global[local_rep];
        }

        if ((i + 1) % 1000000 == 0)
            fprintf(stderr, "  ... processed %d / %d canonicals\n", i + 1, numCanon);
    }

    fprintf(stderr, "k=%d: %d canonicals, %d orbits\n", k, numCanon, global_orbit_count);

    /* Output orbit_map format */
    fprintf(outfp, "%d\n", global_orbit_count);
    for (i = 0; i < numCanon; i++) {
        int j;
        for (j = 0; j < k; j++) {
            if (j > 0) fprintf(outfp, " ");
            fprintf(outfp, "%d", orbit_ids[i][j]);
        }
        fprintf(outfp, " \n");
    }

    /* Cleanup */
    for (i = 0; i < numCanon; i++) free(orbit_ids[i]);
    free(orbit_ids);
    free(gints);
    TinyGraphFree(tg);
    DYNFREE(g, g_sz);
    DYNFREE(cg, cg_sz);
    DYNFREE(lab, lab_sz);
    DYNFREE(ptn, ptn_sz);
    DYNFREE(orbits, orbits_sz);
    fclose(outfp);

    return 0;
}
