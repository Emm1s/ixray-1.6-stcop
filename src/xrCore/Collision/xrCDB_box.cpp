#include "stdafx.h"


#include "xrCDB.h"
#include "override/Model.h"

using namespace CDB;
using namespace Opcode;

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

struct cform_box_collider final
{
	xr_vector<RESULT>* dest = nullptr;
	cform_stack* stack = nullptr;
	Fbox box;
	bool bClass3, bFirst;

	ICF void _prim(ElementID prim)
	{
		VERIFY(prim.IsNotPointer);
		if (prim.IsInstance)
		{
			auto& CurModel = GetCurrentTree();
			IVERIFY(++CurrentIndex < m_def_array.size());
			CurModel
			return;
		}
		
		auto& Tri = tris[prim];
		auto& TriVerts = Tri.verts;
		Fvector tri_verts[3] = { verts[TriVerts[0]], verts[TriVerts[1]], verts[TriVerts[2]] };
		if (!box.intersectTri(tri_verts, bClass3))
			return;
		RESULT& R = dest->r_add();
		R.id = prim;
		R.verts[0] = tri_verts[0];
		R.verts[1] = tri_verts[1];
		R.verts[2] = tri_verts[2];
		R.dummy = Tri.dummy;
	}
	void _stab(const BVHNode& node)
	{
		// Actual box-box test
		Fvector center, extents;
		node.GetAABB().get_CD(center, extents);
		if (!box.intersect(Fbox{center-extents,center+extents}))
		{
			return;
		}
		
		// 1st chield
		if (node.HasPosNode())
		{
			_stab(node.GetPosNode());
		} else
		{
			_prim(node.GetPos());
		}
		
		// Early exit for "only first"
		if (bFirst && dest->r_count())
		{
			return;
		}
		
		// 2nd chield
		if (node.HasNegNode())
		{
			_stab(node.GetNegNode());
		} else
		{
			_prim(node.GetNeg());
		}
	}
};

void COLLIDER::box_query(const MODEL *m_def, const Fbox& _box)
{
	PROF_EVENT("COLLIDER::box_query");
	if (!m_def || m_def->tree == nullptr)
		return;

	m_def->wait_loading();

	r_clear();
	r_vec().reserve(16);
	
	// Get nodes
	auto& Nodes = m_def->tree->GetNodes();

	cform_stack S = *m_def;
	cform_box_collider BC{};
	BC.dest = this;
	BC.stack = &S;
	BC.box = _box;
	BC.bClass3 = box_mode & OPT_FULL_TEST;
	BC.bFirst = box_mode & OPT_ONLYFIRST;
	BC._stab(Nodes[0]);
}

struct cform_obb_collider final
{
	xr_vector<RESULT>* dest = nullptr;
	cform_stack* stack = nullptr;
	
	Fobb obb;

	bool bClass3 = false;
	bool bFirst = false;

	ICF void _prim(ElementID prim)
	{
		VERIFY(prim.IsNotPointer);
		if (prim.IsInstance)
		{
			auto& CurModel = stack->GetCurrentTree();
			auto& Instances = CurModel.get_instances();
			VERIFY(prim.Index < Instances.size());
			auto& InstanceInvTransform = Instances[prim.Index].InvTransform;
			auto& InstanceModel = CurModel.get_models()[Instances[prim.Index].ModelIndex];
			stack->Push(InstanceModel);
			
			Fobb localOBB;
			localOBB.transform(obb, InstanceInvTransform);
			
			if (false /*no rot and scale*/)
			{
				
			}
			else if (false /*uniform transform*/)
			{
				
			}
			else
			{
				xr_vector<RESULT> RawResults = {};
				cform_box_collider InstanceCollider{
					&RawResults,
					stack,
					{},
					bClass3,
					bFirst,
				};
				
				Fvector aX = localOBB.m_rotate.i * localOBB.m_halfsize.x;
				Fvector aY = localOBB.m_rotate.j * localOBB.m_halfsize.y;
				Fvector aZ = localOBB.m_rotate.k * localOBB.m_halfsize.z;
				
				InstanceCollider.box.modify(localOBB.m_translate + aX + aY + aZ);
				InstanceCollider.box.modify(localOBB.m_translate + aX + aY - aZ);
				InstanceCollider.box.modify(localOBB.m_translate + aX - aY + aZ);
				InstanceCollider.box.modify(localOBB.m_translate + aX - aY - aZ);
				InstanceCollider.box.modify(localOBB.m_translate - aX + aY + aZ);
				InstanceCollider.box.modify(localOBB.m_translate - aX + aY - aZ);
				InstanceCollider.box.modify(localOBB.m_translate - aX - aY + aZ);
				InstanceCollider.box.modify(localOBB.m_translate - aX - aY - aZ);
				
				_stab(InstanceModel.tree->GetNodes()[0]);
				
				
				
			}
			
			return;
		}
		auto& Tri = tris[prim];
		auto& TriVerts = Tri.verts;
		Fvector tri_verts[3] = { verts[TriVerts[0]], verts[TriVerts[1]], verts[TriVerts[2]] };

		if (!obb.intersectTri(tri_verts, bClass3))
			return;

		RESULT& R = dest->r_add();
		R.id = prim;
		R.verts[0] = tri_verts[0];
		R.verts[1] = tri_verts[1];
		R.verts[2] = tri_verts[2];
		R.dummy = Tri.dummy;
	}

	void _stab(const BVHNode& node)
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
			_stab(node.GetPosNode());
		}
		else
		{
			_prim(node.GetPos());
		}

		// Early exit for "only first"
		if (bFirst && dest->r_count())
		{
			return;
		}

		// 2nd child
		if (node.HasNegNode())
		{
			_stab(node.GetNegNode());
		}
		else
		{
			_prim(node.GetNeg());
		}
	}
};

void COLLIDER::obb_query(const MODEL* m_def, const Fobb& obb)
{
	PROF_EVENT("COLLIDER::obb_query");
	if (!m_def || m_def->tree == nullptr)
		return;

	m_def->wait_loading();

	// Get nodes
	const AABBNoLeafTree* T = (const AABBNoLeafTree*)m_def->tree->GetTree();
	const AABBNoLeafNode* N = T->GetNodes();

	r_clear();
	r_vec().reserve(16);

	cform_stack S = *m_def;
	cform_obb_collider OC
	{
		&rd,
		&S,
		obb,
		!!(obb_mode & OPT_FULL_TEST),
		!!(obb_mode & OPT_ONLYFIRST)
	};
	OC._stab(N);
}

struct cform_sphere_collider final
{
	COLLIDER* dest;
	const MODEL *m_def;
	Fsphere sphere;

	bool bClass3 = false;
	bool bFirst = false;

	ICF void _prim(size_t prim)
	{
		auto& Tri = tris[prim];
		auto& TriVerts = Tri.verts;
		Fvector tri_verts[3] = { verts[TriVerts[0]], verts[TriVerts[1]], verts[TriVerts[2]] };

		if (!sphere.intersectTri(tri_verts, bClass3))
			return;

		RESULT& R = dest->r_add();
		R.id = prim;
		R.verts[0] = tri_verts[0];
		R.verts[1] = tri_verts[1];
		R.verts[2] = tri_verts[2];
		R.dummy = Tri.dummy;
	}

	void _stab(const AABBNoLeafNode* node)
	{
		// Actual Sphere-AABB test
		if (!sphere.intersectAABB((Fvector&)node->mAABB.mCenter, (Fvector&)node->mAABB.mExtents)) return;

		// 1st child
		if (node->HasPosLeaf())	_prim(node->GetPosPrimitive());
		else					_stab(node->GetPos());

		// Early exit for "only first"
		if (bFirst && dest->r_count()) return;

		// 2nd child
		if (node->HasNegLeaf())	_prim(node->GetNegPrimitive());
		else					_stab(node->GetNeg());
	}
};

void COLLIDER::sphere_query(const MODEL* m_def, const Fsphere& sphere)
{
	PROF_EVENT("COLLIDER::sphere_query");
	if (!m_def || m_def->tree == nullptr)
		return;

	m_def->wait_loading();

	// Get nodes
	const AABBNoLeafTree* T = (const AABBNoLeafTree*)m_def->tree->GetTree();
	const AABBNoLeafNode* N = T->GetNodes();

	r_clear();
	r_vec().reserve(16);

	cform_sphere_collider SC
	{
		this,
		m_def,
		sphere,
		!!(sphere_mode & OPT_FULL_TEST),
		!!(sphere_mode & OPT_ONLYFIRST)
	};
	SC._stab(N);
}
