#pragma once
#include <Opcode.h>

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
	size_t mNumInstances;
	
	void ConvertLeavesToInstances();
	
public:
	AABBInstanceNoLeafTree() = default;
	
	void SetInstanceData(const InstanceData* InstanceData, udword NumInstances)
	{
		mInstanceData = InstanceData;
		mNumInstances = NumInstances;
	}
	
	IC const InstanceData* GetInstanceData(size_t InstanceID) const
	{
		IVERIFY(InstanceID < mNumInstances);
		return mInstanceData + InstanceID;
	}
	
	IC size_t GetNumInstances() const { return mNumInstances; }
	
	bool Build(Opcode::AABBTree* tree) override;
};