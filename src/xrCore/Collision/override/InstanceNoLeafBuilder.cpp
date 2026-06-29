#include "Stdafx.h"
#include "InstanceNoLeafBuilder.h"

#include "InstanceMeshInterface.h"

void InstanceNoLeafBuilder::TransformAABB(AABB& aabb, const Matrix4x4& transform) const
{
	Point corners[8];
	aabb.ComputePoints(corners);
	
	Point min(FLT_MAX, FLT_MAX, FLT_MAX);
	Point max(FLT_MIN, FLT_MIN, FLT_MIN);
	
	for (int i = 0; i < 8; ++i)
	{
		Point transformed = transform * corners[i];
		min.Min(transformed);
		max.Max(transformed);
	}
	
	aabb.SetMinMax(min, max);
}

bool InstanceNoLeafBuilder::ValidateSubdivision(const dTriIndex* primitives, udword nb_prims, const IceMaths::AABB& global_box)
{
	if (IsSingleInstanceGroup(primitives, nb_prims))
	{
		return false;
	}
	
	return AABBTreeOfTrianglesBuilder::ValidateSubdivision(primitives, nb_prims, global_box);
}

bool InstanceNoLeafBuilder::IsSingleInstanceGroup(const dTriIndex* primitives, udword nb_prims)
{
	if (!mInstanceMesh || nb_prims == 0)
	{
		return false;
	}
	
	auto FirstIndex = primitives[0];
	if (!mInstanceMesh->IsInstanceIndex(FirstIndex))
	{
		return false;
	}
	
	auto FirstIndexID = mInstanceMesh->GetInstanceID(FirstIndex);
	
	for (udword i = 0; i < nb_prims; ++i)
	{
		auto Index = primitives[i];
		if (!mInstanceMesh->IsInstanceIndex(Index))
		{
			return false;
		}
		if (mInstanceMesh->GetInstanceID(Index) != FirstIndexID)
		{
			return false;
		}
	}
	
	return true;
}

AABB InstanceNoLeafBuilder::GetInstanceAABB(const InstanceData& instance_data)
{
	auto tree = instance_data.tree;
	auto nodes = tree->GetNodes();
	
	AABB rootAABB;
	rootAABB.SetCenterExtents(nodes[0].mAABB.mCenter, nodes[0].mAABB.mExtents);
	
	TransformAABB(rootAABB, instance_data.transform);
	
	return rootAABB;
}