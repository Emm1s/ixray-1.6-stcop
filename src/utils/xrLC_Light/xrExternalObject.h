#pragma once
#include "base_face.h"
#include "Lightmap.h"
#include "MeshStructure.h"
#include "tcf.h"
#include "src/utils/xrLC/OGF_Face.h"

struct xrExternalDataVertex;
struct xrExternalDataFace;
using xrExternalVertex = Tvertex<xrExternalDataVertex>;
using xrExternalFace = Tface<xrExternalDataVertex>;

struct XRLC_LIGHT_API xrExternalDataVertex : public base_Vertex
{
public:
    using DataFaceType = xrExternalDataFace;

    IC bool similar(Tvertex<xrExternalDataVertex>& V, float eps)
    {
        return P.similar(V.P, eps);
    }
};

struct XRLC_LIGHT_API xrExternalDataFace : public base_Face
{
public:

    Fvector N; // face normal
    svector<_TCF,2> tc; // TC

    void* pDeflector; // does the face has LM-UV map?
    CLightmap* lmap_layer;
    u32 sm_group;
    virtual Fvector2* getTC0() { return tc[0].uv; }


    bool RenderEqualTo(xrExternalFace* F);

    void AddChannel(Fvector2 &p1, Fvector2 &p2, Fvector2 &p3); 
    bool hasImplicitLighting();
};

class XRLC_LIGHT_API xrExternalObjectMesh
{
public:
    struct subdiv
    {
        u32 material;
        u32 start;
        u32 count;

        OGF* ogf;

        u32 vb_id;
        u32 vb_start;

        u32 ib_id;
        u32 ib_start;

        u32 sw_id;
        bool bSharedMaterial;
    };
	
    xr_vector<xrExternalVertex*> m_vertices;
    xr_vector<xrExternalFace*> m_faces;
    xr_vector<subdiv> m_subdivs;
    shared_str m_name;
    
};

class XRLC_LIGHT_API xrExternalObject
{
public:
    xr_vector<xrExternalObjectMesh*> m_meshes;
	shared_str m_name;
    
};

class XRLC_LIGHT_API xrExternalObjectReference
{
public:
    xrExternalObject* model;
    Fmatrix xform;
    Flags32 flags;
    u16 sector;
};