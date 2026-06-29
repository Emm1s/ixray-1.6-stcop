//////////////////////////////////////////////////////////
// Desc   : Collision Detection OptTree + Cache System
// Author : ForserX
//////////////////////////////////////////////////////////
#pragma once
#include <Opcode.h>
#include "Collision/override/AABBInstanceNoLeafTree.h"

class XRCORE_API CDB_OptimizeTree : public AABBInstanceNoLeafTree
{
public:
	CDB_OptimizeTree();
	~CDB_OptimizeTree();

	void Store(IWriter* pWriter);
	bool Restore(IReader* pReader);

	bool Build(Opcode::AABBTree* tree);
};