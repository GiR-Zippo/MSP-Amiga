#ifndef __COMMON_HPP__
#define __COMMON_HPP__

#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include <dos/dos.h>
#include <dos/dostags.h>

#include <exec/ports.h>
#include <exec/types.h>

#include <devices/ahi.h>

#include <hardware/cia.h>

#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>

#include <proto/ahi.h>
#include <proto/alib.h>
#include <proto/asl.h>
#include <proto/bsdsocket.h>
#include <proto/cia.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>

#include <utility/tagitem.h>

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

#ifndef IPTR
typedef unsigned long IPTR;
#endif

extern "C"
{
    size_t wcsrtombs(char* dest, const wchar_t** src, size_t len, mbstate_t* ps) __attribute__((weak));
    size_t wcslen(const wchar_t* s) __attribute__((weak));
}   
void stringToLower(std::string& s);
#endif