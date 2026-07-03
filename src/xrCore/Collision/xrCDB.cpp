#include "stdafx.h"
#include "xrCDB.h"

#include <embree4/rtcore_scene.h>

#include "override/Model.h"
#include "API/xrAPI.h"
#include "src/xrServerEntities/object_destroyer.h"

namespace Opcode 
{
#	include <OPC_TreeBuilders.h>
#	include <OPC_Model.h>
}

using namespace CDB;
using namespace Opcode;

static const char* GetDeviceConfig()
{
	bool avx_test = CPU::ID().hasFeature(CPUFeature::AVX2);
	bool sse = CPU::ID().hasFeature(CPUFeature::SSE);

	const char* config = "";
	if (avx_test)
	{
		config = "isa=avx2";
	}
	else if (sse)
	{
		config = "isa=sse4.2";
	}
	else
	{
		config = "isa=sse2";
	}

	return config;
}

struct EmbreeDeviceWrapper
{
	EmbreeDeviceWrapper()
	{
		auto fError = [](void* userPtr, enum RTCError code, const char* str)
		{
			R_ASSERT2(false, str);
		};

		EmbreeDevice = rtcNewDevice(GetDeviceConfig());
		rtcSetDeviceErrorFunction(EmbreeDevice, fError, nullptr);
	}
	~EmbreeDeviceWrapper()
	{
		rtcReleaseDevice(EmbreeDevice);
	}
	
	RTCDevice EmbreeDevice;
};

RTCDevice& CDB::GetEmbreeDevice()
{
	static EmbreeDeviceWrapper Wrapper;
	return Wrapper.EmbreeDevice;
}

XRCORE_API IReader* CDB::GetModelCache(string_path LevelName, u32 crc)
{
	IReader* pReaderCache = nullptr;

	if (FS.exist("$app_data_root$", LevelName))
	{
		pReaderCache = FS.r_open("$app_data_root$", LevelName);

		if (pReaderCache->length() <= 4 || pReaderCache->r_u32() != crc)
		{
			FS.r_close(pReaderCache);
		}
	}

	return pReaderCache;
}

IReader* CDB::GetModelCache(const xr_stack_string_path& LevelName, u32 crc)
{
	IReader* pReaderCache = nullptr;

	if (FS.exist("$app_data_root$", LevelName.c_str()))
	{
		pReaderCache = FS.r_open("$app_data_root$", LevelName.c_str());

		if (pReaderCache->length() <= 4 || pReaderCache->r_u32() != crc)
		{
			FS.r_close(pReaderCache);
		}
	}

	return pReaderCache;
}

CDB::MODEL::~MODEL()
{
	delete_data(verts);
	delete_data(tris);
}

void MODEL::build_simple()
{
	auto& EmbreeDevice = GetEmbreeDevice();
	InstaceScene = rtcNewScene(GetEmbreeDevice());
	rtcSetSceneBuildQuality(InstaceScene, RTC_BUILD_QUALITY_HIGH);
	
	RTCGeometry BatchedGeometry = rtcNewGeometry(EmbreeDevice, RTC_GEOMETRY_TYPE_TRIANGLE);
	
	rtcSetSharedGeometryBuffer(BatchedGeometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, verts.data(), 0, sizeof(Fvector), verts.size());
	rtcSetSharedGeometryBuffer(BatchedGeometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, verts.data(), 0, sizeof(CDB::TRI), verts.size());
	rtcSetGeometryUserData(BatchedGeometry, this);
	
	rtcCommitGeometry(BatchedGeometry);
	
	rtcAttachGeometry(InstaceScene, BatchedGeometry);
	rtcReleaseGeometry(BatchedGeometry);
	
	rtcCommitScene(InstaceScene);
}
