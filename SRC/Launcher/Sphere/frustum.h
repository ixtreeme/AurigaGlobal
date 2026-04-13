#pragma once

#include "vector.h"

enum ViewState
{
	VS_INSIDE,   // completely inside the frustum.
	VS_PARTIAL,  // partially inside and partially outside the frustum.
	VS_OUTSIDE   // completely outside the frustum
};

class Frustum
{
	public:
		void BuildViewFrustum(const D3DXMATRIX & mat);
		void BuildViewFrustum2(const D3DXMATRIX & mat, float fNear, float fFar, float fFov, float fAspect, const D3DXVECTOR3 & vCamera, const D3DXVECTOR3 & vLook);
		ViewState ViewVolumeTest(const Vector3d &c_v3Center,const float c_fRadius) const;

	private:
		bool m_bUsingSphere = false;
		D3DXVECTOR3 m_v3Center;
		float m_fRadius = 0.0f;
		D3DXPLANE m_plane[6];
};
