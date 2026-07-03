#include "stdafx.h"
#include "CFormIO.h"

#include <embree4/rtcore_scene.h>

using namespace XRay;

CForm::ChunkHeader& CForm::IFormat::GetHeader()
{
    return Header;
}

const CForm::ChunkHeader& CForm::IFormat::GetHeader() const
{
    return Header;
}

u32 CForm::IFormat::GetFileHash() const
{
    return FileHash;
}

CForm::CFormatVanilla::CFormatVanilla()
{
    Header.version = CFormVersions::Vanilla;
}

CForm::CFormatVanilla::~CFormatVanilla()
{
	if (FileReader)
	{
		xr_delete(FileReader);
	}
}

bool CForm::CFormatVanilla::Write(xr_string_view FileName)
{
    xr_stack_string_path Path = FileName.data();
    Path.append(".cform");
    
    auto Writer = FS.wg_open(Path.c_str());
    if (!I_ASSERT(Writer))
    {
        return false;
    }

    Writer->w(&Header, sizeof(Header));
    Writer->w(VertsPtr, Header.vertcount*sizeof(Fvector));
    Writer->w(TrisPtr, Header.facecount*sizeof(CDB::TRI));
    
    return true;
}

bool CForm::CFormatVanilla::Read(xr_string_view FileName)
{
    xr_stack_string_path Path = FileName.data();
    Path.append(".cform");

    FileReader = FS.r_open(Path.c_str());
    if (!I_ASSERT_M(FileReader, "Unable to open file [%s]", Path.c_str()))
    {
        return false;
    }

    FileHash = crc32(FileReader->pointer(), FileReader->length());
    
    FileReader->r(&Header, sizeof(Header));
    if (!I_ASSERT(Header.version == CFormVersions::Vanilla || Header.version == CFormVersions::VanillaChunkedData))
    {
        return false;
    }
	VertsPtr = (Fvector*)FileReader->pointer();
	FileReader->advance(Header.vertcount*sizeof(Fvector));
	TrisPtr = (CDB::TRI*)FileReader->pointer();

    return true;
}

void CForm::CFormatVanilla::AddStaticGeom(xr_span<Fvector> Verts, xr_span<CDB::TRI> Tris)
{
    Header.vertcount = Verts.size();
    Header.facecount = Tris.size();
    Header.aabb.invalidate();
    for (auto& elem : Verts)
    {
        Header.aabb.modify(elem);
    }
	VertsPtr = Verts.data();
	TrisPtr = Tris.data();
}

void CForm::CFormatVanilla::GetStaticGeom(xr_vector<Fvector>& OutVertices, xr_vector<CDB::TRI>& OutTris) const
{
    OutVertices.clear();
    OutTris.clear();
    OutVertices.resize(Header.vertcount);
    OutTris.resize(Header.facecount);
    std::memcpy(OutVertices.data(), VertsPtr, sizeof(Fvector) * OutVertices.size());
    std::memcpy(OutTris.data(), TrisPtr, sizeof(CDB::TRI) * OutTris.size());
}

void CForm::CFormatVanilla::ReadData(CDB::MODEL& Model, CDB::build_callback* bc, void* bcp) const
{
	Model.verts.resize(Header.vertcount);
	std::memcpy(Model.verts.data(), VertsPtr, sizeof(Fvector) * Header.vertcount);
	Model.tris.resize(Header.facecount);
	std::memcpy(Model.tris.data(), TrisPtr, sizeof(CDB::TRI) * Header.facecount);
	
	if (bc)
	{
		bc(Model.verts.data(), Header.vertcount, Model.tris.data(), Header.facecount, bcp);
	}

	Model.build_simple();
	
	/*auto& EmbreeDevice = CDB::GetEmbreeDevice();
	Model.InstaceScene = rtcNewScene(CDB::GetEmbreeDevice());
	rtcSetSceneBuildQuality(Model.InstaceScene, RTC_BUILD_QUALITY_HIGH);
	
	RTCGeometry BatchedGeometry = rtcNewGeometry(EmbreeDevice, RTC_GEOMETRY_TYPE_TRIANGLE);
	
	rtcSetSharedGeometryBuffer(BatchedGeometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, VertsPtr, 0, sizeof(Fvector), Header.vertcount);
	rtcSetSharedGeometryBuffer(BatchedGeometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, TrisPtr, 0, sizeof(CDB::TRI), Header.facecount);
	rtcSetGeometryUserData(BatchedGeometry, &Model);
	
	rtcCommitGeometry(BatchedGeometry);
	
	rtcAttachGeometry(Model.InstaceScene, BatchedGeometry);
	rtcReleaseGeometry(BatchedGeometry);
	
	rtcCommitScene(Model.InstaceScene);*/
}

CForm::CFormatVanillaChunked::CFormatVanillaChunked(u32 ChunkNumber)
{
    if (!IVERIFY(ChunkNumber > 0)){
        ChunkNumber = 1;
    }
    Header.version = CFormVersions::VanillaChunked;
    Data.shrink_to_fit();
    Data.resize(ChunkNumber);
    for (auto& elem : Data)
    {
        elem.GetHeader().version = CFormVersions::VanillaChunkedData;
    }
}

bool CForm::CFormatVanillaChunked::Write(xr_string_view FileName)
{
    xr_stack_string_path Path = FileName.data();
    Path.append(".cform");
    auto Writer = FS.wg_open(Path.c_str());
    if (!I_ASSERT(Writer))
    {
        return false;
    }

    Writer->w(&Header, sizeof(Header));
    Writer->w_u32(Data.size());

    for (size_t i = 0; i < Data.size(); i++)
    {
        auto& elem = Data[i];
        xr_stack_string_path Path = FileName.data();
        Path.append("_");
        Path.append(std::to_string(i).c_str());
        if (!I_ASSERT(elem.Write({Path.c_str(), Path.size()})))
        {
            return false;
        }
    }

    return true;
}

bool CForm::CFormatVanillaChunked::Read(xr_string_view FileName)
{
    xr_stack_string_path Path = FileName.data();
    Path.append(".cform");
    auto Reader = FS.rg_open(Path.c_str());
    if (!I_ASSERT(Reader))
    {
        return false;
    }

    FileHash = crc32(Reader->pointer(), Reader->length());
    
    Reader->r(&Header, sizeof(Header));
    if (!I_ASSERT(Header.version == CFormVersions::VanillaChunked))
    {
        return false;
    }

    u32 ChunkNum = Reader->r_u32();
    Data.resize(ChunkNum);
    for (u32 i = 0; i < ChunkNum; i++)
    {
        auto& elem = Data[i];
        xr_stack_string_path Path = FileName.data();
        Path.append("_");
        Path.append(std::to_string(i).c_str());
        if (!I_ASSERT(elem.Read({Path.c_str(), Path.size()})))
        {
            return false;
        }
    }

    return true;
    
}

void CForm::CFormatVanillaChunked::AddStaticGeom(xr_span<Fvector> Verts, xr_span<CDB::TRI> Tris)
{
    auto ChunksNum = Data.size();
    Header.vertcount = Verts.size();
    Header.facecount = Tris.size();
    Header.aabb.invalidate();
    for (auto& elem : Verts)
    {
        Header.aabb.modify(elem);
    }

    auto PerChunkVertsNum = Verts.size()/ChunksNum + Verts.size()%ChunksNum;
    auto PerChunkFaceNum = Tris.size()/ChunksNum + Tris.size()%ChunksNum;
    size_t CurrentPosVerts = 0;
    size_t CurrentPosFace = 0;
    for (size_t i = 0; i < ChunksNum; i++)
    {
        if (!IVERIFY(CurrentPosVerts < Verts.size()) || !IVERIFY(CurrentPosFace < Tris.size()))
        {
            break;
        }
        auto DeltaVerts = std::min(PerChunkVertsNum, Verts.size() - CurrentPosVerts);
        auto DeltaTris = std::min(PerChunkFaceNum, Tris.size() - CurrentPosFace);
        auto& Chunk = Data[i];
        Chunk.AddStaticGeom(
            {Verts.data()+CurrentPosVerts, DeltaVerts},
            {Tris.data()+CurrentPosFace, DeltaTris});
        CurrentPosVerts += DeltaVerts;
        CurrentPosFace += DeltaTris;
    }
}

void CForm::CFormatVanillaChunked::GetStaticGeom(xr_vector<Fvector>& OutVertices, xr_vector<CDB::TRI>& OutTris) const
{
    OutVertices.clear();
    OutTris.clear();
    OutVertices.reserve(Header.vertcount);
    OutTris.reserve(Header.facecount);
    
#ifdef IXR_WINDOWS
    for (auto& elem : Data)
    {
        OutVertices.append_range(xr_span<Fvector>{elem.VertsPtr, elem.GetHeader().vertcount});
        OutTris.append_range(xr_span<CDB::TRI>{elem.TrisPtr, elem.GetHeader().facecount});
    }
#else
#pragma todo("FX: Wait C++23...")
    for (auto& elem : Data)
    {
        for (const auto& vert : elem.Data.Verts)
        {
            OutVertices.push_back(vert);
        }
        
        for (const auto& tri : elem.Data.Tris)
        {
            OutTris.push_back(tri);
        }
    }
#endif
}

void CForm::CFormatVanillaChunked::ReadData(CDB::MODEL& Model, CDB::build_callback* bc, void* bcp) const
{
	GetStaticGeom(Model.verts, Model.tris);
	
	if (bc)
	{
		bc(Model.verts.data(), Header.vertcount, Model.tris.data(), Header.facecount, bcp);
	}
	
	Model.build_simple();
}

CForm::CFormatInstanced::CFormatInstanced()
{
	Header.version = CFormVersions::Instanced;
}

CForm::CFormatInstanced::~CFormatInstanced()
{
	if (FileReader)
	{
		xr_delete(FileReader);
	}
}

bool CForm::CFormatInstanced::Write(xr_string_view FileName)
{
	xr_stack_string_path Path = FileName.data();
	Path.append(".cform");
    
	auto Writer = FS.wg_open(Path.c_str());
	if (!I_ASSERT(Writer))
	{
		return false;
	}

	Writer->w(&Header, sizeof(Header));
	Writer->w(VertsPtr, Header.vertcount*sizeof(Fvector));
	Writer->w(TrisPtr, Header.facecount*sizeof(CDB::TRI));
	
	Writer->w_u64(instances.size());
	for (auto& elem : instances)
	{
		Writer->w_stringZ(elem.first);
		Writer->w_u64(elem.second.size());
		Writer->w(elem.second.data(), elem.second.size()*sizeof(Fmatrix));
	}
    
	return true;
}

bool CForm::CFormatInstanced::Read(xr_string_view FileName)
{
	xr_stack_string_path Path = FileName.data();
	Path.append(".cform");

	FileReader = FS.r_open(Path.c_str());
	if (!I_ASSERT_M(FileReader, "Unable to open file [%s]", Path.c_str()))
	{
		return false;
	}

	FileHash = crc32(FileReader->pointer(), FileReader->length());
    
	FileReader->r(&Header, sizeof(Header));
	if (!I_ASSERT(Header.version == CFormVersions::Vanilla || Header.version == CFormVersions::VanillaChunkedData))
	{
		return false;
	}
	VertsPtr = (Fvector*)FileReader->pointer();
	FileReader->advance(Header.vertcount*sizeof(Fvector));
	TrisPtr = (CDB::TRI*)FileReader->pointer();
	FileReader->advance(Header.facecount*sizeof(CDB::TRI));
	
	size_t InstancesCount = FileReader->r_u64();
	for (size_t i = 0; i < InstancesCount; ++i)
	{
		shared_str ObjectName;
		FileReader->r_stringZ(ObjectName);
		auto& Slot = instances[ObjectName];
		
		size_t xformCount = FileReader->r_u64();
		Slot.resize(xformCount);
		std::memcpy(Slot.data(), FileReader->pointer(), xformCount * sizeof(Fmatrix));
		FileReader->advance(xformCount * sizeof(Fmatrix));
	}

	return true;
}

void CForm::CFormatInstanced::AddStaticGeom(xr_span<Fvector> Verts, xr_span<CDB::TRI> Tris)
{
	Header.vertcount = Verts.size();
	Header.facecount = Tris.size();
	Header.aabb.invalidate();
	for (auto& elem : Verts)
	{
		Header.aabb.modify(elem);
	}
	VertsPtr = Verts.data();
	TrisPtr = Tris.data();
}

void CForm::CFormatInstanced::AddInstanceRef(shared_str Path, const Fmatrix& xform)
{
	instances.try_emplace(Path).first->second.push_back(xform);
}

void CForm::CFormatInstanced::GetStaticGeom(xr_vector<Fvector>& OutVertices, xr_vector<CDB::TRI>& OutTris) const
{
	VERIFY(false);
}

void CForm::CFormatInstanced::ReadData(CDB::MODEL& Model, CDB::build_callback* bc, void* bcp) const
{
	for (auto& elem : instances)
	{
		auto InstanceMesh = ReadInstance(elem.first, bc, bcp);
		InstanceMesh->Parent = &Model;
		auto& Slot = Model.instances[InstanceMesh];
		Slot = elem.second;
	}
	
	Model.verts.resize(Header.vertcount);
	std::memcpy(Model.verts.data(), VertsPtr, sizeof(Fvector) * Header.vertcount);
	Model.tris.resize(Header.facecount);
	std::memcpy(Model.tris.data(), TrisPtr, sizeof(CDB::TRI) * Header.facecount);
	
	if (bc)
	{
		bc(Model.verts.data(), Header.vertcount, Model.tris.data(), Header.facecount, bcp);
	}

	Model.build_simple();
	
	/*auto& EmbreeDevice = CDB::GetEmbreeDevice();
	Model.InstaceScene = rtcNewScene(CDB::GetEmbreeDevice());
	rtcSetSceneBuildQuality(Model.InstaceScene, RTC_BUILD_QUALITY_HIGH);
	
	for (auto& elem : Model.instances)
	{
		auto InstanceScene = rtcNewScene(EmbreeDevice);
		rtcSetSceneBuildQuality(InstanceScene, RTC_BUILD_QUALITY_HIGH);
		
		RTCGeometry InstanceGeometry = rtcNewGeometry(EmbreeDevice, RTC_GEOMETRY_TYPE_TRIANGLE);
	
		rtcSetSharedGeometryBuffer(InstanceGeometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, elem.first->verts.data(), 0, sizeof(Fvector), elem.first->verts.size());
		rtcSetSharedGeometryBuffer(InstanceGeometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, elem.first->tris.data(), 0, sizeof(CDB::TRI), elem.first->tris.size());
	
		rtcCommitGeometry(InstanceGeometry);
	
		rtcAttachGeometry(InstanceScene, InstanceGeometry);
		rtcReleaseGeometry(InstanceGeometry);
		
		rtcCommitScene(InstanceScene);
		
		for (auto& xform : elem.second)
		{
			auto InstanceOnLevel = rtcNewGeometry(EmbreeDevice, RTC_GEOMETRY_TYPE_INSTANCE);
			rtcSetGeometryInstancedScene(InstanceOnLevel, InstanceScene);
			rtcSetGeometryTransform(InstanceOnLevel, 0, RTC_FORMAT_FLOAT4X4_ROW_MAJOR, &xform);
			
			rtcAttachGeometry(Model.InstaceScene, InstanceOnLevel);
			rtcReleaseGeometry(InstanceOnLevel);
		}
	}
	
	RTCGeometry BatchedGeometry = rtcNewGeometry(EmbreeDevice, RTC_GEOMETRY_TYPE_TRIANGLE);
	
	rtcSetSharedGeometryBuffer(BatchedGeometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, VertsPtr, 0, sizeof(Fvector), Header.vertcount);
	rtcSetSharedGeometryBuffer(BatchedGeometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, TrisPtr, 0, sizeof(CDB::TRI), Header.facecount);
	
	rtcCommitGeometry(BatchedGeometry);
	
	rtcAttachGeometry(Model.InstaceScene, BatchedGeometry);
	rtcReleaseGeometry(BatchedGeometry);
	
	rtcCommitScene(Model.InstaceScene);*/
}

XRCORE_API xr_unique_ptr<CForm::IFormat> CForm::Read(const char* Initial, xr_string_view Filename)
{
    ChunkHeader Header;
    xr_stack_string_path Path = Filename.data();
    if (Initial&&Initial[0])
    {
        FS.update_path(Path,Initial,Filename.data());
    }
    {
        xr_stack_string_path TempPath = Path;
        TempPath.append(".cform");
        auto Reader = FS.rg_open(TempPath.c_str());
        if (!I_ASSERT(Reader))
        {
            return nullptr;
        }
        Reader->r(&Header, sizeof(Header));
    }

    switch (Header.version)
    {
    case CFormVersions::Vanilla:
        {
            auto Parsed = new CFormatVanilla();
            if (!I_ASSERT_M(Parsed->Read(Path.c_str()), "Unable to read [%s]", Path.c_str()))
            {
                xr_delete(Parsed);
                return nullptr;
            }
            return xr_unique_ptr<CForm::IFormat>(Parsed);
        }
    case CFormVersions::VanillaChunked:
        {
            auto Parsed = new CFormatVanillaChunked(1);
            if (!I_ASSERT_M(Parsed->Read(Path.c_str()), "Unable to read [%s]", Path.c_str()))
            {
                xr_delete(Parsed);
                return nullptr;
            }
            return xr_unique_ptr<CForm::IFormat>(Parsed);
        }
    default:
        {
            I_ASSERT_M(false, "Invalid .cform type in [%s]", Path.c_str());
        }
    }
    
    return nullptr;
}

XRCORE_API xr_unique_ptr<CForm::IFormat> CForm::Read(xr_string_view Filename)
{
    return Read(nullptr, Filename);
}

XRCORE_API void CForm::Write(const char* Initial, xr_string_view Filename, IFormat& Data)
{
    xr_stack_string_path Path = Filename.data();
    if (Initial&&Initial[0])
    {
        FS.update_path(Path,Initial,Filename.data());
    }
    I_ASSERT(Data.Write(Path.c_str()));
}

XRCORE_API void CForm::Write(xr_string_view Filename, IFormat& Data)
{
    Write(nullptr, Filename, Data);
}
