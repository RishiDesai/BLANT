/* blant-queue.h -- Circular queue of foint values for BLANT
 * Replaces libwayne's queue.h/queue.c
 */
#ifndef BLANT_QUEUE_H
#define BLANT_QUEUE_H

#include <stdlib.h>
#include <assert.h>
#include "blant-fatal.h"
#include "blant-graph.h" /* for foint */

typedef struct _queueStruct {
    int maxSize, front, length;
    foint *queue;
} QUEUE;

static inline QUEUE *QueueAlloc(int maxSize) {
    QUEUE *q = (QUEUE*)calloc(1, sizeof(QUEUE));
    if (!q) Fatal("QueueAlloc: out of memory");
    q->maxSize = maxSize;
    q->front = q->length = 0;
    q->queue = (foint*)calloc(maxSize, sizeof(foint));
    if (!q->queue) Fatal("QueueAlloc: out of memory");
    return q;
}

static inline void QueueFree(QUEUE *q) {
    if (q) { free(q->queue); free(q); }
}

static inline void QueueEmpty(QUEUE *q) {
    q->front = q->length = 0;
}

static inline int QueueSize(QUEUE *q) {
    return q->length;
}

static inline foint QueueFront(QUEUE *q) {
    assert(q->length > 0);
    return q->queue[q->front];
}

static inline foint QueueGet(QUEUE *q) {
    assert(q->length > 0);
    foint val = q->queue[q->front];
    q->front = (q->front + 1) % q->maxSize;
    q->length--;
    return val;
}

static inline foint QueuePut(QUEUE *q, foint val) {
    if (q->length >= q->maxSize) {
        /* Grow the queue */
        int newSize = q->maxSize * 2;
        foint *newQ = (foint*)calloc(newSize, sizeof(foint));
        if (!newQ) Fatal("QueuePut: out of memory");
        /* Copy existing elements in order */
        for (int i = 0; i < q->length; i++)
            newQ[i] = q->queue[(q->front + i) % q->maxSize];
        free(q->queue);
        q->queue = newQ;
        q->front = 0;
        q->maxSize = newSize;
    }
    int pos = (q->front + q->length) % q->maxSize;
    q->queue[pos] = val;
    q->length++;
    return val;
}

static inline foint QueueBelowTop(QUEUE *q, int n) {
    assert(n < q->length);
    return q->queue[(q->front + n) % q->maxSize];
}

#endif /* BLANT_QUEUE_H */
