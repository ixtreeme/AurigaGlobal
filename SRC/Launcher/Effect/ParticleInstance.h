#pragma once

#include "../Render/GrpBase.h"
#include "../Render/Pool.h"

class CParticleProperty;
class CEmitterProperty;

class CParticleInstance
{
	friend class CParticleSystemData;
	friend class CParticleSystemInstance;


	public:
		CParticleInstance();
		~CParticleInstance();

		float GetRadiusApproximation();

		bool Update(float fElapsedTime, float fAngle);

private:
	void UpdateRotation(float time, float elapsedTime);
	void UpdateTextureAnimation(float time, float elapsedTime);
	void UpdateScale(float time, float elapsedTime);
	void UpdateColor(float time, float elapsedTime);
	void UpdateGravity(float time, float elapsedTime);
	void UpdateAirResistance(float time, float elapsedTime);

	protected:
		//float				m_fLifePercentage;
		D3DXVECTOR3			m_v3StartPosition;

		D3DXVECTOR3			m_v3Position;
		D3DXVECTOR3			m_v3LastPosition;
		D3DXVECTOR3			m_v3Velocity;

		D3DXVECTOR2			m_v2HalfSize;
		D3DXVECTOR2			m_v2Scale;

		float				m_fRotation;
#ifdef WORLD_EDITOR
		D3DXCOLOR			m_Color;
#else
		D3DXCOLOR			m_dcColor;
#endif

		uint8_t				m_byTextureAnimationType;
		float				m_fLastFrameTime;
		uint8_t				m_byFrameIndex;
		float				m_fFrameTime;

		float				m_fLifeTime;
		float				m_fLastLifeTime;

		CParticleProperty *	m_pParticleProperty;
		CEmitterProperty *	m_pEmitterProperty;

		uint8_t				m_rotationType;

		float m_fAirResistance;
		float m_fRotationSpeed;
		float m_fGravity;


	public:
		static CParticleInstance* New();
		static void DestroySystem();

		void Transform(const D3DXMATRIX * c_matLocal= nullptr);
		void Transform(const D3DXMATRIX * c_matLocal, const float c_fZRotation);

		TPTVertex * GetParticleMeshPointer();

		void DeleteThis();

		void Destroy();

	protected:
		void __Initialize();
		TPTVertex			m_ParticleMesh[4];
	public:
		static CDynamicPool<CParticleInstance> ms_kPool;

};
