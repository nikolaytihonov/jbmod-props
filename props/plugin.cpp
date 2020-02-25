#define GAME_DLL
#include "plugin.h"
#include "cbase.h"
#include "engine/iserverplugin.h"
#include "toolframework/itoolentity.h"
#include "filesystem.h"
#include "eiface.h"
#include "mathlib.h"
#include "tier1.h"
#include "strtools.h"
#include "datacache/imdlcache.h"
#include "util.h"
#include "props.h"
#include "filter.h"

#define MIN(a,b) (((a)<(b))?(a):(b))

typedef struct {
	CUtlSymbol name;
	int iGroup;
	Vector hull_min;
	Vector hull_max;
} prop_t;

class CPluginProps : public IServerPluginCallbacks
{
public:
	virtual bool Load(CreateInterfaceFn,CreateInterfaceFn);
	virtual void Unload();
	virtual void Pause(){}
	virtual void UnPause(){}
	virtual const char* GetPluginDescription(){
		return "Props";
	}
	virtual void LevelInit(const char*){}
	virtual void ServerActivate(edict_t*,int,int){}
	virtual void GameFrame(bool){}
	virtual void LevelShutdown(){}
	virtual void ClientActive(edict_t*);
	virtual void ClientDisconnect(edict_t*){}
	virtual void ClientPutInServer(edict_t*,const char*){}
	virtual void SetCommandClient(int iIndex){
		m_iCommandIndex = iIndex;
	}
	virtual void ClientSettingsChanged(edict_t*){}
	virtual PLUGIN_RESULT ClientConnect(bool*,edict_t*,const char*,
		const char*,char*,int){return PLUGIN_CONTINUE;}
	virtual PLUGIN_RESULT ClientCommand(edict_t*);
	virtual PLUGIN_RESULT NetworkIDValidated(const char*,const char*){
		return PLUGIN_CONTINUE;
	}

	int AddGroup(const char* pName);
	void GetProps(CUtlVector<int>& props,int iGroup);
	void LoadModel(const char* pFolder,const char* pName);
	int FindModel(const char* pName);
	void SpawnProp(Vector vPos,prop_t& pr);
	PLUGIN_RESULT ShowPropMenu(edict_t* pPly,int group,int index);

	int m_iCommandIndex;
	CUtlVector<CUtlSymbol> m_Groups;
	CUtlVector<prop_t> m_Props;
} g_PluginProps;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CPluginProps,IServerPluginCallbacks,
	INTERFACEVERSION_ISERVERPLUGINCALLBACKS,g_PluginProps);

IVEngineServer* engine = NULL;
IEngineTrace* enginetrace = NULL;
IServerPluginHelpers* helpers = NULL;
IFileSystem* filesystem = NULL;
IMDLCache* mdlcache = NULL;

SetAllowPrecache_t pSAP = NULL;
DispatchSpawn_t pDP = NULL;
CreateEntityByName_t pCEBN = NULL;
bool* pAllowPrecache;

bool CPluginProps::Load(CreateInterfaceFn interfaces,CreateInterfaceFn game)
{
	char szLine[MAX_PATH] = {0};
	char szPath[MAX_PATH] = {0};
	char* pFst;

	ConnectTier1Libraries(&interfaces,1);
	MathLib_Init(2.2f,2.2f,0.0f,2.0f);

	if(!(engine = (IVEngineServer*)interfaces(INTERFACEVERSION_VENGINESERVER,0))
		|| !(enginetrace = (IEngineTrace*)interfaces(INTERFACEVERSION_ENGINETRACE_SERVER,0))
		|| !(helpers = (IServerPluginHelpers*)interfaces(INTERFACEVERSION_ISERVERPLUGINHELPERS,0))
		|| !(filesystem = (IFileSystem*)interfaces(FILESYSTEM_INTERFACE_VERSION,0))
		|| !(mdlcache = (IMDLCache*)interfaces(MDLCACHE_INTERFACE_VERSION,0)))
	{
		Warning("Failed to load due interface mismatch!\n");
		return false;
	}

	if(!(pFst = (char*)SigScan("server.dll",SAP_SIG,SAP_MASK)))
	{
		Warning("Failed to find sig SAP!\n");
		return false;
	}

	pSAP = (SetAllowPrecache_t)JMP2PTR(pFst);
	pAllowPrecache = *(bool**)((char*)pSAP+0x05);

	if(!(pDP = (DispatchSpawn_t)SigScan("server.dll",DP_SIG,DP_MASK)))
	{
		Warning("Failed to find sig DP!\n");
		return false;
	}

	if(!(pCEBN = (CreateEntityByName_t)SigScan("server.dll",CEBN_SIG,CEBN_MASK)))
	{
		Warning("Failed to find sig CEBN!\n");
		return false;
	}
	
	KeyValues* pProps = new KeyValues("Props");
	if(!(pProps->LoadFromFile(filesystem,"addons/props.txt","GAME")))
	{
		Warning("File addons/props.txt not found!\n");
		return false;
	}

	KeyValues* pV = pProps->GetFirstSubKey();
	do {
		KeyValues* pK = pV->GetFirstValue();
		do {
			LoadModel(pV->GetName(),pK->GetString());
		} while((pK=pK->GetNextValue()));
	} while((pV=pV->GetNextKey()));
	pProps->deleteThis();
	return true;
}

int CPluginProps::AddGroup(const char* pName)
{
	int index = -1;
	for(int i = 0; i < m_Groups.Count(); i++)
	{
		if(FStrEq(m_Groups[i].String(),pName))
		{
			index = i;
			break;
		}
	}

	if(index == -1)
		index = m_Groups.AddToTail(CUtlSymbol(pName));
	return index;
}

void CPluginProps::GetProps(CUtlVector<int>& props,int iGroup)
{
	for(int i = 0; i < m_Props.Count(); i++)
	{
		if(m_Props[i].iGroup == iGroup)
			props.AddToTail(i);
	}
}

void CPluginProps::LoadModel(const char* pFolder,const char* pName)
{
	char szBuf[MAX_PATH] = {0};
	char szBuf2[MAX_PATH] = {0};
	prop_t prop;

	strcpy_s(szBuf2,pName);
	V_strcpy(szBuf,"models/");
	strcat_s(szBuf,szBuf2);

	if(!filesystem->FileExists(szBuf,"GAME"))
	{
		Warning("Model %s doesn't exists!\n");
		return;
	}

	MDLCACHE_CRITICAL_SECTION();
	MDLHandle_t mdl = mdlcache->FindMDL(szBuf);
	if(mdl == MDLHANDLE_INVALID)
	{
		Warning("Model %s not found!\n",szBuf);
		return;
	}

	studiohdr_t* pHdr = mdlcache->GetStudioHdr(mdl);
	if(!pHdr)
	{
		Warning("Model %s doesn't have vphysics!\n");
		return;
	}

	/* Add group */
	prop.name = CUtlSymbol(szBuf2);
	prop.iGroup = AddGroup(pFolder);
	prop.hull_min = pHdr->hull_min;
	prop.hull_max = pHdr->hull_max;
	m_Props.AddToTail(prop);
}

void CPluginProps::Unload()
{
	DisconnectTier1Libraries();
}

void CPluginProps::ClientActive(edict_t* pPly)
{
	/*
	KeyValues *kv = new KeyValues( "msg" );
	kv->SetString( "title", "Hello" );
	kv->SetString( "msg", "Hello there" );
	kv->SetColor( "color", Color( 255, 0, 0, 255 ));
	kv->SetInt( "level", 5);
	kv->SetInt( "time", 10);
	helpers->CreateMessage( pEntity, DIALOG_MSG, kv, this );
	kv->deleteThis();
	*/
}

inline QAngle* GetEntityEyes(CBaseEntity* pEnt)
{
	QAngle* pRet;
	__asm
	{
		push ecx
		push edx

		mov ecx,[pEnt]
		mov edx,[ecx]
		call dword ptr [edx+0x1A8]
		mov [pRet],eax

		pop edx
		pop ecx
	}
	return pRet;
}

inline void GetEyePosition(CBaseEntity* pEnt,Vector* vOut)
{
	__asm
	{
		push ecx
		push edx

		push [vOut]
		mov ecx,[pEnt]
		mov edx,[ecx]
		call dword ptr [edx+0x1A4]

		pop edx
		pop ecx
	}
}

inline PLUGIN_RESULT CPluginProps::ShowPropMenu(edict_t* pPly,int group,int index)
{
	const int iSize = 9;
	const int mSize = 7;
	char szBuf[MAX_PATH];
	
	CUtlVector<int> props;
	KeyValues* msg = new KeyValues("menu");
	GetProps(props,group);

	msg->SetString("title","Prop menu");
	msg->SetString("msg","Prop menu");
	msg->SetInt("level",5);
	msg->SetInt("time",9999);
	msg->SetColor("color",Color(255,255,0,255));

	for(int i = index; i < MIN(index+mSize,props.Count()); i++)
	{
		if(!props.IsValidIndex(i) || !m_Props.IsValidIndex(props[i]))
			continue;
		prop_t& pr = m_Props[props[i]];
		Q_snprintf(szBuf,MAX_PATH,"%d",i-index+1);
		KeyValues* sub = msg->FindKey(szBuf,true);
			
		char* pPtr = strchr((char*)pr.name.String(),'/');
		if(!pPtr) continue;
		pPtr++;
		sub->SetString("msg",pPtr);

		Q_snprintf(szBuf,MAX_PATH,"prop_spawn %s %s %d",
			pr.name.String(),engine->Cmd_Argv(1),index);
		sub->SetString("command",szBuf);
	}

	if(props.Count()-index>=mSize)
	{
		KeyValues* next = msg->FindKey("8",true);
		next->SetString("msg","Next");
		Q_snprintf(szBuf,MAX_PATH,"prop_spawnmenu2 %s %d",
			engine->Cmd_Argv(1),index+mSize);
		next->SetString("command",szBuf);
	}

	if(index)
	{
		KeyValues* back = msg->FindKey("9",true);
		back->SetString("msg","Back");
		Q_snprintf(szBuf,MAX_PATH,"prop_spawnmenu2 %s %d",
			engine->Cmd_Argv(1),index-mSize);
		back->SetString("command",szBuf);
	}

	helpers->CreateMessage(pPly,DIALOG_MENU,msg,this);
	msg->deleteThis();
	return PLUGIN_STOP;
}

PLUGIN_RESULT CPluginProps::ClientCommand(edict_t* pPly)
{
	const char* pCmd = engine->Cmd_Argv(0);
	const int iSize = 9;
	const int mSize = 7;
	char szBuf[MAX_PATH];

	if(FStrEq(pCmd,"prop_spawn"))
	{
		if(engine->Cmd_Argc()<2)
			return PLUGIN_CONTINUE;
		CBaseEntity* pPlayer = pPly->
			GetIServerEntity()->GetBaseEntity();
		if(!pPlayer) return PLUGIN_CONTINUE;

		int iProp = FindModel(engine->Cmd_Argv(1));
		if(iProp == -1) return PLUGIN_STOP;
		prop_t& pr = m_Props[iProp];

		Ray_t ray;
		trace_t tr;
		Vector vForward,vEye;
		QAngle* pAng = GetEntityEyes(pPlayer);
		AngleVectors(*pAng,&vForward);
		GetEyePosition(pPlayer,&vEye);

		CLocalTraceFilter filter(pPlayer);
		ray.Init(vEye,vEye+(vForward*MAX_TRACE_LENGTH),pr.hull_min,pr.hull_max);
		enginetrace->TraceRay(ray,MASK_SOLID,&filter,&tr);

		if(tr.fraction != 1.0)
			SpawnProp(tr.endpos,pr);

		if(engine->Cmd_Argc() == 4)
		{
			ShowPropMenu(pPly,atoi(engine->Cmd_Argv(2)),
				atoi(engine->Cmd_Argv(3)));
		}
		return PLUGIN_STOP;
	}
	else if(FStrEq(pCmd,"prop_spawnmenu"))
	{
		int index = 0;
		if(engine->Cmd_Argc() == 2)
			index = atoi(engine->Cmd_Argv(1));
		KeyValues* msg;
		
		msg = new KeyValues("menu");
		msg->SetString("title","Spawn menu");
		msg->SetString("msg","Spawn menu");
		msg->SetInt("level",4);
		msg->SetInt("time",9999);
		msg->SetColor("color",Color(255,255,0,255));

		for(int i = index; i < MIN((index+mSize),m_Groups.Count()); i++)
		{
			Q_snprintf(szBuf,MAX_PATH,"%d",i-index+1);
			KeyValues* pVal = msg->FindKey(szBuf,true);
			pVal->SetString("msg",m_Groups[i].String());
			Q_snprintf(szBuf,MAX_PATH,"prop_spawnmenu2 %d",i);
			pVal->SetString("command",szBuf);
		}

		if((m_Groups.Count()-index)>=mSize)
		{
			KeyValues* next = msg->FindKey("8",true);
			Q_snprintf(szBuf,MAX_PATH,"prop_spawnmenu %d",index+mSize);
			next->SetString("msg","Next");
			next->SetString("command",szBuf);
		}

		if(index)
		{
			KeyValues* back = msg->FindKey("9",true);
			Q_snprintf(szBuf,MAX_PATH,"prop_spawnmenu %d",index-mSize);
			back->SetString("msg","Back");
			back->SetString("command",szBuf);
		}

		helpers->CreateMessage(pPly,DIALOG_MENU,msg,this);
		msg->deleteThis();
		return PLUGIN_STOP;
	}
	else if(FStrEq(pCmd,"prop_spawnmenu2"))
	{
		if(engine->Cmd_Argc()<2)
			return PLUGIN_CONTINUE;
		if(engine->Cmd_Argc()==2)
			return ShowPropMenu(pPly,atoi(engine->Cmd_Argv(1)),0);
		if(engine->Cmd_Argc()>2)
		{
			return ShowPropMenu(pPly,atoi(engine->Cmd_Argv(1)),
					atoi(engine->Cmd_Argv(2)));
		}
		return PLUGIN_STOP;
	}
	return PLUGIN_CONTINUE;
}

int CPluginProps::FindModel(const char* pName)
{
	for(int i = 0; i < m_Props.Count(); i++)
	{
		if(FStrEq(m_Props[i].name.String(),pName))
			return i;
	}
	return -1;
}

void EntityKeyValue(CBaseEntity* pEnt,
	const char* pKey,const char* pValue)
{
	__asm
	{
		push ecx
		push edx

		push [pValue]
		push [pKey]
		mov ecx,[pEnt]
		mov edx,[ecx]
		call dword ptr [edx+0x78]

		pop edx
		pop ecx
	}
}

void CPluginProps::SpawnProp(Vector vPos,prop_t& pr)
{
	char szBuf[MAX_PATH];
	CPhysicsProp* pProp;
	bool bPrecace = *pAllowPrecache;
	*pAllowPrecache = true;

	pProp = (CPhysicsProp*)pCEBN("prop_physics",-1);
	if(pProp)
	{
		//%.10f %.10f %.10f
		Q_snprintf(szBuf,sizeof(szBuf),"%.10f %.10f %.10f",vPos.x,vPos.y,vPos.z);
		EntityKeyValue(pProp,"origin",szBuf);
		Q_snprintf(szBuf,sizeof(szBuf),"%.10f %.10f %.10f",0.0f,0.0f,0.0f);
		EntityKeyValue(pProp,"angles",szBuf);

		szBuf[0] = '\0';
		strcpy(szBuf,"models/");
		strcat_s(szBuf,pr.name.String());
		EntityKeyValue(pProp, "model", szBuf );
		EntityKeyValue(pProp, "solid","6");
		EntityKeyValue(pProp, "fademindist","-1");
		EntityKeyValue(pProp, "fademaxdist","0");
		EntityKeyValue(pProp, "fadescale","1");
		EntityKeyValue(pProp, "inertiaScale","1.0");
		EntityKeyValue(pProp, "physdamagescale","0.1");
		pProp->Precache();
		pDP(pProp);
		pProp->Activate();
	}
	*pAllowPrecache = false;
}