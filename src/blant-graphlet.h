/* blant-graphlet.h -- Small graphlet (TINY_GRAPH) for BLANT
 * Replaces libwayne's tinygraph.h/tinygraph.c.
 * Written from scratch using bit-packed adjacency for k <= 8.
 */
#ifndef BLANT_GRAPHLET_H
#define BLANT_GRAPHLET_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "blant-bitset.h"  /* for TSET, TSetEmpty, etc. */
#include "blant-fatal.h"
#include "blant-utils-base.h"

typedef struct _tinyGraph {
    char n;                  /* number of nodes, must be <= MAX_TSET */
    char degree[MAX_TSET];   /* degree of each node */
    bool selfLoops;
    TSET A[MAX_TSET];        /* adjacency bit-vector per node */
} TINY_GRAPH;

static inline TINY_GRAPH *TinyGraphAlloc(unsigned n) {
    assert(n <= MAX_TSET);
    TINY_GRAPH *g = (TINY_GRAPH*)calloc(1, sizeof(TINY_GRAPH));
    if (!g) Fatal("TinyGraphAlloc: out of memory");
    g->n = n;
    g->selfLoops = false;
    return g;
}

static inline TINY_GRAPH *TinyGraphSelfAlloc(unsigned n) {
    TINY_GRAPH *g = TinyGraphAlloc(n);
    g->selfLoops = true;
    return g;
}

#define TinyGraphFree(g) free(g)

static inline TINY_GRAPH *TinyGraphEdgesAllDelete(TINY_GRAPH *G) {
    for (int i = 0; i < G->n; i++) {
        G->degree[i] = 0;
        TSetEmpty(G->A[i]);
    }
    return G;
}

static inline TINY_GRAPH *TinyGraphCopy(TINY_GRAPH *dst, TINY_GRAPH *src) {
    memcpy(dst, src, sizeof(TINY_GRAPH));
    return dst;
}

static inline bool TinyGraphAreConnected(TINY_GRAPH *G, int i, int j) {
    assert(0 <= i && i < G->n && 0 <= j && j < G->n);
    return TSetIn(G->A[i], j) ? true : false;
}

static inline TINY_GRAPH *TinyGraphConnect(TINY_GRAPH *G, int i, int j) {
    assert(0 <= i && i < G->n && 0 <= j && j < G->n);
    if (i == j) { assert(G->selfLoops); }
    if (!TSetIn(G->A[i], j)) {
        TSetAdd(G->A[i], j);
        TSetAdd(G->A[j], i);
        G->degree[i]++;
        if (i != j) G->degree[j]++;
    }
    return G;
}

static inline TINY_GRAPH *TinyGraphDisconnect(TINY_GRAPH *G, int i, int j) {
    assert(0 <= i && i < G->n && 0 <= j && j < G->n);
    if (TSetIn(G->A[i], j)) {
        TSetDelete(G->A[i], j);
        TSetDelete(G->A[j], i);
        G->degree[i]--;
        if (i != j) G->degree[j]--;
    }
    return G;
}

static inline TINY_GRAPH *TinyGraphSwapNodes(TINY_GRAPH *G, int u, int v) {
    assert(0 <= u && u < G->n && 0 <= v && v < G->n);
    if (u == v) return G;
    /* Swap adjacency rows */
    TSET tmp = G->A[u]; G->A[u] = G->A[v]; G->A[v] = tmp;
    /* Swap degrees */
    char dtmp = G->degree[u]; G->degree[u] = G->degree[v]; G->degree[v] = dtmp;
    /* Now fix all other nodes: swap bits u and v in their adjacency sets */
    for (int w = 0; w < G->n; w++) {
        bool hasU = TSetIn(G->A[w], u) ? true : false;
        bool hasV = TSetIn(G->A[w], v) ? true : false;
        if (hasU != hasV) {
            if (hasU) { TSetDelete(G->A[w], u); TSetAdd(G->A[w], v); }
            else      { TSetDelete(G->A[w], v); TSetAdd(G->A[w], u); }
        }
    }
    return G;
}

#define TinyGraphDegree(G, v) ((G)->degree[v])

static inline int TinyGraphNumEdges(TINY_GRAPH *G) {
    int sum = 0;
    for (int i = 0; i < G->n; i++) sum += G->degree[i];
    return sum / 2;
}

/* BFS on small graph. Returns number of reachable nodes.
 * nodeArray and distArray should be at least G->n in size.
 */
static inline int TinyGraphBFS(TINY_GRAPH *G, int seed, int distance, int *nodeArray, int *distArray) {
    int i, count = 0;
    assert(0 <= seed && seed < G->n);
    for (i = 0; i < G->n; i++) nodeArray[i] = distArray[i] = -1;
    distArray[seed] = 0;
    if (distance == 0) { nodeArray[0] = seed; return 1; }

    /* Simple BFS using a stack-allocated queue */
    int q[MAX_TSET], qfront = 0, qback = 0;
    q[qback++] = seed;
    while (qfront < qback) {
        int v = q[qfront++];
        nodeArray[count++] = v;
        if (distArray[v] < distance) {
            unsigned neighbors[MAX_TSET];
            int nn = TSetToArray(neighbors, G->A[v]);
            for (i = 0; i < nn; i++) {
                int w = neighbors[i];
                if (distArray[w] < 0) {
                    distArray[w] = distArray[v] + 1;
                    q[qback++] = w;
                }
            }
        }
    }
    return count;
}

static inline unsigned TinyGraphNumReachableNodes(TINY_GRAPH *g, int seed) {
    int nodeArray[MAX_TSET], distArray[MAX_TSET];
    return TinyGraphBFS(g, seed, g->n, nodeArray, distArray);
}

static inline bool TinyGraphDFSConnected(TINY_GRAPH *G, int seed) {
    return TinyGraphNumReachableNodes(G, seed) == (unsigned)G->n;
}

static inline void TinyGraphPrintAdjMatrix(FILE *fp, TINY_GRAPH *G) {
    for (int i = 0; i < G->n; i++) {
        for (int j = 0; j < G->n; j++)
            fprintf(fp, "%d ", TSetIn(G->A[i], j) ? 1 : 0);
        fprintf(fp, "\n");
    }
}

#endif /* BLANT_GRAPHLET_H */
