#include "stdafx.h"

#include "../xrSound/ai_sounds.h"
#include "PolterInterface.h"
#include "PolterTele.h"
#include "PhysicsShellHolder.h"
#include "level.h"
#include "actor.h"
#include "ActorCondition.h"
#include "Inventory.h"
#include "../xrPhysics/icolisiondamageinfo.h"
#include "ai/monsters/telekinesis.h"
#include "ai/monsters/BaseMonster/base_monster.h"

CPolterTele::CPolterTele(IPolterInterface* polter) : inherited (polter),m_pmt_object_collision_damage(0.5f)
{
}

CPolterTele::~CPolterTele()
{
}

void CPolterTele::load(const char* section)
{
	inherited::load(section);

	m_pmt_radius						= pSettings->read_if_exists<float>(section,	"Tele_Find_Radius",					10.f);
	m_pmt_object_min_mass				= pSettings->read_if_exists<float>(section,	"Tele_Object_Min_Mass",				40.f);
	m_pmt_object_max_mass				= pSettings->read_if_exists<float>(section,	"Tele_Object_Max_Mass",				500.f);
	m_pmt_object_count					= pSettings->read_if_exists<u32>(section,	"Tele_Object_Count",				10);
	m_pmt_time_to_hold					= pSettings->read_if_exists<u32>(section,	"Tele_Hold_Time",					3000);
	m_pmt_time_to_wait					= pSettings->read_if_exists<u32>(section,	"Tele_Wait_Time",					3000);
	m_pmt_time_to_wait_in_objects		= pSettings->read_if_exists<u32>(section,	"Tele_Delay_Between_Objects_Time",	500);
	m_pmt_distance						= pSettings->read_if_exists<float>(section,	"Tele_Distance",					50.f);
	m_pmt_object_height					= pSettings->read_if_exists<float>(section,	"Tele_Object_Height",				10.f);
	m_pmt_time_object_keep				= pSettings->read_if_exists<u32>(section,	"Tele_Time_Object_Keep",			10000);
	m_pmt_raise_speed					= pSettings->read_if_exists<float>(section,	"Tele_Raise_Speed",					3.f);
	m_pmt_raise_time_to_wait_in_objects	= pSettings->read_if_exists<u32>(section,	"Tele_Delay_Between_Objects_Raise_Time", 500);
	m_pmt_fly_velocity					= pSettings->read_if_exists<float>(section, "Tele_Fly_Velocity",				30.f);
	m_pmt_object_collision_damage		= pSettings->read_if_exists<float>(section, "Tele_Collision_Damage",			0.5f);
	::Sound->create						(m_sound_tele_hold, pSettings->r_string(section,"sound_tele_hold"),	st_Effect,SOUND_TYPE_WORLD);
	::Sound->create						(m_sound_tele_throw, pSettings->r_string(section,"sound_tele_throw"),st_Effect,SOUND_TYPE_WORLD);

	m_state								= 	eWait;
	m_time								= 	0;
	m_time_next							= 	0;
}

void CPolterTele::update_frame()
{
	inherited::update_frame();
}

void CPolterTele::update_schedule()
{
	inherited::update_schedule();
	
	CMonsterEnemyManager& enemy = m_object->GetMonster()->EnemyMan;
	
	const Fvector enemy_pos = enemy.get_enemy_position();
	const float distance_to_enemy = enemy_pos.distance_to(m_object->GetCurrentPosition());

	// TODO: Where was a detection level! "if ( m_object->GetCurrentDetectionLevel() < m_object->GetDetectionSuccessLevel() ) return"
	if (distance_to_enemy > m_pmt_distance || m_object->GetActorIgnore())
	{
		return;
	}

	switch (m_state)
	{
	case eStartRaiseObjects:
		{
			if (m_time + m_time_next < time()) {
				if (!tele_raise_objects())
				{
					m_state	= eRaisingObjects;
				}
				
				m_time = time();
				m_time_next = m_pmt_raise_time_to_wait_in_objects / 2 + Random.randI(m_pmt_raise_time_to_wait_in_objects / 2);
			}
	
			if (m_state == eStartRaiseObjects) {
				if (m_object->GetTelekinesis()->get_objects_count() >= m_pmt_object_count) {
					m_state		= eRaisingObjects;
					m_time		= time();
				}
			}
	
			break;
		}
	case eRaisingObjects:
		{
			if (m_time + m_pmt_time_to_hold > time())
			{
				break;
			}
		
			m_time = time();
			m_time_next = 0;
			m_state = eFireObjects;
		}
	case eFireObjects:
		{
			if (m_time + m_time_next < time()) {
				tele_fire_objects();
			
				m_time = time();
				m_time_next	= m_pmt_time_to_wait_in_objects / 2 + Random.randI(m_pmt_time_to_wait_in_objects / 2);
			}
		
			if (m_object->GetTelekinesis()->get_objects_count() == 0) {
				m_state = eWait;
				m_time = time();
			}
			break;
		}
	case eWait:
		{
			if (m_time + m_pmt_time_to_wait < time()) {
				m_time_next = 0;
				m_state = eStartRaiseObjects;
			}
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// ����� ���������� �������� ��� ����������
//////////////////////////////////////////////////////////////////////////
class best_object_predicate {
	Fvector enemy_pos;
	Fvector monster_pos;
public:
	best_object_predicate(const Fvector &m_pos, const Fvector &pos) {
		monster_pos = m_pos;
		enemy_pos = pos;
	}

	bool operator()	 (const CGameObject *tpObject1, const CGameObject *tpObject2) const
	{

		float dist1 = monster_pos.distance_to(tpObject1->Position());
		float dist2 = enemy_pos.distance_to(tpObject2->Position());
		float dist3 = enemy_pos.distance_to(monster_pos);

		return dist1 < dist3 && dist2 > dist3;
	}
};

class best_object_predicate2 {
	Fvector enemy_pos;
	Fvector monster_pos;
public:
	using CObject_ptr = CObject*;

	best_object_predicate2(const Fvector &m_pos, const Fvector &pos) {
		monster_pos = m_pos;
		enemy_pos	= pos;
	}

	bool operator()	 (const CObject_ptr &tpObject1, const CObject_ptr &tpObject2) const
	{
		float dist1 = enemy_pos.distance_to(tpObject1->Position());
		float dist2 = enemy_pos.distance_to(tpObject2->Position());

		return dist1 < dist2;		
	}
};

//////////////////////////////////////////////////////////////////////////

bool CPolterTele::trace_object(CObject *obj, const Fvector &target)
{
	Fvector trace_from;
	obj->Center(trace_from);

	Fvector dir;
	dir.sub(target, trace_from);

	float range = dir.magnitude();
	if (range < EPS)
	{
		return false;
	}

	dir.normalize();

	collide::rq_result l_rq;
	if (Level().ObjectSpace.RayPick(trace_from, dir, range, collide::rqtBoth, l_rq, obj)
		&& l_rq.O == m_object->GetMonster()->EnemyMan.get_enemy())
	{
		return true;
	}
	
	return false;
}

void CPolterTele::tele_find_objects(xr_vector<CObject*> &objects, const Fvector &pos) 
{
	objects.clear();
	g_SpatialSpace->q_sphere(m_nearest,0,ESPATIAL_TYPE::COLLIDEABLE, pos, m_pmt_radius);


	for (auto& SS : m_nearest)
	{
		ISpatial* S = SS.get();
		if (!S)
		{
			continue;
		}
		CObject* pObject = S->dcast_CObject();
		if (!pObject || pObject->getDestroy())
		{
			continue;
		}
		
		CPhysicsShellHolder* obj = pObject->cast_physics_shell_holder();
		CMonsterEnemyManager& enemy = m_object->GetMonster()->EnemyMan;
		
		if (!obj ||
			!obj->PPhysicsShell() ||
			!obj->PPhysicsShell()->isActive() ||
			obj->cast_creature() ||
			(obj->spawn_ini() && obj->spawn_ini()->section_exist("ph_heavy")) ||
			obj->m_pPhysicsShell->getMass() < m_pmt_object_min_mass ||
			obj->m_pPhysicsShell->getMass() > m_pmt_object_max_mass ||
			obj == m_object->GetMonster() ||
			m_object->GetTelekinesis()->is_active_object(obj) ||
			!obj->m_pPhysicsShell->get_ApplyByGravity() || !enemy.get_enemy())
		{
			continue;
		}


		Fvector center;
		enemy.get_enemy()->Center(center);

		if (trace_object(obj, center) ||
			trace_object(obj, get_head_position(enemy.get_enemy())))
		{
			objects.push_back(obj);
		}
	}
}

bool CPolterTele::tele_raise_objects()
{
	// find objects near actor
	xr_vector<CObject*>& tele_objects = m_object->GetTeleObjects();
	
	CMonsterEnemyManager& enemy = m_object->GetMonster()->EnemyMan;
	
	// получить список объектов вокруг врага
	tele_find_objects(tele_objects, enemy.get_enemy_position());

	// получить список объектов вокруг монстра
	tele_find_objects(tele_objects, m_object->GetCurrentPosition());

	// получить список объектов между монстром и врагом
	float dist = enemy.get_enemy_position().distance_to(m_object->GetCurrentPosition());
	
	Fvector dir;
	dir.sub(enemy.get_enemy_position(), m_object->GetCurrentPosition());
	dir.normalize();

	Fvector pos;
	pos.mad(m_object->GetCurrentPosition(), dir, dist / 2.f);
	tele_find_objects(tele_objects, pos);	

	// сортировать и оставить только необходимое количество объектов
	std::ranges::sort(tele_objects,best_object_predicate2(m_object->GetCurrentPosition(), Actor()->Position()));

	// оставить уникальные объекты
	tele_objects.erase(std::ranges::unique(tele_objects).begin(),tele_objects.end());
	
	if (!tele_objects.empty())
	{
		CPhysicsShellHolder* obj = tele_objects[0] != nullptr ? tele_objects[0]->cast_physics_shell_holder() : nullptr;
		bool rotate = false;

		CTelekineticObject* tele_obj = m_object->GetTelekinesis()->activate(
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
struct SCollisionHitCallback:
	public ICollisionHitCallback

{																																					;
	CPhysicsShellHolder *m_object;
	float m_pmt_object_collision_damage;
	
	SCollisionHitCallback( CPhysicsShellHolder& object, float pmt_object_collision_damage ):
	m_object(&object), m_pmt_object_collision_damage( pmt_object_collision_damage )
	{
	}
	
	void call( IPhysicsShellHolder* obj, float min_cs, float max_cs, float &cs, float &hl, ICollisionDamageInfo* di ) override
	{
		if (cs > min_cs * 0.5f)
		{
			hl = m_pmt_object_collision_damage;
		}
		
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

void CPolterTele::tele_fire_objects()
{
	const CEntityAlive* enemy = m_object->GetMonster()->EnemyMan.get_enemy();
	if (enemy == nullptr)
	{
		return;
	}

	for (u32 i = 0; i < m_object->GetTelekinesis()->get_objects_total_count(); i++)
	{
		CTelekineticObject tele_object = m_object->GetTelekinesis()->get_object_by_index(i);
		
		if (tele_object.get_state() == TS_Raise || tele_object.get_state() == TS_Keep)
		{
			Fvector enemy_pos = get_head_position(fast_dynamic_cast<CObject*>((CEntityAlive*)enemy));
			CPhysicsShellHolder* hobj = tele_object.get_object();
			CWeaponMagazined* weapon_magazined = tele_object.get_object()->cast_weapon_magazined();

			VERIFY(hobj);
			hobj->set_collision_hit_callback(new SCollisionHitCallback(*hobj, m_pmt_object_collision_damage));

			// Бросамемся обычными объектами.
			m_object->GetTelekinesis()->fire_t(
				tele_object.get_object(),
				enemy_pos,
				tele_object.get_object()->Position().distance_to(enemy_pos) / m_pmt_fly_velocity
			);
			break;
		}
	}
}

