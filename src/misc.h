/* misc.h -- BLANT compatibility header replacing libwayne's misc.h
 * Maps old types and functions to new implementations.
 */
#ifndef _MISC_H
#define _MISC_H

#include "intSizes.h"
#include "blant-utils-base.h"
#include "blant-fatal.h"

/* Boolean type -- use C99 bool but provide typedef for source compat */
typedef unsigned char Boolean;
#ifndef false
#define false (Boolean)0
#define true  (Boolean)1
#endif
#define maybe (Boolean)2
#define both  (Boolean)3

/* foint is defined in blant-graph.h (or wherever it's first needed) */
/* pCmpFcn and other function pointer types used by BINTREE/HEAP */
/* These are forward-declared here for code that includes misc.h early */

#ifndef MMAP
#define MMAP 1
#endif

/* Mmap wrapper */
#include <sys/mman.h>
static inline void *Mmap(void *p, size_t n, int fd) {
    void *result = mmap(p, n, PROT_READ, MAP_PRIVATE, fd, 0);
    if (result == MAP_FAILED) Fatal("Mmap failed for fd %d, size %lu", fd, (unsigned long)n);
    return result;
}

/* Int2BitString: convert integer to binary string */
static inline char *Int2BitString(char word[33], unsigned i) {
    int b;
    for (b = 31; b >= 0; b--) word[31 - b] = ((i >> b) & 1) ? '1' : '0';
    word[32] = '\0';
    return word;
}

/* Log2 */
static inline int Log2(int n) {
    int result = 0;
    while (n > 1) { n >>= 1; result++; }
    return result;
}

#endif /* _MISC_H */
