#include "StdAfx.h"
#include "ai_monster_utils.h"
#include "../../Entity.h"
#include "../../ai_object_location.h"
#include "../../ai_space.h"
#include "../../level_graph.h"
#include "../../../Include/xrRender/Kinematics.h"
#include "basemonster/base_monster.h"

#include "../../ai_object_location_impl.h"

// проверить, находится ли объект entity на ноде
// возвращает позицию объекта, если он находится на ноде, или центр его ноды
Fvector get_valid_position(const CEntity *entity, const Fvector &actual_position) 
{
	if (
		ai().level_graph().valid_vertex_id(entity->ai_location().level_vertex_id()) &&
		ai().level_graph().valid_vertex_position(entity->Position()) && 
		ai().level_graph().inside(entity->ai_location().level_vertex_id(), entity->Position())
		)
		return			(actual_position);
	else
		return			(ai().level_graph().vertex_position(entity->ai_location().level_vertex()));
}

// возвращает true, если объект entity находится на ноде
bool object_position_valid(const CEntity *entity)
{
	return				(
		ai().level_graph().valid_vertex_id(entity->ai_location().level_vertex_id()) &&
		ai().level_graph().valid_vertex_position(entity->Position()) && 
		ai().level_graph().inside(entity->ai_location().level_vertex_id(), entity->Position())
		);
}

namespace
{
bool try_get_bone_world_position(CObject* object, const char* bone_name, Fvector& out)
{
	if (!object || !object->Visual())
		return false;

	IKinematics* const kinematics = PKinematics(object->Visual());
	if (!kinematics)
		return false;

	const u16 bone_id = kinematics->LL_BoneID(bone_name);
	if (bone_id == BI_NONE)
		return false;

	kinematics->LL_GetBoneWorldPosition(bone_id, object->XFORM(), out);
	return true;
}
} // namespace

Fvector get_bone_position(CObject* object, const char* bone_name)
{
	Fvector result;
	if (!object)
	{
		result.set(0.f, 0.f, 0.f);
		return result;
	}

	if (try_get_bone_world_position(object, bone_name, result))
		return result;

	object->Center(result);
	return result;
}

Fvector get_head_position(CObject* object)
{
	Fvector result;
	if (!object)
	{
		result.set(0.f, 0.f, 0.f);
		return result;
	}

	if (CBaseMonster* const monster = object->cast_base_monster())
	{
		if (try_get_bone_world_position(object, monster->get_head_bone_name(), result))
			return result;
	}

	static const char* head_bone_names[] = {"bip01_head", "head", "eye_left", "eye_right"};
	for (const char* name : head_bone_names)
	{
		if (try_get_bone_world_position(object, name, result))
			return result;
	}

	object->Center(result);
	return result;
}
