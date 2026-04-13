#include "StdAfx.h"
#include "../Render/GrpExpandedImageInstance.h"
#include "../Render/GrpTextInstance.h"
#include "../Render/GrpMarkInstance.h"
#include "../Render/GrpSubImage.h"
#include "../Render/GrpText.h"
#include "../Render/AttributeData.h"
#include "../Granny/Thing.h"
#include "../Granny/ThingInstance.h"
#include "../Effect/EffectMesh.h"
#include "../Effect/EffectInstance.h"
#include "../Game/WeaponTrace.h"
#include "../Game/MapType.h"
#include "../Game/GameType.h"
#include "../Game/RaceData.h"
#include "../Game/RaceMotionData.h"
#include "../Game/ActorInstance.h"
#include "../Game/Area.h"
#include "../Game/ItemData.h"
#include "../Game/FlyingData.h"
#include "../Game/FlyTrace.h"
#include "../Game/FlyingInstance.h"
#include "../Game/FlyingData.h"

#include "Resource.h"

CResource * NewImage(const char* c_szFileName)
{
	return new CGraphicImage(c_szFileName);
}

CResource * NewSubImage(const char* c_szFileName)
{
	return new CGraphicSubImage(c_szFileName);
}

CResource * NewText(const char* c_szFileName)
{
	return new CGraphicText(c_szFileName);
}

CResource * NewThing(const char* c_szFileName)
{
	return new CGraphicThing(c_szFileName);
}

CResource * NewEffectMesh(const char* c_szFileName)
{
	return new CEffectMesh(c_szFileName);
}

CResource * NewAttributeData(const char* c_szFileName)
{
	return new CAttributeData(c_szFileName);
}

void CPythonResource::DumpFileList(const char * c_szFileName)
{
	m_resManager.DumpFileListToTextFile(c_szFileName);
}

void CPythonResource::Destroy()
{
	CFlyingInstance::DestroySystem();
	CActorInstance::DestroySystem();
	CArea::DestroySystem();
	CGraphicExpandedImageInstance::DestroySystem();
	CGraphicImageInstance::DestroySystem();
	CGraphicMarkInstance::DestroySystem();
	CGraphicThingInstance::DestroySystem();
	CGrannyModelInstance::DestroySystem();
	CGraphicTextInstance::DestroySystem();
	CEffectInstance::DestroySystem();
	CWeaponTrace::DestroySystem();
	CFlyTrace::DestroySystem();

	m_resManager.DestroyDeletingList();

	CFlyingData::DestroySystem();
	CItemData::DestroySystem();
	CEffectData::DestroySystem();
	CEffectMesh::SEffectMeshData::DestroySystem();
	CRaceData::DestroySystem();
	NRaceData::DestroySystem();
	CRaceMotionData::DestroySystem();

	m_resManager.Destroy();
}

CPythonResource::CPythonResource()
{
	m_resManager.RegisterResourceNewFunctionPointer("sub", NewSubImage);
	m_resManager.RegisterResourceNewFunctionPointer("dds", NewImage);
	m_resManager.RegisterResourceNewFunctionPointer("jpg", NewImage);
	m_resManager.RegisterResourceNewFunctionPointer("tga", NewImage);
	m_resManager.RegisterResourceNewFunctionPointer("png", NewImage);
	m_resManager.RegisterResourceNewFunctionPointer("psd", NewImage);//Razor93
	m_resManager.RegisterResourceNewFunctionPointer("bmp", NewImage);
	m_resManager.RegisterResourceNewFunctionPointer("fnt", NewText);
	m_resManager.RegisterResourceNewFunctionPointer("gr2", NewThing);
	m_resManager.RegisterResourceNewFunctionPointer("mde", NewEffectMesh);
	m_resManager.RegisterResourceNewFunctionPointer("mdatr", NewAttributeData);
}

CPythonResource::~CPythonResource()
{
}
