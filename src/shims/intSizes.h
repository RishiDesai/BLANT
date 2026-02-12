// Shim: intSizes.h - provided by blant-compat.h
#ifndef _INTSIZES_H
#define _INTSIZES_H
// All definitions are in blant-compat.h
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
#ifndef long_width
#define long_width (__SIZEOF_LONG__ * 8)
#endif
#ifndef int_width
#define int_width (__SIZEOF_INT__ * 8)
#endif
#ifndef short_width
#define short_width (__SIZEOF_SHORT__ * 8)
#endif
#endif
