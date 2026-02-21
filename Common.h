#ifndef __COMMON_HPP__
#define __COMMON_HPP__

#include <cstdlib>
#include <cstddef>
#include <ctime>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include <devices/timer.h>

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
#include <proto/timer.h>

#include <utility/tagitem.h>

#include <proto/exec.h>
#include <proto/dos.h>

#define M_PI 3.14159265358979323846
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

typedef unsigned long uintptr_t;

float powf(float base, float exp);

#else
#include <cwchar> 
#include <stdint.h>
#include <cmath>
#endif

#ifndef IPTR
typedef unsigned long IPTR;
#endif

extern "C"
{
    size_t wcsrtombs(char* dest, const wchar_t** src, size_t len, mbstate_t* ps) __attribute__((weak));
    size_t wcslen(const wchar_t* s) __attribute__((weak));
}

/// @brief Converts a string to lowercase
void stringToLower(std::string& s);

/// @brief Check if string contains a string
bool containsString(const char* haystack, const char* needle);

char *strcasestr(const char *haystack, const char *needle);

std::vector<std::string> Split(const std::string &text, const std::string &delimiter);

/// @brief Converts UTF to Amiga chars
void UTF8ToAmiga(char *str);

/// @brief Removes a char from string
void RemoveFromString(std::string &src, std::string arg);
        
/// @brief just a helper for space to %20 conversion for urls
std::string SimpleEncode(const char* src);

/// @brief writes input to output, outbufSize defaults to 127 (array[128])
void writeToBuffer(char* outbuf, const char* input, int outbufSize = 127);

/// @brief show buffer content
void dump_packet(const uint8_t *buffer, int len);

/// @brief integer to ascii
void itoa(uint32_t n, char* s);
#endif