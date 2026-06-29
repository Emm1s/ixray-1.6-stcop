#pragma once
#include <Opcode.h>

#include "InstanceFlags.h"

class AABBInstanceNoLeafNode : public Opcode::AABBNoLeafNode
{
public:
	
	IC bool HasPosInstance() const { return XRay::Collision::IsInstance(mPosData); }
	IC bool HasNegInstance() const { return XRay::Collision::IsInstance(mNegData); }
	
	IC size_t GetPosInstanceID() const { return XRay::Collision::GetInstanceID(mPosData); }
	IC size_t GetNegInstanceID() const { return XRay::Collision::GetInstanceID(mNegData); }
	
	IC void SetPosInstance(size_t InstanceID) { mPosData = XRay::Collision::ConvToInstanceID(InstanceID); }
	IC void SetNegInstance(size_t InstanceID) {	mNegData = XRay::Collision::ConvToInstanceID(InstanceID); }	
	
	IC bool IsInstanceNode() const { return HasPosInstance() || HasNegInstance(); }
};