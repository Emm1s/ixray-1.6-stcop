#include "Stdafx.h"
#include "AABBInstanceNoLeafTree.h"

#include "AABBInstanceNoLeafNode.h"

void AABBInstanceNoLeafTree::ConvertLeavesToInstances()
{
	for (udword i = 0; i < mNbNodes; i++)
	{
		AABBInstanceNoLeafNode* node = (AABBInstanceNoLeafNode*)(mNodes+i);
		
		if (node->HasPosLeaf() && IsInstancePrimitive(node->GetPosPrimitive()))
		{
			node->SetPosInstance(GetInstanceIDFromPrimitive(node->GetPosPrimitive()));
		}
		
		if (node->HasNegLeaf() && IsInstancePrimitive(node->GetNegPrimitive()))
		{
			node->SetNegInstance(GetInstanceIDFromPrimitive(node->GetNegPrimitive()));
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
	
	OptimizeInstances();
	
	return true;
}