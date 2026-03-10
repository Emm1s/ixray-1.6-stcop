#pragma once

enum class ETelekineticState : u8
{
    TS_NONE,
    TS_RAISE,
    TS_KEEP,
    TS_THROW,
};

enum ETelekineticTimings : u16
{
    KEEP_IMPULSE_UPDATE = 200,
    DELAY_AFTER_THROW = 3000,
    RAISE_MAX_TIME = 5000
};

class CGameObject;
class CPhysicsShellHolder;
class STelekineticObject;
class CPHUpdateObject;
class CTelekinesis;
class CWeaponMagazined;
class CGrenade;
class CPoltergeist;
struct CTeleWhirlwindObject;
struct STelekineticWeaponObject;

struct STelekineticObject
{
    ETelekineticState state;

    CPhysicsShellHolder* object;
    CTelekinesis* telekinesis = nullptr;
    ref_sound sound_hold;
    ref_sound sound_throw;

    float target_height;
    float strength;

    // Objects
    u32 time_raise_started;
    u32 time_keep_started;
    u32 time_keep_updated;
    u32 time_to_keep;
    u32 time_throw_started;
	
    bool rotate_object;

    STelekineticObject(CTelekinesis* tele, CPhysicsShellHolder* owner, float s, float h, u32 ttk, bool rot);
    virtual ~STelekineticObject() {};

    void set_sound(const ref_sound& snd_hold, const ref_sound& snd_throw);

    virtual void raise(float step);
    virtual void raise_update();

    void prepare_keep();
    virtual void perform_keep_object();
    virtual void keep_update();
    virtual void release();
    virtual void throw_object(const Fvector& target, float power);
    void throw_object_time(const Fvector& target, float time);
    virtual void throw_update();
    virtual void update_state();
    ICF bool is_released() const { return state == ETelekineticState::TS_NONE; }
    virtual void switch_state(ETelekineticState new_state);
    ICF ETelekineticState get_state() const { return state; }
    ICF CPhysicsShellHolder* get_object() const { return object; }

    bool check_height() const;
    bool check_raise_time_out() const;

    bool keep_time_elapsed() const;
    bool throw_time_elapsed() const;
    
    void enable() const;

    ICF bool operator==(const CPhysicsShellHolder* obj) const
    {
        return object == obj;
    }

    void rotate() const;
    void update_hold_sound();
	
    virtual bool can_be_thrown() { return true; }
	virtual bool can_be_picked_up() { return true; }

    virtual STelekineticObject* cast_telekinetic_object() { return this; }
    virtual STelekineticWeaponObject* cast_telekinetic_weapon_object() { return nullptr; }
    virtual CTeleWhirlwindObject* cast_whirlwind_object() { return nullptr; }
};

struct STelekineticWeaponObject : STelekineticObject
{
	using inherited = STelekineticObject;

	CWeaponMagazined* weapon;
	CTelekinesis* parent;

	u32 shoot_phase_start;
	u32 shoot_phase_end;

	u32 delay_before_first_shoot;
	
	u32 last_slide_time;
	u32 delay_between_weapon_slides;
	
	float backup_weapon_dispersion = 9999.f;
	s8 backup_weapon_fire_mode = s8(-1);

	bool is_shooting;
	
	STelekineticWeaponObject(CTelekinesis* telekinesis, CPhysicsShellHolder* owner, float s, float h, u32 ttk, bool rot);

	void setup_local_weapon_things();
	void restore_global_weapon_things();

	void debug_draw();
	void update_auto_aim();
    bool can_shoot();
	void shoot();
	
	bool is_enemy_tracing() const;
	
    virtual void perform_keep_object();

    virtual bool can_be_thrown();
    virtual void keep_time_elapsed();
    virtual void release();
    virtual void switch_state(ETelekineticState new_state);

    virtual STelekineticWeaponObject* cast_telekinetic_weapon_object() { return this; }
};

struct STelekineticGrenadeObject : STelekineticObject
{
	using inherited = STelekineticObject;
	
    CTelekinesis* parent;
	CGrenade* grenade;
	
	u32 grenade_initial_time = UINT32_MAX;
	u32 throw_threshold = 700;
	u32 time_to_explode = 2000;
	
    STelekineticGrenadeObject(CTelekinesis* telekinesis, CPhysicsShellHolder* owner, float s, float h, u32 ttk, bool rot);
	
	void debug_draw();

	virtual void perform_keep_object();
	virtual void switch_state(ETelekineticState new_state);
	virtual bool can_be_thrown();
	virtual bool can_be_picked_up();
};