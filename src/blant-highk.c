/*
 * blant-highk.c - High-k runtime support for BLANT using nauty canonical labeling.
 *
 * This module provides the runtime infrastructure for graphlet sizes k>8,
 * where the flat mmap'd lookup tables (canon_map.bin, perm_map.bin) are too
 * large to fit in memory. Instead, we use:
 *
 * Mode 1 (k=9-11): Load canon_list{k}.txt into a hash map. For each sampled
 *   graphlet, call NautyCanonical() to get the canonical Gint, then look up
 *   the ordinal in the hash map. O(1) amortized per sample.
 *
 * Mode 2 (k>=12): On-the-fly discovery. NautyCanonical() per sample, ordinals
 *   assigned incrementally as new canonicals are encountered. No precomputed
 *   files needed. Raw counts mode required (no alpha corrections).
 */

#include "blant-fundamentals.h"

#if HIGH_K_SUPPORTED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "blant.h"
#include "blant-highk.h"
#include "nauty-canonical.h"
#include "uthash.h"

/* ---- Hash map entry: canonical Gint -> ordinal ---- */
typedef struct {
    Gint_type canon_gint;   /* key */
    Gordinal_type ordinal;  /* value */
    int connected;
    int num_edges;
    UT_hash_handle hh;
} CanonHashEntry;

static CanonHashEntry *_canonHash = NULL;
static Gordinal_type _nextOrdinal = 0;

/* ---- Global state ---- */
int _highK_mode = 0;
static int _highK_k = 0;

/* Dynamically allocated arrays */
Gint_type *_dyn_canonList = NULL;
char *_dyn_canonNumEdges = NULL;
Gint_type **_dyn_orbitList = NULL;
Gordinal_type *_dyn_orbitCanonMapping = NULL;
char *_dyn_orbitCanonNodeMapping = NULL;
Gint_type *_dyn_alphaList = NULL;
static Gordinal_type _dyn_numCanon = 0;
static Gint_type _dyn_numOrbits = 0;

/* Capacity for dynamic arrays (grows as needed in on-the-fly mode) */
static Gordinal_type _dyn_capacity = 0;

static void _ensureCapacity(Gordinal_type needed)
{
    if (needed <= _dyn_capacity) return;
    Gordinal_type newcap = _dyn_capacity ? _dyn_capacity * 2 : 1024;
    while (newcap < needed) newcap *= 2;

    _dyn_canonList = (Gint_type *)realloc(_dyn_canonList, newcap * sizeof(Gint_type));
    _dyn_canonNumEdges = (char *)realloc(_dyn_canonNumEdges, newcap * sizeof(char));
    _dyn_alphaList = (Gint_type *)realloc(_dyn_alphaList, newcap * sizeof(Gint_type));
    assert(_dyn_canonList && _dyn_canonNumEdges && _dyn_alphaList);

    /* Zero-fill new entries */
    memset(_dyn_canonList + _dyn_capacity, 0, (newcap - _dyn_capacity) * sizeof(Gint_type));
    memset(_dyn_canonNumEdges + _dyn_capacity, 0, (newcap - _dyn_capacity) * sizeof(char));
    /* Default alpha = 1 (raw counts) */
    for (Gordinal_type i = _dyn_capacity; i < newcap; i++)
        _dyn_alphaList[i] = 1;

    _dyn_capacity = newcap;
}

/* Count edges in a TINY_GRAPH */
static int _countEdges(TINY_GRAPH *tg, int k)
{
    int count = 0;
    for (int i = 0; i < k; i++)
        for (int j = i + 1; j < k; j++)
            if (TinyGraphAreConnected(tg, i, j))
                count++;
    return count;
}

/* BFS connectivity check */
static int _isConnected(TINY_GRAPH *tg, int k)
{
    if (k <= 1) return 1;
    int visited[k], queue[k];
    memset(visited, 0, sizeof(visited));
    int head = 0, tail = 0;
    queue[tail++] = 0;
    visited[0] = 1;
    int count = 1;
    while (head < tail) {
        int u = queue[head++];
        for (int v = 0; v < k; v++) {
            if (!visited[v] && TinyGraphAreConnected(tg, u, v)) {
                visited[v] = 1;
                queue[tail++] = v;
                count++;
            }
        }
    }
    return (count == k);
}

void HighK_LoadCanonList(const char *filename, int k)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "HighK: cannot open %s\n", filename);
        return;
    }

    Gordinal_type numCanon = 0;
    if (1 != fscanf(fp, "%u", (unsigned *)&numCanon) || numCanon == 0) {
        /* Try as unsigned long */
        rewind(fp);
        unsigned long tmp;
        if (1 != fscanf(fp, "%lu", &tmp) || tmp == 0) {
            fprintf(stderr, "HighK: failed to read numCanon from %s\n", filename);
            fclose(fp);
            return;
        }
        numCanon = (Gordinal_type)tmp;
    }

    _ensureCapacity(numCanon);
    _dyn_numCanon = numCanon;

    char line[4096];
    /* Consume rest of header line */
    if (fgets(line, sizeof(line), fp) == NULL) { fclose(fp); return; }

    for (Gordinal_type i = 0; i < numCanon; i++) {
        if (!fgets(line, sizeof(line), fp)) {
            fprintf(stderr, "HighK: unexpected EOF at line %lu in %s\n", (unsigned long)i, filename);
            break;
        }
        Gint_type gint = 0;
        int connected = 0;
        int num_edges = 0;

#ifdef GINT_IS_128BIT
        unsigned long tmp_gint;
        sscanf(line, "%lu\t%d %d", &tmp_gint, &connected, &num_edges);
        gint = (Gint_type)tmp_gint;
#else
        sscanf(line, GINT_FMT "\t%d %d", &gint, &connected, &num_edges);
#endif

        _dyn_canonList[i] = gint;
        _dyn_canonNumEdges[i] = (char)num_edges;

        /* Insert into hash map */
        CanonHashEntry *entry = (CanonHashEntry *)malloc(sizeof(CanonHashEntry));
        entry->canon_gint = gint;
        entry->ordinal = i;
        entry->connected = connected;
        entry->num_edges = num_edges;
        HASH_ADD(hh, _canonHash, canon_gint, sizeof(Gint_type), entry);
    }

    _nextOrdinal = numCanon;
    fclose(fp);
    fprintf(stderr, "HighK: loaded %lu canonicals from %s\n", (unsigned long)numCanon, filename);
}

void HighK_LoadOrbitMap(const char *filename, int k)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "HighK: cannot open orbit map %s (orbits will not be available)\n", filename);
        return;
    }

    Gint_type numOrbits = 0;
    if (1 != fscanf(fp, "%lu", (unsigned long *)&numOrbits) || numOrbits == 0) {
        fprintf(stderr, "HighK: failed to read numOrbits from %s\n", filename);
        fclose(fp);
        return;
    }
    _dyn_numOrbits = numOrbits;

    /* Allocate orbit arrays */
    _dyn_orbitList = (Gint_type **)malloc(_dyn_numCanon * sizeof(Gint_type *));
    _dyn_orbitCanonMapping = (Gordinal_type *)calloc(numOrbits, sizeof(Gordinal_type));
    _dyn_orbitCanonNodeMapping = (char *)malloc(numOrbits);
    memset(_dyn_orbitCanonNodeMapping, -1, numOrbits);

    for (Gordinal_type c = 0; c < _dyn_numCanon; c++) {
        _dyn_orbitList[c] = (Gint_type *)malloc(k * sizeof(Gint_type));
        for (int j = 0; j < k; j++) {
            Gint_type orb;
            if (1 != fscanf(fp, "%lu", (unsigned long *)&orb)) {
                fprintf(stderr, "HighK: failed to read orbit at canon %lu node %d\n", (unsigned long)c, j);
                fclose(fp);
                return;
            }
            _dyn_orbitList[c][j] = orb;
            _dyn_orbitCanonMapping[orb] = c;
            if (_dyn_orbitCanonNodeMapping[orb] < 0)
                _dyn_orbitCanonNodeMapping[orb] = j;
        }
    }

    fclose(fp);
    fprintf(stderr, "HighK: loaded %lu orbits from %s\n", (unsigned long)numOrbits, filename);
}

void HighK_Init(int k)
{
    _highK_k = k;
    _canonHash = NULL;
    _nextOrdinal = 0;
    _dyn_numCanon = 0;
    _dyn_numOrbits = 0;
    _dyn_capacity = 0;
    _dyn_canonList = NULL;
    _dyn_canonNumEdges = NULL;
    _dyn_orbitList = NULL;
    _dyn_orbitCanonMapping = NULL;
    _dyn_orbitCanonNodeMapping = NULL;
    _dyn_alphaList = NULL;

    char buf[BUFSIZ];

    /* Try to load canon_list file */
    sprintf(buf, "%s/%s/canon_list%d.txt", _BLANT_DIR, _CANON_DIR, k);
    FILE *fp = fopen(buf, "r");

    if (fp) {
        fclose(fp);
        _highK_mode = 1; /* hash map from canon_list */
        fprintf(stderr, "HighK: mode 1 (hash map from canon_list) for k=%d\n", k);
        HighK_LoadCanonList(buf, k);

        /* Try to load orbit map */
        sprintf(buf, "%s/%s/orbit_map%d.txt", _BLANT_DIR, _CANON_DIR, k);
        HighK_LoadOrbitMap(buf, k);
    } else {
        _highK_mode = 2; /* on-the-fly */
        fprintf(stderr, "HighK: mode 2 (on-the-fly discovery) for k=%d\n", k);
        _ensureCapacity(4096);
    }
}

Gordinal_type HighK_LookupOrdinal(Gint_type canon_gint)
{
    CanonHashEntry *entry = NULL;
    HASH_FIND(hh, _canonHash, &canon_gint, sizeof(Gint_type), entry);

    if (entry) return entry->ordinal;

    if (_highK_mode == 1) {
        /* In mode 1, all canonicals should be in the hash map */
        fprintf(stderr, "HighK warning: canonical not found in hash map (mode 1)\n");
        return (Gordinal_type)-1;
    }

    /* Mode 2: on-the-fly, assign new ordinal */
    Gordinal_type ord = _nextOrdinal++;
    _ensureCapacity(_nextOrdinal);
    _dyn_canonList[ord] = canon_gint;
    _dyn_numCanon = _nextOrdinal;

    /* Compute connectivity and edge count */
    TINY_GRAPH *tg = TinyGraphAlloc(_highK_k);
    Int2TinyGraph(tg, canon_gint);
    _dyn_canonNumEdges[ord] = (char)_countEdges(tg, _highK_k);

    entry = (CanonHashEntry *)malloc(sizeof(CanonHashEntry));
    entry->canon_gint = canon_gint;
    entry->ordinal = ord;
    entry->connected = _isConnected(tg, _highK_k);
    entry->num_edges = _dyn_canonNumEdges[ord];
    HASH_ADD(hh, _canonHash, canon_gint, sizeof(Gint_type), entry);

    TinyGraphFree(tg);
    return ord;
}

Gordinal_type HighK_ExtractPerm(unsigned char perm[], Gint_type Gint, int k)
{
    /* Build TINY_GRAPH from Gint */
    TINY_GRAPH *tg = TinyGraphAlloc(k);
    Int2TinyGraph(tg, Gint);

    /* Call nauty to get canonical form + permutation */
    Gint_type canon_gint = NautyCanonical(tg, k, perm);

    TinyGraphFree(tg);

    /* Look up ordinal */
    return HighK_LookupOrdinal(canon_gint);
}

Gordinal_type HighK_NumCanonicals(void)
{
    return _dyn_numCanon;
}

Gint_type HighK_NumOrbits(void)
{
    return _dyn_numOrbits;
}

void HighK_Cleanup(void)
{
    CanonHashEntry *cur, *tmp;
    HASH_ITER(hh, _canonHash, cur, tmp) {
        HASH_DEL(_canonHash, cur);
        free(cur);
    }
    _canonHash = NULL;

    free(_dyn_canonList);      _dyn_canonList = NULL;
    free(_dyn_canonNumEdges);  _dyn_canonNumEdges = NULL;
    free(_dyn_alphaList);      _dyn_alphaList = NULL;

    if (_dyn_orbitList) {
        for (Gordinal_type i = 0; i < _dyn_numCanon; i++)
            free(_dyn_orbitList[i]);
        free(_dyn_orbitList);
        _dyn_orbitList = NULL;
    }
    free(_dyn_orbitCanonMapping);     _dyn_orbitCanonMapping = NULL;
    free(_dyn_orbitCanonNodeMapping);  _dyn_orbitCanonNodeMapping = NULL;
}

#endif /* HIGH_K_SUPPORTED */
