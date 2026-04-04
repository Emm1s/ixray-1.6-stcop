#pragma once

#include "base_lighting.h"
#include "global_slots_data.h"
#include "b_build_texture.h"
#include "global_slots_data.h"
#include "../../xrCore/Collision/xrCDB.h"

class Shader_xrLC_LIB;
//-----------------------------------------------------------------
struct global_claculation_data
{
	base_lighting g_lights; /////////////////////lc
	Shader_xrLC_LIB* g_shaders_xrlc;////////////////lc
	b_params g_params;//////////////////////lc
	xr_vector<b_material> g_materials;///////////////////lc
	xr_vector<b_material_shared> g_materials_shared;///////////////////lc
	xr_vector<b_BuildTexture> g_textures;////////////////////lc
	xr_hash_map<b_material_shared*, b_BuildTexture> g_textures_shared;////////////////////lc
	CDB::MODEL RCAST_Model;///////////////////lc

	Fbox LevelBB;//-----------============
	global_slots_data slots_data;//-------=============
	xr_vector<b_shader> g_shader_compile;//-----==========
	xr_vector<b_rc_face> g_rc_faces;//---------===============

			global_claculation_data		(): g_shaders_xrlc( 0 ) {}
	void	xrLoad						(bool skipThm);

	b_BuildTexture& FindTexture(u16 Index, bool Shared);
};

extern global_claculation_data	gl_data;