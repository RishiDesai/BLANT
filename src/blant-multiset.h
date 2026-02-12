/* blant-multiset.h -- Array-backed multiset for BLANT
 * Replaces libwayne's multisets.h/multisets.c
 */
#ifndef BLANT_MULTISET_H
#define BLANT_MULTISET_H

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "blant-fatal.h"
#include "blant-bitset.h"

typedef unsigned char FREQTYPE;
#define MAX_MULTISET_FREQ ((1U << (8 * sizeof(FREQTYPE))) - 1)

typedef struct _multisetType {
    unsigned int n;          /* array size (max element + 1) */
    unsigned int support;    /* number of distinct elements */
    FREQTYPE *array;         /* frequency of each element */
    SET *set;                /* set version */
} MULTISET;

static inline MULTISET *MultisetAlloc(unsigned n) {
    MULTISET *m = (MULTISET*)calloc(1, sizeof(MULTISET));
    if (!m) Fatal("MultisetAlloc: out of memory");
    m->n = n;
    m->support = 0;
    m->array = (FREQTYPE*)calloc(n, sizeof(FREQTYPE));
    if (!m->array) Fatal("MultisetAlloc: out of memory");
    m->set = SetAlloc(n);
    return m;
}

static inline MULTISET *MultisetResize(MULTISET *mset, unsigned new_n) {
    if (new_n > mset->n) {
        mset->array = (FREQTYPE*)realloc(mset->array, new_n * sizeof(FREQTYPE));
        memset(mset->array + mset->n, 0, (new_n - mset->n) * sizeof(FREQTYPE));
        SetResize(mset->set, new_n);
    }
    mset->n = new_n;
    return mset;
}

static inline void MultisetFree(MULTISET *mset) {
    if (mset) {
        free(mset->array);
        SetFree(mset->set);
        free(mset);
    }
}

static inline MULTISET *MultisetEmpty(MULTISET *mset) {
    memset(mset->array, 0, mset->n * sizeof(FREQTYPE));
    SetEmpty(mset->set);
    mset->support = 0;
    return mset;
}

static inline unsigned MultisetSupport(MULTISET *mset) { return mset->support; }
static inline FREQTYPE MultisetMultiplicity(MULTISET *mset, unsigned e) { return mset->array[e]; }

static inline MULTISET *MultisetAdd(MULTISET *mset, unsigned e) {
    assert(e < mset->n);
    if (mset->array[e] == 0) {
        mset->support++;
        SetAdd(mset->set, e);
    }
    assert(mset->array[e] < MAX_MULTISET_FREQ);
    mset->array[e]++;
    return mset;
}

static inline MULTISET *MultisetDelete(MULTISET *mset, unsigned e) {
    assert(e < mset->n && mset->array[e] > 0);
    mset->array[e]--;
    if (mset->array[e] == 0) {
        mset->support--;
        SetDelete(mset->set, e);
    }
    return mset;
}

#define MultisetReset MultisetEmpty

#endif /* BLANT_MULTISET_H */
