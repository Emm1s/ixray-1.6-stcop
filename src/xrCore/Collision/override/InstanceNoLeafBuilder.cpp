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

bool InstanceNoLeafBuilder::ComputeGlobalBox(const dTriIndex* primitives, udword nb_prims, IceMaths::AABB& global_box) const
{
	if (!nb_prims)
	{
		return false;
	}
	
	global_box.SetEmpty();
	
	for (udword i = 0; i < nb_prims; ++i)
	{
		auto PrimIndex = primitives[i];
		if (XRay::Collision::IsInstance(PrimIndex))
		{
			auto ID = XRay::Collision::GetInstanceID(PrimIndex);
			auto& data = (*mInstanceData)[ID];
			
			global_box.Add(data.worldAABB);
		} else
		{
			
		}
	}
	
	return AABBTreeOfTrianglesBuilder::ComputeGlobalBox(primitives, nb_prims, global_box);
}

float InstanceNoLeafBuilder::GetSplittingValue(udword index, udword axis) const
{
	if (XRay::Collision::IsInstance(index))
	{
		auto ID = XRay::Collision::GetInstanceID(index);
		auto& data = (*mInstanceData)[ID];
		
		Point center;
		data.worldAABB.GetCenter(center);
		return center[axis];
	}
	return AABBTreeOfTrianglesBuilder::GetSplittingValue(index, axis);
}

Point InstanceNoLeafBuilder::GetSplittingValues(udword index) const
{
	if (XRay::Collision::IsInstance(index))
	{
		auto ID = XRay::Collision::GetInstanceID(index);
		auto& data = (*mInstanceData)[ID];
		
		Point center;
		data.worldAABB.GetCenter(center);
		return center;
	}
	return AABBTreeOfTrianglesBuilder::GetSplittingValues(index);
}