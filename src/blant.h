// This software is part of github.com/waynebhayes/BLANT, and is Copyright (c) BLANT contributors 2025, under the GNU LGPL 3.0
// (GNU Lesser General Public License, version 3, 2007), a copy of which is contained at the top of the repo.
#ifndef BLANT_H
#define BLANT_H

#include <stdbool.h>
#include "blant-fundamentals.h" // defining k related constants, including SELF_LOOPS
#include "intSizes.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "blant-fatal.h"
#include "blant-utils-base.h"
#include "blant-pthreads.h"
#include "sets.h"
#include "tinygraph.h"
#include "blant-window.h"
#include "graph.h"
// #include "mem-debug.h" // need this if you use ENABLE_MEMORY_TRACKING() at the top of main().

void RandomSeed(long seed);
double RandomUniform(void);

#define USE_INSERTION_SORT 0
#define GEN_SYN_GRAPH 0

#define MAX_POSSIBLE_THREADS 64 // set this to something reasonable on your machine (eg odin.ics.uci.edu has 64 cores)
extern int _numThreads, _maxThreads;
extern unsigned long _numSamples;
extern double _confidence; // for confidence intervals
extern bool _earlyAbort;  // Can be set true by anybody anywhere, and they're responsible for producing a warning as to why

#define mcmc_d 2 // arbitrary d graphlet size < k for MCMC algorithm. Should always be 2 or k-1

extern unsigned long _known_canonical_count[]; //known number of canonicals for k=0 to 12 inclusive (13 entries)
	//{0, 1, 2, 4, 11, 34, 156, 1044, 12346, 274668, 12005168, 1018997864, 165091172592};
	//  k=1  2  3   4   5   6     7     8      9        10         11          12

// Type selection for Gint_type (adjacency matrix integer) and Gordinal_type (canonical ordinal).
// k=8:  28 adjacency bits -> 32-bit unsigned
// k=9:  36 bits -> 64-bit unsigned long
// k=10: 45 bits -> 64-bit unsigned long
// k=11: 55 bits -> 64-bit unsigned long
// k=12: 66 bits -> 128-bit __uint128_t
// k=14: 91 bits -> 128-bit __uint128_t
// k=16: 120 bits -> 128-bit __uint128_t
#if TINY_SET_SIZE == 16
  #if MAX_K >= 12
    // k>=12 needs more than 64 bits for the adjacency encoding
    #ifdef __SIZEOF_INT128__
      typedef unsigned __int128 Gint_type;
      typedef unsigned long Gordinal_type;  // ordinals fit in 64 bits (on-the-fly uses hash maps)
      // No printf format for __int128; use custom print functions
      #define GINT_FMT "%lu"  // only used for k<=11 where Gint fits in 64 bits
      #define GORDINAL_FMT "%lu"
      #define GINT_IS_128BIT 1
      #define MAX_BINTREE_K 16
    #else
      #error "MAX_K >= 12 requires __int128 support (GCC/Clang on 64-bit)"
    #endif
  #elif long_width >= 55
    typedef unsigned long Gint_type; // for k=11, need 55 bits, 64-bit long is sufficient
    #define GINT_FMT "%lu"
    #if int_width >= 30
      typedef unsigned Gordinal_type; // max numCanonicals for k=11 = 1018997864 < 2^30
      #define GORDINAL_FMT "%u"
    #else
      typedef unsigned long Gordinal_type;
      #define GORDINAL_FMT "%lu"
    #endif
    #define MAX_BINTREE_K 11
  #else
    #error "cannot do TINY_SET_SIZE 16 due to no integers being wide enough"
  #endif
#elif TINY_SET_SIZE == 8
  #if int_width >= 28 && short_width >= 16
    typedef unsigned Gint_type; // at k=8, max lookup index is 2^28, so we need 32 bits
    #define GINT_FMT "%u"
    typedef unsigned short Gordinal_type; // max numCanonicals is 12348 < 2^16
    #define GORDINAL_FMT "%hu"
    #define MAX_BINTREE_K 8
  #else
    #error "cannot do TINY_SET_SIZE 8 due to no integers long enough"
  #endif
#else
    #error "unknown TINY_SET_SIZE"
#endif

#ifdef GINT_IS_128BIT
static inline void Gint_print(FILE *fp, Gint_type v) {
    if (v <= (Gint_type)0xFFFFFFFFFFFFFFFFULL) fprintf(fp, "%lu", (unsigned long)v);
    else {
        unsigned long hi = (unsigned long)(v >> 64);
        unsigned long lo = (unsigned long)v;
        fprintf(fp, "%lu%019lu", hi, lo);
    }
}
#else
static inline void Gint_print(FILE *fp, Gint_type v) { fprintf(fp, GINT_FMT, v); }
#endif

// High-k on-the-fly canonical labeling support
#if HIGH_K_SUPPORTED
  // Use nauty-based canonical labeling for k>8
  Gordinal_type HighK_ExtractPerm(unsigned char perm[], Gint_type Gint, int k);
  void HighK_SetGlobalCanonMaps(int k);
  Gordinal_type HighK_LookupOrdinal(Gint_type canon_gint);
  extern int _highK_mode; // 0 = flat table, 1 = hash+canon_list, 2 = on-the-fly
#endif

extern Gordinal_type _numCanon, _numConnectedCanon;
extern char *_dyn_canonNumEdges;  // dynamically allocated for k>8
extern char _canonNumEdges[MAX_CANONICALS]; // static array for k<=8
extern double _totalStarMotifs;
extern Gint_type *_dyn_canonList; // dynamically allocated for k>8
extern Gint_type _canonList[MAX_CANONICALS]; // static array for k<=8

void SetBlantDirs(void);
double GetCPUseconds(void);
void Int2TinyGraph(TINY_GRAPH* G, Gint_type Gint);
Gint_type TinyGraph2Int(TINY_GRAPH *g, int numNodes);
Gordinal_type * mapCanonMap(char* BUF, Gordinal_type *K, int k);
SET *canonListPopulate(char *BUF, Gint_type *canon_list, int k, char *canon_num_edges); // returns a SET containing list of connected ordinals
Gint_type orbitListPopulate(char *BUF, Gint_type orbit_list[MAX_CANONICALS][MAX_K],
    Gordinal_type orbit_canon_mapping[MAX_ORBITS], char orbit_canon_node_mapping[MAX_ORBITS], Gordinal_type numCanon, int k);
void orcaOrbitMappingPopulate(char *BUF, int orca_orbit_mapping[58], int k);
char** convertToEL(char* file); // from convert.cpp

#define DEFAULT_BLANT_DIR "."
#define DEFAULT_CANON_DIR "canon_maps"
extern const char* _BLANT_DIR, *_CANON_DIR;

extern double *_graphletDegreeVector[MAX_CANONICALS];
extern double *_orbitDegreeVector[MAX_ORBITS], _absoluteCountMultiplier;

// If you're squeemish then use this one to access the degrees:
// THESE NEED TO BE WHOLLY DEPRECATED TO WORK SAFELY WITH MULTITHREADING.
// UTILIZING THESE ACCESSES THE GLOBAL VARIABLES, NOT THE THREAD LOCAL ACCUMULATOR VALUES
// THUS IF IN THE SAMPLING PROCESS YOU CALL THESE (AS IT DOES IN ProcessNodeGraphletNeighbors, WHICH MUST BE FIXED))
// INACCURATE RESULTS WILL BE RETURNED
#define ODV(node,orbit)          _orbitDegreeVector[orbit][node]
#define GDV(node,graphlet) _graphletDegreeVector[graphlet][node]

// Enable the code that uses C++ to parse input files?
#define SHAWN_AND_ZICAN 0
extern unsigned int _Bk;

extern Gint_type _numOrbits, _alphaList[MAX_CANONICALS];
#if HIGH_K_SUPPORTED
extern Gint_type (*_orbitList)[MAX_K]; // dynamically allocated to avoid huge BSS
#else
extern Gint_type _orbitList[MAX_CANONICALS][MAX_K];
#endif

extern char **_nodeNames, _supportNodeNames;
extern unsigned int _k, _min_edge_count;
extern Gordinal_type *_K; // works because max numCanonicals = 12348 < 2^16, but will need to be > 16 bits for k>8.
extern Gordinal_type L_K_Func(Gint_type Gint);
#if HIGH_K_SUPPORTED
  // For high-k, L_K dispatches to nauty-based lookup when _K is NULL (k>8)
  #define L_K(Gint) (_K ? _K[Gint] : L_K_Func(Gint))
#elif DYNAMIC_CANON_MAP
  #define L_K(Gint) L_K_Func(Gint)
#else
  #define L_K(Gint) (_K ? _K[Gint] : L_K_Func(Gint))
#endif
extern SET *_connectedCanonicals;

// When in community mode: for each node, and for each orbit it is in, a set of its neigbors when it apppears at that orbit
extern SET ***_communityNeighbors;
extern char _communityMode; // 'g' for graphlet or 'o' for orbit; default ???

// Use POWERS OF 2 so we can combine them.
enum OutputMode {undef=0, indexGraphlets=1, indexGraphletsRNO=2, indexOrbits=4, indexMotifs=8, communityDetection=16,
    indexMotifOrbits=32, predict=64, predict_merge=128, graphletFrequency=256, outputODV=512, outputGDV=1024,
    graphletDistribution=2048 // used in Windowing
};
extern enum OutputMode _outputMode; // note they can be LOGINAL OR'd together; modes can overlap!
extern int _outputMapping[MAX_CANONICALS], _canonNumStarMotifs[MAX_CANONICALS];

extern double _graphletCount[MAX_CANONICALS];
extern int **_graphletDistributionTable;
extern double _graphletConcentration[MAX_CANONICALS];
extern unsigned long _batchRawCount[MAX_CANONICALS], _batchRawTotalSamples; // batches for confidence intervals
enum PrecisionMode {PrecUndef, mean, worst};
enum PrecisionWeights {PrecWtNone, PrecWtRaw, PrecWtLog};

enum CanonicalDisplayMode {undefined, ordinal, decimal, binary, orca, jesse, noncanonical};
extern enum CanonicalDisplayMode _displayMode;
extern Gordinal_type _orbitCanonMapping[MAX_ORBITS]; // Maps orbits to canonical (ordinal value, including disconnected graphlets)
extern char _orbitCanonNodeMapping[MAX_ORBITS]; // Maps orbits to canonical nodes

enum FrequencyDisplayMode {freq_display_mode_undef, freq_display_mode_count, freq_display_mode_concentration, freq_display_mode_estimate_absolute};
extern enum FrequencyDisplayMode _freqDisplayMode;

extern int _orca_orbit_mapping[58]; // Mapping from orbit indices in orca_ordering to our orbits
extern int _connectedOrbits[MAX_ORBITS];
extern int _numConnectedOrbits;

extern int _numConnectedComponents;
extern int *_componentSize; // number of nodes in each CC
extern int *_whichComponent; // will be an array of size G->n specifying which CC each node is in.
extern SET **_componentSet;

// from -i "nodes of interest" file, the list of nodes to start all graphlet samples, or the identity mapping if not
extern int *_startNodes, _numStartNodes;
extern SET *_startNodeSet;

extern double *_cumulativeProb;
extern bool _child, _weighted;
extern bool _rawCounts;
extern int _quiet; // suppress notes and/or warnings, higher value = more quiet

#define SPARSE true // do not try false at the moment, it's broken

bool NodeSetSeenRecently(GRAPH *G, unsigned Varray[], int k);

// keeps track of if the stop mode is specified with number of samples (-n) or precision (-p)
enum StopMode {stopOnSamples, stopOnPrecision};
extern enum StopMode _stopMode;

void* RunBlantInThread(void* arg);

#endif
