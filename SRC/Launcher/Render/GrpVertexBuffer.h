#pragma once

#include "GrpBase.h"

class CGraphicVertexBuffer : public CGraphicBase
{
	public:
		CGraphicVertexBuffer();
		virtual ~CGraphicVertexBuffer();

		void	Destroy();
		virtual bool	Create(int vtxCount, uint32_t fvf, uint32_t usage, D3DPOOL d3dPool);

		bool	CreateDeviceObjects();
		void	DestroyDeviceObjects();

		bool	Copy(int bufSize, const void* srcVertices);

		bool	LockRange(unsigned count, void** pretVertices) const;
		bool	Lock(void** pretVertices) const;
		bool	Unlock() const;

		bool	LockDynamic(void** pretVertices);
		virtual bool	Lock(void** pretVertices);
		bool	Unlock();

		void	SetStream(int stride, int layer=0) const;

		int		GetVertexCount() const;
		int		GetVertexStride() const;
		uint32_t	GetFlexibleVertexFormat() const;

		LPDIRECT3DVERTEXBUFFER9 GetD3DVertexBuffer() const	{ return m_lpd3dVB; }
		uint32_t GetBufferSize() const	{ return m_dwBufferSize; }

		bool	IsEmpty() const;

	protected:
		void	Initialize();

	protected:
		LPDIRECT3DVERTEXBUFFER9 m_lpd3dVB;

		uint32_t					m_dwBufferSize;
		uint32_t					m_dwFVF;
		uint32_t					m_dwUsage;
		D3DPOOL					m_d3dPool;
		int						m_vtxCount;
		uint32_t					m_dwLockFlag;
};
