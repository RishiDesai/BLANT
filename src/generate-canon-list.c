/*
 * generate-canon-list.c
 *
 * Reads graph6 format from stdin (piped from nauty's geng), computes the canonical
 * Gint for each graph using NautyCanonical, and outputs canon_list{k}.txt format.
 *
 * Usage: ./geng k 2>/dev/null | ./generate-canon-list k [sigfile] > canon_maps/canon_list{k}.txt
 *
 * Compile with -DTINY_SET_SIZE=16 -DMAX_K=10 for k>8 support.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "blant.h"
#include "tinygraph.h"
#include "nauty-canonical.h"

/* We need nauty's graph6 parsing */
#include "nauty.h"
#include "gtools.h"

/* BFS connectivity check */
static int is_connected(TINY_GRAPH *tg, int k) {
    if (k <= 1) return 1;
    int visited[k];
    int queue[k];
    memset(visited, 0, sizeof(visited));
    int head = 0, tail = 0;
    queue[tail++] = 0;
    visited[0] = 1;
    int count = 1;
    while (head < tail) {
        int u = queue[head++];
        int v;
        for (v = 0; v < k; v++) {
            if (!visited[v] && TinyGraphAreConnected(tg, u, v)) {
                visited[v] = 1;
                queue[tail++] = v;
                count++;
            }
        }
    }
    return (count == k);
}

/* Count edges in a TINY_GRAPH */
static int count_edges(TINY_GRAPH *tg, int k) {
    int count = 0;
    int i, j;
    for (i = 0; i < k; i++)
        for (j = i + 1; j < k; j++)
            if (TinyGraphAreConnected(tg, i, j))
                count++;
    return count;
}

/* Format an edge list string for canon_list format.
 * The edge list is for the CANONICAL form of the graph. */
static void format_edge_list(char *buf, int bufsize, TINY_GRAPH *tg, int k) {
    buf[0] = '\0';
    int pos = 0;
    int first = 1;
    int i, j;
    for (i = 0; i < k; i++)
        for (j = i + 1; j < k; j++)
            if (TinyGraphAreConnected(tg, i, j)) {
                if (!first)
                    pos += snprintf(buf + pos, bufsize - pos, " ");
                pos += snprintf(buf + pos, bufsize - pos, "%d,%d", i, j);
                first = 0;
            }
}

/* Entry: Gint, connected flag, num_edges.
 * Edge list is reconstructed on output from the Gint to save memory.
 * At k=10 with 12M entries, a 1024-byte edge_list per entry would use 12GB. */
typedef struct {
    Gint_type gint;
    int connected;
    int num_edges;
} CanonEntry;

static int cmp_entries(const void *a, const void *b) {
    const CanonEntry *ea = (const CanonEntry *)a;
    const CanonEntry *eb = (const CanonEntry *)b;
    if (ea->gint < eb->gint) return -1;
    if (ea->gint > eb->gint) return 1;
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <k> [sigfile]\n", argv[0]);
        fprintf(stderr, "  Reads graph6 from stdin. If sigfile is given, also writes signature file.\n");
        return 1;
    }

    int k = atoi(argv[1]);
    if (k < 3 || k > MAX_K) {
        fprintf(stderr, "Error: k must be between 3 and %d\n", MAX_K);
        return 1;
    }

    const char *sigfile = (argc >= 3) ? argv[2] : NULL;

    /* Allocate storage for entries. We'll grow as needed. */
    int capacity = 1000000;
    int count = 0;
    CanonEntry *entries = (CanonEntry *)malloc(capacity * sizeof(CanonEntry));
    assert(entries);

    /* nauty graph parsing workspace */
    int m = SETWORDSNEEDED(k);
    DYNALLSTAT(graph, g, g_sz);
    DYNALLOC2(graph, g, g_sz, k, m, "malloc g");

    TINY_GRAPH *tg = TinyGraphAlloc(k);
    TINY_GRAPH *canon_tg = TinyGraphAlloc(k);
    unsigned char perm[MAX_K];
    char line[1024];

    while (fgets(line, sizeof(line), stdin)) {
        /* Strip newline */
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[--len] = '\0';
        if (len == 0) continue;

        /* Parse graph6 into nauty dense graph */
        stringtograph(line, g, m);

        /* Convert nauty dense graph to TINY_GRAPH */
        TinyGraphEdgesAllDelete(tg);
        tg->n = k;
        int i, j;
        for (i = 0; i < k; i++)
            for (j = i + 1; j < k; j++)
                if (ISELEMENT(GRAPHROW(g, i, m), j))
                    TinyGraphConnect(tg, i, j);

        /* Compute canonical Gint using NautyCanonical */
        Gint_type canon_gint = NautyCanonical(tg, k, perm);

        /* Reconstruct canonical TINY_GRAPH for edge list and connectivity check */
        Int2TinyGraph(canon_tg, canon_gint);

        /* Grow array if needed */
        if (count >= capacity) {
            capacity *= 2;
            entries = (CanonEntry *)realloc(entries, capacity * sizeof(CanonEntry));
            assert(entries);
        }

        entries[count].gint = canon_gint;
        entries[count].connected = is_connected(canon_tg, k);
        entries[count].num_edges = count_edges(canon_tg, k);
        count++;

        if (count % 1000000 == 0)
            fprintf(stderr, "  ... processed %d graphs\n", count);
    }

    fprintf(stderr, "Processed %d graphs total for k=%d\n", count, k);

    /* Sort by Gint value */
    qsort(entries, count, sizeof(CanonEntry), cmp_entries);

    /* Output in canon_list format */
    printf("%d\n", count);
    TINY_GRAPH *out_tg = TinyGraphAlloc(k);
    char edge_buf[2048];
    int i;
    for (i = 0; i < count; i++) {
        if (entries[i].num_edges > 0) {
            /* Reconstruct TINY_GRAPH from Gint for edge list */
            Int2TinyGraph(out_tg, entries[i].gint);
            format_edge_list(edge_buf, sizeof(edge_buf), out_tg, k);
            printf(GINT_FMT "\t%d %d\t%s\n", entries[i].gint, entries[i].connected,
                   entries[i].num_edges, edge_buf);
        } else {
            printf(GINT_FMT "\t%d %d\n", entries[i].gint, entries[i].connected,
                   entries[i].num_edges);
        }
    }
    TinyGraphFree(out_tg);

    /* Optionally write signature file */
    if (sigfile) {
        FILE *fp = fopen(sigfile, "w");
        if (!fp) {
            fprintf(stderr, "Error: cannot open %s for writing\n", sigfile);
            return 1;
        }
        for (i = 0; i < count; i++) {
            fprintf(fp, "%d " GINT_FMT "\n", i, entries[i].gint);
        }
        fclose(fp);
        fprintf(stderr, "Wrote signature file: %s\n", sigfile);
    }

    /* Stats */
    int connected_count = 0;
    for (i = 0; i < count; i++)
        if (entries[i].connected) connected_count++;
    fprintf(stderr, "k=%d: %d canonicals, %d connected\n", k, count, connected_count);

    DYNFREE(g, g_sz);
    TinyGraphFree(tg);
    TinyGraphFree(canon_tg);
    free(entries);

    return 0;
}
