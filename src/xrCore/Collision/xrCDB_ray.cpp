#include "stdafx.h"

#include <embree4/rtcore_ray.h>
#include <embree4/rtcore_scene.h>
#include <embree4/rtcore_common.h>

#include "xrCDB.h"
#include "override/Model.h"
#include "cl_intersect.h"
using namespace CDB;
using namespace Opcode;

struct cform_ray_collider final
{
	Fvector pos, fwd_dir;
  	COLLIDER* dest;
	const xr_vector<TRI>& tris;
	const xr_vector<Fvector>& verts;
	float rRange, rRange2;

	bool bCull = false;
	bool bFirst = false;
	bool bNearest = false;

	ICF void _prim(size_t prim)
	{
		float u,v,r;
		auto& Tri = tris[prim];
		auto& TriVerts = Tri.verts;
		Fvector tri_verts[3] = { verts[TriVerts[0]], verts[TriVerts[1]], verts[TriVerts[2]] };

		if (!TestRayTri(pos, fwd_dir, tri_verts, u, v, r, bCull))
			return;

		if (r<=0 || r>rRange)
			return;

		u32 dummy = Tri.dummy;
		if (bNearest)	
		{
			if (dest->r_count())	
			{
				RESULT& R = *dest->r_begin();
				if (r<R.range)
				{
					R.id		= prim;
					R.range		= r;
					R.u			= u;
					R.v			= v;
					R.verts	[0]	= tri_verts[0];
					R.verts	[1]	= tri_verts[1];
					R.verts	[2]	= tri_verts[2];
					R.dummy		= dummy;
					rRange		= r;
					rRange2		= r*r;
				}
			}
			else
			{
				RESULT& R	= dest->r_add();
				R.id		= prim;
				R.range		= r;
				R.u			= u;
				R.v			= v;
				R.verts	[0]	= tri_verts[0];
				R.verts	[1]	= tri_verts[1];
				R.verts	[2]	= tri_verts[2];
				R.dummy		= dummy;
				rRange		= r;
				rRange2		= r*r;
			}
		}
		else
 		{
			RESULT& R	= dest->r_add();				// �� ������� ������� RESULT
			R.id		= prim;
			R.range		= r;
			R.u			= u;
			R.v			= v;
			R.verts	[0]	= tri_verts[0];
			R.verts	[1]	= tri_verts[1];
			R.verts	[2]	= tri_verts[2];
			R.dummy		= dummy;
		}
	}

	void _stab(const AABBNoLeafNode* node)
	{
		Fvector& center = (Fvector&)node->mAABB.mCenter;
		Fvector& extents = (Fvector&)node->mAABB.mExtents;

		Fvector P;
		if (!Fbox(center-extents,center+extents).Pick2(pos, fwd_dir, P))
			return;
		
		if (P.distance_to_sqr(pos) > rRange2)
			return;

		// 1st chield
		if (node->HasPosLeaf())	_prim(node->GetPosPrimitive());
		else					_stab(node->GetPos());

		// Early exit for "only first"
		if (bFirst)
		{
			if (dest->r_count())
				return;
		}

		// 2nd chield
		if (node->HasNegLeaf())	_prim(node->GetNegPrimitive());
		else					_stab(node->GetNeg());
	}
};

void COLLIDER::ray_query(const MODEL* m_def, const Fvector& r_start, const Fvector& r_dir, float r_range)
{
	PROF_EVENT("COLLIDER::ray_query");
	if (!m_def || !m_def->InstaceScene)
	{
		return;
	}

	// Get nodes
	RTCRayHit ray;
	ray.ray.org_x = r_start.x;
	ray.ray.org_y = r_start.y;
	ray.ray.org_z = r_start.z;
	ray.ray.dir_x = r_dir.x;
	ray.ray.dir_y = r_dir.y;
	ray.ray.dir_z = r_dir.z;
	ray.ray.tnear = 0.0f;
	ray.ray.tfar = r_range;
	ray.ray.mask = -1;
	ray.ray.flags = 0;
	ray.hit.geomID = RTC_INVALID_GEOMETRY_ID;
	
	
	if(!!(ray_mode & OPT_ONLYFIRST))
	{
		RTCOccludedArguments args;
		rtcInitOccludedArguments(&args);
		rtcOccluded1(m_def->InstaceScene, &ray, &args);
	} else
	{
		RTCIntersectArguments args;
		rtcInitIntersectArguments(&args);
		struct Filter
		{
			static void Execute(const RTCFilterFunctionNArguments* args)
			{
				VERIFY(args->N == 1);
				auto model = (MODEL*)args->geometryUserPtr;
				
				
			}
		};
		args.filter = Filter::Execute;
		args.flags = RTC_RAY_QUERY_FLAG_INVOKE_ARGUMENT_FILTER;
		rtcIntersect1(m_def->InstaceScene, &ray, &args);
	}
	
	const AABBNoLeafTree* T = (const AABBNoLeafTree*)m_def->tree->GetTree();
	const AABBNoLeafNode* N = T->GetNodes();

	r_clear();
	r_vec().reserve(16);

	cform_ray_collider RC
	{
		r_start,
		r_dir,
		this,
		m_def->tris,
		m_def->verts,
		r_range,
		r_range*r_range,

		!!(ray_mode & OPT_CULL),
		!!(ray_mode & OPT_ONLYFIRST),
		!!(ray_mode & OPT_ONLYNEAREST)
	};
	RC._stab(N);
}