#pragma once
#include <Opcode.h>

#include "AABBInstanceNoLeafNode.h"

class AABBInstanceNoLeafNode;

class AABBInstanceNoLeafTree : public Opcode::AABBNoLeafTree
{
	
public:
	struct InstanceData
	{
		const AABBNoLeafTree* tree = nullptr;
		Matrix4x4 transform;
		AABB worldAABB;
		
		InstanceData()
		{
			transform.Identity();
		}
	};
	
private:
	
	const InstanceData* mInstanceData;
	udword mNumInstances;
	
	void ConvertLeavesToInstances();
	
    static constexpr udword INSTANCE_FLAG = 0x80000000;
	IC bool IsInstancePrimitive(size_t primIndex) const { return (primIndex & size_t(AABBInstanceNoLeafNode::DataFlags::Instance)) != 0; }
	IC size_t GetInstanceIdFromPrimitive(size_t primIndex) const { return primIndex & ~INSTANCE_FLAG; }
	
public:
	AABBInstanceNoLeafTree() = default;
	
	void SetInstanceData(const InstanceData* InstanceData, udword NumInstances)
	{
		mInstanceData = InstanceData;
		mNumInstances = NumInstances;
	}
	
	bool Build(Opcode::AABBTree* tree) override;
};