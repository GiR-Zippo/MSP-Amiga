#ifndef __COMMON_HPP__
#define __COMMON_HPP__

#include <cstdlib>
#include <cstddef>

//if we have clib2 aka wchar.h define this
#define HAS_NO_WCHAR

//Some std typedefs
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;

//The wchar stuff we need
#ifdef HAS_NO_WCHAR
    typedef int mbstate_t;
    typedef unsigned short wchar_t;
    size_t wcsrtombs(char* dest, const wchar_t** src, size_t len, mbstate_t* ps);
    size_t wcslen(const wchar_t* s);
#endif

#endif