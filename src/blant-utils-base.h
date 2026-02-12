/* blant-utils-base.h -- Base utilities replacing libwayne's misc.h
 * Provides macros (MAX, MIN, SQR, etc.), memory wrappers, and file helpers.
 */
#ifndef BLANT_UTILS_BASE_H
#define BLANT_UTILS_BASE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include "blant-fatal.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define ABS(a) ((a)>=0?(a):-(a))
#define SQR(x) ((x)*(x))
#define SIGN(x) ((x)==0?0:(x)<0?-1:1)

#define elsif else if
#define until(x) while(!(x))

/* Memory allocation wrappers with error checking */
static inline void *Malloc(size_t n) {
    void *p = malloc(n);
    if (!p && n) { Fatal("Malloc(%lu) failed", (unsigned long)n); }
    return p;
}
static inline void *Calloc(size_t count, size_t size) {
    void *p = calloc(count, size);
    if (!p && count && size) { Fatal("Calloc(%lu,%lu) failed", (unsigned long)count, (unsigned long)size); }
    return p;
}
static inline void *Realloc(void *ptr, size_t newSize) {
    void *p = realloc(ptr, newSize);
    if (!p && newSize) { Fatal("Realloc(%lu) failed", (unsigned long)newSize); }
    return p;
}
static inline void Free(void *ptr) { free(ptr); }

/* Oalloc replacements -- just use regular allocation since we don't need bulk-free semantics */
static inline void *Omalloc(unsigned n) { return Malloc(n); }
static inline void *Ocalloc(unsigned n, size_t s) { return Calloc(n, s); }

static inline char *Strdup(const char *s) {
    if (!s) return NULL;
    char *d = (char*)malloc(strlen(s) + 1);
    if (!d) Fatal("Strdup failed");
    strcpy(d, s);
    return d;
}

/* File decompression helpers -- support .gz and .xz extensions */
static inline const char* getDecompressionProgram(char* fileName) {
    size_t len = strlen(fileName);
    if (len >= 3 && strcmp(fileName + len - 3, ".gz") == 0) return "gunzip -c";
    if (len >= 3 && strcmp(fileName + len - 3, ".xz") == 0) return "unxz -c";
    return NULL;
}

static inline FILE* readFile(char* fileName, int* piped) {
    const char *decomp = getDecompressionProgram(fileName);
    if (decomp) {
        char cmd[BUFSIZ];
        snprintf(cmd, sizeof(cmd), "%s '%s'", decomp, fileName);
        *piped = 1;
        return popen(cmd, "r");
    }
    *piped = 0;
    return fopen(fileName, "r");
}

static inline void closeFile(FILE* fp, int* isPiped) {
    if (*isPiped) pclose(fp);
    else fclose(fp);
}

/* GetFancySeed: generate a pseudo-unique seed from time, pid, etc. */
static inline unsigned int GetFancySeed(bool trulyRandom) {
    (void)trulyRandom;
    unsigned int seed = (unsigned int)time(NULL);
    seed ^= (unsigned int)getpid() * 65537U;
#ifdef __linux__
    seed ^= (unsigned int)getppid() * 131U;
#endif
    return seed;
}

#endif /* BLANT_UTILS_BASE_H */
