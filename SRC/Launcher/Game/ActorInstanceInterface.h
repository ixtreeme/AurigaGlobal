#pragma once

#include "../Granny/ThingInstance.h"

class IActorInstance : public CGraphicThingInstance
{
public:
	enum
	{
		ID = ACTOR_OBJECT
	};
	int GetType() const override { return ID; }

	IActorInstance() = default;
	~IActorInstance() override = default;
	virtual bool TestCollisionWithDynamicSphere(const CDynamicSphereInstance & dsi) = 0;
	virtual uint32_t GetVirtualID() = 0;
};