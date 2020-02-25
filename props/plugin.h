#ifndef __PLUGIN_H
#define __PLUGIN_H

#include <stdint.h>

#define JMP2PTR(ptr) ((char*)ptr+5+((ptrdiff_t)*(ULONG*)((char*)ptr+1)))
typedef void (*SetAllowPrecache_t)(bool);
#define SAP_SIG "\xE8\x00\x00\x00\x00\x8B\x0D\x00\x00\x00\x00\x8B\x11\x83\xC4\x04\x6A\xFF\x6A\x01\xFF\x92\xC8\x00\x00\x00"
#define SAP_MASK "x????xx????xxxxxxxxxxxxxxx"

#define DP_SIG "\x53\x56\x8B\x74\x24\x0C\x85\xF6\x57\x0F\x84\x00\x00\x00\x00"
#define DP_MASK "xxxxxxxxxxx????"
typedef int (*DispatchSpawn_t)(void*);

#define CEBN_SIG "\x56\x8B\x74\x24\x0C\x83\xFE\xFF\x57\x8B\x7C\x24\x0C\x74\x25\x8B\x0D\x00\x00\x00\x00\x8B\x01\x56\xFF\x50\x54\x85\xC0\xA3\x00\x00\x00\x00\x75\x10"
#define CEBN_MASK "xxxxxxxxxxxxxxxxx????xxxxxxxxx????xx"
typedef void* (*CreateEntityByName_t)(const char*,int);

void* SigScan(const char* pMod,const char* pSig,const char* pMask);

#endif