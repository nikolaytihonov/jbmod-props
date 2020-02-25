#ifndef __FILTER_H
#define __FILTER_H

#include "engine/ienginetrace.h"

class CLocalTraceFilter : public ITraceFilter
{
public:
	CLocalTraceFilter(IHandleEntity* pIgnore)
		: m_pIgnore(pIgnore){}

	virtual TraceType_t GetTraceType() const {
		return TRACE_EVERYTHING;
	}
	
	virtual bool ShouldHitEntity(IHandleEntity* pEnt,
		int content){
		return (pEnt != m_pIgnore);
	}

	IHandleEntity* m_pIgnore;
};

#endif