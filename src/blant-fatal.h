/* blant-fatal.h -- Fatal/Warning/Note/Apology/Abort macros for BLANT
 * Replaces libwayne's misc.h error-handling functions.
 */
#ifndef BLANT_FATAL_H
#define BLANT_FATAL_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define Fatal(...) do { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); exit(1); } while(0)
#define Warning(...) do { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while(0)
#define Note(...) do { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while(0)
#define Apology(...) do { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); exit(1); } while(0)
#define Abort(...) do { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); assert(false); exit(1); } while(0)

#endif /* BLANT_FATAL_H */
