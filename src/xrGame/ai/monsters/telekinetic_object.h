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
class CTelekineticObject;
class CPHUpdateObject;
class CTelekinesis;
class TelekineticWeaponController;
class CWeaponMagazined;
class CGrenade;

/**
 * Интерфейс для описания поведения телекинетического объекта.
 * Зачем нужно: инкапсулирует новую логику для поведения объекта, захваченного телекинезом.
 * 
 * Как использовать: 
 * 1. Создать отдельный класс контроллера, который будет описывать поведение объекта и инкапсулировать в нём логику, а
 * в конструкторе передать регистрируемый объект и унаследоваться.
 * 2. В месте инициализации объекта прокастить объект и создать экземпляр behavior для CTelekineticObject,
 * прокинув ссылкой объект.
 * 3. Обязательно обновлять внешние зависимости (TelekineticObject & CEntnityAlive (наш враг)) в update.
 * 5. Логика в контроллере регулируется через вызываемые события.
 */
class ITelekineticObjectBehavior
{
protected:
	const CEntityAlive* enemy_;
	CTelekineticObject* telekinetic_object_;
	
public:
	ITelekineticObjectBehavior() = default;
	virtual ~ITelekineticObjectBehavior() = default;

	virtual void update(CTelekineticObject* owner, const CEntityAlive* enemy)
	{
		enemy_ = enemy;
		telekinetic_object_ = owner;
	}
	
	// Вызывается перед тем, как объект начнёт или продолжит удерживаться.
	virtual void on_perform_keep_object()
	{
	}

	// Вызывается, когда вышло время удержание объекта.
	// (для CTelekineticPoltergeist обновление вынесено из shedule -> UpdateCL)
	virtual void on_keep_elapsed()
	{
	}

	// Вызывается перед тем, как объект будет брошен с учётом гравитации и высоты.
	virtual void on_throw_object_time()
	{
	}
	
	// Вызывается перед тем, как объект будет брошен без учёта гравитации и высоты.
	virtual void on_throw_object()
	{
	}

	// Вызывается, когда вышло время таймаута для брошенного объекта.
	virtual void on_throw_elapsed()
	{
	}
	
	// Вызывается перед тем, как отпустить объект.
	virtual void on_release()
	{
	}
	
	// Вызывается перед тем, как объект начнёт подниматься на шаг физики, либо продолжит подниматься.
	virtual void on_raise()
	{
	}
	
	// Вызывается после того, как обновилось состояние телекинетического предмета.
	// (инкапсулированный telekinetic_object_)
	virtual void on_state_switch(ETelekineticState prev_state, ETelekineticState new_state)
	{
	}
	
	// Вызвыается кажыдый раз, когда предмет бросается. 
	// Здесь описывается логика, при каких обстоятельствах предмет бросится.
	virtual bool can_be_thrown()
	{
		return true;
	}
};

class CTelekineticObject
{
    ETelekineticState state;

public:
    CPhysicsShellHolder* object;
    CTelekinesis* telekinesis;
    ref_sound sound_hold;
    ref_sound sound_throw;
    ITelekineticObjectBehavior* behavior;

    float target_height;
    float strength;

    // Objects
    u32 time_raise_started;
    u32 time_keep_started;
    u32 time_keep_updated;
    u32 time_to_keep;
    u32 time_throw_started;
	
    bool rotate_object;

    CTelekineticObject();
    virtual ~CTelekineticObject();

    virtual bool init(CTelekinesis* tele, CPhysicsShellHolder* obj, float s, float h, u32 ttk, bool rot = true);
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
    virtual bool can_activate(CPhysicsShellHolder* obj);
    bool is_released() const { return state == ETelekineticState::TS_NONE; }
    virtual void switch_state(ETelekineticState new_state);
    ETelekineticState get_state() const { return state; }
    CPhysicsShellHolder* get_object() const { return object; }

    bool check_height() const;
    bool check_raise_time_out() const;

    bool keep_time_elapsed() const;
    bool throw_time_elapsed() const;
    
    void enable() const;

    bool operator==(const CPhysicsShellHolder* obj) const
    {
        return object == obj;
    }

    void rotate() const;

private:
    void update_hold_sound();
};

class TelekineticWeaponController : public ITelekineticObjectBehavior
{
	using inherited = ITelekineticObjectBehavior;
	
    CWeaponMagazined* weapon_;
	const CEntityAlive* prev_enemy;
	
	u32 shoot_phase_end;
	
	s8 backup_weapon_fire_mode = FLT_MAX;
	float backup_weapon_dispersion = FLT_MAX;
	
	bool is_shooting;
	
public:
	explicit TelekineticWeaponController(CWeaponMagazined* weapon);

	void update(CTelekineticObject* owner, const CEntityAlive* enemy) override;
	void setup_local_weapon_things();
	void restore_global_weapon_things() const;
	void on_perform_keep_object() override;
	void debug_draw() const;
	void update_auto_aim() const;
    bool can_shoot() const;
	void on_keep_elapsed() override;
	void on_release() override;
	void shoot();
	bool can_be_thrown() override;
	void on_state_switch(ETelekineticState prev_state, ETelekineticState new_state) override;
};

// class TelekineticGrenadeController : public ITelekineticObjectBehavior
// {
// 	using inherited = ITelekineticObjectBehavior;
// 	
// 	CGrenade* grenade_;
// 	u32 grenade_initial_time = 0xffffffff;
// 	
// public:
// 	explicit TelekineticGrenadeController(CGrenade* grenade);
//
// 	void update(CTelekineticObject* owner, const CEntityAlive* enemy) override;
// 	void on_throw_object_time() override;
// 	void on_perform_keep_object() override;
// 	bool can_be_thrown() override;
// };