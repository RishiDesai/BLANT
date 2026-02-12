/* blant-graph.h -- Graph data structure for BLANT replacing libwayne's GRAPH
 * Sparse adjacency list representation with O(degree) adjacency check.
 * Written from scratch for BLANT.
 */
#ifndef BLANT_GRAPH_H
#define BLANT_GRAPH_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "blant-fatal.h"
#include "blant-utils-base.h"
#include "blant-bitset.h"
#include "uthash.h"

/* Forward declaration for edge weight callback */
typedef double (*GraphEdgeWeightFn)(unsigned int u, unsigned int v);

/* Name-to-int hash table entry (replaces libwayne's BINTREE for name dictionary) */
typedef struct _nameEntry {
    char *name;          /* key (strdup'd) */
    int id;              /* value: node integer ID */
    UT_hash_handle hh;   /* makes this structure hashable */
} NameEntry;

typedef struct _Graph {
    unsigned n;             /* number of nodes, numbered 0..n-1 */
    bool useComplement;     /* invert adjacency queries */
    bool sparse;            /* always true in this implementation */
    bool selfAllowed;       /* allow self-loops */
    unsigned *degree;       /* degree[v] = number of neighbors of v */
    unsigned **neighbor;    /* neighbor[v][0..degree[v]-1] = adjacency list */
    float **weight;         /* weight[v][k] = weight of edge to neighbor[v][k], or NULL if unweighted */
    unsigned maxEdges, numEdges, *edgeList; /* flat edge list: edgeList[2*e], edgeList[2*e+1] */
    /* Node name support */
    bool supportNodeNames;
    NameEntry *nameDict;    /* hash table: name -> int */
    char **name;            /* int -> name array */
    GraphEdgeWeightFn edgeWeightFn;
} GRAPH;

/* Allocate a graph with n nodes */
static inline GRAPH *GraphAlloc(unsigned n, bool supportNodeNames, GraphEdgeWeightFn ewf) {
    GRAPH *G = (GRAPH*)calloc(1, sizeof(GRAPH));
    if (!G) Fatal("GraphAlloc: out of memory");
    G->sparse = true;
    G->n = n;
    G->degree = (unsigned*)calloc(n, sizeof(unsigned));
    G->maxEdges = 1024;
    G->edgeList = (unsigned*)malloc(2 * G->maxEdges * sizeof(unsigned));
    G->numEdges = 0;
    G->neighbor = (unsigned**)calloc(n, sizeof(unsigned*));
    G->supportNodeNames = supportNodeNames;
    G->edgeWeightFn = ewf;
    G->nameDict = NULL;
    return G;
}

static inline GRAPH *GraphSelfAlloc(unsigned n, bool supportNodeNames, GraphEdgeWeightFn ewf) {
    GRAPH *G = GraphAlloc(n, supportNodeNames, ewf);
    G->selfAllowed = true;
    return G;
}

static inline GRAPH *GraphMakeWeighted(GRAPH *G) {
    assert(G);
    G->weight = (float**)calloc(G->n, sizeof(float*));
    return G;
}

/* Name dictionary operations using uthash */
static inline int _GraphNameLookup(NameEntry *dict, const char *name) {
    NameEntry *entry = NULL;
    HASH_FIND_STR(dict, name, entry);
    return entry ? entry->id : -1;
}

static inline NameEntry *_GraphNameInsert(NameEntry **dict, const char *name, int id) {
    NameEntry *entry = (NameEntry*)malloc(sizeof(NameEntry));
    entry->name = Strdup(name);
    entry->id = id;
    HASH_ADD_KEYPTR(hh, *dict, entry->name, strlen(entry->name), entry);
    return entry;
}

/* Check adjacency: scan shorter neighbor list */
static inline bool _GraphRawConnected(GRAPH *G, int i, int j) {
    unsigned me, other, n, k;
    const unsigned *neighbors;
    if (G->degree[i] < G->degree[j]) { me = i; other = j; }
    else { me = j; other = i; }
    n = G->degree[me];
    neighbors = G->neighbor[me];
    for (k = 0; k < n; k++)
        if (neighbors[k] == (unsigned)other) return true;
    return false;
}

static inline bool GraphAreConnected(GRAPH *G, int i, int j) {
    if (G->useComplement) return !_GraphRawConnected(G, i, j);
    return _GraphRawConnected(G, i, j);
}

#define GraphDegree(G, v) ((G)->useComplement ? (G)->n - (G)->degree[v] : (G)->degree[v])
#define GraphNumEdges(G)  ((G)->useComplement ? (((G)->n * ((G)->n - 1)) / 2 - (G)->numEdges) : (G)->numEdges)

/* Connect two nodes */
static inline GRAPH *GraphConnect(GRAPH *G, unsigned i, unsigned j) {
    assert(i < G->n && j < G->n);
    if (i == j) assert(G->selfAllowed);
    if (GraphAreConnected(G, i, j)) return G;

    G->neighbor[i] = (unsigned*)realloc(G->neighbor[i], (G->degree[i] + 1) * sizeof(unsigned));
    if (j != i) G->neighbor[j] = (unsigned*)realloc(G->neighbor[j], (G->degree[j] + 1) * sizeof(unsigned));
    if (G->weight) {
        G->weight[i] = (float*)realloc(G->weight[i], (G->degree[i] + 1) * sizeof(float));
        if (j != i) G->weight[j] = (float*)realloc(G->weight[j], (G->degree[j] + 1) * sizeof(float));
    }
    G->neighbor[i][G->degree[i]] = j;
    G->neighbor[j][G->degree[j]] = i;
    if (G->weight) {
        G->weight[i][G->degree[i]] = 1;
        G->weight[j][G->degree[j]] = 1;
    }

    if (G->numEdges == G->maxEdges) {
        G->maxEdges = 2 * G->maxEdges;
        G->edgeList = (unsigned*)realloc(G->edgeList, 2 * G->maxEdges * sizeof(unsigned));
    }
    G->edgeList[2 * G->numEdges] = MIN(i, j);
    G->edgeList[2 * G->numEdges + 1] = MAX(i, j);
    G->numEdges++;
    G->degree[i]++;
    if (j != i) G->degree[j]++;
    return G;
}

/* Disconnect two nodes */
static inline GRAPH *GraphDisconnect(GRAPH *G, unsigned i, unsigned j) {
    if (i == j) assert(G->selfAllowed);
    assert(i < G->n && j < G->n);
    if (!GraphAreConnected(G, i, j)) return G;

    G->degree[i]--;
    if (j != i) G->degree[j]--;

    /* Fix edge list */
    unsigned ii = MIN(i, j), jj = MAX(i, j);
    for (unsigned k = 0; k < G->numEdges; k++) {
        if (G->edgeList[2*k] == ii && G->edgeList[2*k+1] == jj) {
            G->numEdges--;
            G->edgeList[2*k] = G->edgeList[2*G->numEdges];
            G->edgeList[2*k+1] = G->edgeList[2*G->numEdges+1];
            break;
        }
    }

    /* Fix neighbor lists */
    unsigned k = 0;
    while (G->neighbor[i][k] != j) k++;
    G->neighbor[i][k] = G->neighbor[i][G->degree[i]];
    if (G->weight) G->weight[i][k] = G->weight[i][G->degree[i]];

    if (j != i) {
        k = 0;
        while (G->neighbor[j][k] != i) k++;
        G->neighbor[j][k] = G->neighbor[j][G->degree[j]];
        if (G->weight) G->weight[j][k] = G->weight[j][G->degree[j]];
    }
    return G;
}

/* Set/get edge weight */
static inline double GraphSetWeight(GRAPH *G, unsigned i, unsigned j, double w) {
    assert(w > 0);
    GraphConnect(G, i, j);
    assert(G->weight);
    unsigned k = 0;
    double old;
    while (G->neighbor[i][k] != j) k++;
    old = G->weight[i][k];
    G->weight[i][k] = w;
    if (j != i) {
        k = 0;
        while (G->neighbor[j][k] != i) k++;
        G->weight[j][k] = w;
    }
    return old;
}

static inline double GraphGetWeight(GRAPH *G, unsigned i, unsigned j) {
    if (!GraphAreConnected(G, i, j)) return 0.0;
    if (G->edgeWeightFn) return G->edgeWeightFn(i, j);
    if (!G->weight) return 1.0;
    unsigned k = 0;
    while (G->neighbor[i][k] != j) k++;
    return G->weight[i][k];
}

/* Next neighbor iterator: *buf=0 initially, returns -1 when exhausted */
static inline int GraphNextNeighbor(GRAPH *G, int u, int *buf) {
    if (G->useComplement) {
        while (*buf < (int)G->n && _GraphRawConnected(G, u, *buf)) (*buf)++;
        if (*buf == (int)G->n) return -1;
        return (*buf)++;
    }
    if (*buf == (int)G->degree[u]) return -1;
    return G->neighbor[u][(*buf)++];
}

/* Random neighbor */
static inline int GraphRandomNeighbor(GRAPH *G, int u) {
    extern double RandomUniform(void);
    if (G->useComplement) {
        assert(G->degree[u] < G->n);
        int v;
        do { v = G->n * RandomUniform(); } while ((u == v && !G->selfAllowed) || _GraphRawConnected(G, u, v));
        return v;
    }
    assert(G->degree[u] > 0);
    return G->neighbor[u][(int)(G->degree[u] * RandomUniform())];
}

/* DFS connected components */
static inline int GraphVisitCC(GRAPH *G, unsigned v, SET *visited, unsigned *Varray, int *pn) {
    if (!SetIn(visited, v)) {
        SetAdd(visited, v);
        Varray[(*pn)++] = v;
        for (unsigned i = 0; i < G->degree[v]; i++) {
            if (G->neighbor[v][i] == v) assert(G->selfAllowed);
            else GraphVisitCC(G, G->neighbor[v][i], visited, Varray, pn);
        }
    }
    return *pn;
}

/* DFS to check if CC has at least k nodes */
static inline bool _GraphCCatLeastKHelper(GRAPH *G, SET *visited, int v, int *k) {
    SetAdd(visited, v);
    *k -= 1;
    if (*k <= 0) return true;
    for (unsigned i = 0; i < G->degree[v]; i++) {
        if (G->neighbor[v][i] == (unsigned)v) continue;
        if (!SetIn(visited, G->neighbor[v][i])) {
            if (_GraphCCatLeastKHelper(G, visited, G->neighbor[v][i], k)) return true;
        }
    }
    return false;
}

static inline bool GraphCCatLeastK(GRAPH *G, int v, int k) {
    SET *visited = SetAlloc(G->n);
    bool result = _GraphCCatLeastKHelper(G, visited, v, &k);
    SetFree(visited);
    return result;
}

/* Copy a graph (deep) */
static inline GRAPH *GraphCopy(GRAPH *G) {
    GRAPH *Gc = GraphAlloc(G->n, false, G->edgeWeightFn);
    Gc->useComplement = G->useComplement;
    Gc->selfAllowed = G->selfAllowed;
    free(Gc->degree);
    Gc->degree = (unsigned*)calloc(G->n, sizeof(unsigned));
    for (unsigned i = 0; i < G->n; i++) Gc->degree[i] = G->degree[i];
    if (G->weight) Apology("Sorry GraphCopy not yet implemented for weighted graphs");
    for (unsigned i = 0; i < G->n; i++) {
        Gc->neighbor[i] = (unsigned*)calloc(G->degree[i], sizeof(unsigned));
        for (unsigned j = 0; j < G->degree[i]; j++) Gc->neighbor[i][j] = G->neighbor[i][j];
    }
    Gc->numEdges = G->numEdges;
    Gc->maxEdges = G->maxEdges;
    free(Gc->edgeList);
    Gc->edgeList = (unsigned*)calloc(2 * G->maxEdges, sizeof(unsigned));
    for (unsigned i = 0; i < G->numEdges; i++) {
        Gc->edgeList[2*i] = G->edgeList[2*i];
        Gc->edgeList[2*i+1] = G->edgeList[2*i+1];
    }
    return Gc;
}

static inline GRAPH *GraphEdgesAllDelete(GRAPH *G) {
    for (unsigned i = 0; i < G->n; i++) G->degree[i] = 0;
    G->numEdges = 0;
    return G;
}

/* Read edge list from file */
static inline GRAPH *GraphFromEdgeList(unsigned n, unsigned m, unsigned *pairs, bool sparse, float *weights) {
    GRAPH *G = GraphAlloc(n, false, NULL);
    if (weights) GraphMakeWeighted(G);
    for (unsigned i = 0; i < m; i++) {
        if (pairs[2*i] == pairs[2*i+1] && !G->selfAllowed) {
            static bool warned = false;
            if (!warned) Warning("GraphFromEdgeList: node %d has a self-loop; assuming allowed", pairs[2*i]);
            warned = G->selfAllowed = true;
        }
        GraphConnect(G, pairs[2*i], pairs[2*i+1]);
        if (weights) { assert(weights[i] != 0.0); GraphSetWeight(G, pairs[2*i], pairs[2*i+1], weights[i]); }
    }
    return G;
}

static inline int GraphNodeName2Int(GRAPH *G, char *name) {
    if (!G->nameDict) return -1;
    return _GraphNameLookup(G->nameDict, name);
}

static inline GRAPH *GraphReadEdgeList(FILE *fp, bool sparse, bool supportNodeNames, bool weighted) {
    unsigned numNodes = 0, numEdges = 0, maxEdges = 1024;
    unsigned *pairs = (unsigned*)malloc(2 * maxEdges * sizeof(unsigned));
    float *fweight = NULL;
    if (weighted) fweight = (float*)malloc(maxEdges * sizeof(float));

    unsigned maxNames = 1024;
    char **names = NULL;
    NameEntry *nameDict = NULL;
    if (supportNodeNames) {
        names = (char**)malloc(maxNames * sizeof(char*));
    }

    char line[BUFSIZ];
    static bool selfWarned = false;
    while (fgets(line, sizeof(line), fp)) {
        int len = strlen(line);
        while (len > 0 && isspace((unsigned char)line[len-1])) line[--len] = '\0';
        if (len == 0) continue; /* skip blank lines */
        if (line[0] == '#') continue; /* skip comments */

        /* Grow edge array if needed */
        if (numEdges >= maxEdges) {
            maxEdges *= 2;
            pairs = (unsigned*)realloc(pairs, 2 * maxEdges * sizeof(unsigned));
            if (weighted) fweight = (float*)realloc(fweight, maxEdges * sizeof(float));
        }

        float w = 0;
        if (supportNodeNames) {
            char name1[BUFSIZ], name2[BUFSIZ];
            int nr = weighted ? sscanf(line, "%s %s %f", name1, name2, &w) : sscanf(line, "%s %s", name1, name2);
            if (nr < (weighted ? 3 : 2)) {
                /* Try parsing as integer count on first line */
                int testNum;
                if (numEdges == 0 && sscanf(line, "%d", &testNum) == 1) {
                    char *p = line; while (*p && (isdigit(*p) || isspace(*p))) p++;
                    if (*p == '\0') continue; /* skip integer-only first line */
                }
                Fatal("GraphReadEdgeList: line %d must contain 2 strings%s, but instead is\n%s\n",
                    numEdges, weighted ? " and a weight" : "", line);
            }
            if (strcmp(name1, name2) == 0 && !selfWarned) {
                Warning("GraphReadEdgeList: line %d has self-loop (%s to itself)", numEdges, name1);
                selfWarned = true;
            }
            /* Grow name array if needed */
            if (numNodes + 2 >= maxNames) {
                maxNames *= 2;
                names = (char**)realloc(names, maxNames * sizeof(char*));
            }
            /* Look up or insert name1 */
            int id1 = _GraphNameLookup(nameDict, name1);
            if (id1 < 0) {
                names[numNodes] = Strdup(name1);
                id1 = numNodes++;
                _GraphNameInsert(&nameDict, name1, id1);
            }
            int id2 = _GraphNameLookup(nameDict, name2);
            if (id2 < 0) {
                names[numNodes] = Strdup(name2);
                id2 = numNodes++;
                _GraphNameInsert(&nameDict, name2, id2);
            }
            pairs[2*numEdges] = id1;
            pairs[2*numEdges+1] = id2;
        } else {
            int v1, v2;
            int nr = weighted ? sscanf(line, "%d %d %f", &v1, &v2, &w) : sscanf(line, "%d %d", &v1, &v2);
            if (nr < (weighted ? 3 : 2)) {
                /* Try parsing as integer count on first line */
                int testNum;
                if (numEdges == 0 && sscanf(line, "%d", &testNum) == 1) {
                    char *p = line; while (*p && (isdigit(*p) || isspace(*p))) p++;
                    if (*p == '\0') continue; /* skip integer-only first line */
                }
                Fatal("GraphReadEdgeList: line %d must contain 2 ints%s, but instead is\n%s\n",
                    numEdges, weighted ? " and a weight" : "", line);
            }
            if (v1 == v2 && !selfWarned) {
                Warning("GraphReadEdgeList: line %d has self-loop (%d to itself)", numEdges, v1);
                selfWarned = true;
            }
            numNodes = MAX(numNodes, (unsigned)(v1 + 1));
            numNodes = MAX(numNodes, (unsigned)(v2 + 1));
            pairs[2*numEdges] = v1;
            pairs[2*numEdges+1] = v2;
        }
        if (pairs[2*numEdges] > pairs[2*numEdges+1]) {
            unsigned tmp = pairs[2*numEdges];
            pairs[2*numEdges] = pairs[2*numEdges+1];
            pairs[2*numEdges+1] = tmp;
        }
        if (weighted) { assert(w > 0.0); fweight[numEdges] = w; }
        numEdges++;
    }

    GRAPH *G = GraphFromEdgeList(numNodes, numEdges, pairs, sparse, fweight);
    G->supportNodeNames = supportNodeNames;
    if (supportNodeNames) {
        G->nameDict = nameDict;
        G->name = names;
    }
    free(pairs);
    if (weighted) free(fweight);
    return G;
}

/* BinTree compatibility: lookup using uthash name dictionary */
/* BinTreeLookup(G->nameDict, (foint)name, &nodeNum) -> returns bool, sets nodeNum.i */
/* We define a compatibility wrapper */
typedef union _fointCompat {
    unsigned long ul;
    long int l;
    void *v;
    char *s;
    int i;
    unsigned int ui;
    float f;
} foint;

static inline bool BinTreeLookup(NameEntry *dict, foint key, foint *pInfo) {
    int id = _GraphNameLookup(dict, key.s);
    if (id < 0) return false;
    if (pInfo) pInfo->i = id;
    return true;
}

/* Induced subgraph: create a new graph with only the vertices in V and edges between them */
static inline GRAPH *GraphInduced(GRAPH *G, SET *V) {
    unsigned array[G->n];
    unsigned nV = SetToArray(array, V);
    GRAPH *Gv = GraphAlloc(nV, false, G->edgeWeightFn);
    Gv->selfAllowed = G->selfAllowed;
    for (unsigned i = 0; i < nV; i++)
        for (unsigned j = i + 1; j < nV; j++)
            if (GraphAreConnected(G, array[i], array[j]))
                GraphConnect(Gv, i, j);
    return Gv;
}

/* Free graph */
static inline void GraphFree(GRAPH *G) {
    for (unsigned i = 0; i < G->n; i++) {
        free(G->neighbor[i]);
        if (G->weight && G->weight[i]) free(G->weight[i]);
    }
    free(G->degree);
    free(G->edgeList);
    free(G->neighbor);
    if (G->weight) free(G->weight);
    if (G->name) {
        for (unsigned i = 0; i < G->n; i++) free(G->name[i]);
        free(G->name);
    }
    /* Free name dictionary */
    if (G->nameDict) {
        NameEntry *entry, *tmp;
        HASH_ITER(hh, G->nameDict, entry, tmp) {
            HASH_DEL(G->nameDict, entry);
            free(entry->name);
            free(entry);
        }
    }
    free(G);
}

#endif /* BLANT_GRAPH_H */
