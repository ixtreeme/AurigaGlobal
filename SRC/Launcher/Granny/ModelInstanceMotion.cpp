#include "StdAfx.h"
#include "ModelInstance.h"
#include "Model.h"

void CGrannyModelInstance::CopyMotion(CGrannyModelInstance* pModelInstance, bool bIsFreeSourceControl)
{
	if (!pModelInstance->IsMotionPlaying())
		return;

	if (m_pgrnCtrl)
		GrannyFreeControl(m_pgrnCtrl);

	const float localTime = GetLocalTime();
	m_pgrnAni = pModelInstance->m_pgrnAni;
	m_pgrnCtrl = GrannyPlayControlledAnimation(localTime, m_pgrnAni, m_pgrnModelInstance);

	if (!m_pgrnCtrl)
		return;

	GrannySetControlSpeed(m_pgrnCtrl, GrannyGetControlSpeed(pModelInstance->m_pgrnCtrl));
	GrannySetControlLoopCount(m_pgrnCtrl, GrannyGetControlLoopCount(pModelInstance->m_pgrnCtrl));

	GrannySetControlEaseIn(m_pgrnCtrl, true);
	GrannySetControlEaseOut(m_pgrnCtrl, false);

	GrannySetControlRawLocalClock(m_pgrnCtrl, GrannyGetControlRawLocalClock(pModelInstance->m_pgrnCtrl));

	GrannyFreeControlOnceUnused(m_pgrnCtrl);

	if (bIsFreeSourceControl)
	{
		GrannyFreeControl(pModelInstance->m_pgrnCtrl);
		pModelInstance->m_pgrnCtrl = nullptr;
	}
}

bool CGrannyModelInstance::IsMotionPlaying() const
{
	if (!m_pgrnCtrl)
		return false;

	if (GrannyControlIsComplete(m_pgrnCtrl))
		return false;

	return true;
}

void CGrannyModelInstance::SetMotionPointer(const std::shared_ptr<CGrannyMotion> pMotion, float blendTime, int loopCount, float speedRatio)
{
	if (!m_pgrnWorldPoseReal)
		return;

	granny_model_instance* pgrnModelInstance = m_pgrnModelInstance;
	if (!pgrnModelInstance)
		return;

	const float localTime = GetLocalTime();

	bool isFirst = false;
	if (m_pgrnCtrl)
	{
		GrannySetControlEaseOutCurve(m_pgrnCtrl, localTime, localTime + blendTime, 1.0f, 1.0f, 0.0f, 0.0f);
		GrannySetControlEaseIn(m_pgrnCtrl, false);
		GrannySetControlEaseOut(m_pgrnCtrl, true);
		GrannyCompleteControlAt(m_pgrnCtrl, localTime + blendTime);
		GrannyFreeControlIfComplete(m_pgrnCtrl);
	}
	else
	{
		isFirst = true;
	}

	m_pgrnAni = pMotion->GetGrannyAnimationPointer();
	m_pgrnCtrl = GrannyPlayControlledAnimation(localTime, m_pgrnAni, pgrnModelInstance);
	if (!m_pgrnCtrl)
		return;

	GrannySetControlSpeed(m_pgrnCtrl, speedRatio);
	GrannySetControlLoopCount(m_pgrnCtrl, loopCount);

	if (isFirst)
	{
		GrannySetControlEaseIn(m_pgrnCtrl, false);
		GrannySetControlEaseOut(m_pgrnCtrl, false);
	}
	else
	{
		GrannySetControlEaseIn(m_pgrnCtrl, true);
		GrannySetControlEaseOut(m_pgrnCtrl, false);
		if (blendTime > 0.0f)
			GrannySetControlEaseInCurve(m_pgrnCtrl, localTime, localTime + blendTime, 0.0f, 0.0f, 1.0f, 1.0f);
	}
	GrannyFreeControlOnceUnused(m_pgrnCtrl);
}

void CGrannyModelInstance::ChangeMotionPointer(const std::shared_ptr<CGrannyMotion> pMotion, int loopCount, float speedRatio)
{
	granny_model_instance* pgrnModelInstance = m_pgrnModelInstance;
	if (!pgrnModelInstance)
		return;

	constexpr float fSkipTime = 0.3f;
	const float localTime = GetLocalTime() - fSkipTime;

	if (m_pgrnCtrl)
	{
		GrannySetControlEaseIn(m_pgrnCtrl, false);
		GrannySetControlEaseOut(m_pgrnCtrl, false);
		GrannyCompleteControlAt(m_pgrnCtrl, localTime);
		GrannyFreeControlIfComplete(m_pgrnCtrl);
	}

	m_pgrnAni = pMotion->GetGrannyAnimationPointer();
	granny_model_instance* InstanceOfModel = pgrnModelInstance;
	granny_animation* Animation = m_pgrnAni;
	granny_real32 StartTime = localTime;

	granny_controlled_animation_builder* Builder =
		GrannyBeginControlledAnimation(StartTime, Animation);
	if (Builder)
	{
		granny_int32x TrackGroupIndex;
		if (GrannyFindTrackGroupForModel(Animation,
			GrannyGetSourceModel(InstanceOfModel)->Name,
			&TrackGroupIndex))
		{
			GrannySetTrackGroupLOD(Builder, TrackGroupIndex, true, 1.0f);
			GrannySetTrackGroupTarget(Builder, TrackGroupIndex, InstanceOfModel);
		}
		else {
			GrannySetTrackGroupLOD(Builder, 1.0f, true, 1.0f);

			GrannySetTrackGroupTarget(Builder, 0, InstanceOfModel);
		}
		m_pgrnCtrl = GrannyEndControlledAnimation(Builder);

	}
	if (!m_pgrnCtrl)
		return;

	GrannySetControlSpeed(m_pgrnCtrl, speedRatio);
	GrannySetControlLoopCount(m_pgrnCtrl, loopCount);
	GrannySetControlEaseIn(m_pgrnCtrl, false);
	GrannySetControlEaseOut(m_pgrnCtrl, false);

	GrannyFreeControlOnceUnused(m_pgrnCtrl);
}

void CGrannyModelInstance::SetMotionAtEnd() const
{
	if (!m_pgrnCtrl)
		return;

	const float endingTime = GrannyGetControlLocalDuration(m_pgrnCtrl);
	GrannySetControlRawLocalClock(m_pgrnCtrl, endingTime);
}
