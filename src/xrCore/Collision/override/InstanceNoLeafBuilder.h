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
	};
	
private:
	InstanceMeshInterface* mInstanceMesh;
	const xr_vector<InstanceData>* mInstanceData;
	
	void TransformAABB(AABB& aabb, const Matrix4x4& transform) const;
	
public:	
	InstanceNoLeafBuilder() : mInstanceMesh(nullptr), mInstanceData(nullptr) {}
	
	void SetInstanceMesh(InstanceMeshInterface* instanceMesh){ mInstanceMesh = instanceMesh; }
	void SetInstanceData(const xr_vector<InstanceData>* instanceData){ mInstanceData = instanceData; }
	bool ValidateSubdivision(const dTriIndex* primitives, udword nb_prims, const IceMaths::AABB& global_box) override;
	bool IsSingleInstanceGroup(const dTriIndex* primitives, udword nb_prims);
	AABB GetInstanceAABB(const InstanceData& instance_data);
};