// Shim: Oalloc.h - replace Omalloc/Ocalloc/Ofree with standard malloc/calloc
// Oalloc is a pool allocator where Ofree frees everything at once.
// We replace with standard allocators (items are never individually freed anyway).
#ifdef __cplusplus
extern "C" {
#endif
#ifndef _OALLOC_H
#define _OALLOC_H
#include <stdlib.h>
#include <string.h>

static inline void *Omalloc(unsigned n) { return malloc(n); }
static inline void *Ocalloc(unsigned n, size_t s) { return calloc(n, s); }
static inline void Ofree(void) { /* no-op: items allocated via Ocalloc/Omalloc are never bulk-freed in practice */ }

#endif /* _OALLOC_H */
#ifdef __cplusplus
} // end extern "C"
#endif
