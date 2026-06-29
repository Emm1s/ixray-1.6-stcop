#pragma once
#include <Opcode.h>

#include "AABBInstanceNoLeafNode.h"

class InstanceMeshInterface final : public Opcode::MeshInterface
{
	xr_vector<const Opcode::AABBNoLeafTree*> mInstanceTrees;
	xr_vector<Matrix4x4> mInstanceTransforms;
	size_t mInstanceCounter = 0;
	
public:
	InstanceMeshInterface() = default;
	
	IC size_t RegisterInstanceTree(const Opcode::AABBNoLeafTree* tree, const Matrix4x4& transform)
	{
		mInstanceTrees.push_back(tree);
		mInstanceTransforms.push_back(transform);
		return mInstanceCounter++;
	}
	
	IC const Opcode::AABBNoLeafTree* GetInstanceTree(size_t index) const
	{
		if (!IVERIFY(index < mInstanceTrees.size()))
		{
			return nullptr;
		}
		return mInstanceTrees[index];
	}
	
	IC const Matrix4x4& GetInstanceTransform(size_t index) const
	{
		if (!IVERIFY(index < mInstanceTransforms.size()))
		{
			static Matrix4x4 mInstanceTransformIdentity;
			return mInstanceTransformIdentity;
		}
		return mInstanceTransforms[index];
	}
    
	static constexpr size_t INSTANCE_FLAG = 1 << (sizeof(size_t)*8 - 1);
	
	IC bool IsInstanceIndex(size_t index) const
	{
		return (index & INSTANCE_FLAG) != 0;
	}
	
	IC size_t GetInstanceID(size_t index) const
	{
		return index & ~INSTANCE_FLAG;
	}

	IC virtual void GetTriangle(Opcode::VertexPointers& vp, udword index) const override
	{
		if (XRay::Collision::IsInstance(index))
		if (IsInstanceIndex(index))
		{
			return;	
		}
		Opcode::MeshInterface::GetTriangle(vp, index);
	}
};