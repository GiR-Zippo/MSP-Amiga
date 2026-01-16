#ifndef __COMMON_HPP__
#define __COMMON_HPP__

#include <cstdlib>
#include <cstddef>
#include <exec/types.h>

//set OLD_GCC as -DOLD_GCC flag, when using ADE
#ifdef OLD_GCC
#ifndef CONST_STRPTR
typedef UBYTE * CONST_STRPTR;
#endif

//Some std typedefs
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;

//The wchar stuff we need
typedef int mbstate_t;
typedef unsigned short wchar_t;
#else
#include <cwchar> 
#include <stdint.h>
#endif

extern "C"
{
    size_t wcsrtombs(char* dest, const wchar_t** src, size_t len, mbstate_t* ps) __attribute__((weak));
    size_t wcslen(const wchar_t* s) __attribute__((weak));
}   
#endif