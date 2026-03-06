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
	state = ETelekineticState::TS_RAISE;
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

	ETelekineticState prev_state = state;
	
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

// --- WEAPON CONTROLLER ---
CTelekineticWeaponObject::CTelekineticWeaponObject(CPoltergeist* parent, CPhysicsShellHolder* owner, float s, float h, u32 ttk, bool rot) :
	CTelekineticObject(parent, owner, s, h, ttk, rot),
	weapon_(owner->cast_weapon_magazined()),
	shoot_phase_end(time() + Random.randI(500, 1000)),
	is_shooting(false),
	Parent(parent)
{
	switch_state(ETelekineticState::TS_RAISE);
}

void CTelekineticWeaponObject::setup_local_weapon_things()
{
	const CEntityAlive* enemy_ = Parent->EnemyMan.get_enemy();
	if (enemy_ == nullptr)
		return;
	
	if (weapon_ == nullptr)
		return;
	
	if (IsGameTypeSingle() == false)
		return;
	
	backup_weapon_dispersion = weapon_->getFireDispersionBase();
	//Msg("%d: backup_weapon_dispersion: %f", weapon_->ID(), backup_weapon_dispersion);
	backup_weapon_fire_mode = weapon_->GetQueueSize();
	
	// WEAPON_ININITE_QUEUE (-1) = auto, 1 = single, 3 = burst
	weapon_->SetQueueSize(WEAPON_ININITE_QUEUE); // чтобы пистолетам задать режим стрельбы auto
	
	if (enemy_ == g_actor)
	{
		switch (g_SingleGameDifficulty)
		{
		case egdNovice:
			weapon_->setFireDispersionBase(0.20f);
			return;
		
		case egdStalker:
			weapon_->setFireDispersionBase(0.17f);
			return;
		
		case egdVeteran:
			weapon_->setFireDispersionBase(0.15f);
			return;
		
		case egdMaster:
			weapon_->setFireDispersionBase(0.13f);
			return;
		}

		weapon_->setFireDispersionBase(backup_weapon_dispersion);
		return;
	} 

	weapon_->setFireDispersionBase(0.25f);
}

void CTelekineticWeaponObject::restore_global_weapon_things()
{
	if (weapon_ == nullptr)
		return;
	
	weapon_->SetQueueSize(backup_weapon_fire_mode);
	weapon_->setFireDispersionBase(backup_weapon_dispersion);
}

void CTelekineticWeaponObject::debug_draw()
{
	const CEntityAlive* enemy_ = Parent->EnemyMan.get_enemy();
	if (!enemy_) return;

	Fvector enemy_pos = enemy_->Position();
	Fvector enemy_dir = enemy_pos - weapon_->Position();
        	
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
	
	switch (weapon_->GetQueueSize())
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
		time_to_shoot_end = shared_str().printf("Time to shoot end: %u", shoot_phase_end - time());	
		break;
		
	case false:
		time_to_shoot_end = shared_str().printf("Time to start shoot: %u", shoot_phase_end - time());	
		break;
	}
	
	shared_str main_text = shared_str().printf(
		"Ammo %d/%d | Distance to enemy: %.2f m | State: %s | Weapon dispersion: %.3f | Queue type: %s | %s",
		weapon_->GetAmmoElapsed(), 
		weapon_->GetAmmoMagSize(),
		distance_to_enemy,
		state_text.c_str(),
		weapon_->getFireDispersionBase(),
		queue_type.c_str(),
		time_to_shoot_end.c_str()
	);
        
	HUD().world_prims.append_text3d(weapon_->Position(), main_text);
}

void CTelekineticWeaponObject::update_auto_aim()
{
	if (weapon_->GetAmmoElapsed() <= 0)
		return;
	
	if (weapon_->IsMisfire() == true)
		return;
	
	const CEntityAlive* enemy_ = Parent->EnemyMan.get_enemy();
	if (!enemy_) return;
    	
	Fmatrix target_xf;
	target_xf.k.set(enemy_->Center() - weapon_->get_LastFP());

	Fvector::generate_orthonormal_basis_normalized(target_xf.k,target_xf.j,target_xf.i);
	
	Fvector curr_eulers, target_eulers;
	target_xf.getXYZi(target_eulers);
	weapon_->XFORM().getXYZi(curr_eulers);

	Fvector diff
	{
		angle_difference_signed(target_eulers.x, curr_eulers.x),
		angle_difference_signed(target_eulers.y, curr_eulers.y),
		angle_difference_signed(target_eulers.z, curr_eulers.z)
	};
    	
	diff.mul(weapon_->m_pPhysicsShell->getMass());
	weapon_->m_pPhysicsShell->setTorque(diff);
    	
	weapon_->XFORM().transform_dir(diff);
	weapon_->m_pPhysicsShell->set_AngularVel(diff);
}

bool CTelekineticWeaponObject::can_shoot()
{
	const CEntityAlive* enemy_ = Parent->EnemyMan.get_enemy();
	if (!enemy_) return false;
	
	if (weapon_ == nullptr)
		return false;
	
	if (weapon_->GetAmmoElapsed() <= 0)
		return false;
	
	if (enemy_->g_Alive() == false)
		return false;
	
	const Fvector& fire_pos = weapon_->get_LastFP();
	collide::rq_result rq_result;
	Level().ObjectSpace.RayPick(fire_pos, weapon_->get_LastFD(), fire_pos.distance_to(enemy_->Center()), collide::rqtBoth, rq_result, weapon_);
	return rq_result.O == enemy_;
}

void CTelekineticWeaponObject::shoot()
{
	if (u32 now = time(); now >= shoot_phase_end)
	{
		u32 shot_interval = static_cast<u32>(weapon_->getRPM() * 1000.f);
		u32 mag_size_third = weapon_->GetAmmoMagSize() / 3;
		
		clamp<u32>(mag_size_third, 2, weapon_->GetAmmoMagSize());
		
		if (is_shooting)
		{
			u32 shots_skip = Random.randI(1, mag_size_third);
			u32 pause_time = time() + shot_interval * shots_skip;
			
			is_shooting = false;
			shoot_phase_end = pause_time;
			
			weapon_->FireEnd();
		}
		else
		{
			u32 do_shots = Random.randI(1, mag_size_third);
			u32 end_shoot_time = time() + shot_interval * do_shots;
			
			is_shooting = true;
			shoot_phase_end = end_shoot_time;
			
			weapon_->FireStart();
		}
	}
}

bool CTelekineticWeaponObject::can_be_thrown()
{
	return weapon_->GetAmmoElapsed() <= 0 || weapon_->IsMisfire();
}

void CTelekineticWeaponObject::perform_keep_object()
{
	inherited::perform_keep_object();

	update_auto_aim();

	if (can_shoot() == false)
	{
		weapon_->FireEnd();
		return;
	}

	shoot();
}

void CTelekineticWeaponObject::keep_time_elapsed()
{
	inherited::keep_time_elapsed();
	weapon_->FireEnd();
}

void CTelekineticWeaponObject::release()
{
	inherited::release();
	weapon_->FireEnd();
}

void CTelekineticWeaponObject::switch_state(ETelekineticState new_state)
{
	inherited::switch_state(new_state);

	if (new_state == ETelekineticState::TS_RAISE)
	{
		weapon_->SetCanTake(false);
		setup_local_weapon_things();
	}	
	
	if (new_state == ETelekineticState::TS_THROW || new_state == ETelekineticState::TS_NONE)
	{
		weapon_->SetCanTake(true);
		restore_global_weapon_things();
	}
}

CTelekineticGrenadeObject::CTelekineticGrenadeObject(CPoltergeist* parent, CPhysicsShellHolder* owner, float s, float h, u32 ttk, bool rot) :
	CTelekineticObject(parent, owner, s, h, ttk, rot),
	grenade_(owner->cast_grenade()),
	Parent(parent)
{
	switch_state(ETelekineticState::TS_RAISE);
}

void CTelekineticGrenadeObject::perform_keep_object()
{
	inherited::perform_keep_object();
};

void CTelekineticGrenadeObject::keep_time_elapsed()
{
	inherited::keep_time_elapsed();
};
void CTelekineticGrenadeObject::release()
{
	inherited::release();
};
void CTelekineticGrenadeObject::switch_state(ETelekineticState new_state)
{
	inherited::switch_state(new_state);

	if (new_state == ETelekineticState::TS_KEEP && grenade_->destroy_time() == 0xffffffff)
	{
		grenade_->PlaySound("sndCheckout", grenade_->Position());
		grenade_checkout_time = Device.dwTimeGlobal + 3000;

		grenade_->SetState(CGrenade::eThrowStart);
		grenade_->set_destroy_time(3000);
	}
};

bool CTelekineticGrenadeObject::can_be_thrown()
{
	return Device.dwTimeGlobal > grenade_checkout_time && grenade_->destroy_time() != 0xffffffff;
}