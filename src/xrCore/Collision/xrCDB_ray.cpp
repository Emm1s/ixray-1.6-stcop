#include "stdafx.h"

#include <embree4/rtcore_ray.h>
#include <embree4/rtcore_scene.h>
#include <embree4/rtcore_common.h>

#include "xrCDB.h"
#include "override/Model.h"

using namespace CDB;

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
	
	r_clear();
	r_vec().reserve(16);
	
	if(!!(ray_mode & OPT_ONLYFIRST))
	{
		RTCOccludedArguments args;
		rtcInitOccludedArguments(&args);
		struct Filter
		{
			static void Execute(const RTCFilterFunctionNArguments* args)
			{
				VERIFY(args->N == 1);
				if (*args->valid != -1)
				{
					return;
				}
				auto model = (MODEL*)args->geometryUserPtr;
				VERIFY(model);

				if(!!(ray_mode & OPT_CULL))
				{
					auto Nx = RTCHitN_Ng_x(args->hit, 1, 0);
					auto Ny = RTCHitN_Ng_y(args->hit, 1, 0);
					auto Nz = RTCHitN_Ng_z(args->hit, 1, 0);
					auto Dx = RTCRayN_dir_x(args->ray, 1, 0);
					auto Dy = RTCRayN_dir_y(args->ray, 1, 0);
					auto Dz = RTCRayN_dir_z(args->ray, 1, 0);
					float dot = Nx*Dx+Ny*Dy+Nz*Dz;
					if(dot > 0)
					{
						args->valid[0] = 0;
						return;
					}
				}
				
				auto PrimID = RTCHitN_primID(args->hit, 1, 0);
				VERIFY(PrimID < model->tris.size());
				
				RESULT& R	= r_add(); // Нам нужен же просто факт, есть ли хит, а не то с чем столкнулись, да?
				/*R.model = model;
				R.tris_id = PrimID;
				R.range		= RTCRayN_tfar(args->ray, 1, 0);
				R.u			= RTCHitN_u(args->hit, 1, 0);
				R.v			= RTCHitN_v(args->hit, 1, 0);

				if(model->Parent)
				{
					rtcGetGeometryTransformFromScene(
						model->Parent->InstaceScene,
						RTCHitN_instID(args->hit, 1, 0, 0),
						0, RTC_FORMAT_FLOAT4X4_ROW_MAJOR, &R.ParentTransform);
				}*/
			}
		};
		args.filter = Filter::Execute;
		args.flags = RTC_RAY_QUERY_FLAG_INVOKE_ARGUMENT_FILTER;
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
				if (*args->valid != -1)
				{
					return;
				}
				auto model = (MODEL*)args->geometryUserPtr;
				VERIFY(model);

				if(!!(ray_mode & OPT_CULL))
				{
					auto Nx = RTCHitN_Ng_x(args->hit, 1, 0);
					auto Ny = RTCHitN_Ng_y(args->hit, 1, 0);
					auto Nz = RTCHitN_Ng_z(args->hit, 1, 0);
					auto Dx = RTCRayN_dir_x(args->ray, 1, 0);
					auto Dy = RTCRayN_dir_y(args->ray, 1, 0);
					auto Dz = RTCRayN_dir_z(args->ray, 1, 0);
					float dot = Nx*Dx+Ny*Dy+Nz*Dz;
					if(dot > 0)
					{
						args->valid[0] = 0;
						return;
					}
				}

				auto PrimID = RTCHitN_primID(args->hit, 1, 0);
				VERIFY(PrimID < model->tris.size());
				
				RESULT& R	= r_add();
				R.model = model;
				R.tris_id = PrimID;
				R.range		= RTCRayN_tfar(args->ray, 1, 0);
				R.u			= RTCHitN_u(args->hit, 1, 0);
				R.v			= RTCHitN_v(args->hit, 1, 0);

				if(model->Parent)
				{
					rtcGetGeometryTransformFromScene(
						model->Parent->InstaceScene,
						RTCHitN_instID(args->hit, 1, 0, 0),
						0, RTC_FORMAT_FLOAT4X4_ROW_MAJOR, &R.ParentTransform);
				}

				if(!(ray_mode & OPT_ONLYNEAREST))
				{
					args->valid[0] = 0;
				}
			}
		};
		args.filter = Filter::Execute;
		args.flags = RTC_RAY_QUERY_FLAG_INVOKE_ARGUMENT_FILTER;
		rtcIntersect1(m_def->InstaceScene, &ray, &args);
	}
}