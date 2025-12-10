#pragma once
#include "../abilities/poltergeist/PolterInterface.h"
#include "../basemonster/base_monster.h"
#include "../telekinesis.h"
#include "../energy_holder.h"
#include "../../../../xrScripts/script_export_space.h"
#include "../abilities/poltergeist/PolterTele.h"
#include "../abilities/poltergeist/PolterFlame.h"
#include "../abilities/poltergeist/PolterChem.h"

class CPhysicsShellHolder;
class CStateManagerPoltergeist;
class CPoltergeisMovementManager;
class CPolterSpecialAbility;

//////////////////////////////////////////////////////////////////////////


class CPoltergeist final :	public CBaseMonster ,
						public CTelekinesis,
						public CEnergyHolder,
						public IPolterInterface
{
	
	typedef		CBaseMonster	inherited;
	typedef		CEnergyHolder	Energy;

	friend class CPoltergeisMovementManager;
	friend class CPolterTele;

	float					m_height;
	bool					m_disable_hide;

	SMotionVel				invisible_vel;


	xr_unique_ptr<CPolterFlame> m_flame;
	xr_unique_ptr<CPolterTele> m_tele;
	xr_unique_ptr<CPolterChem> m_chem;
	xr_vector<CObject*>		tele_objects;

	bool					m_actor_ignore;

	TTime					m_last_detection_time;
	Fvector					m_last_actor_pos;
	char const*				m_detection_pp_effector_name;
	u32						m_detection_pp_type_index;
	float					m_detection_near_range_factor;
	float					m_detection_far_range_factor;
	float					m_detection_far_range;
	float					m_detection_speed_factor;
	float					m_detection_loose_speed;
	float					m_current_detection_level;
	float					m_detection_success_level;
	float					m_detection_max_level;

	bool					m_enable_corpse_on_death;

public:
	bool					m_detect_without_sight;
	
	virtual CBaseMonster* GetMonster() override { return this; }
	virtual CTelekinesis* GetTelekinesis() override { return this; }
	virtual const Fvector& GetCurrentPosition() override { return m_current_position; }
	virtual float GetTargetHeight() override { return target_height; }
	virtual float GetCurrentDetectionLevel() override { return get_current_detection_level(); }
	virtual float GetDetectionSuccessLevel() override { return get_detection_success_level(); }
	virtual bool GetActorIgnore() override { return get_actor_ignore(); }
	virtual xr_vector<CObject*>& GetTeleObjects() override { return tele_objects; }

public:
					CPoltergeist		();
	virtual			~CPoltergeist		();	

	virtual void	Load				(const char* section);
	virtual void	reload				(const char* section);
	virtual void	reinit				();

	virtual bool	net_Spawn			(CSE_Abstract* DC);
	virtual void	net_Destroy			();
	virtual void	net_Relcase			(CObject *O);

	virtual void	UpdateCL			();
	virtual	void	shedule_Update		(u32 dt);
	virtual bool	AlwaysTheCrow();

			void	set_actor_ignore	(bool const actor_ignore) { m_actor_ignore = actor_ignore; }
			bool	get_actor_ignore	() const { return m_actor_ignore; }

	virtual void	Die					(CObject* who);

	virtual CMovementManager *create_movement_manager();
	
	virtual void	ForceFinalAnimation	();

	virtual	void	on_activate			();
	virtual	void	on_deactivate		();
	virtual	void	Hit					(SHit* pHDS);
	virtual	char*	get_monster_class_name () { return (char*)"poltergeist"; }

			bool	detected_enemy		();
			float	get_fly_around_distance	() const { return m_fly_around_distance; }
			float	get_fly_around_change_direction_time() const { return m_fly_around_change_direction_time; }

	virtual	void	renderable_Render	();

	IC CPolterSpecialAbility* ability() const
	{
		return (m_flame ? (CPolterSpecialAbility*)(m_flame.get()) : (CPolterSpecialAbility*)(m_tele.get()));
	} // TODO: replace with direct access
	
	

	IC		bool	is_hidden			() {return state_invisible;}

	
	// Poltergeist ability
			void	PhysicalImpulse		(const Fvector &position);
			void	StrangeSounds		(const Fvector &position);
			
			ref_sound m_strange_sound;
	
	// Movement
			Fvector m_current_position;		// Позиция на ноде

	// Dynamic Height
			u32		time_height_updated;
			float	target_height;

			void	UpdateHeight			();

	// Invisibility 

			void	EnableHide				(){m_disable_hide = false;}
			void	DisableHide				(){m_disable_hide = true;}

public:
	virtual bool	run_home_point_when_enemy_inaccessible () const { return false; }
	
private:
			void	Hide					();
			void	Show					();

			float	m_height_change_velocity;
			u32		m_height_change_min_time;
			u32		m_height_change_max_time;
			float	m_height_min;
			float	m_height_max;
			
			float	m_fly_around_level;
			float	m_fly_around_distance;
			float	m_fly_around_change_direction_time;

			float	get_current_detection_level			() const { return m_current_detection_level; }
			bool	check_work_condition				() const;
			void    remove_pp_effector					();
			void	update_detection					();

			float	get_detection_near_range_factor		();
			float	get_detection_far_range_factor		();
			float	get_detection_loose_speed			();
			float	get_detection_far_range				();
			float	get_detection_speed_factor			();
			float	get_detection_success_level			();
			float    get_post_process_factor	() const;

public:
#ifdef DEBUG
			virtual CBaseMonster::SDebugInfo show_debug_info();
#endif
	

			friend class CPolterFlame;
	DECLARE_SCRIPT_REGISTER_FUNCTION
};