#pragma once

namespace XRay::Collision
{
	enum class DataFlags : size_t
	{
		Internal = 0,
		Leaf = 1,
		Instance = 1 << 1,
		MAX,
		Mask = ((MAX - 1) << 1) - 1
	};

	IC bool IsInstance(size_t value){ return (value & size_t(DataFlags::Instance)) != 0; }
	IC size_t GetInstanceID(size_t value)
	{
		if (!IVERIFY(IsInstance(value)))
		{
			return -1;
		}
		return value >> (std::countr_zero(size_t(DataFlags::Instance))+1);
	}
	IC size_t ConvToInstanceID(size_t value)
	{
		constexpr size_t Shift = std::countr_zero(size_t(DataFlags::Instance))+1;
		IVERIFY(value <= (size_t(-1)>>Shift));
		return (value << Shift)|size_t(DataFlags::Instance);
	}

}