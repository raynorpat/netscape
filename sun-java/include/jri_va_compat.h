#ifndef sun_java_jri_va_compat_h___
#define sun_java_jri_va_compat_h___

#include <stdarg.h>

#if defined(va_copy)
#define JRI_VA_COPY(destination, source) va_copy(destination, source)
#elif defined(__va_copy)
#define JRI_VA_COPY(destination, source) __va_copy(destination, source)
#else
/* Pre-C99 ports without either copy macro used assignable va_list types. */
#define JRI_VA_COPY(destination, source) ((destination) = (source))
#endif

#endif
