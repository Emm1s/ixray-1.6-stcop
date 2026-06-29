#pragma once
#include <Opcode.h>

class InstanceMeshInterface;

class InstanceNoLeafBuilder : public Opcode::AABBTreeOfTrianglesBuilder
{
public:
	struct InstanceData
	{
		const Opcode::AABBNoLeafTree* tree;
		Matrix4x4 transform;
		size_t id;
		AABB worldAABB;
	};
	
private:
	InstanceMeshInterface* mInstanceMesh;
	const xr_vector<InstanceData>* mInstanceData;
	
	void TransformAABB(AABB& aabb, const Matrix4x4& transform) const;
	
public:	
	InstanceNoLeafBuilder() : mInstanceMesh(nullptr), mInstanceData(nullptr) {}
	
	void SetInstanceMesh(InstanceMeshInterface* instanceMesh){ mInstanceMesh = instanceMesh; }
	void SetInstanceData(const xr_vector<InstanceData>* instanceData){ mInstanceData = instanceData; }
	bool ComputeGlobalBox(const dTriIndex* primitives, udword nb_prims, IceMaths::AABB& global_box) const override;
	float GetSplittingValue(udword index, udword axis) const override;
	Point GetSplittingValues(udword index) const override;
};