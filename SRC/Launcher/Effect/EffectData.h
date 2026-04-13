#pragma once

#include "../AudioLib/Type.h"

#include "ParticleSystemData.h"
#include "EffectMesh.h"
#include "SimpleLightData.h"

class CEffectData
{
	public:
		typedef std::vector<CParticleSystemData*>	TParticleVector;
		typedef std::vector<CEffectMeshScript*>		TMeshVector;
		typedef std::vector<CLightData*>			TLightVector;

	public:
		CEffectData();
		~CEffectData();

		void							Clear();
		bool							LoadScript(const char * c_szFileName);
		bool							LoadSoundScriptData(const char * c_szFileName);

		[[nodiscard]] uint32_t						GetParticleCount() const;
		[[nodiscard]] CParticleSystemData *			GetParticlePointer(uint32_t dwPosition) ;

		[[nodiscard]] uint32_t						GetMeshCount() const;
		[[nodiscard]] CEffectMeshScript *			GetMeshPointer(uint32_t dwPosition) const;

		[[nodiscard]] uint32_t						GetLightCount() const;
		[[nodiscard]] CLightData *					GetLightPointer(uint32_t dwPosition) const;
		
		NSound::TSoundInstanceVector *	GetSoundInstanceVector();

		[[nodiscard]] float							GetBoundingSphereRadius() const;
		[[nodiscard]] D3DXVECTOR3						GetBoundingSpherePosition() const;

		[[nodiscard]] const char *					GetFileName() const;

	protected:
		void __ClearParticleDataVector();
		void __ClearLightDataVector();
		void __ClearMeshDataVector();

		virtual CParticleSystemData *	AllocParticle();
		virtual CEffectMeshScript *		AllocMesh();
		virtual CLightData *			AllocLight();

	protected:
		TParticleVector					m_ParticleVector;
		TMeshVector						m_MeshVector;
		TLightVector					m_LightVector;
		NSound::TSoundInstanceVector	m_SoundInstanceVector;

		float							m_fBoundingSphereRadius;
		D3DXVECTOR3						m_v3BoundingSpherePosition;

		std::string						m_strFileName;

	public:
		static void DestroySystem();

		static CEffectData* New();
		static void Delete(CEffectData* pkData);

		static CDynamicPool<CEffectData> ms_kPool;
};