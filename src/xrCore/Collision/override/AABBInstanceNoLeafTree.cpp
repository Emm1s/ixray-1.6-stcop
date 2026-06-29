#include "Stdafx.h"
#include "AABBInstanceNoLeafTree.h"

#include "AABBInstanceNoLeafNode.h"

void AABBInstanceNoLeafTree::ConvertLeavesToInstances()
{
	for (udword i = 0; i < mNbNodes; i++)
	{
		AABBInstanceNoLeafNode* node = (AABBInstanceNoLeafNode*)(mNodes+i);
		
		if (node->HasPosLeaf() && XRay::Collision::IsInstance(node->GetPosPrimitive()))
		{
			node->SetPosInstance(XRay::Collision::GetInstanceID(node->GetPosPrimitive()));
		}
		
		if (node->HasNegLeaf() && XRay::Collision::IsInstance(node->GetNegPrimitive()))
		{
			node->SetNegInstance(XRay::Collision::GetInstanceID(node->GetNegPrimitive()));
		}
	}
}

bool AABBInstanceNoLeafTree::Build(Opcode::AABBTree* tree)
{
	if (!tree)
	{
		return false;
	}
	
	if (!AABBNoLeafTree::Build(tree))
	{
		return false;
	}
	
	ConvertLeavesToInstances();
	
	return true;
}