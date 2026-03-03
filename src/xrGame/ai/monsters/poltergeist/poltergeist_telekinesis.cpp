#include "HUDManager.h"
#include "StdAfx.h"
#include "poltergeist.h"
#include "WeaponMagazined.h"
#include "../../../PhysicsShellHolder.h"
#include "../../../Level.h"
#include "../../../Actor.h"
#include "../../../../xrPhysics/IColisiondamageInfo.h"
#include "../../../ActorCondition.h"
#include "../../../Inventory.h"
#include "../../../Weapon.h"

//////////////////////////////////////////////////////////////////////////
// Выбор подходящих объектов для телекинеза
//////////////////////////////////////////////////////////////////////////
class best_object_predicate
{
	Fvector enemy_pos;
	Fvector monster_pos;

public:
	best_object_predicate(const Fvector& m_pos, const Fvector& pos)
	{
		monster_pos = m_pos;
		enemy_pos = pos;
	}

	bool operator()(const CGameObject* tpObject1, const CGameObject* tpObject2) const
	{
		float dist1 = monster_pos.distance_to(tpObject1->Position());
		float dist2 = enemy_pos.distance_to(tpObject2->Position());
		float dist3 = enemy_pos.distance_to(monster_pos);

		return dist1 < dist3 && dist2 > dist3;
	}
};

class best_object_predicate2
{
	Fvector enemy_pos;
	Fvector monster_pos;

public:
	using CObject_ptr = CObject*;

	best_object_predicate2(const Fvector& m_pos, const Fvector& pos)
	{
		monster_pos = m_pos;
		enemy_pos = pos;
	}

	bool operator()(const CObject_ptr& tpObject1, const CObject_ptr& tpObject2) const
	{
		float dist1 = enemy_pos.distance_to(tpObject1->Position());
		float dist2 = enemy_pos.distance_to(tpObject2->Position());

		return dist1 < dist2;
	}
};

CTelekineticPoltergeist::CTelekineticPoltergeist(CPoltergeist* polter) : inherited(polter),
                                                                         m_pmt_object_collision_damage(0.5f)
{
	
}

CTelekineticPoltergeist::~CTelekineticPoltergeist()
{
	
}

void CTelekineticPoltergeist::load(LPCSTR section)
{
	inherited::load(section);

	m_pmt_radius = READ_IF_EXISTS(pSettings, r_float, section, "Tele_Find_Radius", 10.f);
	m_pmt_object_min_mass = READ_IF_EXISTS(pSettings, r_float, section, "Tele_Object_Min_Mass", 40.f);
	m_pmt_object_max_mass = READ_IF_EXISTS(pSettings, r_float, section, "Tele_Object_Max_Mass", 500.f);
	m_pmt_object_count = READ_IF_EXISTS(pSettings, r_u32, section, "Tele_Object_Count", 10);
	m_pmt_time_to_hold = READ_IF_EXISTS(pSettings, r_u32, section, "Tele_Hold_Time", 3000);
	m_pmt_time_to_wait = READ_IF_EXISTS(pSettings, r_u32, section, "Tele_Wait_Time", 3000);
	m_pmt_time_to_wait_in_objects = READ_IF_EXISTS(pSettings, r_u32, section, "Tele_Delay_Between_Objects_Time", 500);
	m_pmt_distance = READ_IF_EXISTS(pSettings, r_float, section, "Tele_Distance", 50.f);
	m_pmt_object_height = READ_IF_EXISTS(pSettings, r_float, section, "Tele_Object_Height", 10.f);
	m_pmt_time_object_keep = READ_IF_EXISTS(pSettings, r_u32, section, "Tele_Time_Object_Keep", 10000);
	m_pmt_raise_speed = READ_IF_EXISTS(pSettings, r_float, section, "Tele_Raise_Speed", 3.f);
	m_pmt_raise_time_to_wait_in_objects = READ_IF_EXISTS(pSettings, r_u32, section,
	                                                     "Tele_Delay_Between_Objects_Raise_Time", 500);
	m_pmt_fly_velocity = READ_IF_EXISTS(pSettings, r_float, section, "Tele_Fly_Velocity", 30.f);
	m_pmt_object_collision_damage = READ_IF_EXISTS(pSettings, r_float, section, "Tele_Collision_Damage", 0.5f);
	
	Sound->create(m_sound_tele_hold, pSettings->r_string(section, "sound_tele_hold"),
	              st_Effect, SOUND_TYPE_WORLD);
	Sound->create(m_sound_tele_throw, pSettings->r_string(section, "sound_tele_throw"),
	              st_Effect, SOUND_TYPE_WORLD);

	m_state = ETeleState::WAIT;
	m_state_start_time = 0;
	m_state_next_update = 0;
}

void CTelekineticPoltergeist::update_schedule()
{
	inherited::update_schedule();
}

void CTelekineticPoltergeist::update_frame()
{
	inherited::update_frame();
}

void CTelekineticPoltergeist::UpdateCL()
{
	const CEntityAlive* enemy = m_poltergeist->EnemyMan.get_enemy();

	if (!enemy)
		return;

	if (m_poltergeist->get_actor_ignore() || enemy->Position().distance_to(m_poltergeist->Position()) > m_pmt_distance)
		return;

	const Fvector enemy_pos = enemy->Position();
	const float distance_to_enemy = enemy_pos.distance_to(m_poltergeist->Position());

	if (distance_to_enemy > m_pmt_distance)
		return;

	if (m_poltergeist->get_actor_ignore())
		return;

	switch (m_state)
	{
	case ETeleState::RAISE_OBJECTS:
		if (m_state_start_time + m_state_next_update < time())
		{
			if (!tele_raise_objects())
				m_state = ETeleState::MAIN_PHASE;

			m_state_start_time = time();
			m_state_next_update = m_pmt_raise_time_to_wait_in_objects / 2 + 
				Random.randI(m_pmt_raise_time_to_wait_in_objects / 2);
		}

		if (m_state == ETeleState::RAISE_OBJECTS)
		{
			if (m_poltergeist->get_controlled_objects_count() >= m_pmt_object_count)
			{
				m_state_start_time = time();
				m_state = ETeleState::MAIN_PHASE;
			}
		}
		break;

		// Главная фаза телекинеза полтера: стрельба + бросаемся предметами.
	case ETeleState::MAIN_PHASE:
		m_poltergeist->update_telekinetic_behaviour(enemy);
		
		// Это уже каждый кадр вызывать не надо, а тольок тогда, когда время на удержание одного объекта истекло.
		// Нужно это чтобы сразу же не кидаться поднятыми предметами.
		if (m_state_start_time + m_pmt_time_to_hold > time() &&
			m_state_start_time + m_state_next_update > time())
				break;
		
		throw_objects(); // подержали m_pmt_time_to_hold + m_state_next_update и кидаемся объектами.
		
		m_state_start_time = time();
		m_state_next_update = m_pmt_time_to_wait_in_objects / 2 + Random.randI(m_pmt_time_to_wait_in_objects / 2);
		
		if (m_poltergeist->get_controlled_objects_count() <= 0)
		{
			m_state_start_time = time();
			m_state = ETeleState::WAIT;
		}

		// Отстрелялись, откидались, ждём три секунды, перед тем как вновь поднимать предметы.
	case ETeleState::WAIT:
		if (m_state_start_time + m_pmt_time_to_wait < time())
		{
			m_state_next_update = 0;
			m_state_start_time = time();
			m_state = ETeleState::RAISE_OBJECTS;
		}
		break;
	}
}

void CTelekineticPoltergeist::tele_find_objects(xr_vector<CObject*>& objects, const Fvector& pos)
{
	objects.clear();
	g_SpatialSpace->q_sphere(m_nearest,0,ESPATIAL_TYPE::COLLIDEABLE, pos, m_pmt_radius);
	for (ISpatialShared& SS : m_nearest)
	{
		ISpatial* S = SS.get();
		if (!S) continue;
		CObject* pObject = S->dcast_CObject();
		if (!pObject || pObject->getDestroy()) continue;

		CPhysicsShellHolder* obj = pObject->cast_physics_shell_holder();
		CMonsterEnemyManager& enemy = this->m_poltergeist->EnemyMan;

		if (!obj ||
			!obj->PPhysicsShell() ||
			!obj->PPhysicsShell()->isActive() ||
			obj->cast_creature() ||
			(obj->spawn_ini() && obj->spawn_ini()->section_exist("ph_heavy")) ||
			obj->m_pPhysicsShell->getMass() < m_pmt_object_min_mass ||
			obj->m_pPhysicsShell->getMass() > m_pmt_object_max_mass ||
			obj == m_poltergeist ||
			m_poltergeist->is_active_object(obj) ||
			!obj->m_pPhysicsShell->get_ApplyByGravity() || !enemy.get_enemy())
			continue;

		Fvector center;
		enemy.get_enemy()->Center(center);

		if (trace_object(obj, center) ||
			trace_object(obj, get_head_position(fast_dynamic_cast<CObject*>((CEntityAlive*)enemy.get_enemy()))))
		{
			objects.push_back(obj);
		}
	}
}

bool CTelekineticPoltergeist::tele_raise_objects()
{
	// find objects near enemy
	xr_vector<CObject*>& tele_objects = m_poltergeist->tele_objects;
	const CEntityAlive* enemy = this->m_poltergeist->EnemyMan.get_enemy();

	// получить список объектов вокруг врага	
	tele_find_objects(tele_objects, enemy->Position());
	// получить список объектов вокруг монстра
	tele_find_objects(tele_objects, m_poltergeist->Position());

	// получить список объектов между монстром и врагом
	float dist = enemy->Position().distance_to(m_poltergeist->Position());

	Fvector dir;
	dir.sub(enemy->Position(), m_poltergeist->Position());
	dir.normalize();

	Fvector pos;
	pos.mad(m_poltergeist->Position(), dir, dist / 2.f);
	tele_find_objects(tele_objects, pos);

	// сортировать и оставить только необходимое количество объектов
	std::ranges::sort(tele_objects, best_object_predicate2(m_poltergeist->Position(), enemy->Position()));
	// оставить уникальные объекты
	tele_objects.erase(std::ranges::unique(tele_objects).begin(), tele_objects.end());

	if (!tele_objects.empty())
	{
		CPhysicsShellHolder* obj = tele_objects[0] != nullptr ? tele_objects[0]->cast_physics_shell_holder() : nullptr;
		bool rotate = false;

		CTelekineticObject* tele_obj = m_poltergeist->CTelekinesis::activate(
			obj,
			m_pmt_raise_speed, m_pmt_object_height,
			m_pmt_time_object_keep,
			rotate
		);
		
		tele_obj->set_sound(m_sound_tele_hold, m_sound_tele_throw);
		return true;
	}
	return false;
}

bool CTelekineticPoltergeist::trace_object(CObject* obj, const Fvector& target)
{
	Fvector trace_from;
	obj->Center(trace_from);

	Fvector dir;
	dir.sub(target, trace_from);

	float range = dir.magnitude();

	if (range < EPS)
		return false;

	dir.normalize();

	collide::rq_result rq_result;

	if (Level().ObjectSpace.RayPick(trace_from, dir, range, collide::rqtBoth, rq_result, obj))
	{
		CObject* raypicked_object = rq_result.O;
		const CEntityAlive* our_enemy = this->m_poltergeist->EnemyMan.get_enemy();

		if (raypicked_object == our_enemy)
			return true;
	}
	return false;
}

struct SCollisionHitCallback : ICollisionHitCallback
{
	CPhysicsShellHolder* m_object;
	float m_pmt_object_collision_damage;

	SCollisionHitCallback(CPhysicsShellHolder* object, float pmt_object_collision_damage) : m_object(object),
		m_pmt_object_collision_damage(pmt_object_collision_damage)
	{
		VERIFY(object);
	}

	void call(IPhysicsShellHolder* obj, float min_cs, float max_cs, float& cs, float& hl,
	          ICollisionDamageInfo* di) override
	{
		if (cs > min_cs * 0.5f)
			hl = m_pmt_object_collision_damage;
		VERIFY(m_object);
		di->SetInitiated();

		if (obj->ObjectID() == 0 && !GodMode())
		{
			const float stamina = Actor()->conditions().GetPower();

			bool need_kick_animator = false;

			PIItem active_item = Actor()->inventory().ActiveItem();
			CCustomDevice* device = Actor()->GetDevice();

			if (stamina > hl)
			{
				Actor()->conditions().SetPower(stamina - hl);
			}
			else if (active_item != nullptr || device != nullptr)
			{
				if (Random.randF(0.0f, 1.0f) < hl - stamina)
				{
					if (active_item != nullptr)
					{
						u16 slot = active_item->BaseSlot();
						if (!Actor()->inventory().SlotIsPersistent(slot) && !Actor()->inventory().Action(
							kDROP, CMD_STOP))
						{
							Actor()->g_PerformDrop();
							need_kick_animator = true;
						}
					}

					if (device != nullptr)
					{
						device->SetDropManual(true);
						need_kick_animator = true;
					}
				}
			}
			else
			{
				need_kick_animator = true;
			}

			if (need_kick_animator && !Actor()->HudAnimator()->ItemAnimator()->IsActive())
			{
				auto GetAngleCos = [&](const Fvector& v1, const Fvector& v2)
				{
					return v1.dotproduct(v2) / (v1.magnitude() * v2.magnitude());
				};

				Fvector dir = zero_vel;
				di->HitDir(dir);
				bool is_actor_see_monster = GetAngleCos(dir, Device.vCameraDirection) < 0.0f;

				Actor()->inventory().SetActiveSlot(NO_ACTIVE_SLOT);

				const shared_str& front_kick_animator = Actor()->m_sFrontKickAnimator;
				const shared_str& back_kick_animator = Actor()->m_sBackKickAnimator;

				if (is_actor_see_monster)
				{
					if (front_kick_animator.size() > 0)
					{
						Actor()->HudAnimator()->ItemAnimator()->StartAnimator(front_kick_animator);
					}
				}
				else
				{
					if (back_kick_animator.size() > 0)
					{
						Actor()->HudAnimator()->ItemAnimator()->StartAnimator(back_kick_animator);
					}
				}
			}
		}

		m_object->set_collision_hit_callback(nullptr); //delete this!!
	}
};

void CTelekineticPoltergeist::throw_objects()
{
	const CEntityAlive* enemy = this->m_poltergeist->EnemyMan.get_enemy();

	if (enemy == nullptr)
	{
		return;
	}

	for (CTelekineticObject* tele_object : m_poltergeist->telekinetic_objects)
	{
		if (tele_object->get_state() == ETelekineticState::TS_KEEP)
		{
			Fvector enemy_head = get_head_position(fast_dynamic_cast<CObject*>(enemy));
			CPhysicsShellHolder* hobj = tele_object->get_object();

			VERIFY(hobj);
			hobj->set_collision_hit_callback(new SCollisionHitCallback(hobj, m_pmt_object_collision_damage));

			CWeaponMagazined* weapon = smart_cast<CWeaponMagazined*>(tele_object->get_object());
			
			if (weapon != nullptr && weapon->GetAmmoElapsed() > 0 && weapon->IsMisfire() != true)
				continue;

			if (trace_object(tele_object->get_object(), enemy_head))
			{
				m_poltergeist->throw_object_time(
					tele_object->get_object(),
					enemy_head,
					tele_object->get_object()->Position().distance_to(enemy_head) / m_pmt_fly_velocity
				);
				break;
			}
		}
	}
}

// void CTelekineticPoltergeist::weapon_shoot()
// {
// 	const CEntityAlive* enemy = this->m_poltergeist->EnemyMan.get_enemy();
//
// 	if (enemy == nullptr)
// 		return;
//
// 	for (CTelekineticObject* tele_object : m_poltergeist->telekinetic_objects)
// 	{
// 		update_weapon_behaviour(tele_object);
// 		
// 		if (can_shoot(tele_object))
// 		{
// 			CWeaponMagazined* weapon = smart_cast<CWeaponMagazined*>(tele_object->get_object());
// 			m_poltergeist->weapon_shoot(weapon);
// 		}
// 	}
// }
//
// void CTelekineticPoltergeist::update_weapon_behaviour(CTelekineticObject* tele_object)
// {
// 	const CEntityAlive* our_enemy = m_poltergeist->EnemyMan.get_enemy();
// 	
// 	if (our_enemy == nullptr)
// 		return;
// 	
// 	if (tele_object->is_weapon() == false)
// 		return;
// 	
// 	CWeaponMagazined* weapon = smart_cast<CWeaponMagazined*>(tele_object->get_object());
// 	
// 	Fvector fire_pos = weapon->get_LastFP();
// 	Fvector weapon_dir = weapon->get_LastFD();
// 	weapon_dir.normalize();
//
// 	Fvector dir_to_target = our_enemy->Center() - fire_pos;
// 	float dist_to_target = dir_to_target.magnitude();
// 	float angle_difference = weapon_dir.dotproduct(dir_to_target);
// 	
// 	collide::rq_result rq_result;
// 	
// 	if (Level().ObjectSpace.RayPick(fire_pos, weapon_dir, dist_to_target, collide::rqtBoth, rq_result, weapon))
// 	{
// 		if (rq_result.O != our_enemy)
// 			weapon->FireEnd();
// 	}
// 	
// 	if (dist_to_target > m_pmt_distance)
// 		weapon->FireEnd();
// 	
// 	if (tele_object->get_state() != ETelekineticState::TS_KEEP)
// 		weapon->FireEnd();
// 	
// 	if (our_enemy->g_Alive() == false)
// 		weapon->FireEnd();
// 	
// 	if (weapon->GetAmmoElapsed() <= 0)
// 		weapon->FireEnd();
// }
//
// bool CTelekineticPoltergeist::can_shoot(const CTelekineticObject* tele_object)
// {
// 	const CEntityAlive* our_enemy = m_poltergeist->EnemyMan.get_enemy();
// 	auto weapon = smart_cast<CWeaponMagazined*>(tele_object->get_object());
//
// 	if (our_enemy == nullptr || tele_object == nullptr || weapon == nullptr)
// 		return false;
//
// 	if (tele_object->get_state() != ETelekineticState::TS_KEEP)
// 		return false;
// 	
// 	if (weapon->GetAmmoElapsed() <= 0)
// 		return false;
// 	
// 	if (our_enemy->g_Alive() == false)
// 		return false;
// 	
// 	if (weapon->IsMisfire()) 
// 		return false;
// 	
// 	Fvector fire_pos = weapon->get_LastFP();
// 	Fvector weapon_dir = weapon->get_LastFD();
// 	weapon_dir.normalize();
//
// 	Fvector dir_to_target = our_enemy->Center() - fire_pos;
// 	float dist_to_target = dir_to_target.magnitude();
// 	dir_to_target.normalize();
// 	
// 	collide::rq_result rq_result;
// 	
// 	if (Level().ObjectSpace.RayPick(fire_pos, weapon_dir, dist_to_target, collide::rqtBoth, rq_result, weapon))
// 	{
// 		if (rq_result.O != our_enemy)
// 			return false;
// 		
// 		Fvector end_pos;
// 		end_pos.mad(fire_pos, weapon_dir, rq_result.range);
// 		HUD().world_prims.append_line(fire_pos, end_pos, color_rgba(0, 255, 0, 255));
// 		
// 		return true;
// 	}
// }

// CTelekineticWeaponController::CTelekineticWeaponController(CTelekineticPoltergeist* telekinetic_poltergeist) :
// 	telekinetic_poltergeist_(telekinetic_poltergeist)
// {
// }
//
// CTelekineticWeaponController::~CTelekineticWeaponController()
// {
// 	for (CTelekineticWeapon* telekinetic_weapon : tele_weapons_)
// 		xr_delete(telekinetic_weapon);
// }
//
// void CTelekineticWeaponController::register_object(CTelekineticObject* obj)
// {
// 	CTelekineticWeapon* weapon = new CTelekineticWeapon(obj);
// 	
// 	if (weapon->is_valid())
// 		tele_weapons_.push_back(weapon);
// 	else 
// 		xr_delete(weapon);
// }
//
// void CTelekineticWeaponController::unregister_object(CTelekineticObject* obj)
// {
// 	const auto it = std::ranges::find_if(tele_weapons_, [&](const CTelekineticWeapon* weapon)
// 	{
// 		return weapon->get_object() == obj->get_object();
// 	});
//
// 	if (it != tele_weapons_.end())
// 	{
// 		xr_delete(*it);
// 		tele_weapons_.erase(it);
// 	}
// }
//
// void CTelekineticWeaponController::update(const CEntityAlive* enemy)
// {
// 	for (CTelekineticWeapon* weapon : tele_weapons_)
// 	{
// 		if (weapon->is_valid() == false)
// 			return;
// 		
// 		update_weapon_behaviour(weapon, enemy);
// 		update_auto_aim(weapon, enemy);
// 		debug_draw(weapon, enemy);
// 		try_shoot(weapon);
// 	}
// }
//
// void CTelekineticWeaponController::update_auto_aim(CTelekineticWeapon* telekinetic_weapon, const CEntityAlive* enemy)
// {
// 	if (telekinetic_weapon->get_state() != ETelekineticState::TS_KEEP) 
// 		return;
// 	
// 	if (telekinetic_weapon->get_weapon_magazined()->GetAmmoElapsed() <= 0)
// 		return;
// 	
// 	Fvector target_dir;
// 	target_dir.sub(enemy->Center(), telekinetic_weapon->get_weapon_magazined()->get_LastFP());
// 	target_dir.normalize_safe();
//
// 	Fmatrix target_matrix;
// 	target_matrix.identity();
// 	target_matrix.k.set(target_dir);
// 	Fvector::generate_orthonormal_basis_normalized(target_matrix.k, target_matrix.j, target_matrix.i);
//
// 	Fvector curr_eulers, target_eulers;
// 	telekinetic_weapon->get_weapon_magazined()->XFORM().getXYZi(curr_eulers);
// 	target_matrix.getXYZi(target_eulers);
//
// 	Fvector diff = {
// 		angle_difference_signed(target_eulers.x, curr_eulers.x),
// 		angle_difference_signed(target_eulers.y, curr_eulers.y),
// 		angle_difference_signed(target_eulers.z, curr_eulers.z)
// 	};
//
// 	diff.mul(telekinetic_weapon->get_weapon_magazined()->m_pPhysicsShell->getMass());
// 	telekinetic_weapon->get_weapon_magazined()->m_pPhysicsShell->setTorque(diff);
//
// 	dVector3 vel;
// 	dBodyID body = telekinetic_weapon->get_weapon_magazined()->m_pPhysicsShell->get_ElementByStoreOrder(0)->get_body();
// 	dBodyVectorToWorld(body, diff.x, diff.y, diff.z, vel);
// 	dBodySetAngularVel(body, vel[0], vel[1], vel[2]);
// }
//
// void CTelekineticWeaponController::update_weapon_behaviour(CTelekineticWeapon* telekinetic_weapon, const CEntityAlive* enemy) const
// {
// 	if (enemy == nullptr)
// 		return;
//
// 	Fvector fire_pos = telekinetic_weapon->weapon_->get_LastFP();
// 	Fvector weapon_dir = telekinetic_weapon->weapon_->get_LastFD();
// 	weapon_dir.normalize();
//
// 	Fvector dir_to_target = enemy->Center() - fire_pos;
// 	float dist_to_target = dir_to_target.magnitude();
// 	
// 	collide::rq_result rq_result;
//
// 	if (Level().ObjectSpace.RayPick(fire_pos, weapon_dir, dist_to_target, collide::rqtBoth, rq_result,
// 	                                telekinetic_weapon->weapon_))
// 	{
// 		if (rq_result.O != enemy)
// 			telekinetic_weapon->weapon_->FireEnd();
// 	}
// 	
// 	if (dist_to_target > telekinetic_poltergeist_->m_pmt_distance)
// 		telekinetic_weapon->weapon_->FireEnd();
// 	
// 	if (telekinetic_weapon->get_state() != ETelekineticState::TS_KEEP)
// 		telekinetic_weapon->weapon_->FireEnd();
// 	
// 	if (enemy->g_Alive() == false)
// 		telekinetic_weapon->weapon_->FireEnd();
// 	
// 	if (telekinetic_weapon->weapon_->GetAmmoElapsed() <= 0)
// 		telekinetic_weapon->weapon_->FireEnd();
// }
//
// bool CTelekineticWeaponController::can_shoot(CTelekineticWeapon* telekinetic_weapon, const CEntityAlive* enemy) const
// {
// 	if (!telekinetic_weapon->is_ready_to_shoot()) 
// 		return false;
//
// 	Fvector fire_pos = telekinetic_weapon->get_weapon_magazined()->get_LastFP();
// 	Fvector fire_dir = telekinetic_weapon->get_weapon_magazined()->get_LastFD();
// 	fire_dir.normalize();
//
// 	Fvector to_target = enemy->Center() - fire_pos;
// 	float dist = to_target.magnitude();
// 	
// 	if (dist > telekinetic_poltergeist_->m_pmt_distance) 
// 		return false;
//
// 	to_target.normalize();
//
// 	collide::rq_result rq_result;
//
// 	if (Level().ObjectSpace.RayPick(fire_pos, fire_dir, dist, collide::rqtBoth, rq_result,
// 	                                telekinetic_weapon->get_weapon_magazined()))
// 	{
// 		if (rq_result.O == enemy)
// 			return true;
// 	}
// 	return false;
// }
//
// void CTelekineticWeaponController::try_shoot(CTelekineticWeapon* telekinetic_weapon)
// {
// 	if (can_shoot(telekinetic_weapon, telekinetic_poltergeist_->m_poltergeist->EnemyMan.get_enemy()))
// 	{
// 		if (u32 now = time(); now >= telekinetic_weapon->shoot_phase_end)
// 		{
// 			if (telekinetic_weapon->is_shooting)
// 			{
// 				telekinetic_weapon->get_weapon_magazined()->FireEnd();
//
// 				telekinetic_weapon->is_shooting = false;
// 				telekinetic_weapon->shoot_phase_end = now + Random.randI(100, 1000);
// 			}
// 			else
// 			{
// 				telekinetic_weapon->get_weapon_magazined()->FireStart();
//
// 				telekinetic_weapon->is_shooting = true;
// 				telekinetic_weapon->shoot_phase_end = now + Random.randI(100, 200);
// 			}
// 		}
// 	}
// }
//
// void CTelekineticWeaponController::debug_draw(CTelekineticWeapon* telekinetic_weapon, const CEntityAlive* enemy)
// {
// 	Fvector enemy_pos = enemy->Position();
// 	Fvector enemy_dir = enemy_pos - telekinetic_weapon->weapon_->Position();
// 		
// 	float distance_to_enemy = enemy_dir.magnitude();
// 	
// 	if (bool dbg_weapons_state = true)
// 	{
// 		shared_str state_text;
// 		
// 		switch (telekinetic_weapon->get_state())
// 		{
// 		case ETelekineticState::TS_RAISE:
// 			state_text = shared_str().printf("Raising %d ms", time() - telekinetic_weapon->time_raise_started);
// 			break;
// 			
// 		case ETelekineticState::TS_KEEP:
// 			state_text = shared_str().printf("Keeping %d ms",
// 			                                 telekinetic_weapon->time_keep_started + telekinetic_weapon->time_to_keep - time());
// 			break;
// 			
// 		case ETelekineticState::TS_THROW:
// 			state_text = shared_str().printf("Throw %d ms",
// 			                                 telekinetic_weapon->time_throw_started + DELAY_AFTER_THROW - time());
// 			break;
// 			
// 		case ETelekineticState::TS_NONE:
// 			state_text = "NONE";
// 			break;
// 		}
// 		
// 		shared_str main_text = shared_str().printf(
// 			"Ammo %d/%d | Distance to enemy: %.2f m | State: %s",
// 			telekinetic_weapon->weapon_->GetAmmoElapsed(), 
// 			telekinetic_weapon->weapon_->GetAmmoMagSize(),
// 			distance_to_enemy,
// 			state_text.c_str()
// 		);
// 		
// 		HUD().world_prims.append_text3d(telekinetic_weapon->weapon_->Position(), main_text);
// 	}
// 		
// 	if (bool dbg_draw_weapon_working_sphere = false)
// 	{
// 		HUD().world_prims.append_sphere(
// 			telekinetic_weapon->weapon_->Position(),
// 			telekinetic_poltergeist_->m_pmt_distance,
// 			color_rgba(0, 255, 0, 255),
// 			color_rgba(0, 255, 0, 127)
// 		);
// 	}
// }
