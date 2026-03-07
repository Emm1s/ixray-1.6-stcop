#include "StdAfx.h"
#include "../../PhysicsShellHolder.h"
#include "telekinetic_object.h"
#include "../../../xrPhysics/PhysicsShell.h"
#include "../../../xrPhysics/MathUtils.h"
#include "WeaponMagazined.h"
#include "Grenade.h"
#include "HUDManager.h"
#include "../../Level.h"
#include "poltergeist/poltergeist.h"
extern ESingleGameDifficulty g_SingleGameDifficulty; 

CTelekineticObject::CTelekineticObject(CTelekinesis* tele, CPhysicsShellHolder* owner, float s, float h, u32 ttk, bool rot)
{
	telekinesis = tele;
	CTelekineticObject::switch_state(ETelekineticState::TS_RAISE);
	object = owner;

	target_height = owner->Position().y + h;

	time_keep_started = 0;
	time_keep_updated = 0;
	time_to_keep = ttk;

	strength = s;
	time_throw_started = 0;
	rotate_object = rot;

	if (object->m_pPhysicsShell)
		object->m_pPhysicsShell->set_ApplyByGravity(false);
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
	else if (rotate_object)
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
	case ETelekineticState::TS_RAISE:
		raise_update();
		break;

	case ETelekineticState::TS_KEEP:
		keep_update();
		break;

	case ETelekineticState::TS_THROW:
		throw_update();
		break;

	case ETelekineticState::TS_NONE:
		break;
	}
}

void CTelekineticObject::switch_state(ETelekineticState new_state)
{
	u32 current_time = time();
	
	switch (new_state)
	{
	case ETelekineticState::TS_RAISE:
		time_raise_started = current_time;
		break;
	case ETelekineticState::TS_KEEP:
		time_keep_started = current_time;
		break;
	case ETelekineticState::TS_THROW:
		time_throw_started = current_time;
		break;
	case ETelekineticState::TS_NONE: break;
	}
	
	state = new_state;
}

void CTelekineticObject::raise(float step)
{
	if (!object || !object->m_pPhysicsShell || !object->m_pPhysicsShell->isActive()) 
		return;
	
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
	switch_state(ETelekineticState::TS_KEEP);
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
	if (time_throw_started + DELAY_AFTER_THROW < time())
		return true;

	return false;
}

void CTelekineticObject::perform_keep_object()
{
	if (!object || !object->m_pPhysicsShell || !object->m_pPhysicsShell->isActive()) 
		return;
	
	Fvector dir;
	float current_height = object->Position().y;
	
	if (current_height > target_height) 
		dir.set(0.f, -1.0f, 0.f);
	else if (current_height < target_height) 
		dir.set(0.f, 1.0f, 0.f);
	else
	{
		dir.set(
			Random.randF(-1.0f, 1.0f), 
			Random.randF(-1.0f, 1.0f), 
			Random.randF(-1.0f, 1.0f)
		);
		dir.normalize_safe();
	}

	dir.mul(5.0f);

	if (OnServer())
		object->m_pPhysicsShell->get_ElementByStoreOrder(0)->applyGravityAccel(dir);
	
	time_keep_updated = Device.dwTimeGlobal;
	update_hold_sound();
}

void CTelekineticObject::release()
{
	if (!object || !object->m_pPhysicsShell)
		return;
	
	Fvector random_dir;
	random_dir.random_dir();
	random_dir.normalize();
	
	object->m_pPhysicsShell->set_ApplyByGravity(true);
	
	if (OnServer())
		object->m_pPhysicsShell->applyImpulseTrace(object->Position(), random_dir,
		                                           object->m_pPhysicsShell->getMass() * 2.f);
	
	switch_state(ETelekineticState::TS_NONE);
}

void CTelekineticObject::throw_object_time(const Fvector& target, float time)
{
	switch_state(ETelekineticState::TS_THROW);

	if (!object || !object->m_pPhysicsShell || !object->m_pPhysicsShell->isActive())
		return;

	// включить гравитацию
	object->m_pPhysicsShell->set_ApplyByGravity(true);

	Fvector transference;
	transference.sub(target, object->Position());
	TransferenceToThrowVel(transference, time, object->EffectiveGravity());
	object->m_pPhysicsShell->applyImpulseTrace(object->Position(), transference, object->m_pPhysicsShell->getMass());

	if (sound_throw.handle())
		sound_throw.play_at_pos(object, object->Position());

	if (sound_hold.is_playing())
		sound_hold.stop();
}

void CTelekineticObject::throw_object(const Fvector& target, float power)
{
	switch_state(ETelekineticState::TS_THROW);

	if (!object || !object->m_pPhysicsShell || !object->m_pPhysicsShell->isActive())
		return;

	// вычислить направление
	Fvector dir;
	dir.sub(target, object->Position());
	dir.normalize();

	// включить гравитацию
	object->m_pPhysicsShell->set_ApplyByGravity(true);

	if (OnServer())
		for (u32 i = 0; i < object->m_pPhysicsShell->get_ElementsNumber(); i++)
			object->m_pPhysicsShell->get_ElementByStoreOrder(static_cast<u16>(i))->applyImpulse(
				dir, power * 20.f * object->m_pPhysicsShell->getMass() / object->m_pPhysicsShell->Elements().size());
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
	
	Fvector dir;
	dir.random_dir();
	dir.normalize();

	if (OnServer())
		object->m_pPhysicsShell->applyImpulse(dir, 2.5f * object->m_pPhysicsShell->getMass());
}

void CTelekineticObject::update_hold_sound()
{
	if (sound_hold.handle()) 
		return;
	
	if (sound_hold.is_playing())
		sound_hold.set_position(object->Position());
	else
		sound_hold.play_at_pos(object, object->Position());
}

#pragma optimize("", off)

// -------------------- WEAPON CONTROLLER --------------------

CTelekineticWeaponObject::CTelekineticWeaponObject(CPoltergeist* parent, CPhysicsShellHolder* owner, float s, float h,
                                                   u32 ttk, bool rot) :
	CTelekineticObject(parent, owner, s, h, ttk, rot),
	weapon(owner->cast_weapon_magazined()),
	parent(parent),
	shoot_phase_end(0),
	delay_before_first_shoot(0),
	last_slide_time(time()),
	delay_between_weapon_slides(1000), 
	is_shooting(false)
{
	delay_before_first_shoot = 1500;
	CTelekineticWeaponObject::switch_state(ETelekineticState::TS_RAISE);
}

void CTelekineticWeaponObject::setup_local_weapon_things()
{
	const CEntityAlive* enemy_ = parent->EnemyMan.get_enemy();
	
	if (enemy_ == nullptr)
		return;
	
	if (weapon == nullptr)
		return;
	
	if (IsGameTypeSingle() == false)
		return;
	
	backup_weapon_dispersion = weapon->getFireDispersionBase();
	//Msg("%d: backup_weapon_dispersion: %f", weapon_->ID(), backup_weapon_dispersion);
	backup_weapon_fire_mode = weapon->GetQueueSize();
	
	// WEAPON_ININITE_QUEUE (-1) = auto, 1 = single, 3 = burst
	weapon->SetQueueSize(WEAPON_ININITE_QUEUE); // чтобы пистолетам задать режим стрельбы auto
	
	if (enemy_ == g_actor)
	{
		switch (g_SingleGameDifficulty)
		{
		case egdNovice:
			weapon->setFireDispersionBase(0.20f);
			return;
		
		case egdStalker:
			weapon->setFireDispersionBase(0.17f);
			return;
		
		case egdVeteran:
			weapon->setFireDispersionBase(0.15f);
			return;
		
		case egdMaster:
			weapon->setFireDispersionBase(0.13f);
			return;
		}

		weapon->setFireDispersionBase(backup_weapon_dispersion);
		return;
	} 

	weapon->setFireDispersionBase(0.25f);
}

void CTelekineticWeaponObject::restore_global_weapon_things()
{
	if (weapon == nullptr)
		return;
	
	weapon->SetQueueSize(backup_weapon_fire_mode);
	weapon->setFireDispersionBase(backup_weapon_dispersion);
}

void CTelekineticWeaponObject::debug_draw()
{
	const CEntityAlive* enemy_ = parent->EnemyMan.get_enemy();
	if (!enemy_) return;

	Fvector enemy_pos = enemy_->Position();
	Fvector enemy_dir = enemy_pos - weapon->Position();
        	
	float distance_to_enemy = enemy_dir.magnitude();
	
	shared_str state_text;

	switch (get_state())
	{
	case ETelekineticState::TS_RAISE:
		state_text = shared_str().printf("Raising %d ms", time() - time_raise_started);
		break;

	case ETelekineticState::TS_KEEP:
		state_text = shared_str().printf("Keeping %d ms", time_keep_started + time_to_keep - time());
		break;

	case ETelekineticState::TS_THROW:
		state_text = shared_str().printf("Throw %d ms", time_throw_started + DELAY_AFTER_THROW - time());
		break;

	case ETelekineticState::TS_NONE:
		state_text = "NONE";
		break;
	}
        
	shared_str queue_type;
	
	switch (weapon->GetQueueSize())
	{
	case WEAPON_ININITE_QUEUE:
		queue_type = "AUTO";
		break;
		
	case 0:
		queue_type = "SINGLE";
		break;
		
	case 1:
		queue_type = "BURST";
		break;
	}
	
	shared_str time_to_shoot_end;
	
	switch (is_shooting)
	{
	case true:
		{
			u32 shot_interval = static_cast<u32>(weapon->getRPM() * 1000.f);
			u32 shot_time = shoot_phase_end - shoot_phase_start;
			u32 doing_shoots = shot_time / shot_interval;
			
			time_to_shoot_end = shared_str().printf("Time to shoot end: %u, doing %u shots", shoot_phase_end - time(), doing_shoots);
		}	
		break;
		
	case false:
		time_to_shoot_end = shared_str().printf("Time to start shoot: %u", shoot_phase_end - time());	
		break;
	}
	
	shared_str main_text = shared_str().printf(
		"Ammo %d/%d | Distance to enemy: %.2f m | State: %s | Weapon dispersion: %.3f | Queue type: %s | %s",
		weapon->GetAmmoElapsed(), 
		weapon->GetAmmoMagSize(),
		distance_to_enemy,
		state_text.c_str(),
		weapon->getFireDispersionBase(),
		queue_type.c_str(),
		time_to_shoot_end.c_str()
	);
        
	HUD().world_prims.append_text3d(weapon->Position(), main_text);
}

void CTelekineticWeaponObject::update_auto_aim()
{
	if (weapon->GetAmmoElapsed() <= 0)
		return;
	
	if (weapon->IsMisfire() == true)
		return;
	
	CTelekineticPoltergeist* telekinetic_poltergeist = parent->ability()->cast_to_polter_tele();
	
	float current_distance = parent->EnemyMan.get_enemy_position().distance_to_sqr(parent->Position());
	float max_tele_work_distance = telekinetic_poltergeist->m_pmt_distance * telekinetic_poltergeist->m_pmt_distance;

	if (current_distance > max_tele_work_distance)
		return;
	
	const CEntityAlive* enemy_ = parent->EnemyMan.get_enemy();
	
	if (!enemy_)
		return;
    	
	Fmatrix target_xf;
	target_xf.k.set(enemy_->Center() - weapon->get_LastFP());

	Fvector::generate_orthonormal_basis_normalized(target_xf.k,target_xf.j,target_xf.i);
	
	Fvector curr_eulers, target_eulers;
	target_xf.getXYZi(target_eulers);
	weapon->XFORM().getXYZi(curr_eulers);

	Fvector diff
	{
		angle_difference_signed(target_eulers.x, curr_eulers.x),
		angle_difference_signed(target_eulers.y, curr_eulers.y),
		angle_difference_signed(target_eulers.z, curr_eulers.z)
	};
    	
	diff.mul(weapon->m_pPhysicsShell->getMass());
	weapon->m_pPhysicsShell->setTorque(diff);
    	
	weapon->XFORM().transform_dir(diff);
	weapon->m_pPhysicsShell->set_AngularVel(diff);
}

bool CTelekineticWeaponObject::can_shoot()
{
	const CEntityAlive* enemy_ = parent->EnemyMan.get_enemy();
	
	if (enemy_ == nullptr) 
		return false;
	
	if (weapon == nullptr)
		return false;
	
	if (weapon->GetAmmoElapsed() <= 0)
		return false;
	
	if (enemy_->g_Alive() == false)
		return false;
	
	if (delay_before_first_shoot > time())
		return false;
	
	const Fvector& fire_pos = weapon->get_LastFP();
	const Fvector& fire_dir = weapon->get_LastFD();
	
	collide::rq_result rq_result;
	
	Level().ObjectSpace.RayPick(
		fire_pos,
		fire_dir,
		fire_pos.distance_to(enemy_->Center()),
		collide::rqtBoth,
		rq_result,
		weapon
	);
	
	return rq_result.O == enemy_;
}

void CTelekineticWeaponObject::shoot()
{
	if (u32 now = time(); now >= shoot_phase_end)
	{
		u32 shot_interval = static_cast<u32>(weapon->getRPM() * 1000.f);
		u32 mag_size_third = weapon->GetAmmoMagSize() / 3;
		
		mag_size_third = std::max(2u, mag_size_third);
		
		if (is_shooting)
		{
			u32 shots_skip = Random.randI(1, mag_size_third);
			u32 pause_time = time() + shot_interval * shots_skip;
			
			is_shooting = false;
			shoot_phase_end = pause_time;
			
			weapon->FireEnd();
		}
		else
		{
			u32 do_shots = Random.randI(1, mag_size_third);
			u32 end_shoot_time = time() + shot_interval * do_shots;
			
			is_shooting = true;
			shoot_phase_start = time();
			shoot_phase_end = end_shoot_time;
			
			weapon->FireStart();
		}
	}
}

void CTelekineticWeaponObject::perform_keep_object()
{
	if (last_slide_time + delay_between_weapon_slides < time())
	{
		Fvector random_lr_dir;
		Fvector object_position = object->Position();
		
		float horizontal_angle = Random.randF(0, 2.f * M_PI);
		
		random_lr_dir.x = sinf(horizontal_angle);
		random_lr_dir.y = 0.1f;
		random_lr_dir.z = cosf(horizontal_angle);
		
		random_lr_dir.normalize();
		
		object->m_pPhysicsShell->applyImpulseTrace(object_position, random_lr_dir, object->GetMass() * 5.0f);
		
		u32 max_keep_time = parent->ability()->cast_to_polter_tele()->m_pmt_time_object_keep;
		
		// Скалируем время на удержание в зависимости от max_keep_time, нижний порог не <1s и верхний не <2s.
		// Ибо если max_keep_time = 2000ms, то 2000 / 5 = 400ms, а 2000 / 2 = 1000ms, то будет слишком дико)))
		u32 min = std::max<u32>(max_keep_time / 5, 1000); 
		u32 max = std::max<u32>(max_keep_time / 2, 2000); 
		
		last_slide_time = time();
		delay_between_weapon_slides = Random.randI(static_cast<s32>(min), static_cast<s32>(max));
	}
	
	inherited::perform_keep_object();
	
	update_auto_aim();

	if (!can_shoot())
	{
		is_shooting = false;
		shoot_phase_end = time();
		
		weapon->FireEnd();
		
		return;
	}
	shoot();
}

bool CTelekineticWeaponObject::can_be_thrown()
{
	return weapon->GetAmmoElapsed() <= 0 || weapon->IsMisfire();
}

void CTelekineticWeaponObject::keep_time_elapsed()
{
	inherited::keep_time_elapsed();
	weapon->FireEnd();
}

void CTelekineticWeaponObject::release()
{
	inherited::release();
	weapon->FireEnd();
}

void CTelekineticWeaponObject::switch_state(ETelekineticState new_state)
{
	inherited::switch_state(new_state);

	if (state == ETelekineticState::TS_RAISE)
	{
		weapon->SetCanTake(false);
		setup_local_weapon_things();
	}	
	
	if (state == ETelekineticState::TS_THROW || new_state == ETelekineticState::TS_NONE)
	{
		weapon->SetCanTake(true);
		restore_global_weapon_things();
	}
}

// -------------------- GRENADE CONTROLLER --------------------

CTelekineticGrenadeObject::CTelekineticGrenadeObject(CPoltergeist* parent, CPhysicsShellHolder* owner, float s, float h, u32 ttk, bool rot) :
	CTelekineticObject(parent, owner, s, h, ttk, rot),
	grenade(owner->cast_grenade()),
	parent(parent)
{
	CTelekineticGrenadeObject::switch_state(ETelekineticState::TS_RAISE);
}

void CTelekineticGrenadeObject::switch_state(ETelekineticState new_state)
{
	inherited::switch_state(new_state);
	
	switch (state)
	{
		case ETelekineticState::TS_KEEP: 
		{
			if (grenade->destroy_time() == 0xffffffff)
			{
				grenade->State(CGrenade::eThrowStart);
				grenade->set_destroy_time(time_to_explode);
			}
		}
		break;
	}
};

bool CTelekineticGrenadeObject::can_be_thrown()  
{
	u32 now = time();
	u32 explode_global_time = grenade->destroy_time();  
	
	u32 activation_time = explode_global_time - time_to_explode;  
	u32 elapsed_since_activation = now - activation_time;

	return elapsed_since_activation > throw_threshold;  
}