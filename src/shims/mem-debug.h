// Shim: mem-debug.h - no-op (memory debugging disabled)
#ifndef _MEM_DEBUG_H
#define _MEM_DEBUG_H
#define ENABLE_MEM_DEBUG()
#define ENABLE_MEMORY_TRACKING()
// Suppress Strdup redefinition from libwayne's mem-debug.h
#ifdef Strdup
#undef Strdup
#endif
#define Strdup(s) strdup(s)
#endif
