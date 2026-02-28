#pragma once

enum ETelekineticState
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

class CTelekineticObject
{
    ETelekineticState state;

public:
    CPhysicsShellHolder* object;
    CTelekinesis* telekinesis;
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

    // Weapon
    u32 m_shoot_phase_end;
    bool m_is_weapon;
    bool m_weapon_have_target;
    bool m_is_shooting;

    bool m_rotate;

    CTelekineticObject();
    virtual ~CTelekineticObject();

    virtual bool init(CTelekinesis* tele, CPhysicsShellHolder* obj, float s, float h, u32 ttk, bool rot = true);
    void set_sound(const ref_sound& snd_hold, const ref_sound& snd_throw);

    virtual void raise(float step);
    virtual void raise_update();

    void prepare_keep();
    virtual void perform_keep_object();
    virtual void weapon_shoot();
    virtual void keep_update();
    virtual void release();
    virtual void throw_object(const Fvector& target, float power);
    void throw_object_t(const Fvector& target, float time);
    virtual void throw_update();
    virtual void update_state();
    virtual bool can_activate(CPhysicsShellHolder* obj);
    bool is_released() const { return state == TS_NONE; }
    virtual void switch_state(ETelekineticState new_state);
    ETelekineticState get_state() const { return state; }
    CPhysicsShellHolder* get_object() const { return object; }

    bool check_height() const;
    bool check_raise_time_out() const;

    bool keep_time_elapsed() const;
    bool throw_time_elapsed() const;
    bool is_weapon() const;
    
    void enable() const;

    bool operator==(const CPhysicsShellHolder* obj) const
    {
        return object == obj;
    }

    void rotate() const;

private:
    void update_hold_sound();
};
