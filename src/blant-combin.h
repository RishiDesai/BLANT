/* blant-combin.h -- Combinatorics utilities for BLANT
 * Replaces libwayne's combin.h/combin.c
 */
#ifndef BLANT_COMBIN_H
#define BLANT_COMBIN_H

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include "blant-fatal.h"
#include "blant-utils-base.h"

typedef struct _combin {
    unsigned n, m, *array;
} COMBIN;

static inline unsigned long long CombinChoose(int n, int m) {
    if (m < 0 || m > n) return 0;
    if (m == 0 || m == n) return 1;
    if (m > n - m) m = n - m;
    unsigned long long result = 1;
    for (int i = 0; i < m; i++) {
        result = result * (n - i) / (i + 1);
    }
    return result;
}

static inline double CombinChooseDouble(int n, int m) {
    if (m < 0 || m > n) return 0;
    if (m == 0 || m == n) return 1;
    if (m > n - m) m = n - m;
    double result = 1;
    for (int i = 0; i < m; i++) {
        result = result * (double)(n - i) / (double)(i + 1);
    }
    return result;
}

static inline COMBIN *CombinZeroth(int n, int m, unsigned *array) {
    COMBIN *c = (COMBIN*)malloc(sizeof(COMBIN));
    if (!c) Fatal("CombinZeroth: out of memory");
    c->n = n;
    c->m = m;
    c->array = array;
    for (int i = 0; i < m; i++) array[i] = i;
    return c;
}

static inline bool CombinNext(COMBIN *c) {
    int i = c->m - 1;
    c->array[i]++;
    while (i > 0 && c->array[i] >= c->n - c->m + 1 + (unsigned)i) {
        i--;
        c->array[i]++;
    }
    if (c->array[0] > c->n - c->m) return false;
    for (i = i + 1; i < (int)c->m; i++)
        c->array[i] = c->array[i - 1] + 1;
    return true;
}

static inline COMBIN *CombinIth(int N, int M, unsigned *A, unsigned long long I) {
    COMBIN *c = CombinZeroth(N, M, A);
    /* skip first I combinations */
    for (unsigned long long s = 0; s < I; s++)
        if (!CombinNext(c)) break;
    return c;
}

static inline bool CombinSkipN(COMBIN *c, int n) {
    for (int i = 0; i < n; i++)
        if (!CombinNext(c)) return false;
    return true;
}

static inline bool CombinAssign(COMBIN *c, unsigned newCombin[]) {
    for (unsigned i = 0; i < c->m; i++) c->array[i] = newCombin[i];
    return true;
}

#define CombinFree(c) free(c)

/* All permutations of n elements. Calls fcn for each permutation. */
static inline bool _CombinPermHelper(int n, int start, int *array, bool (*fcn)(int, int*)) {
    if (start == n) return fcn(n, array);
    for (int i = start; i < n; i++) {
        int tmp = array[start]; array[start] = array[i]; array[i] = tmp;
        if (_CombinPermHelper(n, start + 1, array, fcn)) return true;
        tmp = array[start]; array[start] = array[i]; array[i] = tmp;
    }
    return false;
}

static inline bool CombinAllPermutations(int n, int array[], bool (*fcn)(int, int*)) {
    return _CombinPermHelper(n, 0, array, fcn);
}

#endif /* BLANT_COMBIN_H */
