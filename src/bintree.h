/* bintree.h -- BLANT compatibility header replacing libwayne's bintree.h
 * The BinTree functionality is now provided by uthash in blant-graph.h.
 * This header provides the BINTREE type and BinTreeAlloc/BinTreeLookup/BinTreeInsert
 * as a simple wrapper around uthash for the name dictionary use case.
 */
#ifndef _BINTREE_H
#define _BINTREE_H

#include "blant-graph.h" /* NameEntry, BinTreeLookup compat, foint */

/* BINTREE is now just a NameEntry hash table pointer.
 * The actual type is NameEntry* (a uthash handle).
 * We provide a thin compatibility layer for code that uses BINTREE.
 */
typedef struct _binTree {
    unsigned n;
    NameEntry *root;
} BINTREE;

/* BinTreeAlloc: allocate an empty BINTREE. Parameters are ignored for our use case. */
typedef int (*pCmpFcnBT)(foint, foint);
typedef foint (*pFointCopyFcn)(foint);
typedef void (*pFointFreeFcnBT)(foint);

static inline BINTREE *BinTreeAlloc(pCmpFcnBT cmp, pFointCopyFcn ck, pFointFreeFcnBT fk,
    pFointCopyFcn ci, pFointFreeFcnBT fi) {
    (void)cmp; (void)ck; (void)fk; (void)ci; (void)fi;
    BINTREE *bt = (BINTREE*)calloc(1, sizeof(BINTREE));
    bt->n = 0;
    bt->root = NULL;
    return bt;
}

/* BinTreeInsert: insert key-value pair (key is a string, info is a foint) */
static inline void BinTreeInsert(BINTREE *bt, foint key, foint info) {
    _GraphNameInsert((NameEntry**)&bt->root, key.s, info.i);
    bt->n++;
}

/* BinTreeLookup is already defined in blant-graph.h for NameEntry* */
/* Overload for BINTREE* parameter */
#define BinTreeLookupBT(bt, key, pInfo) BinTreeLookup((bt)->root, (key), (pInfo))

/* BinTreeFree */
static inline void BinTreeFree(BINTREE *bt) {
    if (bt) {
        NameEntry *entry, *tmp;
        HASH_ITER(hh, bt->root, entry, tmp) {
            HASH_DEL(bt->root, entry);
            free(entry->name);
            free(entry);
        }
        free(bt);
    }
}

#endif /* _BINTREE_H */
