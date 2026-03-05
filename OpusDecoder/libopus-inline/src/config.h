/* config.h - Opus Configuration for Amiga 68040 (GCC 2.9 compatible) */

#ifndef CONFIG_H
#define CONFIG_H

/* Opus Build */
#define OPUS_BUILD 1

/* Fixed-Point (no FPU) */
#define FIXED_POINT 1

/* Disable Float API */
#define DISABLE_FLOAT_API 1

/* Version */
#define OPUS_VERSION "1.4-inline"

/* Package info */
#define PACKAGE_VERSION "1.4"
#define PACKAGE "opus"

/* CPU Detection - Use C implementation */
#define CPU_INFO_BY_C 1

/* Disable all assembly optimizations */
#define USE_SMALL_DIV_TABLE 0
#define OPUS_X86_ASM 0
#define OPUS_ARM_INLINE_ASM 0
#define OPUS_ARM_INLINE_EDSP 0
#define OPUS_ARM_INLINE_MEDIA 0
#define OPUS_ARM_INLINE_NEON 0

/* Stack allocation - use VAR_ARRAYS for GCC 2.9 compatibility */
#define VAR_ARRAYS 1
#define USE_ALLOCA 0

/* Disable assertions in release builds */
#ifndef DEBUG
#define NDEBUG 1
#endif

/* Enable checks (good for debugging) */
#ifdef DEBUG
#define ENABLE_ASSERTIONS 1
#endif

/* Standard includes */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* GCC 2.9 compatibility - inline keyword */
#if !defined(__cplusplus) && !defined(inline)
#define inline __inline__
#endif

/* Restrict keyword (not in GCC 2.9) */
#ifndef restrict
#define restrict
#endif

#endif /* CONFIG_H */
