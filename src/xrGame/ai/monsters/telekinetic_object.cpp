#include "StdAfx.h"
#include "../../PhysicsShellHolder.h"
#include "telekinetic_object.h"
#include "../../../xrPhysics/PhysicsShell.h"
#include "../../../xrPhysics/MathUtils.h"
#include "WeaponMagazined.h"
#include "../../Level.h"
#include "../../GameObject.h"

enum : u16
{
	KEEP_IMPULSE_UPDATE = 200,
	FIRE_TIME = 3000,
	RAISE_MAX_TIME = 5000
};

CTelekineticObject::CTelekineticObject()
{
	state = TS_NONE;
	object = nullptr;
	telekinesis = nullptr;
	m_rotate = false;
	m_is_weapon = false;
	m_is_shooting = false;
	m_shoot_phase_end = Random.randI(100, 1000);;
}

CTelekineticObject::~CTelekineticObject()
{
}

bool CTelekineticObject::init(CTelekinesis* tele, CPhysicsShellHolder* obj, float s, float h, u32 ttk, bool rot)
{
	if (!can_activate(obj))
		return false;

	switch_state(TS_RAISE);
	object = obj;

	target_height = obj->Position().y + h;

	time_keep_started = 0;
	time_keep_updated = 0;
	time_to_keep = ttk;

	strength = s;
	time_throw_started = 0;
	m_rotate = rot;

	if(object->m_pPhysicsShell)
		object->m_pPhysicsShell->set_ApplyByGravity(false);

	m_is_weapon = false;
	m_is_shooting = false;
	m_shoot_phase_end = 0;

	if (smart_cast<CWeaponMagazined*>(obj))
	{
		m_is_weapon = true;
		m_is_shooting = false;
		m_shoot_phase_end = 0;
	}
	return true;
}

void CTelekineticObject::set_sound(const ref_sound& snd_hold, const ref_sound& snd_throw)
{
	sound_hold.clone(snd_hold, st_Effect, sg_SourceType);
	sound_throw.clone(snd_throw, st_Effect, sg_SourceType);
}

void CTelekineticObject::raise_update()
{
	if (check_height() || check_raise_time_out())
		prepare_keep();
	else if (m_rotate)
		rotate();
}

void CTelekineticObject::keep_update()
{
	if (keep_time_elapsed())
		release();
}

void CTelekineticObject::throw_update()
{
	if (throw_time_elapsed())
		release();
}

void CTelekineticObject::update_state()
{
	switch (get_state())
	{
	case TS_RAISE:
		raise_update();
		break;

	case TS_KEEP:
		keep_update();
		break;

	case TS_THROW:
		throw_update();
		break;

	case TS_NONE:
		break;
	}
}

void CTelekineticObject::switch_state(ETelekineticState new_state)
{
	u32 time = Device.dwTimeGlobal;

	switch (new_state)
	{
	case TS_RAISE:
		time_raise_started = time;
		break;
	case TS_KEEP:
		time_keep_started = time;
		break;
	case TS_THROW:
		time_throw_started = time;
		break;
	case TS_NONE: break;
	}
	state = new_state;
}

void CTelekineticObject::raise(float step)
{
	if (!object || !object->m_pPhysicsShell || !object->m_pPhysicsShell->isActive()) return;

	step *= strength;

	Fvector dir;
	dir.set(0.f, 1.0f, 0.f);

	float elem_size = float(object->m_pPhysicsShell->Elements().size());
	dir.mul(elem_size * elem_size * strength);

	if (OnServer())
		object->m_pPhysicsShell->get_ElementByStoreOrder(0)->applyGravityAccel(dir);


	update_hold_sound();
}

void CTelekineticObject::prepare_keep()
{
	switch_state(TS_KEEP);
	time_keep_updated = 0;
}


bool CTelekineticObject::keep_time_elapsed() const
{
	if (time_keep_started + time_to_keep < Device.dwTimeGlobal)
		return true;

	return false;
}

bool CTelekineticObject::throw_time_elapsed() const
{
	// time_fire_started + FIRE_TIME (3s) чтобы не сразу не захватывать только что брошенный предмет.
	if (time_throw_started + FIRE_TIME < Device.dwTimeGlobal)
		return true;

	return false;
}

bool CTelekineticObject::is_weapon() const
{
	return smart_cast<CWeaponMagazined*>(object);
}

void CTelekineticObject::perform_keep_object()
{
	if (!object || !object->m_pPhysicsShell || !object->m_pPhysicsShell->isActive()) return;

	// проверить высоту
	float cur_h = object->Position().y;

	// установить dir в соответствие с текущей высотой
	Fvector dir;
	if (cur_h > target_height + 0.6f) dir.set(0.f, -1.0f, 0.f);
	else if (cur_h < target_height + 0.6f) dir.set(0.f, 1.0f, 0.f);
	else
	{
		dir.set(Random.randF(-1.0f, 1.0f), Random.randF(-1.0f, 1.0f), Random.randF(-1.0f, 1.0f));
		dir.normalize_safe();
	}

	dir.mul(5.0f);

	if (OnServer())
		(object->m_pPhysicsShell->get_ElementByStoreOrder(0))->applyGravityAccel(dir);

	// установить время последнего обновления
	time_keep_updated = Device.dwTimeGlobal;
	update_hold_sound();
}

void CTelekineticObject::weapon_shoot()
{
	if (!m_is_weapon || !object)
		return;

	auto weapon = smart_cast<CWeaponMagazined*>(object);

	if (!weapon)
		return;
	
	if (u32 now = time(); now >= m_shoot_phase_end)
	{
		if (m_is_shooting)
		{
			weapon->FireEnd();

			m_is_shooting = false;
			m_shoot_phase_end = now + Random.randI(100, 1000);
		}
		else
		{
			weapon->FireStart();

			m_is_shooting = true;
			m_shoot_phase_end = now + Random.randI(100, 300);
		}
	}
}

void CTelekineticObject::release()
{
	if (!object || !object->m_pPhysicsShell || !object->m_pPhysicsShell->isActive()) return;

	if (CWeaponMagazined* weapon_magazined = object->cast_weapon_magazined())
	{
		Msg("[CTelekineticObject::release()] weapon_magazined->FireEnd(); %u", Device.dwTimeGlobal);
		weapon_magazined->FireEnd();
	}

	Fvector dir_inv;
	dir_inv.set(0.f, -1.0f, 0.f);

	// включить гравитацию
	object->m_pPhysicsShell->set_ApplyByGravity(true);
	if (OnServer())
	{
		// приложить небольшую силу для того, чтобы объект начал падать
		object->m_pPhysicsShell->applyImpulse(dir_inv, 0.5f * object->m_pPhysicsShell->getMass());
	}
	//state = TS_None;
	switch_state(TS_NONE);
}

void CTelekineticObject::throw_object_t(const Fvector& target, float time)
{
	switch_state(TS_THROW);

	if (!object || !object->m_pPhysicsShell || !object->m_pPhysicsShell->isActive())
		return;

	// включить гравитацию
	object->m_pPhysicsShell->set_ApplyByGravity(true);

	Fvector transference;
	transference.sub(target, object->Position());
	TransferenceToThrowVel(transference, time, object->EffectiveGravity());
	object->m_pPhysicsShell->set_LinearVel(transference);

	if (sound_throw.handle())
		sound_throw.play_at_pos(object, object->Position());

	if (sound_hold.is_playing())
		sound_hold.stop();
}

void CTelekineticObject::throw_object(const Fvector& target, float power)
{
	switch_state(TS_THROW);

	if (!object || !object->m_pPhysicsShell || !object->m_pPhysicsShell->isActive())
		return;

	// вычислить направление
	Fvector dir;
	dir.sub(target, object->Position());
	dir.normalize();

	// включить гравитацию
	object->m_pPhysicsShell->set_ApplyByGravity(true);

	if (OnServer())
	{
		// выполнить бросок
		for (u32 i = 0; i < object->m_pPhysicsShell->get_ElementsNumber(); i++)
			object->m_pPhysicsShell->get_ElementByStoreOrder(u16(i))->applyImpulse(
				dir, power * 20.f * object->m_pPhysicsShell->getMass() / object->m_pPhysicsShell->Elements().size());
	}
};

bool CTelekineticObject::check_height() const
{
	if (!object)
		return true;

	return object->Position().y > target_height;
}

bool CTelekineticObject::check_raise_time_out() const
{
	if (time_raise_started + RAISE_MAX_TIME < Device.dwTimeGlobal)
		return true;

	return false;
}

void CTelekineticObject::enable() const
{
	if (object->m_pPhysicsShell)
		object->m_pPhysicsShell->Enable();
}

void CTelekineticObject::rotate() const
{
	if (!object || !object->m_pPhysicsShell || !object->m_pPhysicsShell->isActive())
		return;

	// вычислить направление
	Fvector dir;
	dir.random_dir();
	dir.normalize();

	if (OnServer())
		object->m_pPhysicsShell->applyImpulse(dir, 2.5f * object->m_pPhysicsShell->getMass());
}

bool CTelekineticObject::can_activate(CPhysicsShellHolder* obj)
{
	return obj && obj->m_pPhysicsShell;
}

void CTelekineticObject::update_hold_sound()
{
	if (sound_hold.handle()) return;
	if (sound_hold.is_playing())
		sound_hold.set_position(object->Position());
	else
		sound_hold.play_at_pos(object, object->Position());
}
