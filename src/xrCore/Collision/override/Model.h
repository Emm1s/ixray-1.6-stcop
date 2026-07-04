//////////////////////////////////////////////////////////
// Desc   : Collision Detection Model + Cache System
// Author : ForserX
// Update : 20.04.2020 - Support for Hybrid Trees System 
//////////////////////////////////////////////////////////
#pragma once
#include "Collision/xrCDB.h"

namespace CDB
{
	class BVHNode;

	union ElementID
	{
		BVHNode* p;
		struct
		{
			size_t Index:62;
			size_t IsInstance:1;
			size_t IsNotPointer:1;
		};
	};
	static_assert(sizeof(ElementID) == sizeof(size_t));

	class BVHNode
	{
		Fbox AABB;
		ElementID ElemPos = {};
		ElementID ElemNeg = {};
	public:
		
		const Fbox& GetAABB() const { return AABB; }
		Fbox& GetAABB() { return AABB; }
		
		ElementID& GetPos() { return ElemPos; }
		ElementID& GetNeg() { return ElemNeg; }
		
		bool HasPosNode() const { return !ElemPos.IsNotPointer; }
		const BVHNode& GetPosNode() const
		{
			IVERIFY(HasPosNode());
			return *ElemPos.p;
		}
		BVHNode& GetPosNode()
		{
			IVERIFY(HasPosNode());
			return *ElemPos.p;
		}
		
		bool HasNegNode() const { return !ElemNeg.IsNotPointer; }
		const BVHNode& GetNegNode() const
		{
			IVERIFY(HasNegNode());
			return *ElemNeg.p;
		}
		BVHNode& GetNegNode()
		{
			IVERIFY(HasNegNode());
			return *ElemNeg.p;
		}
	};

	class XRCORE_API BVHModel
	{
		xr_vector<BVHNode> Nodes;
		Fbox AABB;
		
	public:		
		void Store(IWriter* pWriter);
		bool Restore(IReader* pReader);
		
		const Fbox& GetAABB() const { return AABB; }
		Fbox& GetAABB() { return AABB; }
		const xr_vector<BVHNode>& GetNodes() const { return Nodes; }
		xr_vector<BVHNode>& GetNodes() { return Nodes; }
	};

	struct BuilderConfig
	{
		struct InstanceData
		{
			Fmatrix Transform;
			size_t ModelIndex;
		};
		xr_vector<Fvector>* Vertices;
		xr_vector<TRI>* Faces;
		xr_vector<BVHModel*>* Models;
		xr_vector<InstanceData>* Instances;
		
		Dvector GetPrimitiveMean(size_t Index) const;
		void GetPrimitiveAABB(size_t Index, Dbox& Out) const;
		double GetPrimitivePosOnAxis(size_t Index, Vector3Axes Axis) const;
		bool IsPrimitiveIndex(size_t Index) const;
		bool IsInstanceIndex(size_t Index) const;
		
	private:
		void GetInstanceAABB(size_t Index, Dbox& Out) const;
	};

	BVHModel* BuildModel(const BuilderConfig& config);
}


#include <Opcode.h>
#include <embree4/rtcore_geometry.h>

class CDB_OptimizeTree;

class XRCORE_API CDB_Model : public Opcode::Model
{
public:
	CDB_Model();
	virtual ~CDB_Model();

	void Store(IWriter* pWriter);
	bool Restore(IReader* pReader);

	// Overload for using CDB_OptimizeTree into Build Model 
	bool Build(const Opcode::OPCODECREATE& create);
	virtual void Release() override;

	IC  RTCScene& GetCDBTree() { return InstaceScene; }

protected:
	RTCScene InstaceScene;
	//CDB_OptimizeTree* pTree;

	enum ModelFlag
	{
		OPC_QUANTIZED = (1 << 0),	//!< Compressed/uncompressed tree
		OPC_NO_LEAF = (1 << 1),	//!< Leaf/NoLeaf tree
		OPC_SINGLE_NODE = (1 << 2)	//!< Special case for 1-node models
	};
};