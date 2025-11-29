////////////////////////////////////////////////////////////////////////////
//	Module 		: script_binder_object_wrapper.cpp
//	Created 	: 29.03.2004
//  Modified 	: 29.03.2004
//	Author		: Dmitriy Iassenev
//	Description : Script object binder wrapper
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "pch_script.h"
#include "script_binder_object_wrapper.h"
#include "script_game_object.h"
#include "xrServer_Objects_ALife.h"

CScriptBinderObjectWrapper::CScriptBinderObjectWrapper	(luabind::object self, luabind::object object) :
	CScriptBinderObject	(self, object)
{
}

CScriptBinderObjectWrapper::~CScriptBinderObjectWrapper ()
{
}

void CScriptBinderObjectWrapper::reinit					()
{
	luabind::call_member<void>		(this,"reinit");
}

void CScriptBinderObjectWrapper::reinit_static			(CScriptBinderObject *script_binder_object)
{
	script_binder_object->CScriptBinderObject::reinit	();
}

void CScriptBinderObjectWrapper::reload					(const char* section)
{
	luabind::call_member<void>		(this,"reload",section);
}

void CScriptBinderObjectWrapper::reload_static			(CScriptBinderObject *script_binder_object, const char* section)
{
	script_binder_object->CScriptBinderObject::reload	(section);
}

bool CScriptBinderObjectWrapper::net_Spawn				(SpawnType DC)
{
	return							(luabind::call_member<bool>(this,"net_spawn",DC));
}

bool CScriptBinderObjectWrapper::net_Spawn_static		(CScriptBinderObject *script_binder_object, SpawnType DC)
{
	return							(script_binder_object->CScriptBinderObject::net_Spawn(DC));
}

void CScriptBinderObjectWrapper::net_Destroy			()
{
	luabind::call_member<void>		(this,"net_destroy");
}

void CScriptBinderObjectWrapper::net_Destroy_static		(CScriptBinderObject *script_binder_object)
{
	script_binder_object->CScriptBinderObject::net_Destroy();
}

void CScriptBinderObjectWrapper::net_Import				(NET_Packet *net_packet)
{
	luabind::call_member<void>		(this,"net_import",net_packet);
}

void CScriptBinderObjectWrapper::net_Import_static		(CScriptBinderObject *script_binder_object, NET_Packet *net_packet)
{
	script_binder_object->CScriptBinderObject::net_Import	(net_packet);
}

void CScriptBinderObjectWrapper::net_Export				(NET_Packet *net_packet)
{
	luabind::call_member<void>		(this,"net_export",net_packet);
}

void CScriptBinderObjectWrapper::net_Export_static		(CScriptBinderObject *script_binder_object, NET_Packet *net_packet)
{
	script_binder_object->CScriptBinderObject::net_Export	(net_packet);
}

void CScriptBinderObjectWrapper::shedule_Update			(u32 time_delta)
{
	luabind::call_member<void>		(this,"update",time_delta);
}

void CScriptBinderObjectWrapper::shedule_Update_static	(CScriptBinderObject *script_binder_object, u32 time_delta)
{
	script_binder_object->CScriptBinderObject::shedule_Update	(time_delta);
}

void CScriptBinderObjectWrapper::save					(NET_Packet *output_packet)
{
	luabind::call_member<void>		(this,"save",output_packet);
}

void CScriptBinderObjectWrapper::save_static			(CScriptBinderObject *script_binder_object, NET_Packet *output_packet)
{
	script_binder_object->CScriptBinderObject::save		(output_packet);
}

void CScriptBinderObjectWrapper::load					(IReader *input_packet)
{
	luabind::call_member<void>		(this,"load",input_packet);
}

void CScriptBinderObjectWrapper::load_static			(CScriptBinderObject *script_binder_object, IReader *input_packet)
{
	script_binder_object->CScriptBinderObject::load		(input_packet);
}

void CScriptBinderObjectWrapper::Serialize(ISaveObject* Object)
{
	if (I_ASSERT(m_luaBinderObject.is_valid()))
	{
		auto method = m_luaBinderObject["Serialize"];
		auto type = method.type();
		if (method && type == LUA_TFUNCTION)
		{
			luabind::call_member<void>(this, "Serialize", Object);
		} else
		{
			Msg("Missing method Serialize in binder for object [%s]", m_object->Name());			
			// Это ёбанный пиздец: именно метод Serialize, именно для биндеров (для серверных всё норм работает)
			//	отказывается нормально регистрироваться в lua, вызывая внутреннюю ошибку luabind
			//	(если в самой lua не сделать его, без вызова плюсовой части).
			// Я хуй знает как это говно чинить, уже что только можно перепробовал - нихуя.
			// Поэтому использую этот костыль.
			// Если кто поймёт, что за хуйня тут происходит - почините пж этот метод!
		}
	}
}

void CScriptBinderObjectWrapper::Serialize_static(CScriptBinderObject* script_binder_object, ISaveObject* Object)
{
	script_binder_object->CScriptBinderObject::Serialize(Object);
}

bool CScriptBinderObjectWrapper::net_SaveRelevant		()
{
	return							(luabind::call_member<bool>(this,"net_save_relevant"));
}

bool CScriptBinderObjectWrapper::net_SaveRelevant_static(CScriptBinderObject *script_binder_object)
{
	return							(script_binder_object->CScriptBinderObject::net_SaveRelevant());
}

void CScriptBinderObjectWrapper::net_Relcase			(CScriptGameObject *object)
{
	static const bool isSoC = EngineExternal().ShadowOfChernobylMode();
	if (isSoC)
	{
		luabind::call_member<void>		(this,"net_Relcase",object);
	}
}

void CScriptBinderObjectWrapper::net_Relcase_static		(CScriptBinderObject *script_binder_object, CScriptGameObject *object)
{
	script_binder_object->CScriptBinderObject::net_Relcase	(object);
}