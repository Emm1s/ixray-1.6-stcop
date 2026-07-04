#include "stdafx.h"
#include "Model.h"
#include <OPC_TreeBuilders.h>
#include <Opcode.h>

#include "InstanceNoLeafBuilder.h"
#include "Tree.h"

namespace CDB::Internal
{
	struct BuilderNodeRaw
	{
		size_t* Primitives;
		size_t Num;
		BuilderNodeRaw* Pos = nullptr;
		BuilderNodeRaw* Neg = nullptr;
	};

	static void ComputeAABB(const BuilderConfig& config, BuilderNodeRaw& Node, Dbox& Out)
	{
		Out = {};
		for (size_t i = 0; i < Node.Num; ++i)
		{
			Dbox PrimitiveAABB;
			config.GetPrimitiveAABB(Node.Primitives[i], PrimitiveAABB);
			Out.merge(PrimitiveAABB);
		}
	}

	// SPLIT_SPLATTER_POINTS type
	static Vector3Axes FindSplitAxis(const BuilderConfig& config, BuilderNodeRaw& Node)
	{
		Dvector Means(0.0);
		for (size_t i = 0; i < Node.Num; ++i)
		{
			Means += config.GetPrimitiveMean(Node.Primitives[i]);
		}
		Means /= Node.Num;
		
		Dvector Vars(0.0);
		for (size_t i = 0; i < Node.Num; ++i)
		{
			Dvector Center = config.GetPrimitiveMean(Node.Primitives[i]);
			auto Delta = Center - Means;
			Vars += Delta*Delta;
		}
		
		return Vars.LargestAxis();
	}

	// SPLIT_GEOM_CENTER type
	static double SplitGeomCenter(const BuilderConfig& config, BuilderNodeRaw& Node, Vector3Axes Axis)
	{
		double SplitValue = 0.0;
		for (size_t i = 0; i < Node.Num; ++i)
		{
			SplitValue += config.GetPrimitivePosOnAxis(Node.Primitives[i], Axis);
		}
		return SplitValue/Node.Num;
	}

	static void Subdivide(const BuilderConfig& config, BuilderNodeRaw& Node, xr_vector<BuilderNodeRaw>& Buffer)
	{
		if (!(Node.Num-1))
		{
			return;
		}
		
		auto SplitAxis = FindSplitAxis(config, Node);
		auto SplitPos = SplitGeomCenter(config, Node, SplitAxis);
		
		size_t LeftNum = 0;
		for (size_t i = 0; i < Node.Num; ++i)
		{
			auto PrimitiveValue = config.GetPrimitivePosOnAxis(Node.Primitives[i], SplitAxis);
			if (PrimitiveValue > SplitPos)
			{
				std::swap(Node.Primitives[i], Node.Primitives[LeftNum++]);
			}
		}
		if (!LeftNum)
		{
			LeftNum=Node.Num/2;
		}
		
		auto& PosNode = Buffer.emplace_back();
		PosNode.Primitives = Node.Primitives;
		PosNode.Num = LeftNum;
		
		auto& NegNode = Buffer.emplace_back();
		NegNode.Primitives = Node.Primitives+LeftNum;
		NegNode.Num = Node.Num-LeftNum;
		
		Node.Pos = &PosNode;
		Node.Neg = &NegNode;
		
	}

	static void BuildModelInternal(const BuilderConfig& config, BuilderNodeRaw& Node, xr_vector<BuilderNodeRaw>& Buffer)
	{
		if (!(Node.Num-1))
		{
			return;
		}
		
		Subdivide(config, Node, Buffer);
		
		if (Node.Pos)
		{
			BuildModelInternal(config, *Node.Pos, Buffer);
		}
		if (Node.Neg)
		{
			BuildModelInternal(config, *Node.Neg, Buffer);
		}
		
	}

	static BVHNode* BuildFinalModelInternal(const BuilderConfig& config, BuilderNodeRaw& Node, BVHModel* Model)
	{
		IVERIFY(Model->GetNodes().capacity() > Model->GetNodes().size());
		auto& FinalNode = Model->GetNodes().emplace_back();
		Dbox WholeAABB;
		ComputeAABB(config, Node, WholeAABB);
		FinalNode.GetAABB().min = WholeAABB.min;
		FinalNode.GetAABB().max = WholeAABB.max;
		if (IVERIFY(Node.Pos))
		{
			auto& Data = FinalNode.GetPos();
			if (!(Node.Pos->Num-1))
			{
				Data.IsNotPointer = true;
				Data.IsInstance = config.IsInstanceIndex(Node.Pos->Primitives[0]);
				Data.Index = Node.Pos->Primitives[0];
			} else if (IVERIFY(Node.Pos->Num))
			{
				Data.p = BuildFinalModelInternal(config, *Node.Pos, Model);
				IVERIFY(!Data.IsNotPointer);
			}
		}
		if (IVERIFY(Node.Neg))
		{
			auto& Data = FinalNode.GetNeg();
			if (!(Node.Neg->Num-1))
			{
				Data.IsNotPointer = true;
				Data.IsInstance = config.IsInstanceIndex(Node.Neg->Primitives[0]);
				Data.Index = Node.Neg->Primitives[0];
			} else if (IVERIFY(Node.Neg->Num))
			{
				Data.p = BuildFinalModelInternal(config, *Node.Neg, Model);
				IVERIFY(!Data.IsNotPointer);
			}
		}
		return &FinalNode;
	}
}

void CDB::BVHModel::Store(IWriter* pWriter)
{
}

bool CDB::BVHModel::Restore(IReader* pReader)
{
	return false;
}

Dvector CDB::BuilderConfig::GetPrimitiveMean(size_t Index) const
{
	if (Faces)
	{
		if(Index < Faces->size())
		{
			if (!IVERIFY(Vertices))
			{
				return {};
			}
			auto& Tri = (*Faces)[Index];
			return Dvector((*Vertices)[Tri.verts[0]]+(*Vertices)[Tri.verts[1]]+(*Vertices)[Tri.verts[2]])/3;
		}
		Index -= Faces->size();
	}
	if (Instances && Index < Instances->size())
	{
		if (!IVERIFY(Models))
		{
			return {};
		}
		Dbox RealAABB;
		GetInstanceAABB(Index, RealAABB);
		Dvector center;
		RealAABB.getcenter(center);
		return center;
	}
	VERIFY(false);
	return {};
}

void CDB::BuilderConfig::GetPrimitiveAABB(size_t Index, Dbox& Out) const
{
	if (Faces)
	{
		if(Index < Faces->size())
		{
			if (!IVERIFY(Vertices))
			{
				return;
			}
			auto& Tri = (*Faces)[Index];
			Out.modify((*Vertices)[Tri.verts[0]]);
			Out.modify((*Vertices)[Tri.verts[1]]);
			Out.modify((*Vertices)[Tri.verts[2]]);
			return;
		}
		Index -= Faces->size();
	}
	if (Instances && Index < Instances->size())
	{
		if (!IVERIFY(Models))
		{
			return;
		}
		GetInstanceAABB(Index, Out);
		return;
	}
	VERIFY(false);
}

double CDB::BuilderConfig::GetPrimitivePosOnAxis(size_t Index, Vector3Axes Axis) const
{
	double SplitValue = 0.0;
	if (Faces)
	{
		if(Index < Faces->size())
		{
			if (!IVERIFY(Vertices))
			{
				return 0.0;
			}
			auto& Tri = (*Faces)[Index];
			SplitValue += (*Vertices)[Tri.verts[0]][Axis];
			SplitValue += (*Vertices)[Tri.verts[1]][Axis];
			SplitValue += (*Vertices)[Tri.verts[2]][Axis];
			return SplitValue/3;
		}
		Index -= Faces->size();
	}
	if (Instances && Index < Instances->size())
	{
		if (!IVERIFY(Models))
		{
			return 0.0;
		}
		Dbox RealAABB;
		GetInstanceAABB(Index, RealAABB);
		SplitValue += RealAABB.max[Axis];
		SplitValue += RealAABB.min[Axis];
		return SplitValue/2;
	}
	VERIFY(false);
	return SplitValue;
}

bool CDB::BuilderConfig::IsPrimitiveIndex(size_t Index) const
{
	if (Faces)
	{
		if(Index < Faces->size())
		{
			return true;
		}
		Index -= Faces->size();
	}
	if (Instances && Index < Instances->size())
	{
		return false;
	}
	IVERIFY(false);
	return false;
}

bool CDB::BuilderConfig::IsInstanceIndex(size_t Index) const
{
	if (Faces)
	{
		if(Index < Faces->size())
		{
			return false;
		}
		Index -= Faces->size();
	}
	if (Instances && Index < Instances->size())
	{
		return true;
	}
	IVERIFY(!Index);
	return false;
}

void CDB::BuilderConfig::GetInstanceAABB(size_t Index, Dbox& Out) const
{
	if (!IVERIFY(Models) || !IVERIFY(Instances))
	{
		return;
	}
	auto ModelAABB = (*Models)[(*Instances)[Index].ModelIndex]->GetAABB();
	auto InstanceTransform = (*Instances)[Index].Transform;
		
	Dbox DModelAABB;
	DModelAABB.min = ModelAABB.min;
	DModelAABB.max = ModelAABB.max;
		
	Dmatrix DInstanceTransform;
	DInstanceTransform.i = InstanceTransform.i;
	DInstanceTransform.j = InstanceTransform.j;
	DInstanceTransform.k = InstanceTransform.k;
	DInstanceTransform.c = InstanceTransform.c;
	DInstanceTransform._14_ = InstanceTransform._14_;
	DInstanceTransform._24_ = InstanceTransform._24_;
	DInstanceTransform._34_ = InstanceTransform._34_;
	DInstanceTransform._44_ = InstanceTransform._44_;
		
	Out = {};
	Out.modify(DModelAABB, DInstanceTransform);
}

CDB::BVHModel* CDB::BuildModel(const BuilderConfig& config)
{
	using namespace CDB::Internal;
	auto model = new BVHModel();
	
	size_t TotalPrimitives = 0;
	if (config.Faces)
	{
		TotalPrimitives += config.Faces->size();
	}
	if (config.Instances)
	{
		TotalPrimitives += config.Instances->size();
	}
	if (!TotalPrimitives)
	{
		return model;
	}
	xr_vector<size_t> Primitives;
	Primitives.reserve(TotalPrimitives);
	size_t i = 0;
	if (config.Faces)
	{
		for (auto& Face : *config.Faces)
		{
			Primitives.push_back(i++);
		}
	}
	if (config.Instances)
	{
		for (auto& Instance : *config.Instances)
		{
			Primitives.push_back(i++);
		}
	}
		
	xr_vector<BuilderNodeRaw> NodesBuffer(2*Primitives.size()+1);
	auto& Root = NodesBuffer.emplace_back();
	Root.Primitives = Primitives.data();
	Root.Num = Primitives.size();
	BuildModelInternal(config, Root, NodesBuffer);
	
	auto& FinalNodes = model->GetNodes();
	FinalNodes.reserve(Primitives.size() - 1);
	
	auto FinalRootNode = BuildFinalModelInternal(config, Root, model);
	model->GetAABB() = FinalRootNode->GetAABB();
	
	return model;
}

CDB_Model::CDB_Model()
{
	pTree = new CDB_OptimizeTree();
}

CDB_Model::~CDB_Model()
{
	Release();
}

void CDB_Model::Store(IWriter* writer)
{
	writer->w_u64(mModelCode);
	pTree->Store(writer);
}

bool CDB_Model::Restore(IReader* reader)
{
	if (reader->elapsed() < sizeof(u64))
	{
		Msg("* Level Collision DB cache file missing model code!");
		return false;
	}
	mModelCode = (udword)reader->r_u64();

	return pTree->Restore(reader);
}

bool CDB_Model::Build(const Opcode::OPCODECREATE& create)
{
	if (!create.mIMesh || !create.mIMesh->IsValid())
	{
		return false;
	}

	if (create.mSettings.mLimit != 1)
	{
		Msg("OPCODE WARNING: supports complete trees only! Use mLimit = 1. Current mLimit = %d", create.mSettings.mLimit);
		return false;
	}

	u64 NbDegenerate = create.mIMesh->CheckTopology();

	if (NbDegenerate)
	{
		Msg("OPCODE WARNING: found %d degenerate faces in model! Collision might report wrong results!\n", NbDegenerate);
	}

	ReleaseBase();
	SetMeshInterface(create.mIMesh);

	u64 NbTris = create.mIMesh->GetNbTriangles();

	if (NbTris == 1)
	{
		mModelCode |= ModelFlag::OPC_SINGLE_NODE;
		return false;
	}

	mSource = new Opcode::AABBTree();
	xr_scope_exit OnExit = [&]()
	{
		if (!create.mKeepOriginal)
		{
			xr_delete(mSource);
		}
	};
	{
		InstanceNoLeafBuilder TB;
		TB.mIMesh = create.mIMesh;
		TB.mSettings = create.mSettings;
		TB.mNbPrimitives = (udword)NbTris;
		if (!mSource->Build(&TB))
		{
			return false;
		}
	}

	if (!CreateTree(create.mNoLeaf, create.mQuantized))
	{
		return false;
	}
	if (!pTree->Build(mSource))
	{
		return false;
	}

	// Finally ok...
	return true;
}

void CDB_Model::Release()
{
	xr_delete(pTree);
}