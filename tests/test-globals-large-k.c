/*
 * test-globals-large-k.c
 *
 * Global variable definitions for large-k test programs.
 * Compiled with -DMAX_K=10 -DTINY_SET_SIZE=16.
 * Provides definitions for all extern symbols declared in blant.h and related headers.
 *
 * Each test binary links this object to satisfy the linker.
 */

#include "blant.h"

/* Core variables */
unsigned int _k;
unsigned int _Bk;
Gordinal_type _numCanon, _numConnectedCanon;
Gordinal_type *_K = NULL;
unsigned int _min_edge_count;

/* Canon data (pointers for MAX_K > 8) */
char *_canonNumEdges = NULL;
double _totalStarMotifs;
Gint_type *_canonList = NULL;
SET *_connectedCanonicals = NULL;

/* Orbit data */
Gint_type _numOrbits;
Gint_type (*_orbitList)[MAX_K] = NULL;
Gint_type *_alphaList = NULL;
Gordinal_type *_orbitCanonMapping = NULL;
char *_orbitCanonNodeMapping = NULL;
int *_connectedOrbits = NULL;
int _numConnectedOrbits;
int _orca_orbit_mapping[58];

/* Output and display */
enum OutputMode _outputMode = undef;
enum CanonicalDisplayMode _displayMode = undefined;
enum FrequencyDisplayMode _freqDisplayMode = freq_display_mode_undef;

/* Degree vectors */
double **_graphletDegreeVector = NULL;
double **_orbitDegreeVector = NULL;
double _absoluteCountMultiplier;

/* Mapping arrays */
int *_outputMapping = NULL;
int *_canonNumStarMotifs = NULL;
double *_graphletCount = NULL;
int **_graphletDistributionTable = NULL;
double *_graphletConcentration = NULL;
unsigned long *_batchRawCount = NULL;
unsigned long _batchRawTotalSamples;

/* Threading */
int _numThreads = 1, _maxThreads = 1;
unsigned long _numSamples;
double _confidence;
Boolean _earlyAbort;
enum StopMode _stopMode = stopOnSamples;

/* Node handling */
char **_nodeNames = NULL;
char _supportNodeNames;
int *_startNodes = NULL;
int _numStartNodes;
SET *_startNodeSet = NULL;
double *_cumulativeProb = NULL;
Boolean _child, _weighted, _rawCounts;
int _quiet;

/* Component tracking */
int _numConnectedComponents;
int *_componentSize = NULL;
int *_whichComponent = NULL;
SET **_componentSet = NULL;

/* Community detection */
SET ***_communityNeighbors = NULL;
char _communityMode;

/* Known canonical counts (k=0 to k=12) */
unsigned long _known_canonical_count[] =
    {0, 1, 2, 4, 11, 34, 156, 1044, 12346, 274668, 12005168, 1018997864, 165091172592UL};

/* Window variables (from blant-window.h) */
int _windowSampleMethod;
int _windowRep_limit_method;
HEAP *_windowRep_limit_heap = NULL;
int _windowIterationMethod;
unsigned **_windowReps = NULL;
int _MAXnumWindowRep;
int _numWindowRep;
int _numWindowRepLimit;
int _numWindowRepArrSize;
int _topThousandth;
int _orbitNumber;
char *_odvFile = NULL;
bool _alphabeticTieBreaking;
int _windowSize;
Boolean _window;
SET *_windowRep_allowed_ambig_set = NULL;
int _windowRep_min_num_edge;
float *_graphNodeImportance = NULL;
Boolean _supportNodeImportance;
Boolean _windowRep_limit_neglect_trivial;

/* Pthreads accumulator (from blant-pthreads.h) */
Accumulators _trashAccumulator;

/* Permutation stub (kperm is 5 bytes for TINY_SET_SIZE=16) */
typedef unsigned char kperm[5];
kperm *Permutations = NULL;
