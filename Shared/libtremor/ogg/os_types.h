#ifndef _OS_TYPES_H
#define _OS_TYPES_H

/* AmigaOS 3.x m68k - kein POSIX sys/types.h verfügbar
   Typen manuell definieren für bebbo gcc */

#define _ogg_malloc  malloc
#define _ogg_calloc  calloc
#define _ogg_realloc realloc
#define _ogg_free    free

typedef signed char        ogg_int8_t;
typedef unsigned char      ogg_uint8_t;
typedef signed short       ogg_int16_t;
typedef unsigned short     ogg_uint16_t;
typedef signed int         ogg_int32_t;
typedef unsigned int       ogg_uint32_t;
typedef signed long long   ogg_int64_t;
typedef unsigned long long ogg_uint64_t;

#endif /* _OS_TYPES_H */
