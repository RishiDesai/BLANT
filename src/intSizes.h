/* intSizes.h -- BLANT compatibility header replacing libwayne's intSizes.h */
#ifndef _INTSIZES_H
#define _INTSIZES_H
#include <limits.h>
#include <stdint.h>

/* These must be usable in #if preprocessor directives */
#define sizeof_short 2
#define sizeof_int 4
#define sizeof_long 8
#define sizeof_double 8

/* Bit-width macros - must be compile-time constants for #if */
#define short_width 16
#define int_width 32
#define long_width 64

#endif /* _INTSIZES_H */
