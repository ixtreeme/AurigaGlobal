#include "StdAfx.h"
#include "PythonBackground.h"
#include "../Render/Camera.h"

//////////////////////////////////////////////////////////////////////////
// ¸ŢĽĽÁö

extern void SetHeightLog(bool isLog);

float CCamera::CAMERA_MIN_DISTANCE = 200.0f;
float CCamera::CAMERA_MAX_DISTANCE = 2500.0f;

void CCamera::ProcessTerrainCollision()
{
	CPythonBackground & rPythonBackground = CPythonBackground::Instance();
	D3DXVECTOR3 v3CollisionPoint;

	if (rPythonBackground.GetPickingPointWithRayOnlyTerrain(m_kTargetToCameraBottomRay, &v3CollisionPoint))
	{
		SetCameraState(CAMERA_STATE_CANTGODOWN);
		D3DXVECTOR3 v3CheckVector = m_v3Eye - 2.0f * m_fTerrainCollisionRadius * m_v3Up;
		v3CheckVector.z = rPythonBackground.GetHeight(floorf(v3CheckVector.x), floorf(v3CheckVector.y));
		D3DXVECTOR3 v3NewEye = v3CheckVector + 2.0f * m_fTerrainCollisionRadius * m_v3Up;
		if (v3NewEye.z > m_v3Eye.z)
		{
			//printf("ToCameraBottom(%f, %f, %f) TCR %f, UP(%f, %f, %f), new %f > old %f",
			//	v3CheckVector.x, v3CheckVector.y, v3CheckVector.z,
			//	m_fTerrainCollisionRadius,
			//	m_v3Up.x, m_v3Up.y, m_v3Up.z,
			//	v3NewEye.z, m_v3Eye.z);
			SetEye(v3NewEye);
		}
		/*
		SetCameraState(CAMERA_STATE_NORMAL);
		D3DXVECTOR3 v3NewEye = v3CollisionPoint;
		SetEye(v3NewEye);
		*/
	}
	else
		SetCameraState(CAMERA_STATE_NORMAL);

	if (rPythonBackground.GetPickingPointWithRayOnlyTerrain(m_kCameraBottomToTerrainRay, &v3CollisionPoint))
	{
		SetCameraState(CAMERA_STATE_CANTGODOWN);
		auto eyecol = m_v3Eye - v3CollisionPoint;
		if (D3DXVec3Length(&eyecol) < 2.0f * m_fTerrainCollisionRadius)
		{
			D3DXVECTOR3 v3NewEye = v3CollisionPoint + 2.0f * m_fTerrainCollisionRadius * m_v3Up;
			//printf("CameraBottomToTerrain new %f > old %f", v3NewEye.z, m_v3Eye.z);
			SetEye(v3NewEye);
		}
	}
	else
		SetCameraState(CAMERA_STATE_NORMAL);
/*
	if (rPythonBackground.GetPickingPointWithRayOnlyTerrain(m_kCameraFrontToTerrainRay, &v3CollisionPoint))
	{
		if (D3DXVec3Length(&(m_v3Eye - v3CollisionPoint)) < 4.0f * m_fTerrainCollisionRadius)
		{
			D3DXVECTOR3 v3NewEye = v3CollisionPoint - 4.0f * m_fTerrainCollisionRadius * m_v3View;
			//printf("CameraFrontToTerrain new %f > old %f", v3NewEye.z, m_v3Eye.z);
			SetEye(v3NewEye);
		}
	}

	if (rPythonBackground.GetPickingPointWithRayOnlyTerrain(m_kCameraBackToTerrainRay, &v3CollisionPoint))
	{
		if (D3DXVec3Length(&(m_v3Eye - v3CollisionPoint)) < m_fTerrainCollisionRadius)
		{
			D3DXVECTOR3 v3NewEye = v3CollisionPoint + m_fTerrainCollisionRadius * m_v3View;
			//printf("CameraBackToTerrain new %f > old %f", v3NewEye.z, m_v3Eye.z);
			SetEye(v3NewEye);
		}
	}

	// Left
	if (rPythonBackground.GetPickingPointWithRayOnlyTerrain(m_kCameraLeftToTerrainRay, &v3CollisionPoint))
	{
		SetCameraState(CAMERA_STATE_CANTGOLEFT);
		if (D3DXVec3Length(&(m_v3Eye - v3CollisionPoint)) < 3.0f * m_fTerrainCollisionRadius)
		{
			D3DXVECTOR3 v3NewEye = v3CollisionPoint + 3.0f * m_fTerrainCollisionRadius * m_v3Cross;
			//printf("CameraLeftToTerrain new %f > old %f", v3NewEye.z, m_v3Eye.z);
			SetEye(v3NewEye);
		}
	}
	else
		SetCameraState(CAMERA_STATE_NORMAL);

	// Right
	if (rPythonBackground.GetPickingPointWithRayOnlyTerrain(m_kCameraRightToTerrainRay, &v3CollisionPoint))
	{
		SetCameraState(CAMERA_STATE_CANTGORIGHT);
		if (D3DXVec3Length(&(m_v3Eye - v3CollisionPoint)) < 3.0f * m_fTerrainCollisionRadius)
		{
			D3DXVECTOR3 v3NewEye = v3CollisionPoint - 3.0f * m_fTerrainCollisionRadius * m_v3Cross;
			//printf("CameraRightToTerrain new %f > old %f", v3NewEye.z, m_v3Eye.z);
			SetEye(v3NewEye);
		}
	}
	else
		SetCameraState(CAMERA_STATE_NORMAL);
	*/
}

struct CameraCollisionChecker
{
	bool m_isBlocked;
	std::vector<D3DXVECTOR3>* m_pkVct_v3Position;
	CDynamicSphereInstance* m_pdsi;

	CameraCollisionChecker(CDynamicSphereInstance* pdsi, std::vector<D3DXVECTOR3>* pkVct_v3Position) : m_isBlocked(false), m_pkVct_v3Position(pkVct_v3Position), m_pdsi(pdsi)
	{
	}
	void operator () (CGraphicObjectInstance* pOpponent)
	{
		if (pOpponent->CollisionDynamicSphere(*m_pdsi))
		{
			m_pkVct_v3Position->push_back(pOpponent->GetPosition());
			m_isBlocked = true;
		}
	}
};


//#include <directxsdk/d3dx9.h>
#include <vector>

//void CCamera::ProcessBuildingCollision()
//{
//	constexpr float fMoveAmountSmall = 2.0f;
//	constexpr float fMoveAmountLarge = 4.0f;
//	constexpr float frictionFactor = 0.85f;  // Súrlódási tényező
//	constexpr float bounceDamping = 0.5f;     // Visszapattanás csillapítása
//	constexpr float steepAngleLimit = 0.7f;   // Ha a normál túl meredek, nem csúszunk felfelé (1.0 = teljesen felfelé)
//
//	CDynamicSphereInstance s;
//	s.fRadius = m_fObjectCollisionRadius;
//	s.v3LastPosition = m_v3Eye;
//
//	Vector3d aVector3d;
//	aVector3d.Set(m_v3Eye.x, m_v3Eye.y, m_v3Eye.z);
//
//	CCullingManager& rkCullingMgr = CCullingManager::Instance();
//
//	// Segédfüggvény az ütközések kezelésére (jobb csúszással és stabilizálással)
//	auto CheckCollisionAndAdjustVelocity = [&](const D3DXVECTOR3& direction, float moveAmount, char axis) {
//		D3DXVECTOR3 checkPos;
//		auto ix = D3DXVECTOR3(direction * m_fObjectCollisionRadius);
//		D3DXVec3Add(&checkPos, &m_v3Eye, &ix);
//		s.v3Position = checkPos;
//
//		std::vector<D3DXVECTOR3> kVct_kPosition;
//		CameraCollisionChecker kCameraCollisionChecker(&s, &kVct_kPosition);
//		rkCullingMgr.ForInRange(aVector3d, m_fObjectCollisionRadius, &kCameraCollisionChecker);
//
//		if (kCameraCollisionChecker.m_isBlocked && !kVct_kPosition.empty())
//		{
//			D3DXVECTOR3 surfaceNormal = kVct_kPosition[0] - m_v3Eye;
//			D3DXVec3Normalize(&surfaceNormal, &surfaceNormal);
//
//			// 📌 **Ne csússzon fel meredek normálokon**
//			if (surfaceNormal.y > steepAngleLimit) {
//				return; // Ha a normál túl meredek, akkor ne csúszunk felfelé.
//			}
//
//			// 📌 **Ne pattanjon vissza túl erősen**
//			if (axis == 'x') m_v3AngularVelocity.x = -m_v3AngularVelocity.x * bounceDamping;
//			if (axis == 'y') m_v3AngularVelocity.y = -m_v3AngularVelocity.y * bounceDamping;
//			if (axis == 'z') m_v3AngularVelocity.z = -m_v3AngularVelocity.z * bounceDamping;
//
//			// 📌 **Jobb csúszási irány kiszámítása**
//			D3DXVECTOR3 slideVector;
//			D3DXVec3Cross(&slideVector, &surfaceNormal, &direction);
//			D3DXVec3Normalize(&slideVector, &slideVector);
//
//			// Finom csúsztatás az akadály mentén
//			if (axis == 'x') m_v3AngularVelocity.x += moveAmount * slideVector.x;
//			if (axis == 'y') m_v3AngularVelocity.y += moveAmount * slideVector.y;
//			if (axis == 'z') m_v3AngularVelocity.z += moveAmount * slideVector.z;
//
//			// 📌 **Csillapítás, hogy ne remegjen a kamera**
//			m_v3AngularVelocity.x *= frictionFactor;
//			m_v3AngularVelocity.y *= frictionFactor;
//			m_v3AngularVelocity.z *= frictionFactor;
//		}
//		};
//
//	// 🔄 **Ütközések ellenőrzése különböző irányokban**
//	CheckCollisionAndAdjustVelocity(-m_v3View, fMoveAmountLarge, 'y');  // Előre
//	CheckCollisionAndAdjustVelocity(m_v3Up * 2.0f, -fMoveAmountSmall, 'z');    // Felfelé
//	CheckCollisionAndAdjustVelocity(m_v3Cross * 3.0f, fMoveAmountLarge, 'y');  // Oldalra (jobbra)
//	CheckCollisionAndAdjustVelocity(m_v3Cross * -3.0f, fMoveAmountLarge, 'y'); // Oldalra (balra)
//	CheckCollisionAndAdjustVelocity(m_v3Up * -2.0f, fMoveAmountLarge, 'y');    // Lefelé
//	CheckCollisionAndAdjustVelocity(m_v3View * 4.0f, fMoveAmountLarge, 'z');   // Hátra
//}





void CCamera::Update()
{
	// 🛠️ **1. Ütközésdetektálás (épületek és terep)**
	//ProcessBuildingCollision();

	// 🛠️ **2. Kamera forgatás (x és z tengelyek helyes cseréje)**
	RotateEyeAroundTarget(m_v3AngularVelocity.z, m_v3AngularVelocity.x);

	// 🛠️ **3. Kamera távolságának beállítása, hogy ne lépje túl a korlátokat**
	float fNewDistance = fMAX(CAMERA_MIN_DISTANCE, fMIN(CAMERA_MAX_DISTANCE, GetDistance() - m_v3AngularVelocity.y));
	SetDistance(fNewDistance);

	// 🛠️ **4. Terep ütközés ellenőrzése (ha be van kapcsolva)**
	if (m_bProcessTerrainCollision)
		ProcessTerrainCollision();

	// 🛠️ **5. Finomabb sebességcsillapítás (exponenciális helyett lineáris lassítás)**
	constexpr float frictionFactor = 0.85f; // Kevésbé drasztikus csillapítás
	m_v3AngularVelocity *= frictionFactor;

	if (fabs(m_v3AngularVelocity.x) < 0.5f)
		m_v3AngularVelocity.x = 0.0f;
	if (fabs(m_v3AngularVelocity.y) < 0.5f)
		m_v3AngularVelocity.y = 0.0f;
	if (fabs(m_v3AngularVelocity.z) < 0.5f)
		m_v3AngularVelocity.z = 0.0f;

	// 🛠️ **6. Kamera célpont magasságának finomabb interpolációja**
	const float CAMERA_MOVABLE_DISTANCE = CAMERA_MAX_DISTANCE - CAMERA_MIN_DISTANCE;
	const float CAMERA_TARGET_DELTA = CAMERA_TARGET_FACE - CAMERA_TARGET_STANDARD;

	float fCameraCurMovableDistance = CAMERA_MAX_DISTANCE - GetDistance();
	float fTargetHeight = CAMERA_TARGET_STANDARD + CAMERA_TARGET_DELTA * (fCameraCurMovableDistance / CAMERA_MOVABLE_DISTANCE);

	// **Simított átmenet a kamera célpont magasságára**
	float fCurrentHeight = GetTargetHeight();
	float fNewTargetHeight = fCurrentHeight + (fTargetHeight - fCurrentHeight) * 0.1f; // Lágy átmenet
	SetTargetHeight(fNewTargetHeight);
}


