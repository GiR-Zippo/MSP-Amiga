#ifndef _OS_H
#define _OS_H
/********************************************************************
 * PATCHED for AmigaOS 3.x / bebbo gcc / m68k
 * Original: OggVorbis 'TREMOR' CODEC SOURCE CODE
 * (C) COPYRIGHT 1994-2002 BY THE Xiph.Org FOUNDATION
 ********************************************************************/

/* Amiga: ogg/os_types.h liegt lokal im libtremor Ordner */
#include "ogg/os_types.h"

#ifndef _V_IFDEFJAIL_H_
#  define _V_IFDEFJAIL_H_
#  ifdef __GNUC__
#    define STIN static __inline__
#  elif _WIN32
#    define STIN static __inline
#  else
#    define STIN static
#  endif
#endif

#ifndef M_PI
#  define M_PI (3.1415926536f)
#endif

/* AmigaOS / m68k ist Big-Endian */
#ifdef __AMIGA__
#  ifndef BIG_ENDIAN
#    define BIG_ENDIAN    4321
#  endif
#  ifndef LITTLE_ENDIAN
#    define LITTLE_ENDIAN 1234
#  endif
#  ifndef BYTE_ORDER
#    define BYTE_ORDER    BIG_ENDIAN
#  endif
#  ifndef alloca
#    define alloca __builtin_alloca
#  endif
#endif

#ifdef HAVE_ALLOCA_H
#  include <alloca.h>
#endif

#ifdef USE_MEMORY_H
#  include <memory.h>
#endif

#ifndef min
#  define min(x,y)  ((x)>(y)?(y):(x))
#endif

#ifndef max
#  define max(x,y)  ((x)<(y)?(y):(x))
#endif

#endif /* _OS_H */
