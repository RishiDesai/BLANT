/* blant-heap.h -- Binary min-heap for BLANT
 * Replaces libwayne's heap.h/heap.c
 */
#ifndef BLANT_HEAP_H
#define BLANT_HEAP_H

#include <stdlib.h>
#include <assert.h>
#include "blant-fatal.h"
#include "blant-graph.h" /* for foint, pCmpFcn */

typedef int (*pCmpFcn)(foint, foint);
typedef void (*pFointFreeFcn)(foint);
typedef void (*HEAP_PRINT_FCN)(foint);

typedef struct _heaptype {
    int HEAPSIZE;
    pCmpFcn HeapCmp;
    pFointFreeFcn FointFree;
    foint *heap;
    HEAP_PRINT_FCN PrintElement;
} HEAP;

static inline HEAP *HeapAlloc(int maxNumber, pCmpFcn cmp, HEAP_PRINT_FCN printFcn) {
    HEAP *h = (HEAP*)calloc(1, sizeof(HEAP));
    if (!h) Fatal("HeapAlloc: out of memory");
    h->HeapCmp = cmp;
    h->HEAPSIZE = 0;
    h->heap = (foint*)malloc((maxNumber + 1) * sizeof(foint));
    if (!h->heap) Fatal("HeapAlloc: out of memory");
    h->PrintElement = printFcn;
    return h;
}

static inline void HeapFree(HEAP *h) {
    if (h) { free(h->heap); free(h); }
}

static inline void HeapReset(HEAP *h) { h->HEAPSIZE = 0; }
static inline int HeapSize(HEAP *h) { return h->HEAPSIZE; }

static inline foint HeapPeek(HEAP *h) {
    assert(h->HEAPSIZE > 0);
    return h->heap[0];
}

static inline void _HeapBubbleUp(HEAP *h, int i) {
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->HeapCmp(h->heap[i], h->heap[parent]) < 0) {
            foint tmp = h->heap[i]; h->heap[i] = h->heap[parent]; h->heap[parent] = tmp;
            i = parent;
        } else break;
    }
}

static inline void _HeapBubbleDown(HEAP *h, int i) {
    int n = h->HEAPSIZE;
    for (;;) {
        int smallest = i, left = 2*i+1, right = 2*i+2;
        if (left < n && h->HeapCmp(h->heap[left], h->heap[smallest]) < 0) smallest = left;
        if (right < n && h->HeapCmp(h->heap[right], h->heap[smallest]) < 0) smallest = right;
        if (smallest == i) break;
        foint tmp = h->heap[i]; h->heap[i] = h->heap[smallest]; h->heap[smallest] = tmp;
        i = smallest;
    }
}

static inline foint HeapInsert(HEAP *h, foint val) {
    h->heap[h->HEAPSIZE] = val;
    _HeapBubbleUp(h, h->HEAPSIZE);
    h->HEAPSIZE++;
    return val;
}

static inline foint HeapNext(HEAP *h) {
    assert(h->HEAPSIZE > 0);
    foint val = h->heap[0];
    h->HEAPSIZE--;
    if (h->HEAPSIZE > 0) {
        h->heap[0] = h->heap[h->HEAPSIZE];
        _HeapBubbleDown(h, 0);
    }
    return val;
}

static inline foint HeapDelete(HEAP *h, foint val) {
    int i;
    for (i = 0; i < h->HEAPSIZE; i++)
        if (h->HeapCmp(h->heap[i], val) == 0) break;
    if (i == h->HEAPSIZE) { foint err; err.i = -1; return err; }
    h->HEAPSIZE--;
    if (i < h->HEAPSIZE) {
        h->heap[i] = h->heap[h->HEAPSIZE];
        _HeapBubbleDown(h, i);
        _HeapBubbleUp(h, i);
    }
    return val;
}

#endif /* BLANT_HEAP_H */
