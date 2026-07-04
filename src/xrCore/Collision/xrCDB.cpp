#include "stdafx.h"
#include "xrCDB.h"

#include <embree4/rtcore_ray.h>
#include <embree4/rtcore_scene.h>
#include <embree4/rtcore_common.h>

#include "Frustum.h"
#include "override/Model.h"
#include "API/xrAPI.h"
#include "src/xrServerEntities/object_destroyer.h"

namespace Opcode 
{
#	include <OPC_TreeBuilders.h>
#	include <OPC_Model.h>
}

using namespace CDB;
using namespace Opcode;

static const char* GetDeviceConfig()
{
	bool avx_test = CPU::ID().hasFeature(CPUFeature::AVX2);
	bool sse = CPU::ID().hasFeature(CPUFeature::SSE);

	const char* config = "";
	if (avx_test)
	{
		config = "isa=avx2";
	}
	else if (sse)
	{
		config = "isa=sse4.2";
	}
	else
	{
		config = "isa=sse2";
	}

	return config;
}

struct EmbreeDeviceWrapper
{
	EmbreeDeviceWrapper()
	{
		auto fError = [](void* userPtr, enum RTCError code, const char* str)
		{
			R_ASSERT2(false, str);
		};

		EmbreeDevice = rtcNewDevice(GetDeviceConfig());
		rtcSetDeviceErrorFunction(EmbreeDevice, fError, nullptr);
	}
	~EmbreeDeviceWrapper()
	{
		rtcReleaseDevice(EmbreeDevice);
	}
	
	RTCDevice EmbreeDevice;
};

RTCDevice& CDB::GetEmbreeDevice()
{
	static EmbreeDeviceWrapper Wrapper;
	return Wrapper.EmbreeDevice;
}

XRCORE_API IReader* CDB::GetModelCache(string_path LevelName, u32 crc)
{
	IReader* pReaderCache = nullptr;

	if (FS.exist("$app_data_root$", LevelName))
	{
		pReaderCache = FS.r_open("$app_data_root$", LevelName);

		if (pReaderCache->length() <= 4 || pReaderCache->r_u32() != crc)
		{
			FS.r_close(pReaderCache);
		}
	}

	return pReaderCache;
}

IReader* CDB::GetModelCache(const xr_stack_string_path& LevelName, u32 crc)
{
	IReader* pReaderCache = nullptr;

	if (FS.exist("$app_data_root$", LevelName.c_str()))
	{
		pReaderCache = FS.r_open("$app_data_root$", LevelName.c_str());

		if (pReaderCache->length() <= 4 || pReaderCache->r_u32() != crc)
		{
			FS.r_close(pReaderCache);
		}
	}

	return pReaderCache;
}

CDB::MODEL::~MODEL()
{
	delete_data(verts);
	delete_data(tris);
}

void MODEL::build_simple()
{
	auto& EmbreeDevice = GetEmbreeDevice();
	InstaceScene = rtcNewScene(GetEmbreeDevice());
	rtcSetSceneBuildQuality(InstaceScene, RTC_BUILD_QUALITY_HIGH);

	for(auto elem : models)
	{
		elem->build_simple();
	}

	for(auto& elem : instances)
	{
		auto InstanceOnLevel = rtcNewGeometry(EmbreeDevice, RTC_GEOMETRY_TYPE_INSTANCE);
		rtcSetGeometryInstancedScene(InstanceOnLevel, models[elem.ModelIndex]->InstaceScene);
		rtcSetGeometryTransform(InstanceOnLevel, 0, RTC_FORMAT_FLOAT4X4_ROW_MAJOR, &elem.Transform);
			
		rtcAttachGeometry(InstaceScene, InstanceOnLevel);
		rtcReleaseGeometry(InstanceOnLevel);
	}
	
	RTCGeometry BatchedGeometry = rtcNewGeometry(EmbreeDevice, RTC_GEOMETRY_TYPE_TRIANGLE);
	
	rtcSetSharedGeometryBuffer(BatchedGeometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, verts.data(), 0, sizeof(Fvector), verts.size());
	rtcSetSharedGeometryBuffer(BatchedGeometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, verts.data(), 0, sizeof(CDB::TRI), verts.size());
	rtcSetGeometryUserData(BatchedGeometry, this);
	
	rtcCommitGeometry(BatchedGeometry);
	
	rtcAttachGeometry(InstaceScene, BatchedGeometry);
	rtcReleaseGeometry(BatchedGeometry);
	
	rtcCommitScene(InstaceScene);
}

// Collision queries



void COLLIDER::ray_query(const MODEL* m_def, const Fvector& r_start, const Fvector& r_dir, float r_range)
{
	PROF_EVENT("COLLIDER::ray_query");
	if (!m_def || !m_def->InstaceScene)
	{
		return;
	}

	// Get nodes
	
	r_clear();
	r_vec().reserve(16);
	
	if(!!(ray_mode & OPT_ONLYFIRST))
	{
		RTCRay ray;
		ray.org_x = r_start.x;
		ray.org_y = r_start.y;
		ray.org_z = r_start.z;
		ray.dir_x = r_dir.x;
		ray.dir_y = r_dir.y;
		ray.dir_z = r_dir.z;
		ray.tnear = 0.0f;
		ray.tfar = r_range;
		ray.mask = -1;
		ray.flags = 0;
		ray.time = 0.0f;
		
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
		ray.ray.time = 0.0f;
		ray.hit.geomID = RTC_INVALID_GEOMETRY_ID;
		
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

				/*if(model->Parent)
				{
					rtcGetGeometryTransformFromScene(
						model->Parent->InstaceScene,
						RTCHitN_instID(args->hit, 1, 0, 0),
						0, RTC_FORMAT_FLOAT4X4_ROW_MAJOR, &R.ParentTransform);
				}*/

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

constexpr size_t MAX_INSTANCE_DEPTH = 4;

struct cform_stack final
{
	xr_array<const MODEL*, MAX_INSTANCE_DEPTH> m_def_array = {};
	size_t CurrentIndex = 0;
	
	cform_stack(const MODEL& RootModel){ m_def_array[0] = &RootModel; }
	
	ICF const MODEL& GetCurrentTree()
	{
		VERIFY(CurrentIndex < m_def_array.size());
		VERIFY(m_def_array[CurrentIndex]);
		return *m_def_array[CurrentIndex];
	}
	
	ICF void Push(const MODEL& NewTree){ CurrentIndex++; VERIFY(CurrentIndex < m_def_array.size()); m_def_array[CurrentIndex] = &NewTree; }
	ICF void Pop(){ m_def_array[CurrentIndex] = nullptr; VERIFY(CurrentIndex > 0); CurrentIndex--;  }
};

// ultimate solution for non-uniform instances - SAT and converted AABB, OBB and Frustum as 6 planes
// will be slower than simple box check, but no additional filter check required
struct cform_frustum_collider final
{
	xr_vector<RESULT>* dest = nullptr;
	cform_stack* stack = nullptr;
	const CFrustum* F;

	bool bClass3, bFirst;

	ICF void Prim(ElementID InPrim)
	{
		VERIFY(InPrim.IsNotPointer);
		if (InPrim.IsInstance)
		{
			auto& CurModel = stack->GetCurrentTree();
			auto& Instances = CurModel.get_instances();
			auto& Prototype = Instances[InPrim.Index];
			auto& Models = CurModel.get_models();
			auto& ChildModel = Models[Prototype.ModelIndex];
			stack->Push(ChildModel);
			xr_scope_exit g = [&]()
			{
				stack->Pop();
			};

			CFrustum LocalF;
			for(int i = 0; i < F->p_count; ++i)
			{
				Fplane LocalPlane = F->planes[i];
				Prototype.Transform.transform_dir(LocalPlane.n);
				LocalPlane.d += Prototype.Transform.c.dotproduct(F->planes[i].n);
				LocalF._add(LocalPlane);
			}

			cform_frustum_collider FC{
				dest,
				stack,
				&LocalF,
				bClass3,
				bFirst
			};
			FC.Stab(ChildModel.tree->GetNodes()[0], F->getMask());
			return;
		}
		
		auto& CurModel = stack->GetCurrentTree();
		auto& Tri = CurModel.tris[InPrim.Index];
		auto& TriVerts = Tri.verts;
		Fvector tri_verts[3] = {
			CurModel.verts[TriVerts[0]],
			CurModel.verts[TriVerts[1]],
			CurModel.verts[TriVerts[2]]
		};

		if (bClass3)
		{
			thread_local sPoly Src, Dst;
			Src.resize(3);
			Fvector* src = Src.begin();
			src[0] = tri_verts[0];
			src[1] = tri_verts[1];
			src[2] = tri_verts[2];
			if (F->ClipPoly(Src, Dst))
			{
				RESULT& R = dest->emplace_back();
				R.model = &CurModel;
				R.tris_id = InPrim.Index;
			}
		}
		else
		{
			RESULT& R = dest->emplace_back();
			R.model = &CurModel;
			R.tris_id = InPrim.Index;
		}
	}

	void Stab(const BVHNode& node, u32 mask)
	{
		// Actual frustum/aabb test
		if (fcvNone == F->testAABB(node.GetAABB().data(), mask))
		{
			return;
		}

		// 1st chield
		if (node.HasPosNode())
		{
			Stab(node.GetPosNode(), mask);
		} else
		{
			Prim(node.GetPos());
		}

		// Early exit for "only first"
		if (bFirst && dest->size()) 
		{
			return;
		}

		// 2nd chield
		if (node.HasNegNode())
		{
			Stab(node.GetNegNode(), mask);
		} else
		{
			Prim(node.GetNeg());
		}
	}
};

void COLLIDER::frustum_query(const MODEL* m_def, const CFrustum& F)
{
	PROF_EVENT("COLLIDER::frustum_query");
	if (!m_def || m_def->tree == nullptr)
		return;

	m_def->wait_loading();

	r_clear();
	r_vec().reserve(16);
	
	auto& Nodes = m_def->tree->GetNodes();

	cform_stack S = *m_def;
	cform_frustum_collider BC{};
	BC.dest = &rd;
	BC.stack = &S;
	BC.F = &F;
	BC.bClass3 = box_mode & OPT_FULL_TEST;
	BC.bFirst = box_mode & OPT_ONLYFIRST;
	BC.Stab(Nodes[0], F.getMask());
}

struct cform_box_collider final
{
	xr_vector<RESULT>* dest = nullptr;
	cform_stack* stack = nullptr;
	Fbox box;
	bool bClass3, bFirst;

	ICF void Prim(ElementID InPrim)
	{
		VERIFY(InPrim.IsNotPointer);
		if (InPrim.IsInstance)
		{
			auto& CurModel = stack->GetCurrentTree();
			auto& Instances = CurModel.get_instances();
			auto& Prototype = Instances[InPrim.Index];
			auto& Models = CurModel.get_models();
			auto& ChildModel = Models[Prototype.ModelIndex];
			stack->Push(ChildModel);
			xr_scope_exit g = [&]()
			{
				stack->Pop();
			};

			// TODO: SAT test will always works fine, but there are some ways to optimize this
			CFrustum LocalF;
			auto PlaneFunc = [&](Fvector n, float d)
			{
				Fplane LocalPlane = {n, d};
				Prototype.Transform.transform_dir(LocalPlane.n);
				LocalPlane.d += Prototype.Transform.c.dotproduct(n);
				LocalF._add(LocalPlane);
			};
			PlaneFunc({1,0,0}, -box.min.x);
			PlaneFunc({-1,0,0}, box.max.x);
			PlaneFunc({0,1,0}, -box.min.y);
			PlaneFunc({0,-1,0}, box.max.y);
			PlaneFunc({0,0,1}, -box.min.z);
			PlaneFunc({0,0,-1,}, box.max.z);

			cform_frustum_collider FC{
				dest,
				stack,
				&LocalF,
				bClass3,
				bFirst
			};
			FC.Stab(ChildModel.tree->GetNodes()[0], LocalF.getMask());
			return;
		}
		
		auto& CurModel = stack->GetCurrentTree();
		auto& Tri = CurModel.tris[InPrim.Index];
		auto& TriVerts = Tri.verts;
		Fvector tri_verts[3] = {
			CurModel.verts[TriVerts[0]],
			CurModel.verts[TriVerts[1]],
			CurModel.verts[TriVerts[2]]
		};
		if (!box.intersectTri(tri_verts, bClass3))
		{
			return;
		}

		RESULT& R = dest->emplace_back();
		R.model = &CurModel;
		R.tris_id = InPrim.Index;
	}
	
	void Stab(const BVHNode& node)
	{
		// Actual box-box test
		if (!box.intersect(node.GetAABB()))
		{
			return;
		}
		
		// 1st chield
		if (node.HasPosNode())
		{
			Stab(node.GetPosNode());
		} else
		{
			Prim(node.GetPos());
		}
		
		// Early exit for "only first"
		if (bFirst && dest->size())
		{
			return;
		}
		
		// 2nd chield
		if (node.HasNegNode())
		{
			Stab(node.GetNegNode());
		} else
		{
			Prim(node.GetNeg());
		}
	}
};

void COLLIDER::box_query(const MODEL *m_def, const Fbox& _box)
{
	PROF_EVENT("COLLIDER::box_query");
	/*if (!m_def || m_def->tree == nullptr)
		return;

	m_def->wait_loading();*/

	r_clear();
	r_vec().reserve(16);
	
	// Get nodes
	auto& Nodes = m_def->tree->GetNodes();

	cform_stack S = *m_def;
	cform_box_collider BC{};
	BC.dest = &rd;
	BC.stack = &S;
	BC.box = _box;
	BC.bClass3 = box_mode & OPT_FULL_TEST;
	BC.bFirst = box_mode & OPT_ONLYFIRST;
	BC.Stab(Nodes[0]);
}

struct cform_obb_collider final
{
	xr_vector<RESULT>* dest = nullptr;
	cform_stack* stack = nullptr;
	
	Fobb obb;

	bool bClass3 = false;
	bool bFirst = false;

	ICF void Prim(ElementID prim)
	{
		VERIFY(prim.IsNotPointer);
		if (prim.IsInstance)
		{
			auto& CurModel = stack->GetCurrentTree();
			auto& Instances = CurModel.get_instances();
			auto& Prototype = Instances[prim.Index];
			auto& Models = CurModel.get_models();
			auto& ChildModel = Models[Prototype.ModelIndex];
			stack->Push(ChildModel);
			xr_scope_exit g = [&]()
			{
				stack->Pop();
			};

			// TODO: SAT test will always works fine, but there are some ways to optimize this
			CFrustum LocalF;
			auto PlaneFunc = [&](Fvector n, float d)
			{
				Fplane LocalPlane = {n, d};
				Prototype.Transform.transform_dir(LocalPlane.n);
				LocalPlane.d += Prototype.Transform.c.dotproduct(n);
				LocalF._add(LocalPlane);
			};
			PlaneFunc(obb.m_rotate.i, -(obb.m_rotate.i*obb.m_translate + obb.m_halfsize.x));
			PlaneFunc(-obb.m_rotate.i, obb.m_rotate.i*obb.m_translate - obb.m_halfsize.x);
			PlaneFunc(obb.m_rotate.j, -(obb.m_rotate.j*obb.m_translate + obb.m_halfsize.y));
			PlaneFunc(-obb.m_rotate.j, obb.m_rotate.j*obb.m_translate - obb.m_halfsize.y);
			PlaneFunc(obb.m_rotate.k, -(obb.m_rotate.k*obb.m_translate + obb.m_halfsize.z));
			PlaneFunc(-obb.m_rotate.k, obb.m_rotate.k*obb.m_translate - obb.m_halfsize.z);

			cform_frustum_collider BC{
				dest,
				stack,
				&LocalF,
				bClass3,
				bFirst
			};
			BC.Stab(ChildModel.tree->GetNodes()[0], LocalF.getMask());
			return;
		}
		
		auto& CurModel = stack->GetCurrentTree();
		auto& Tri = CurModel.tris[prim.Index];
		auto& TriVerts = Tri.verts;
		Fvector tri_verts[3] = {
			CurModel.verts[TriVerts[0]],
			CurModel.verts[TriVerts[1]],
			CurModel.verts[TriVerts[2]]
		};

		if (!obb.intersectTri(tri_verts, bClass3))
		{
			return;
		}

		RESULT& R = dest->emplace_back();
		R.model = &CurModel;
		R.tris_id = prim.Index;
	}

	void Stab(const BVHNode& node)
	{
		VERIFY(dest);
		VERIFY(stack);
		// Actual OBB-AABB test
		
		if (!obb.intersectAABB(node.GetAABB()))
		{
			return;
		}

		// 1st child
		if (node.HasPosNode())
		{
			Stab(node.GetPosNode());
		}
		else
		{
			Prim(node.GetPos());
		}

		// Early exit for "only first"
		if (bFirst && dest->size())
		{
			return;
		}

		// 2nd child
		if (node.HasNegNode())
		{
			Stab(node.GetNegNode());
		}
		else
		{
			Prim(node.GetNeg());
		}
	}
};

void COLLIDER::obb_query(const MODEL* m_def, const Fobb& obb)
{
	PROF_EVENT("COLLIDER::obb_query");
	/*if (!m_def || m_def->tree == nullptr)
		return;

	m_def->wait_loading();

	// Get nodes
	const AABBNoLeafTree* T = (const AABBNoLeafTree*)m_def->tree->GetTree();
	const AABBNoLeafNode* N = T->GetNodes();*/

	r_clear();
	r_vec().reserve(16);
	
	// Get nodes
	auto& Nodes = m_def->tree->GetNodes();

	cform_stack S = *m_def;
	cform_obb_collider OC
	{
		&rd,
		&S,
		obb,
		!!(obb_mode & OPT_FULL_TEST),
		!!(obb_mode & OPT_ONLYFIRST)
	};
	OC.Stab(Nodes[0]);
}

struct cform_sphere_collider final
{
	xr_vector<RESULT>* dest = nullptr;
	cform_stack* stack = nullptr;
	
	Fsphere sphere;

	bool bClass3 = false;
	bool bFirst = false;

	ICF void Prim(ElementID prim)
	{
		VERIFY(prim.IsNotPointer);
		if (prim.IsInstance)
		{
			auto& CurModel = stack->GetCurrentTree();
			auto& Instances = CurModel.get_instances();
			auto& Prototype = Instances[prim.Index];
			auto& Models = CurModel.get_models();
			auto& ChildModel = Models[Prototype.ModelIndex];
			stack->Push(ChildModel);
			xr_scope_exit g = [&]()
			{
				stack->Pop();
			};

			// TODO: SAT test will always works fine, but there are some ways to optimize this
			Fobb obb;
			Prototype.Transform.transform_tiny(obb.m_translate, sphere.P);
			obb.m_rotate.i = Prototype.Transform.i;
			obb.m_rotate.j = Prototype.Transform.j;
			obb.m_rotate.k = Prototype.Transform.k;
			obb.m_halfsize.x = sphere.R*obb.m_rotate.i.magnitude();
			obb.m_halfsize.y = sphere.R*obb.m_rotate.j.magnitude();
			obb.m_halfsize.z = sphere.R*obb.m_rotate.k.magnitude();
			obb.m_rotate.i.normalize();
			obb.m_rotate.j.normalize();
			obb.m_rotate.k.normalize();
			VERIFY(_valid(obb));
			
			CFrustum LocalF;
			auto PlaneFunc = [&](Fvector n, float d)
			{
				Fplane LocalPlane = {n, d};
				Prototype.Transform.transform_dir(LocalPlane.n);
				LocalPlane.d += Prototype.Transform.c.dotproduct(n);
				LocalF._add(LocalPlane);
			};
			PlaneFunc(obb.m_rotate.i, -(obb.m_rotate.i*obb.m_translate + obb.m_halfsize.x));
			PlaneFunc(-obb.m_rotate.i, obb.m_rotate.i*obb.m_translate - obb.m_halfsize.x);
			PlaneFunc(obb.m_rotate.j, -(obb.m_rotate.j*obb.m_translate + obb.m_halfsize.y));
			PlaneFunc(-obb.m_rotate.j, obb.m_rotate.j*obb.m_translate - obb.m_halfsize.y);
			PlaneFunc(obb.m_rotate.k, -(obb.m_rotate.k*obb.m_translate + obb.m_halfsize.z));
			PlaneFunc(-obb.m_rotate.k, obb.m_rotate.k*obb.m_translate - obb.m_halfsize.z);

			xr_vector<RESULT> local_results;
			cform_frustum_collider BC{
				&local_results,
				stack,
				&LocalF,
				bClass3,
				bFirst
			};
			BC.Stab(ChildModel.tree->GetNodes()[0], LocalF.getMask());

			for(auto& R : local_results)
			{
				auto& CurrentTris = R.model->tris[R.tris_id];
				
				Fvector tri_verts[3] = {};
				Prototype.InvTransform.transform_tiny(tri_verts[0], R.model->verts[CurrentTris.verts[0]]);
				Prototype.InvTransform.transform_tiny(tri_verts[1], R.model->verts[CurrentTris.verts[1]]);
				Prototype.InvTransform.transform_tiny(tri_verts[2], R.model->verts[CurrentTris.verts[2]]);
				
				if (!sphere.intersectTri(tri_verts, bClass3))
				{
					dest->push_back(R);
				}
			}			
			return;
		}
		
		auto& CurModel = stack->GetCurrentTree();
		auto& Tri = CurModel.tris[prim.Index];
		auto& TriVerts = Tri.verts;
		Fvector tri_verts[3] = {
			CurModel.verts[TriVerts[0]],
			CurModel.verts[TriVerts[1]],
			CurModel.verts[TriVerts[2]]
		};

		if (!sphere.intersectTri(tri_verts, bClass3))
		{
			return;
		}

		RESULT& R = dest->emplace_back();
		R.model = &CurModel;
		R.tris_id = prim.Index;
	}

	void Stab(const BVHNode& node)
	{
		// Actual Sphere-AABB test
		Fvector center, extents;
		node.GetAABB().get_CD(center, extents);
		if (!sphere.intersectAABB(center, extents))
		{
			return;
		}

		// 1st child
		if (node.HasPosNode())
		{
			Stab(node.GetPosNode());
		}
		else
		{
			Prim(node.GetPos());
		}

		// Early exit for "only first"
		if (bFirst && dest->size())
		{
			return;
		}

		// 2nd child
		if (node.HasNegNode())
		{
			Stab(node.GetNegNode());
		}
		else
		{
			Prim(node.GetNeg());
		}
	}
};

void COLLIDER::sphere_query(const MODEL* m_def, const Fsphere& sphere)
{
	PROF_EVENT("COLLIDER::sphere_query");
	/*if (!m_def || m_def->tree == nullptr)
		return;

	m_def->wait_loading();

	// Get nodes
	const AABBNoLeafTree* T = (const AABBNoLeafTree*)m_def->tree->GetTree();
	const AABBNoLeafNode* N = T->GetNodes();*/

	r_clear();
	r_vec().reserve(16);
	
	// Get nodes
	auto& Nodes = m_def->tree->GetNodes();

	cform_stack S = *m_def;
	cform_sphere_collider SC
	{
		&rd,
		&S,
		sphere,
		!!(sphere_mode & OPT_FULL_TEST),
		!!(sphere_mode & OPT_ONLYFIRST)
	};
	SC.Stab(Nodes[0]);
}

struct cform_custom_collider final
{
	bool(*AABBCheck)(const Fvector&, const Fvector&, bool, void*);
	void* paabbc = nullptr;
	void(*GetTris)(size_t, void*);
	void* ptric = nullptr;
	void Stab(const AABBNoLeafNode* node)
	{
		bool pos_leaf = node->HasPosLeaf();
		bool neg_leaf = node->HasNegLeaf();
		if (nullptr==AABBCheck || !AABBCheck((Fvector&)node->mAABB.mCenter, (Fvector&)node->mAABB.mExtents, pos_leaf||neg_leaf, paabbc)) return;

		// 1st chield
		if (pos_leaf)
		{
			if (GetTris)
				GetTris(node->GetPosPrimitive(), ptric);
		}
		else
			Stab(node->GetPos());

		// 2nd chield
		if (neg_leaf)
		{
			if (GetTris)
				GetTris(node->GetNegPrimitive(), ptric);
		}
		else
			Stab(node->GetNeg());
	}
};

void COLLIDER::custom_query(const MODEL* m_def, bool(AABBCheckF)(const Fvector&, const Fvector&, bool, void*), void* paabbc, void(GetTrisF)(size_t, void*), void* ptric)
{
	PROF_EVENT("COLLIDER::custom_query");
	if (!m_def || m_def->tree == nullptr)
		return;

	m_def->wait_loading();

	cform_custom_collider CC
	{
		AABBCheckF,
		paabbc,
		GetTrisF,
		ptric
	};
	CC.Stab(m_def->tree->GetCDBTree()->GetNodes());
}