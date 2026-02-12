// Shim: malloc.h - redirect to stdlib.h (which provides malloc/free on all POSIX systems)
#ifndef _SHIM_MALLOC_H
#define _SHIM_MALLOC_H
#include <stdlib.h>
#endif
