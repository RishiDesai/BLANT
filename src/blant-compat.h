// blant-compat.h: Compatibility shim replacing libwayne's misc.h and related headers
// This provides the same API as libwayne using standard C only.
#ifndef BLANT_COMPAT_H
#define BLANT_COMPAT_H

// Ensure POSIX/XSI functions (strdup, drand48, etc.) are declared even in strict C modes
#if !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200809L
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_XOPEN_SOURCE) || _XOPEN_SOURCE < 500
#undef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <assert.h>
#include <limits.h>
#include <time.h>
#include <math.h>
#include <stdarg.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------- PARANOID_ASSERTS ----------
#ifndef NDEBUG
#ifndef PARANOID_ASSERTS
#define PARANOID_ASSERTS 1
#endif
#endif

// ---------- Common macros from misc.h ----------
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef ABS
#define ABS(a) ((a)>=0?(a):-(a))
#endif
#ifndef SQR
#define SQR(x) ((x)*(x))
#endif
#define CUBE(x) ((x)*(x)*(x))
#define SIGN(x) ((x)==0?0:(x)<0?-1:1)
#define SIGN2(x,y) ((x)*SIGN(y))
#define IMPLIES(a,b) (!(a)||(b))
static __inline__ double sqr(double x) { return x*x; }
static __inline__ double cube(double x) { return x*x*x; }
#define MACH_EPS 1e-16

#define elsif else if
#define until(x) while(!(x))

// ---------- Boolean type ----------
typedef unsigned char Boolean;
#ifndef false
#define false (Boolean)0
#define true  (Boolean)1
#endif
#define maybe (Boolean)2
#define both (Boolean)3

// ---------- foint type (union for generic data) ----------
typedef union _voidInt {
    unsigned long ul;
    long int l;
    void *v;
    char *s;
    char c[sizeof(long)];
    unsigned char uc[sizeof(long)];
    short sh[sizeof(long)/sizeof(short)];
    unsigned short ush[sizeof(long)/sizeof(unsigned short)];
    int i, i_array[sizeof(long)/sizeof(int)];
    unsigned int ui, ui_array[sizeof(long)/sizeof(unsigned int)];
    float f;
#if __SIZEOF_LONG__ >= __SIZEOF_DOUBLE__
    double d;
#endif
} foint;

typedef int (*pCmpFcn)(foint, foint);
typedef foint (*pFointCopyFcn)(foint);
typedef void (*pFointFreeFcn)(foint);
typedef int (*pFointTraverseFcn)(foint globals, foint key, foint data);

extern const foint ABSTRACT_ERROR;

// ---------- Logging/Error functions ----------
// These are implemented in blant-compat.c
extern void Note(const char *fmt, ...);
extern void Warning(const char *fmt, ...);
extern void Apology(const char *fmt, ...);
extern void Abort(const char *fmt, ...);
extern void Fatal(const char *fmt, ...);

// ---------- Memory functions ----------
#define MMAP 1
extern void *Mmap(void *p, size_t n, int fd);
extern void *Malloc(size_t);
extern void *Calloc(size_t, size_t);
extern void *Realloc(void *ptr, size_t newSize);
extern void Free(void *ptr);
extern void *Memdup(void *v, size_t n);

// ---------- String utility ----------
#ifndef Strdup
#define Strdup(s) strdup(s)
#endif

// ---------- File utilities ----------
extern FILE *Fopen(char *filename, const char *mode);
extern FILE *readFile(char* fileName, int* piped);
extern const char* getDecompressionProgram(char* fileName);
extern FILE* decompressFile(const char* decompProg, char* fileName);
extern void closeFile(FILE* fp, int* isPiped);
extern const char* getFileExtension(char* filename);

// ---------- Math utilities ----------
extern double uTime(void);
extern char *Int2BitString(char word[33], unsigned i);
extern void PrintArray(FILE *fp, int n, int *array);
extern long long IIntPow(int, int);
extern double IntPow(double, int);
extern int Log2(int);
extern double Exp(double x);
extern double AccurateLog1(double x);
extern double LogSumLogs(double log_a, double log_b);
extern int gcd(int a, int b);
extern int IsPrime(long long n);
extern int PrimeFactors(int n, int count[]);
extern unsigned int GetFancySeed(Boolean trulyRandom);

// ---------- Rotation macros ----------
#define RotLeft(type,i,s,n) ((((i) << (n)) | ((i) >> (s-(n)))) & (~(type)0 >> (8*sizeof(i)-(s))))
#define RotRight(type,i,s,n) ((((i) >> (n)) | ((i) << (s-(n)))) & (~(type)0 >> (8*sizeof(i)-(s))))

// ---------- intSizes.h replacement ----------
// These macros are used by misc.h/sets.h for conditional compilation
// On 64-bit systems: sizeof(long)=8, sizeof(double)=8, sizeof(int)=4, sizeof(short)=2
#ifndef sizeof_long
#define sizeof_long __SIZEOF_LONG__
#endif
#ifndef sizeof_double
#define sizeof_double __SIZEOF_DOUBLE__
#endif
#ifndef sizeof_int
#define sizeof_int __SIZEOF_INT__
#endif
#ifndef sizeof_short
#define sizeof_short __SIZEOF_SHORT__
#endif

// Bit widths
#ifndef long_width
#define long_width (__SIZEOF_LONG__ * 8)
#endif
#ifndef int_width
#define int_width (__SIZEOF_INT__ * 8)
#endif
#ifndef short_width
#define short_width (__SIZEOF_SHORT__ * 8)
#endif

#ifdef __cplusplus
} // end extern "C"
#endif

#endif // BLANT_COMPAT_H
