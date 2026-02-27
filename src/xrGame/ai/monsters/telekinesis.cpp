#include "StdAfx.h"
#include "telekinesis.h"
#include "../../entity_alive.h"
#include "../../../xrPhysics/PhysicsShell.h"

struct SFindPred
{
	CPhysicsShellHolder* obj;

	SFindPred(CPhysicsShellHolder* aobj)
	{
		obj = aobj;
	}

	bool operator ()(CTelekineticObject* tele_object) const
	{
		return tele_object->get_object() == obj;
	}
};

static bool RemovePred(CTelekineticObject* tele_object)
{
	return !tele_object->get_object() ||
		tele_object->get_object()->getDestroy() ||
		!tele_object->get_object()->PPhysicsShell() ||
		!tele_object->get_object()->PPhysicsShell()->isActive();
}

CTelekinesis::CTelekinesis()
{
	active = false;
}

CTelekinesis::~CTelekinesis()
{
	for (CTelekineticObject* object : telekinetic_objects)
	{
		object->release();
		xr_delete(object);
	}
}

CTelekineticObject* CTelekinesis::activate(CPhysicsShellHolder* obj, float strength, float height, u32 max_time_keep,
                                           bool rot)
{
	active = true;

	auto tele_object = new CTelekineticObject();

	if (!tele_object->init(this, obj, strength, height, max_time_keep, rot))
	{
		xr_delete(tele_object);
		return nullptr;
	}

	// добавить объект
	telekinetic_objects.push_back(tele_object);

	if (!telekinetic_objects.empty())
		Activate();

	return tele_object;
}

void CTelekinesis::clear()
{
	telekinetic_objects.clear();
}

void CTelekinesis::deactivate()
{
	active = false;

	// отпустить все объекты
	for (CTelekineticObject* object : telekinetic_objects)
	{
		object->release();
		xr_delete(object);
	}

	clear();
	Deactivate();
}

void CTelekinesis::clear_deactivate()
{
	active = false;

	// отпустить все объекты
	for (CTelekineticObject* object : telekinetic_objects)
	{
		object->switch_state(TS_NONE);
		xr_delete(object);
	}

	clear();
	Deactivate();
}

void CTelekinesis::deactivate(CPhysicsShellHolder* obj)
{
	// найти объект
	TELE_OBJECTS_IT it = std::find_if(telekinetic_objects.begin(), telekinetic_objects.end(), SFindPred(obj));

	if (it == telekinetic_objects.end())
		return;

	// отпустить объект
	(*it)->release();

	//remove from list, delete...
	remove_object(it);
}

void CTelekinesis::remove_object(CPhysicsShellHolder* obj)
{
	// найти объект
	TELE_OBJECTS_IT it = std::find_if(telekinetic_objects.begin(), telekinetic_objects.end(), SFindPred(obj));

	if (it == telekinetic_objects.end())
		return;

	//remove from list, delete...
	remove_object(it);
}

void CTelekinesis::remove_object(TELE_OBJECTS_IT it)
{
	// release memory
	xr_delete(*it);

	// удалить
	telekinetic_objects.erase(it);

	// проверить на полную деактивацию
	if (telekinetic_objects.empty())
	{
		clear();
		Deactivate();
		active = false;
	}
}

void CTelekinesis::throw_all_objects(const Fvector& target)
{
	if (!active)
		return;

	for (CTelekineticObject* object : telekinetic_objects)
		object->throw_object(target, 1.f);

	deactivate();
}

// бросить объект 'obj' в позицию 'target' с учетом коэф силы 
void CTelekinesis::fire(CPhysicsShellHolder* obj, const Fvector& target, float power)
{
	// найти объект
	TELE_OBJECTS_IT it = std::find_if(telekinetic_objects.begin(), telekinetic_objects.end(), SFindPred(obj));

	if (it == telekinetic_objects.end())
		return;

	// бросить объект
	(*it)->throw_object(target, power);
}

void CTelekinesis::throw_object_t(CPhysicsShellHolder* obj, const Fvector& target, float time)
{
	TELE_OBJECTS_IT it = std::find_if(telekinetic_objects.begin(), telekinetic_objects.end(), SFindPred(obj));

	if (it == telekinetic_objects.end())
		return;

	// бросить объект
	(*it)->throw_object_t(target, time);
}

void CTelekinesis::weapon_shoot(CPhysicsShellHolder* weapon)
{
	TELE_OBJECTS_IT it = std::find_if(telekinetic_objects.begin(), telekinetic_objects.end(), SFindPred(weapon));

	if (it == telekinetic_objects.end())
		return;

	(*it)->weapon_shoot();
}

bool CTelekinesis::is_active_object(CPhysicsShellHolder* obj)
{
	// найти объект
	TELE_OBJECTS_IT it = std::find_if(telekinetic_objects.begin(), telekinetic_objects.end(), SFindPred(obj));

	if (it == telekinetic_objects.end())
		return false;

	return true;
}

void CTelekinesis::schedule_update()
{
	if (!active) return;

	// обновить состояние объектов
	for (u32 i = 0; i < telekinetic_objects.size(); i++)
	{
		CTelekineticObject* cur_obj = telekinetic_objects[i];
		cur_obj->update_state();

		if (cur_obj->is_released())
			remove_object(telekinetic_objects.begin() + i);
	}
}

void CTelekinesis::PhDataUpdate(float step)
{
	if (!active)
		return;

	for (CTelekineticObject* object : telekinetic_objects)
	{
		switch (object->get_state())
		{
		case TS_RAISE:
			object->raise(step);
			break;

		case TS_KEEP:
			object->perform_keep_object();
			break;

		case TS_NONE:
			break;

		default: ;
		}
	}
}

void CTelekinesis::clear_notrelevant()
{
	//убрать все объеты со старыми параметрами
	telekinetic_objects.erase(
		std::remove_if(
			telekinetic_objects.begin(),
			telekinetic_objects.end(),
			&RemovePred
		),
		telekinetic_objects.end()
	);
}

void CTelekinesis::PhTune(float step)
{
	if (!active)
		return;

	clear_notrelevant();

	for (CTelekineticObject* telekinetic_object : telekinetic_objects)
	{
		switch (telekinetic_object->get_state())
		{
		case TS_RAISE:
		case TS_KEEP:
			telekinetic_object->enable();

		case TS_NONE:
			break;
		default: ;
		}
	}
}

u32 CTelekinesis::get_controlled_objects_count() const
{
	u32 count = 0;

	for (CTelekineticObject* object : telekinetic_objects)
	{
		ETelekineticState state = object->get_state();

		if (state == TS_RAISE || state == TS_KEEP)
			count++;
	}
	return count;
}

// объект был удален - удалить все связи на объект
void CTelekinesis::remove_links(CObject* O)
{
	remove_object(O->cast_physics_shell_holder());
}
