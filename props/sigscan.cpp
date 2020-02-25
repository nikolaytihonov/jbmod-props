#include "plugin.h"
#include <Windows.h>

void* SigScan(const char* pMod,const char* pSig,const char* pMask)
{
	size_t uSigLen = strlen(pMask);
	char* pBegin = (char*)GetModuleHandle(pMod);
	size_t uLen;

	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pBegin;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)((char*)pDos+pDos->e_lfanew);
	uLen = pNt->OptionalHeader.SizeOfCode-uSigLen;
	pBegin = (char*)pDos+pNt->OptionalHeader.BaseOfCode;

	for(int i = 0; i < uLen; i++)
	{
		int j;
		for(j = 0; j < uSigLen; j++)
		{
			if(pBegin[i+j] != pSig[j] && pMask[j] == 'x')
				break;
		}
		if(j==uSigLen) return &pBegin[i];
	}
	return NULL;
}