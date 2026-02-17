// This software is part of github.com/waynebhayes/BLANT, and is Copyright (c) BLANT contributors 2025, under the GNU LGPL 3.0
// (GNU Lesser General Public License, version 3, 2007), a copy of which is contained at the top of the repo.
#include <sys/file.h>
#include <sys/mman.h>
#include <math.h>
#include "blant.h"
#include "blant-utils.h"
#include "blant-predict.h"
#include "bintree.h"
#include "sim_anneal.h"
#if HIGH_K_SUPPORTED
#include "blant-highk.h"
#endif

// The following is the most compact way to store the permutation between a non-canonical and its canonical representative,
// when k=8: there are 8 entries, and each entry is a integer from 0 to 7, which requires 3 bits. 8*3=24 bits total.
// For simplicity we use the same 3 bits per entry, and assume 8 entries, even for k<8.  It wastes memory for k<4, but
// makes the coding much simpler.
typedef unsigned char kperm[3]; // The 24 bits are stored in 3 unsigned chars.

// WRONG COMMENT HERE: we actually *DO* use less memory...
// Here's where we're lazy on saving memory, and we could do better.  We're going to allocate a static array
// that is big enough for the 256 million permutations from non-canonicals to canonicals for k=8, even if k<8.
// So we're allocating 256MBx3=768MB even if we need much less.  I figure anything less than 1GB isn't a big deal
// these days. It needs to be aligned to a page boundary since we're going to mmap the binary file into this array.
//static kperm Permutations[maxBk] __attribute__ ((aligned (8192)));
kperm *Permutations = NULL; // Allocating memory dynamically

static int _magicTable[MAX_CANONICALS][12]; //Number of canonicals for k=8 by number of columns in magic table

#if DYNAMIC_CANON_MAP
static int CmpInt(foint a, foint b) {
    return a.i - b.i;
}

// Surprisingly, BinTree works MUCH faster than a HASH map.
static unsigned int L_K_Func_Memory(Gint_type Gint) {
    static BINTREE *B;
    if(!B) B = BinTreeAlloc(CmpInt, NULL, NULL, NULL, NULL);
    foint f;
    if(BinTreeLookup(B,(foint)(unsigned int)Gint,&f)) {
	assert(_K && f.ui == _K[Gint]);
	return f.ui;
    }
    else {
	// For now, just cheat and use the pre-computed lookup table
	// FIXME: this is where we need to insert the search that's currently inside fast-canon-map
	assert(_K);
	BinTreeInsert(B,(foint)(unsigned int)Gint,(foint)(unsigned int)_K[Gint]);
	//Warning("BinTreeSize %d", B->n);
	assert(BinTreeLookup(B,(foint)(unsigned int)Gint,&f) && f.ui == _K[Gint]);
	return _K[Gint];
    }
}

static TINY_GRAPH _g, _h; // the current and "moved" TINY_GRAPHs for simulated annealing

foint SA_GenMove(bool generate, foint current) {
    extern double drand48();
    assert(current.v == (void*)(&_g));
    if(generate) {
	TinyGraphCopy(&_h, &_g);
	int u = _k*drand48(), v = u;
	while(u==v) v = _k*drand48();
	TinyGraphSwapNodes(&_h, u,v);
	return (foint)(void*)(&_h);
    }
    else
	return (foint)(void*)(&_g);
}

foint SA_Accept(bool accept, foint sol) {
    assert(sol.v == (void*)(&_h));
    if(accept) _g = _h;
    return (foint)(void*)(&_g);
}

double SA_Score(foint solution) {
    assert(solution.v == (void*)(&_g) || solution.v == (void*)(&_h));
    return TinyGraph2Int((TINY_GRAPH*)solution.v, _k);
}

static _tInitials[] = {0,0,0, 40, 500, 6000, 150000, 8e6, 9e8};
static _tDecays[] =   {0,0,0, 5.5, 6, 7, 9, 11, 14};

static Gint_type L_K_Func_SA(Gint_type Gint) {
    // Note the follownig is CHEATING (for now), SA doesn't work well if the sampled graphlet is already canonical
    if(_canonList[_K[Gint]] == Gint) return _K[Gint];

    static unsigned tries, fails;
    ++tries;

    TinyGraphEdgesAllDelete(&_g);
    TinyGraphEdgesAllDelete(&_h);
    _g.n = _h.n = _k;
    Int2TinyGraph(&_g, Gint);
    SIM_ANNEAL *sa = SimAnnealAlloc(-1, (foint)(void*)(&_g), SA_GenMove, SA_Score, SA_Accept, SQR(_k));
    SimAnnealSetSchedule(sa, _tInitials[_k], _tDecays[_k]);
    int best=Gint, same=0, canonInt;
    bool done=false;
    do {
	int result = SimAnnealRun(sa);
	if(result <= 0) Fatal("SimAnnealRun returned %d", result);
	foint solution = SimAnnealSol(sa);
	assert(solution.v == (void*)(&_g));
	canonInt = TinyGraph2Int(&_g, _k);
	if(canonInt < best) {
	    best = canonInt; same=0;
	} else if(canonInt==best) ++same;
    } while( best != _canonList[_K[Gint]] || // we CAN actually check this if we know the canonList but not the mapping
	same < _k);
    SimAnnealFree(sa);
    assert(_K[canonInt] == _K[Gint]);
    if(_canonList[_K[canonInt]] != canonInt)
	Warning("fail %d of %d: canonInt(%d) returned %d but is actually %d", ++fails, tries,
	    Gint, canonInt, _canonList[_K[Gint]]);
    return _K[Gint];
}

Gordinal_type L_K_Func(Gint_type Gint) {
    Gint_type m = L_K_Func_Memory(Gint);
    Gint_type s = L_K_Func_SA(Gint);
    assert(m==s);
    assert(m==_K[Gint]);
    return s;
}
#else
Gordinal_type L_K_Func(Gint_type Gint) {
#if HIGH_K_SUPPORTED
    /* HIGH_K binary: always use nauty for canonical lookup (all k).
     * The mmap'd .bin files have wrong type sizes for the HIGH_K build. */
    unsigned char perm[MAX_K];
    memset(perm, 0, sizeof(perm));
    return HighK_ExtractPerm(perm, Gint, _k);
#else
    Apology("no L_K_Func"); return -1;
#endif
}
#endif

// Assuming the global variable _k is set properly, go read in and/or mmap the big global
// arrays related to canonical mappings and permutations.
void SetGlobalCanonMaps(void)
{
    int i;
    char BUF[BUFSIZ];
    assert(3 <= _k && _k <= MAX_K);

#if HIGH_K_SUPPORTED
    {
	/* HIGH_K binary: use nauty-based canonical labeling for ALL k values.
	 * The mmap'd .bin files have incompatible type sizes (written with
	 * TINY_SET_SIZE=8, but this binary uses TINY_SET_SIZE=16), so we
	 * always use the text-based canon_list files + nauty hash map.
	 */
	HighK_Init(_k);
	_Bk = 0;
	_K = NULL;
	Permutations = NULL;

	if (_highK_mode == 1) {
	    _numCanon = HighK_NumCanonicals();
	    _numOrbits = HighK_NumOrbits();
	} else {
	    _numCanon = 0;
	    _numOrbits = 0;
	}
	_numConnectedCanon = 0;
	_numConnectedOrbits = 0;
	_connectedCanonicals = SetAlloc(MAX(_numCanon, 1));

	/* Copy dynamic arrays into static arrays for compatibility with
	 * sampling/output code that references _canonList[], _orbitList[], etc.
	 */
	if (_highK_mode == 1 && _numCanon > 0) {
	    Gordinal_type copyN = _numCanon < MAX_CANONICALS ? _numCanon : MAX_CANONICALS;
	    for (i = 0; i < (int)copyN; i++) {
		_canonList[i] = _dyn_canonList[i];
		_canonNumEdges[i] = _dyn_canonNumEdges[i];
		_alphaList[i] = _dyn_alphaList ? _dyn_alphaList[i] : 1;
		if (_dyn_canonNumEdges[i] > 0) {
		    SetAdd(_connectedCanonicals, i);
		    _numConnectedCanon++;
		}
	    }
	    /* Allocate and copy orbit data if loaded.
	     * _orbitList is a pointer in the HIGH_K build (too large for BSS
	     * at MAX_CANONICALS=12M). Only allocate up to the actual copyN.
	     */
	    if (_dyn_orbitList) {
		Gint_type copyO = _numOrbits < MAX_ORBITS ? _numOrbits : MAX_ORBITS;
		_orbitList = (Gint_type (*)[MAX_K])calloc(copyN, sizeof(Gint_type[MAX_K]));
		if (_orbitList) {
		    for (i = 0; i < (int)copyN; i++)
			for (int j = 0; j < _k && j < MAX_K; j++)
			    _orbitList[i][j] = _dyn_orbitList[i][j];
		}
		for (i = 0; i < (int)copyO; i++) {
		    _orbitCanonMapping[i] = _dyn_orbitCanonMapping[i];
		    _orbitCanonNodeMapping[i] = _dyn_orbitCanonNodeMapping[i];
		    if (SetIn(_connectedCanonicals, _orbitCanonMapping[i]))
			_connectedOrbits[_numConnectedOrbits++] = i;
		}
	    }
	}

	/* Initialize static alpha list to 1 (raw counts) to prevent
	 * division by zero in sampling code for on-the-fly mode. */
	for (i = 0; i < MAX_CANONICALS; i++)
	    _alphaList[i] = 1;

	_rawCounts = true;
	fprintf(stderr, "HighK: k=%d, numCanon=%lu, mode=%d, raw counts enabled\n",
	    _k, (unsigned long)_numCanon, _highK_mode);
	return;
    }
#else
    /* Original flat-table path (non-HIGH_K build, k<=8 only) */
#if SELF_LOOPS
    _Bk = (1U <<(_k*(_k+1)/2));
#else
    _Bk = (1U <<(_k*(_k-1)/2));
#endif
    _connectedCanonicals = canonListPopulate(BUF, _canonList, _k, _canonNumEdges);
    _numCanon = _connectedCanonicals->maxElem;
    _numConnectedCanon = SetCardinality(_connectedCanonicals);
    _numOrbits = orbitListPopulate(BUF, _orbitList, _orbitCanonMapping, _orbitCanonNodeMapping, _numCanon, _k);
    _K = (Gordinal_type*) mapCanonMap(BUF, _K, _k);
    sprintf(BUF, "%s/%s/perm_map%d.bin", _BLANT_DIR, _CANON_DIR, _k);
    int pfd = open(BUF, 0*O_RDONLY);
    if(pfd>=0) {
	Permutations = (kperm*) mmap(Permutations, sizeof(kperm)*_Bk, PROT_READ, MAP_PRIVATE, pfd, 0);
	assert(Permutations != MAP_FAILED);
    }
    _numConnectedOrbits = 0;
    for (i=0; i < _numOrbits; i++)
	if (SetIn(_connectedCanonicals, _orbitCanonMapping[i]))
	    _connectedOrbits[_numConnectedOrbits++] = i;
    if ((_outputMode & outputODV && _k == 4) || _k == 5)
	orcaOrbitMappingPopulate(BUF, _orca_orbit_mapping, _k);
#endif
}

void LoadMagicTable(void)
{
    int i,j;
    char BUF[BUFSIZ];
    sprintf(BUF, "%s/orca_jesse_blant_table/UpperToLower%d.txt", _BLANT_DIR, _k);
    FILE *fp_ord=fopen(BUF, "r");
    if(fp_ord) {
	for(i=0; i<_numCanon; i++) {
	    for(j=0; j<12 ;j++) {
		if(1!=fscanf(fp_ord, "%d", &_magicTable[i][j]))
		    Fatal("LoadMagicTable: failed to load [i][j]=[%d][%d]", i,j);
	    }
	}
	fclose(fp_ord);
	if(_displayMode == orca || _displayMode == jesse) {
	    // Load mapping from lower_ordinal to ORCA/Jesse ID into table for fast lookup
	    for (i=0; i < _numCanon; i++) _outputMapping[_magicTable[i][4]] = _magicTable[i][7];
	}
    }
}

// You provide a permutation array, we fill it with the permutation extracted from the compressed Permutation mapping.
// There is the inverse transformation, called "EncodePerm", in createBinData.c.
Gordinal_type ExtractPerm(unsigned char perm[_k], Gint_type Gint)
{
#if HIGH_K_SUPPORTED
    /* HIGH_K binary: always use nauty for canonical form + permutation.
     * The mmap'd .bin files have incompatible type sizes. */
    return HighK_ExtractPerm(perm, Gint, _k);
#else
    /* Original flat-table path for k<=8 */
    assert(Permutations);
    if(Permutations) {
	int j, i32 = 0;
	for(j=0;j<3;j++) i32 |= (Permutations[Gint][j] << j*8);
	for(j=0;j<_k;j++)
	    perm[j] = (i32 >> 3*j) & 7;
	return _K[Gint];
    } else return 0;
#endif
}

void InvertPerm(unsigned char inv[_k], const unsigned char perm[_k])
{
    int j;
    for(j=0; j<_k; j++)
	inv[(int)perm[j]]=j;
}

bool arrayIn(unsigned *arr, int size, int item) {
    int i;
    for(i=0; i<size; i++) {
        if (arr[i] == item) {
            return true;
        }
    }
    return false;
}

void printIntArray(int* arr, int n, char* name) {
    fprintf(stderr, "%s:", name);
    int i;
    for (i = 0; i < n; i++) {
        fprintf(stderr, " %d", arr[i]);
    }

    fprintf(stderr, "\n");
}

int asccompFunc(const foint i, const foint j) {return i.i > j.i ? 1 : i.i == j.i ? 0 : -1;} // ascending (smallest at top)

int descompFunc(const void *a, const void *b)
{
    const int *i = (const int*)a, *j = (const int*)b;
    return (*j)-(*i);
}

int nwhn_des_alph_comp_func(const void *a, const void *b) {
    const node_whn *nwhn_a = (const node_whn*)a;
    const node_whn *nwhn_b = (const node_whn*)b;
    int i = nwhn_a->heur, j = nwhn_b->heur;

    if (j != i) {
        return j - i;
    } else {
        return strcmp(nwhn_a->name, nwhn_b->name);
    }
}

int nwhn_des_rev_comp_func(const void *a, const void *b) {
    const node_whn *nwhn_a = (const node_whn*)a;
    const node_whn *nwhn_b = (const node_whn*)b;
    int i = nwhn_a->heur, j = nwhn_b->heur;

    if (j != i) {
        return j - i;
    } else {
        return -1 * strcmp(nwhn_a->name, nwhn_b->name);
    }
}

int nwhn_asc_alph_comp_func(const void *a, const void *b) {
    const node_whn *nwhn_a = (const node_whn*)a;
    const node_whn *nwhn_b = (const node_whn*)b;
    int i = nwhn_a->heur, j = nwhn_b->heur;

    if (j != i) {
        return i - j;
    } else {
        return strcmp(nwhn_a->name, nwhn_b->name);
    }
}

int nwhn_asc_rev_comp_func(const void *a, const void *b) {
    const node_whn *nwhn_a = (const node_whn*)a;
    const node_whn *nwhn_b = (const node_whn*)b;
    int i = nwhn_a->heur, j = nwhn_b->heur;

    if (j != i) {
        return i - j;
    } else {
        return -1 * strcmp(nwhn_a->name, nwhn_b->name);
    }
}

// Fast inline adjacency check for sparse non-complement graphs: scan the shorter neighbor list.
static inline bool _FastAreConnectedUtil(GRAPH *G, unsigned i, unsigned j)
{
    unsigned me, other, n, k;
    const unsigned *neighbors;
    if(G->degree[i] < G->degree[j]) { me = i; other = j; }
    else { me = j; other = i; }
    n = G->degree[me];
    neighbors = G->neighbor[me];
    for(k=0; k<n; k++)
	if(neighbors[k] == other)
	    return true;
    return false;
}

// Given the big graph G and a set of nodes in V, return the TINY_GRAPH created from the induced subgraph of V on G.
TINY_GRAPH *TinyGraphInducedFromGraph(TINY_GRAPH *g, GRAPH *G, unsigned *Varray)
{
    unsigned i, j;
    TinyGraphEdgesAllDelete(g);
    assert(!G->selfAllowed);
    if(G->sparse && !G->useComplement) {
	// Fast path: use direct neighbor array scan
	for(i=0; i < g->n; i++) for(j=i+1; j < g->n; j++)
	    if(_FastAreConnectedUtil(G, Varray[i], Varray[j]))
		TinyGraphConnect(g, i, j);
    } else {
	for(i=0; i < g->n; i++) for(j=i+1; j < g->n; j++)
	    if(GraphAreConnected(G, Varray[i], Varray[j]))
		TinyGraphConnect(g, i, j);
    }
    return g;
}

int orbitpair_cmp(long int a, long int b) {
    return (int) (a - b);
}


long int orbitpair_copy(long int src) {
    return src;
}

int NumOrbits(Gordinal_type ord) {
    assert(0<=ord && ord<_numCanon);
    if(ord==0 || ord==_numCanon-1) return 1; // the clique and indep set each have only 1 orbit
    else return _orbitList[ord+1][0] - _orbitList[ord][0];
}
