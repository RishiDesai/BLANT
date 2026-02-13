/* blant-bitset.h -- Compact bitset replacing libwayne's SET/BITVEC
 * Header-only implementation using uint64_t words with __builtin_popcountll.
 */
#ifndef BLANT_BITSET_H
#define BLANT_BITSET_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include "blant-fatal.h"

#define BITSET_WORD_BITS 64

typedef struct {
    unsigned maxElem;   /* max elements 0..maxElem-1 */
    unsigned nwords;    /* number of uint64_t words */
    uint64_t *w;        /* bit storage */
} bitset_t;

/* Allocate a bitset capable of storing elements 0..max_elem-1 */
static inline bitset_t *bitset_alloc(unsigned max_elem) {
    bitset_t *s = (bitset_t*)calloc(1, sizeof(bitset_t));
    if (!s) { Fatal("bitset_alloc: out of memory"); }
    s->maxElem = max_elem;
    s->nwords = (max_elem + BITSET_WORD_BITS - 1) / BITSET_WORD_BITS;
    if (s->nwords == 0) s->nwords = 1;
    s->w = (uint64_t*)calloc(s->nwords, sizeof(uint64_t));
    if (!s->w) { Fatal("bitset_alloc: out of memory"); }
    return s;
}

static inline void bitset_free(bitset_t *s) {
    if (s) { free(s->w); free(s); }
}

static inline void bitset_clear(bitset_t *s) {
    memset(s->w, 0, s->nwords * sizeof(uint64_t));
}

static inline bool bitset_in(const bitset_t *s, unsigned e) {
    assert(e < s->maxElem);
    return (s->w[e / BITSET_WORD_BITS] >> (e % BITSET_WORD_BITS)) & 1;
}

static inline void bitset_add(bitset_t *s, unsigned e) {
    assert(e < s->maxElem);
    s->w[e / BITSET_WORD_BITS] |= ((uint64_t)1 << (e % BITSET_WORD_BITS));
}

static inline void bitset_delete(bitset_t *s, unsigned e) {
    assert(e < s->maxElem);
    s->w[e / BITSET_WORD_BITS] &= ~((uint64_t)1 << (e % BITSET_WORD_BITS));
}

static inline unsigned bitset_cardinality(const bitset_t *s) {
    unsigned count = 0;
    for (unsigned i = 0; i < s->nwords; i++)
        count += __builtin_popcountll(s->w[i]);
    return count;
}

static inline void bitset_union(bitset_t *c, const bitset_t *a, const bitset_t *b) {
    assert(a->nwords == b->nwords && a->nwords == c->nwords);
    for (unsigned i = 0; i < c->nwords; i++)
        c->w[i] = a->w[i] | b->w[i];
}

static inline void bitset_intersect(bitset_t *c, const bitset_t *a, const bitset_t *b) {
    assert(a->nwords == b->nwords && a->nwords == c->nwords);
    for (unsigned i = 0; i < c->nwords; i++)
        c->w[i] = a->w[i] & b->w[i];
}

static inline void bitset_copy(bitset_t *dst, const bitset_t *src) {
    assert(dst->nwords == src->nwords);
    memcpy(dst->w, src->w, src->nwords * sizeof(uint64_t));
}

/* Extract set bits into array, return count */
static inline unsigned bitset_to_array(const bitset_t *s, unsigned *array) {
    unsigned count = 0;
    for (unsigned i = 0; i < s->nwords; i++) {
        uint64_t word = s->w[i];
        unsigned base = i * BITSET_WORD_BITS;
        while (word) {
            unsigned bit = __builtin_ctzll(word);
            array[count++] = base + bit;
            word &= word - 1; /* clear lowest set bit */
        }
    }
    return count;
}

/* Find smallest element in the set. Returns maxElem if empty. */
static inline unsigned bitset_smallest(const bitset_t *s) {
    for (unsigned i = 0; i < s->nwords; i++) {
        if (s->w[i]) return i * BITSET_WORD_BITS + __builtin_ctzll(s->w[i]);
    }
    return s->maxElem;
}

/* Resize a bitset (realloc) */
static inline bitset_t *bitset_resize(bitset_t *s, unsigned new_max) {
    unsigned new_nwords = (new_max + BITSET_WORD_BITS - 1) / BITSET_WORD_BITS;
    if (new_nwords == 0) new_nwords = 1;
    if (new_nwords > s->nwords) {
        s->w = (uint64_t*)realloc(s->w, new_nwords * sizeof(uint64_t));
        if (!s->w) Fatal("bitset_resize: out of memory");
        memset(s->w + s->nwords, 0, (new_nwords - s->nwords) * sizeof(uint64_t));
    }
    s->maxElem = new_max;
    s->nwords = new_nwords;
    return s;
}

static inline bool bitset_eq(const bitset_t *a, const bitset_t *b) {
    if (a->nwords != b->nwords) return false;
    return memcmp(a->w, b->w, a->nwords * sizeof(uint64_t)) == 0;
}

/* ===== COMPATIBILITY LAYER: SET API mapped to bitset_t =====
 * These typedefs and macros allow existing BLANT code to use SET* calls
 * while actually operating on bitset_t underneath.
 */
typedef bitset_t SET;
#define SetAlloc(n) bitset_alloc(n)
#define SetFree(s) bitset_free(s)
#define SetEmpty(s) bitset_clear(s)
#define SetReset SetEmpty
#define SetCopy(dst, src) bitset_copy((dst), (src))
#define SetAdd(s, e) bitset_add((s), (e))
#define SetDelete(s, e) bitset_delete((s), (e))
#define SetIn(s, e) bitset_in((s), (e))
#define SetCardinality(s) bitset_cardinality(s)
#define SetToArray(arr, s) bitset_to_array((s), (arr))
#define SetUnion(c, a, b) (bitset_union((c), (a), (b)), (c))
#define SetIntersect(c, a, b) (bitset_intersect((c), (a), (b)), (c))
#define SetResize(s, n) bitset_resize((s), (n))
#define SetEq(a, b) bitset_eq((a), (b))
#define SetMaxSize(s) ((s)->maxElem)
#define SetPrint(s) do { \
    unsigned __arr[bitset_cardinality(s)]; \
    unsigned __n = bitset_to_array((s), __arr); \
    for (unsigned __i = 0; __i < __n; __i++) printf("%u ", __arr[__i]); \
} while(0)

/* Set from array */
static inline bitset_t *SetFromArray(bitset_t *s, int n, unsigned *array) {
    bitset_clear(s);
    for (int i = 0; i < n; i++) bitset_add(s, array[i]);
    return s;
}

/* ===== TSET: Tiny set (for graphlet adjacency, 8-bit default) ===== */
#ifndef TINY_SET_SIZE
#define TINY_SET_SIZE 8
#endif

#if TINY_SET_SIZE == 8
    typedef uint8_t TSET;
#elif TINY_SET_SIZE == 16
    typedef uint16_t TSET;
#elif TINY_SET_SIZE == 32
    typedef uint32_t TSET;
#elif TINY_SET_SIZE == 64
    typedef unsigned long long TSET;
#else
    #error "unknown TINY_SET_SIZE"
#endif

#define TSET1 ((TSET)1)
#define TSET_NULLSET ((TSET)0)
#define MAX_TSET (8*sizeof(TSET))

#define TSetEmpty(s) ((s) = 0)
#define TSetReset TSetEmpty
#define TSetAdd(s,e) ((s) |= (TSET1 << (e)))
#define TSetDelete(s,e) ((s) &= ~(TSET1 << (e)))
#define TSetIn(s,e) ((s) & (TSET1 << (e)))
#define TSetEq(s1,s2) ((s1)==(s2))
#define TSetUnion(a,b) ((a) | (b))
#define TSetIntersect(a,b) ((a) & (b))

/* Count bits in a TSET */
static inline unsigned TSetCountBitsFunc(TSET s) {
#if TINY_SET_SIZE <= 32
    return __builtin_popcount((unsigned)s);
#else
    return __builtin_popcountll((unsigned long long)s);
#endif
}
#define TSetCountBits(s) TSetCountBitsFunc(s)
#define TSetCardinality TSetCountBits

/* TSET to/from array */
static inline unsigned TSetToArray(unsigned *array, TSET set) {
    unsigned count = 0;
    for (unsigned i = 0; i < MAX_TSET && set; i++) {
        if (set & (TSET1 << i)) { array[count++] = i; }
    }
    return count;
}

static inline TSET TSetFromArray(int n, unsigned *array) {
    TSET s = 0;
    for (int i = 0; i < n; i++) s |= (TSET1 << array[i]);
    return s;
}

/* SSET: Small set (64-bit) */
#define SMALL_SET_SIZE 64
typedef unsigned long long SSET;
#define SSET1 1ULL
#define SSET_NULLSET 0ULL
#define MAX_SSET (8*sizeof(SSET))
#define SSetEmpty(s) ((s) = 0)
#define SSetAdd(s,e) ((s) |= (SSET1 << (e)))
#define SSetDelete(s,e) ((s) &= ~(SSET1 << (e)))
#define SSetIn(s,e) ((s) & (SSET1 << (e)))
#define SSetEq(s1,s2) ((s1)==(s2))
#define SSetUnion(a,b) ((a) | (b))
#define SSetIntersect(a,b) ((a) & (b))
#define SSetCountBits(s) ((unsigned)__builtin_popcountll(s))
#define SSetCardinality SSetCountBits

static inline SSET SSetFromArray(int n, unsigned *array) {
    SSET s = 0;
    for (int i = 0; i < n; i++) s |= (SSET1 << array[i]);
    return s;
}
static inline unsigned SSetToArray(unsigned *array, SSET set) {
    unsigned count = 0;
    for (unsigned i = 0; i < MAX_SSET && set; i++) {
        if (set & (SSET1 << i)) { array[count++] = i; }
    }
    return count;
}

/* BITVEC compatibility */
typedef bitset_t BITVEC;
#define BitvecAlloc(n) bitset_alloc(n)
#define BitvecFree(v) bitset_free(v)
#define BitvecEmpty(v) bitset_clear(v)
#define BitvecReset BitvecEmpty
#define BitvecAdd(v,e) bitset_add((v),(e))
#define BitvecDelete(v,e) bitset_delete((v),(e))
#define BitvecIn(v,e) bitset_in((v),(e))
#define BitvecInSafe BitvecIn
#define BitvecCardinality(v) bitset_cardinality(v)
#define BitvecToArray(arr,v) bitset_to_array((v),(arr))
#define BitvecCopy(dst,src) bitset_copy((dst),(src))
#define BitvecUnion(c,a,b) (bitset_union((c),(a),(b)),(c))
#define BitvecIntersect(c,a,b) (bitset_intersect((c),(a),(b)),(c))
#define BitvecMaxSize(v) ((v)->maxElem)
#define BitvecResize(v,n) bitset_resize((v),(n))
#define BitvecEq(a,b) bitset_eq((a),(b))

static inline bool BitvecStartup(void) { return true; }
static inline bool SetStartup(void) { return true; }

/* BitvecCountBits for raw integer */
static inline unsigned _BitvecCountBitsRaw(unsigned i) { return __builtin_popcount(i); }
#define BitvecCountBits(i) __builtin_popcount((unsigned)(i))

/* lookupBitCount table compatibility */
#define lookupBitCount _blant_lookupBitCount_dummy
/* Not actually used since TSetCountBits uses __builtin_popcount now */

#endif /* BLANT_BITSET_H */
