#ifndef GENESIS3D_BASETYPE_H
#define GENESIS3D_BASETYPE_H

/* Native Linux replacement for the original Win32 primitive-type header. */
#include <stddef.h>
#include <stdint.h>

#ifndef _WIN32
#include <strings.h>
#define _inline inline
#define strnicmp strncasecmp
#define stricmp strcasecmp
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t  int32;
typedef int16_t  int16;
typedef int8_t   int8;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t  uint8;
typedef float    geFloat;
typedef double   geDouble;
typedef int32_t  geBoolean;
typedef geBoolean BOOL;

typedef struct geSystemTime {
    uint16_t wYear, wMonth, wDayOfWeek, wDay;
    uint16_t wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME;

#define GE_FALSE ((geBoolean)0)
#define GE_TRUE  ((geBoolean)1)

#ifndef _WIN32
#ifndef FALSE
#define FALSE GE_FALSE
#endif
#ifndef TRUE
#define TRUE GE_TRUE
#endif
#endif

/* Win32 calling-convention/export annotations are no-ops for ELF. */
#define GENESISCC
#define GENESISAPI

#ifndef NULL
#define NULL ((void *)0)
#endif

#define GE_ABS(x)        ((x) < 0 ? -(x) : (x))
#define GE_CLAMP(x,lo,hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#define GE_CLAMP8(x)     GE_CLAMP((x), 0, 255)
#define GE_CLAMP16(x)    GE_CLAMP((x), 0, 65535)
#define GE_BOOLSAME(x,y) (((x) && (y)) || (!(x) && !(y)))

#define GE_EPSILON       ((geFloat)0.000797f)
#define GE_PI            ((geFloat)3.14159265358979323846f)
#define GE_2PI           ((geFloat)6.28318530717958647693f)
#define GE_PIOVER2       ((geFloat)1.57079632679489661923f)

#ifdef __cplusplus
}
#endif

#endif /* GENESIS3D_BASETYPE_H */
