#include "stdafx.h"

#include "xrLC_GlobalData.h"
#include "xrFace.h"
#include "xrDeflector.h"
#include "Lightmap.h"
#include "mu_model_face.h"
#include "xrExternalObject.h"
#include "xrMU_Model.h"
#include "xrMU_Model_Reference.h"
#include "../../xrCore/Collision/xrCDB.h"

bool g_using_smooth_groups = true;
bool g_smooth_groups_by_faces = false;

xrLC_GlobalData* data =0;


xrLC_GlobalData*	lc_global_data()
{
	return data;
}

xr_vector<base_Face*> FacesStorage;
void	create_global_data()
{
	VERIFY( !inlc_global_data() );
	data = new xrLC_GlobalData();
}
void	destroy_global_data()
{
	VERIFY( inlc_global_data() );
	if(data)
		data->clear();
	xr_delete(data);
	FacesStorage.clear();
}


xrLC_GlobalData::xrLC_GlobalData() : b_vert_not_register( false )
{
	_cl_globs._RCAST_Model = 0;
}

void	xrLC_GlobalData	::destroy_rcmodel	()
{
	xr_delete		(_cl_globs._RCAST_Model);
}

void	xrLC_GlobalData	::create_rcmodel	(CDB::CollectorPacked& CL)
{
	VERIFY(!_cl_globs._RCAST_Model);
	_cl_globs._RCAST_Model				= new CDB::MODEL();
	_cl_globs._RCAST_Model->build		(CL.getV(),(int)CL.getVS(),CL.getT(),(int)CL.getTS());
}

void		xrLC_GlobalData	::				initialize		()
{
}


xrSRWLock NaxGuard;

XRLC_LIGHT_API base_Face* convert_nax(u32 dummy)
{
	xrSRWLockGuard guard(NaxGuard, true);

	if (FacesStorage.size() < dummy) {
		DebugBreak();
	}

	return FacesStorage[dummy];
}

XRLC_LIGHT_API u32 convert_nax(base_Face* F)
{
	xrSRWLockGuard guard(NaxGuard);

	FacesStorage.push_back(F);
	return FacesStorage.size() - 1;
}
 
void	xrLC_GlobalData::mu_models_calc_materials()
{
	for (u32 m=0; m<mu_models().size(); m++)
			mu_models()[m]->calc_materials();

}
  
bool	xrLC_GlobalData	::			b_r_vertices	()		
{
	return false;
}

xrExternalObject* xrLC_GlobalData::LoadExternalObject(shared_str name)
{
	
	b_external_object_data slot;
	string_path fn;
	FS.update_path(fn, _objects_, EFS.ChangeFileExt(name.c_str(), ".object").c_str());
	if (I_ASSERT_M(FS.TryLoad(fn), "Unable to load external object [%s]", fn))
	{
		auto F = FS.rg_open(fn); R_ASSERT(F);
		F->open_chunk(EEditableObjectChunks::OBJECT_BODY, [this, &fn, &slot](IReader* OBJ){
			I_ASSERT_M(OBJ,"Corrupted file [%s].", &fn);

			u32 version = 0;
			shared_str buf;
			shared_str sh_name;
			I_ASSERT_M(OBJ->r_chunk(EEditableObjectChunks::VERSION,&version), "Corrupted file [%s].", &fn);
			I_ASSERT_M(version==(u32)EEditableObjectVersions::Vanilla, "Unsupported file version. Object [%s] can't load.", &fn);

		
			if (OBJ->find_chunk(EEditableObjectChunks::SURFACES_SHARED))
			{
				slot.m_Surfaces.resize(OBJ->r_u32());
				for (auto& elem : slot.m_Surfaces)
				{
					I_ASSERT_M(OBJ->r_u8(),"Object [%s] contains non-shared material!", &fn);
					shared_str mat_name;
					OBJ->r_stringZ(mat_name);
					elem = CSharedMaterialLibrary::Instance().GetData(mat_name);
				}
			}
			
			OBJ->open_chunk(EEditableObjectChunks::EDITMESHES, [this, &fn, &slot](IReader& F)
			{
				bool Stop = false;
				u32 count = 0;
				while (!Stop)
				{
					F.open_chunk(count++, [this, &fn, &slot, &Stop](IReader* F)
					{
						if (!F)
						{
							Stop = true;
							return;
						}
						
						u32 version=0;
						R_ASSERT(F->r_chunk(EEditableMeshChunks::EMESH_CHUNK_VERSION,&version));
						if (!I_ASSERT_M(version==(u32)EEditableMeshVersions::EMESH_CURRENT_VERSION,
							"CEditableMesh [%s]: unsuported file version. Mesh can't load.", &fn)){
							return;
						}

						auto& mesh_slot = slot.m_meshes.emplace_back();
						
						F->open_chunk(EEditableMeshChunks::EMESH_CHUNK_MESHNAME, [this, &mesh_slot](IReader& F)
						{
							F.r_stringZ(mesh_slot.m_Name);
						});
						F->open_chunk(EEditableMeshChunks::EMESH_CHUNK_BBOX,[this, &mesh_slot](IReader& F)
						{
							F.r(&mesh_slot.m_Box, sizeof(mesh_slot.m_Box));
						});
						
						F->open_chunk(EEditableMeshChunks::EMESH_CHUNK_VERTS,[this, &mesh_slot](IReader& F)
						{
							mesh_slot.m_Vertices.resize(F.r_u32());
							F.r(mesh_slot.m_Vertices.data(), mesh_slot.m_Vertices.size()*sizeof(Fvector));
						});

						xr_vector<b_external_object_vmap> VMaps;
						F->open_chunk(EEditableMeshChunks::EMESH_CHUNK_VMAPS_2, [this, &VMaps](IReader& F)
						{
							VMaps.resize(F.r_u32());
							for (auto& elem : VMaps)
							{
								F.r_stringZ(elem.Name);
								elem.dim = F.r_u8();
								elem.polymap = F.r_u8();
								elem.type = F.r_u8();
								elem.resize(F.r_u32());
								F.r(elem.vm.data(), elem.vm.size()*sizeof(float));
								F.r(elem.vindices.data(), elem.vindices.size()*sizeof(int));
								if (elem.polymap)
								{
									F.r(elem.pindices.data(), elem.pindices.size()*sizeof(int));
								}
							}
						});
						xr_vector<b_external_object_vmap_list> VMapRefs;
						F->open_chunk(EEditableMeshChunks::EMESH_CHUNK_VMREFS,[this, &VMapRefs](IReader& F)
						{
							VMapRefs.resize(F.r_u32());
							for (auto& elem : VMapRefs)
							{
								elem.list.resize(F.r_u8());
								F.r(elem.list.data(), elem.list.size()*sizeof(b_external_object_vmap_pt));
							}
						});
						xr_vector<b_external_object_face_data> m_FacesRaw;
						F->open_chunk(EEditableMeshChunks::EMESH_CHUNK_FACES,[this, &m_FacesRaw](IReader& F)
						{
							m_FacesRaw.resize(F.r_u32());
							F.r(m_FacesRaw.data(), m_FacesRaw.size()*sizeof(b_external_object_face_data));
						});
						xr_vector<b_external_object_face_mat_link> links;
						F->open_chunk(EEditableMeshChunks::EMESH_CHUNK_SFACE, [this, &links](IReader& F)
						{
							links.resize(F.r_u16());
							for (auto& elem : links)
							{
								F.r_stringZ(elem.name);
								elem.Faces.resize(F.r_u32());
								F.r(elem.Faces.data(), elem.Faces.size()*sizeof(int));
							}
						});

						mesh_slot.m_Faces.reserve(m_FacesRaw.size()*2);
						for (auto& link : links)
						{
							u16 MatID = u16(-1);
							for (u16 i = 0; i < slot.m_Surfaces.size(); i++)
							{
								if (slot.m_Surfaces[i]->m_Name == link.name)
								{
									MatID = i;
									break;
								}
							}
							
							for (auto FIndex : link.Faces)
							{
								auto& raw = m_FacesRaw[FIndex];
								auto& target = mesh_slot.m_Faces.emplace_back();
								target.flags = b_face_flags::UseSharedMaterial;
								target.dwMaterial = MatID;
								target.dwMaterialGame = slot.m_Surfaces[MatID]->m_GameMtlName;
								for (int i = 0; i < 3; ++i)
								{
									target.v[i] = raw.pv[i].pindex;
									auto& VMRefList = VMapRefs[raw.pv[i].vmref].list;
									for (auto& elem : VMRefList)
									{
										auto& vmap = VMaps[elem.vmap_index];
										if (vmap.type!=0)
										{
											continue;
										}
										target.t[i].set(vmap.getUV(elem.index));
									}
								}

								if (slot.m_Surfaces[MatID]->m_Flags.test(SSurfaceData::sf2Sided))
								{
									auto& target2 = mesh_slot.m_Faces.emplace_back();
									target2.flags = b_face_flags::UseSharedMaterial;
									target2.dwMaterial = target.dwMaterial;
									target2.dwMaterialGame = target2.dwMaterial;
									target2.v[0] = target.v[0];
									target2.v[1] = target.v[1];
									target2.v[2] = target.v[2];
									target2.t[0] = target.t[0];
									target2.t[1] = target.t[1];
									target2.t[2] = target.t[2];
								}
							}
						}
						mesh_slot.m_Faces.shrink_to_fit();
						
						/*F->open_chunk(EEditableMeshChunks::EMESH_CHUNK_SG,[this, &mesh_slot](IReader& F)
						{
							mesh_slot.m_SmoothGroups.resize(mesh_slot.m_Vertices.size());
							F.r(mesh_slot.m_SmoothGroups.data(), mesh_slot.m_SmoothGroups.size()*sizeof(u32));
						});
						F->open_chunk(EEditableMeshChunks::EMESH_CHUNK_NORMALS,[this, &mesh_slot](IReader& F)
						{
							mesh_slot.m_Normals.resize(mesh_slot.m_Faces.size()*3);
							F.r(mesh_slot.m_Normals.data(), mesh_slot.m_Normals.size()*sizeof(Fvector));
							for (auto& Normal : mesh_slot.m_Normals)
							{
								Normal.x = -Normal.x;
								Normal.z = -Normal.z;
							}
						});*/
					});
				}
			});
		});

		auto Obj = _external_objects.emplace_back(new xrExternalObject());
		Obj->m_name = name;
		Obj->m_meshes.resize(slot.m_meshes.size());
		for (size_t i = 0; i < slot.m_meshes.size(); i++)
		{
			auto& MeshData = slot.m_meshes[i];
			auto& NewMesh = Obj->m_meshes[i];
			NewMesh = new xrExternalObjectMesh();
			NewMesh->m_name = MeshData.m_Name;
			NewMesh->m_vertices.resize(MeshData.m_Vertices.size());
			for (size_t j = 0; j < MeshData.m_Vertices.size(); j++)
			{
				auto VertexData = MeshData.m_Vertices[j];
				auto& NewVertex = NewMesh->m_vertices[j];
				NewVertex = new xrExternalVertex();
				NewVertex->P.set(VertexData);
				NewVertex->N.set(0,0,0);
			}
			NewMesh->m_faces.resize(MeshData.m_Faces.size());
			for (size_t j = 0; j < MeshData.m_Faces.size(); j++)
			{
				auto& FaceData = MeshData.m_Faces[j];
				auto& NewFace = NewMesh->m_faces[j];
				NewFace = new xrExternalFace();
				NewFace->dwMaterial = FaceData.dwMaterial;
				NewFace->dwMaterialGame = FaceData.dwMaterial;
				R_ASSERT(FaceData.dwMaterialGame<65536);
				NewFace->flags.bSharedMaterial = !!(FaceData.flags & b_face_flags::UseSharedMaterial);
				NewFace->SetVertex(0,NewMesh->m_vertices[FaceData.v[0]]);
				NewFace->SetVertex(1,NewMesh->m_vertices[FaceData.v[1]]);
				NewFace->SetVertex(2,NewMesh->m_vertices[FaceData.v[2]]);

				// tc
				Fvector2 uv1,uv2,uv3;
				uv1.set(FaceData.t[0].x,FaceData.t[0].y);
				uv2.set(FaceData.t[1].x,FaceData.t[1].y);
				uv3.set(FaceData.t[2].x,FaceData.t[2].y);
				NewFace->AddChannel( uv1, uv2, uv3 );
				NewFace->CalcNormal();
			}
		}
		return Obj;
	}
	return nullptr;
}

xrLC_GlobalData::~xrLC_GlobalData()
{
 
}
 
template<typename T>
void vec_clear( xr_vector<T*> &v )
{
	typename xr_vector<T*>::iterator i = v.begin(), e = v.end();
	for(;i!=e;++i)
		xr_delete(*i);
	v.clear();
 	v.shrink_to_fit();
}
 
template<typename T>
void vec_free(xr_vector<T*>& v)
{
	typename xr_vector<T*>::iterator i = v.begin(), e = v.end();
	for (; i != e; ++i)
		xr_free(*i);

 	v.clear();
	v.shrink_to_fit();
}

#include "../xrLC/Build.h"
 
void mu_mesh_clear();

// create - destroy 

// typedef poolSS<Vertex, 16 * 1024>	poolVertices;
// typedef poolSS<Face, 16 * 1024>		poolFaces;
// static poolVertices	_VertexPool;
// static poolFaces	_FacePool;

Face* xrLC_GlobalData::create_face()
{
	return new Face();
}

void xrLC_GlobalData::destroy_face(Face*& f)
{
	xr_delete(f);
}

Vertex* xrLC_GlobalData::create_vertex()
{
	return new Vertex();
}

void xrLC_GlobalData::destroy_vertex(Vertex*& v)
{
	return xr_delete(v);
}

void xrLC_GlobalData::clear() 
{
	// se7kills (�������� ��� ����������� ������ !)
	for (auto& surface : textures())
		surface.pSurface.Clear();
 	textures().clear();
	textures().shrink_to_fit();

 	_cl_globs._materials.clear();
	_cl_globs._shaders.Unload();
	clMsg("[xrLC_Remove] mem textures: %u mb", GetHeapMemory() / 1024 / 1024);

	// ������� ����� �� ������� ������� (_g_faces, _g_vertex) � ����������� !
	g_bUnregister = false;
 	
	for (auto F : _g_faces)
	{
		F->~Tface();
		xr_free(F);
	}
	_g_faces.clear();
	_g_faces.shrink_to_fit();
 
	for (auto V : _g_vertices)
	{
		V->~Tvertex();
		xr_free(V);
	}
 	_g_vertices.clear();
	_g_vertices.shrink_to_fit();
	
	clMsg("[xrLC_Remove] mem faces-vertex: %u mb", GetHeapMemory() / 1024 / 1024);
 
	// �� ������� ������ ������ !
	vec_clear(_mu_models); 
	vec_clear(_mu_refs);
	mu_mesh_clear();

	clMsg("[xrLC_Remove] mem mu-models: %u mb", GetHeapMemory() / 1024 / 1024);


	// Lighting stuff
	for (auto D : _g_deflectors)
		D->~CDeflector();
 	vec_free(_g_deflectors);

	vec_clear(_g_lightmaps);
	xr_delete(_cl_globs._RCAST_Model);

	clMsg("[xrLC_Remove] mem defl-lmaps: %u mb", GetHeapMemory() / 1024 / 1024);
}
 
