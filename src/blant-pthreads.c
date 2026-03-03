// This software is part of github.com/waynebhayes/BLANT, and is Copyright (c) BLANT contributors 2025, under the GNU LGPL 3.0
// (GNU Lesser General Public License, version 3, 2007), a copy of which is contained at the top of the repo.
#include "blant.h"
#include "blant-output.h"
#include "blant-sampling.h"
#include "blant-pthreads.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "blant-fatal.h"
#include "blant-utils-base.h"
#include <pthread.h>
#include <errno.h>
#include "atomic_utils.h"
atomic_u64_t nextIndex CACHE_ALIGNED = 0; // single definition

static double *AllocAlignedZeroedDoubles(size_t count)
{
    size_t bytes = count * sizeof(double);
    if (bytes % CACHE_LINE_SIZE != 0) {
        bytes = (bytes / CACHE_LINE_SIZE + 1) * CACHE_LINE_SIZE;
    }
    double *ptr = aligned_alloc(CACHE_LINE_SIZE, bytes);
    if (!ptr) return NULL;
    memset(ptr, 0, bytes);
    return ptr;
}

Accumulators* InitializeAccumulatorStruct(GRAPH* G) {

    // Ensure size is a multiple of alignment
    size_t size = sizeof(Accumulators);
    size_t aligned_size = ((size + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE) * CACHE_LINE_SIZE;
    
    Accumulators *accums = aligned_alloc(CACHE_LINE_SIZE, aligned_size);

    if (!accums) {
        perror("Failed to allocate memory for Accumulators");
        exit(EXIT_FAILURE);
    }

    memset(accums, 0, aligned_size); // zero out everything including padding
    accums->numCanon = (size_t)_numCanon;
    accums->numOrbits = (size_t)_numOrbits;
    accums->graphNumNodes = (size_t)G->n;
    accums->canonicalTrackingEnabled = 1;

    if (accums->numOrbits > MAX_ORBITS) {
        Fatal("Internal error: numOrbits=%zu exceeds MAX_ORBITS=%d", accums->numOrbits, MAX_ORBITS);
    }

    if (accums->numCanon > 0) {
        accums->graphletCount = (double *)calloc(accums->numCanon, sizeof(double));
        accums->graphletConcentration = (double *)calloc(accums->numCanon, sizeof(double));
        accums->canonNumStarMotifs = (double *)calloc(accums->numCanon, sizeof(double));
        accums->batchRawCount = (unsigned long *)calloc(accums->numCanon, sizeof(unsigned long));
        if (!accums->graphletCount || !accums->graphletConcentration || !accums->canonNumStarMotifs || !accums->batchRawCount) {
            Fatal("Failed to allocate Accumulators canonical arrays for %zu canonicals", accums->numCanon);
        }
    }

    // Initialize batch counters
    accums->batchRawTotalSamples = 0;

    // initialize GDV vectors if needed
    if(_outputMode & outputGDV || (_outputMode & communityDetection && _communityMode=='g')) {
        if (accums->numCanon > 0) {
            accums->graphletDegreeVector = malloc(accums->numCanon * sizeof(double*));
            if (!accums->graphletDegreeVector) Fatal("Failed to allocate GDV memory");
            for(size_t i = 0; i < accums->numCanon; i++) {
                accums->graphletDegreeVector[i] = AllocAlignedZeroedDoubles((size_t)G->n);
                if (!accums->graphletDegreeVector[i]) {
                    perror("Failed to allocate memory for GDV vector");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    // initialize ODV vectors if needed
    if(_outputMode & outputODV || (_outputMode & communityDetection && _communityMode=='o')) {
        for(size_t i = 0; i < accums->numOrbits; i++) {
            size_t bytes = G->n * sizeof(double);
            // Ensure that size is a multiple of CACHE_LINE_SIZE
            if (bytes % CACHE_LINE_SIZE != 0) {
                bytes = (bytes / CACHE_LINE_SIZE + 1) * CACHE_LINE_SIZE;
            }
            accums->orbitDegreeVector[i] = aligned_alloc(CACHE_LINE_SIZE, bytes);
            if (!accums->orbitDegreeVector[i]) {
                perror("Failed to allocate memory for ODV vector");
                exit(EXIT_FAILURE);
            }
            memset(accums->orbitDegreeVector[i], 0, bytes);
        }
    }

    // initialize communityNeighbors if needed
    if(_outputMode & communityDetection) accums->communityNeighbors = (SET***) calloc(G->n, sizeof(SET**));

    for (size_t i = 0; i < accums->numCanon; i++) accums->canonNumStarMotifs[i] = -1;

    return accums;
}

int EnsureAccumulatorCanonCapacity(Accumulators *accums, size_t minCanon)
{
    if (!accums || !accums->canonicalTrackingEnabled) return 0;
    if (minCanon <= accums->numCanon) return 1;

#if HIGH_K_SUPPORTED
    if (_highK_mode != 2) {
        Fatal("Accumulator canonical index out of bounds: need=%zu have=%zu (highK_mode=%d)",
              minCanon, accums->numCanon, _highK_mode);
    }
    if ((_outputMode & communityDetection) && _communityMode == 'g') {
        Fatal("Dynamic canonical growth is not supported with communityDetection graph mode in HIGH_K on-the-fly mode");
    }
#endif

    if (minCanon > (size_t)MAX_CANONICALS) {
        Fatal("Requested canonical capacity %zu exceeds MAX_CANONICALS=%d",
              minCanon, MAX_CANONICALS);
    }

    size_t oldCanon = accums->numCanon;
    size_t newCanon = oldCanon ? oldCanon : 1024;
    while (newCanon < minCanon) {
        newCanon *= 2;
        if (newCanon > (size_t)MAX_CANONICALS) {
            newCanon = (size_t)MAX_CANONICALS;
            break;
        }
    }
    if (newCanon < minCanon) {
        Fatal("Could not grow canonical capacity to %zu (capped at %zu)", minCanon, newCanon);
    }

    double *newGraphletCount = (double *)realloc(accums->graphletCount, newCanon * sizeof(double));
    double *newGraphletConcentration = (double *)realloc(accums->graphletConcentration, newCanon * sizeof(double));
    double *newCanonNumStarMotifs = (double *)realloc(accums->canonNumStarMotifs, newCanon * sizeof(double));
    unsigned long *newBatchRawCount = (unsigned long *)realloc(accums->batchRawCount, newCanon * sizeof(unsigned long));
    if (!newGraphletCount || !newGraphletConcentration || !newCanonNumStarMotifs || !newBatchRawCount) {
        Fatal("Failed to grow canonical accumulator arrays from %zu to %zu", oldCanon, newCanon);
    }

    accums->graphletCount = newGraphletCount;
    accums->graphletConcentration = newGraphletConcentration;
    accums->canonNumStarMotifs = newCanonNumStarMotifs;
    accums->batchRawCount = newBatchRawCount;

    memset(accums->graphletCount + oldCanon, 0, (newCanon - oldCanon) * sizeof(double));
    memset(accums->graphletConcentration + oldCanon, 0, (newCanon - oldCanon) * sizeof(double));
    memset(accums->batchRawCount + oldCanon, 0, (newCanon - oldCanon) * sizeof(unsigned long));
    for (size_t i = oldCanon; i < newCanon; i++) accums->canonNumStarMotifs[i] = -1;

    if (_outputMode & outputGDV || (_outputMode & communityDetection && _communityMode=='g')) {
        double **newGDV = (double **)realloc(accums->graphletDegreeVector, newCanon * sizeof(double*));
        if (!newGDV) Fatal("Failed to grow GDV pointer array from %zu to %zu", oldCanon, newCanon);
        accums->graphletDegreeVector = newGDV;
        for (size_t i = oldCanon; i < newCanon; i++) {
            accums->graphletDegreeVector[i] = AllocAlignedZeroedDoubles(accums->graphNumNodes);
            if (!accums->graphletDegreeVector[i]) {
                Fatal("Failed to allocate GDV row %zu while growing to %zu canonicals", i, newCanon);
            }
        }
    }

    accums->numCanon = newCanon;
    return 1;
}

void FreeAccumulatorStruct(Accumulators *accums) {
    if((_outputMode & outputGDV || (_outputMode & communityDetection && _communityMode=='g')) && accums->graphletDegreeVector)
        for (size_t i = 0; i < accums->numCanon; i++) free(accums->graphletDegreeVector[i]);
    free(accums->graphletDegreeVector);
    if(_outputMode & outputODV || (_outputMode & communityDetection && _communityMode=='o'))
        for(size_t i = 0; i < accums->numOrbits; i++) if (accums->orbitDegreeVector[i] != NULL) free(accums->orbitDegreeVector[i]);
    if(_outputMode & communityDetection) free(accums->communityNeighbors);
    free(accums->graphletCount);
    free(accums->graphletConcentration);
    free(accums->canonNumStarMotifs);
    free(accums->batchRawCount);
    free(accums);
}

void SampleNGraphletsInThreads(int seed, int k, GRAPH *G, int varraySize, int numSamples, int numThreads) {
    // Reset the global “next” counter before launching workers
    ATOMIC_STORE_U64(&nextIndex, 0);

    if (numThreads < 1) numThreads = 1;
    if (numSamples < 0) numSamples = 0;

    pthread_t threads[numThreads];
    ThreadData threadData[numThreads];


    // Choose a batch size
    int batchSize = G->numEdges * sqrt(G->n) * sqrt(numThreads);
    if (batchSize <= 0) batchSize = 1;

    if (numSamples > 0 && batchSize > numSamples) batchSize = numSamples;

    // make sure no single thread claims the entire workload when numSamples is small
    int fairShare = numSamples > 0 ? (numSamples + numThreads - 1) / numThreads : 1;
    if (batchSize > fairShare) batchSize = fairShare;
    if (batchSize <= 0) batchSize = 1;

    int totalBatches = batchSize > 0 ? (numSamples + batchSize - 1) / batchSize : 1; // Ceiling division to cover all samples
    if (totalBatches <= 0) totalBatches = 1;

    // seed the threads with a base seed that may or may not be specified
    long base_seed = seed == -1 ? GetFancySeed(false) : seed;

    // initialize the threads and their data
    for (int t = 0; t < numThreads; t++)
    {
        threadData[t].k = k;
        threadData[t].G = G;
        threadData[t].varraySize = varraySize;
        threadData[t].threadId = t;
        threadData[t].seed = base_seed + t;
        threadData[t].accums = NULL; // Will be initialized in the thread

        // batching params consumed by RunBlantInThread
        threadData[t].batchSize    = batchSize;
        threadData[t].totalSamples = (unsigned long)numSamples;
        threadData[t].totalBatches = totalBatches;

        // create thread with error handling
        if (pthread_create(&threads[t], NULL, RunBlantInThread, &threadData[t]) != 0) {
            Fatal("Failed to create thread");
        }
    }

    // wait for each thread to finish execution, then accumulate data from the thread into the passed accumulator
    for (unsigned t = 0; t < numThreads; t++) {
        pthread_join(threads[t], NULL);
        if (threadData[t].accums->numCanon != (size_t)_numCanon) {
            Fatal("Internal error: thread accumulator canonical size mismatch: got %zu expected %zu",
                  threadData[t].accums->numCanon, (size_t)_numCanon);
        }
        if (threadData[t].accums->numOrbits != (size_t)_numOrbits) {
            Fatal("Internal error: thread accumulator orbit size mismatch: got %zu expected %zu",
                  threadData[t].accums->numOrbits, (size_t)_numOrbits);
        }
        for (size_t i = 0; i < threadData[t].accums->numCanon; i++) {
            _graphletConcentration[i] += threadData[t].accums->graphletConcentration[i];
            _graphletCount[i] += threadData[t].accums->graphletCount[i];
            if (_canonNumStarMotifs[i] == -1) _canonNumStarMotifs[i] = threadData[t].accums->canonNumStarMotifs[i];
            // Accumulate batch counters
            _batchRawCount[i] += threadData[t].accums->batchRawCount[i];
        }
        // Accumulate total batch samples
        _batchRawTotalSamples += threadData[t].accums->batchRawTotalSamples;

        if (_outputMode & outputODV || (_outputMode & communityDetection && _communityMode=='o')) {
            for(size_t i = 0; i < threadData[t].accums->numOrbits; i++) {
            for(int j=0; j<G->n; j++) {
                _orbitDegreeVector[i][j] += threadData[t].accums->orbitDegreeVector[i][j];
            }
            }
        }
        if (_outputMode & outputGDV || (_outputMode & communityDetection && _communityMode=='g')) {
            for(size_t i = 0; i < threadData[t].accums->numCanon; i++) {
            for(int j=0; j<G->n; j++) {
                _graphletDegreeVector[i][j] += threadData[t].accums->graphletDegreeVector[i][j];
            }
            }
        }
        if (_outputMode & communityDetection) {
            int numCommunities = (_communityMode=='o') ? _numOrbits : _numCanon;
            for(int i=0; i<G->n; i++) {
                if(threadData[t].accums->communityNeighbors[i]) {
                if(!_communityNeighbors[i]) {
                    _communityNeighbors[i] = (SET**) calloc(numCommunities, sizeof(SET*));
                }
                for(int j=0; j<numCommunities; j++) {
                    if(threadData[t].accums->communityNeighbors[i][j]) {
                    if(!_communityNeighbors[i][j]) {
                        _communityNeighbors[i][j] = SetAlloc(G->n);
                    }
                    _communityNeighbors[i][j] = SetUnion(_communityNeighbors[i][j], _communityNeighbors[i][j], threadData[t].accums->communityNeighbors[i][j]);
                    }
                }
                }
            }
        }
    }

    // In each threadData[t] accumulator, the GDV and ODV vectors are allocated with Ocalloc, and must be freed with Ofree
    // However, if anything else has been allocated with Ocalloc or Omalloc with libwayne BETWEEN the time this function starts and ends
    // same goodbye to that memory and say hello to segfault -Ethan
    for (unsigned t = 0; t < numThreads; t++) {
        FreeAccumulatorStruct(threadData[t].accums);
    }
}
