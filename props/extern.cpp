#define GAME_DLL
#include "cbase.h"
#include "engine/ienginetrace.h"
#include "tier1.h"
#include "convar.h"

/* util_shared.cpp */
void DebugDrawLine(const Vector&,const Vector&,int,int,bool,float){}
ConVar r_visualizetraces("","",0);