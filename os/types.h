#pragma once

#define LONG_MAX 2147483647

#define intptr_t (int)(void*)
#define mintptr intptr_t
#define NULL ((void*)0)

typedef long off_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef uint32_t size_t;
typedef uint32_t mint; //64 bit malloc
typedef int (*intfp)();
typedef void (*voidfp)();
typedef void* (*voidpfp)();
typedef char* (*charpfp)();
