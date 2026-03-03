#pragma once
#include "WeaponMagazined.h"

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
 * 2. В месте инициализации объекта CTelekineticObject::init прокастить объект и создать экземпляр созданного класса из 
 * первого пункта.
 * 3. Обязательно обновлять внешние зависимости (TelekineticObject & CEntnityAlive (наш враг()) в update.
 * 5. Поведение сущности можно регуллировать через виртуальные событийные методы.
 * 
 * Надеюсь, понятно описал.
 */
class ITelekineticBehavior
{
protected:
	const CEntityAlive* enemy_;
	CTelekineticObject* telekinetic_object_;
	
public:
	ITelekineticBehavior() = default;
	virtual ~ITelekineticBehavior() = default;

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
	
	// Вызывается перед тем, как поднимется объект за тик кадра.
	virtual void on_raise()
	{
	}
	
	// Вызывается после того, как обновилось состояние телекинетического предмета.
	// (инкапсулированный telekinetic_object_)
	virtual void on_state_switch(ETelekineticState prev_state, ETelekineticState new_state)
	{
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
    ITelekineticBehavior* behavior;

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

class TelekineticWeaponController : public ITelekineticBehavior
{
    CWeaponMagazined* weapon_;
	
	u32 shoot_phase_end;
	bool is_shooting;
    
public:
	explicit TelekineticWeaponController(CWeaponMagazined* weapon);

	void update(CTelekineticObject* owner, const CEntityAlive* enemy) override;
	void on_perform_keep_object() override;
	void debug_draw();
	void update_auto_aim() const;
	void update_weapon_state() const;
	void on_keep_elapsed() override;
    bool can_shoot() const;
	void shoot();
};

class TelekineticGrenadeController : public ITelekineticBehavior
{
	CGrenade* grenade_;
	u32 grenade_initial_time = 0xffffffff;
	
public:
	explicit TelekineticGrenadeController(CGrenade* grenade) : grenade_(grenade)
	{
	}
	
	void update(CTelekineticObject* owner, const CEntityAlive* enemy) override;
	void on_throw_object_time() override;
	void on_raise() override;
};