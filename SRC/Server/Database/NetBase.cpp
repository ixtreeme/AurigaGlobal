#include "stdafx.h"
#include "NetBase.h"
#include "Config.h"
#include "ClientManager.h"

CNetBase::CNetBase()
{
}

CNetBase::~CNetBase()
{
}

CNetPoller::CNetPoller()
{
}

CNetPoller::~CNetPoller()
{
	Destroy();
}

bool CNetPoller::Create()
{
	LOG_TRACE("NetPoller::Create()");

	return true;
}

void CNetPoller::Destroy()
{
	LOG_TRACE("NetPoller::Destroy()");

	thecore_destroy();
}
