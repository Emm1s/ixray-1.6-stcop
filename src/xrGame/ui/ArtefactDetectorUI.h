#pragma once
#include "../../xrUI/Widgets/UIFrameLineWnd.h"
#include "../../xrCore/API/xrAPI.h"
#include "UIAttachedPlaneWnd.h"

class CUIStatic;
class CUIFrameLineWnd;
class CUIDetectorWave;
class CSimpleDetector;
class CAdvancedDetector;
class CEliteDetector;
class CUIXml;
class CLAItem;

class CUIArtefactDetectorBase
{
public:
	virtual ~CUIArtefactDetectorBase() = default;
	virtual void update() {};
};

class CUIDetectorWave : public CUIFrameLineWnd
{
	using inherited = CUIFrameLineWnd;
protected:
	float m_curr_v = 0.0f;
	float m_step = 0.0f;
public:
	CUIDetectorWave() = default;
	void InitFromXML(CUIXml& xml, LPCSTR path);
	void SetVelocity(float v) { m_curr_v = v; }
	void Update() override;

	virtual CUIWindow* ui_cast_window() { return this; }
};

class CUIArtefactDetectorSimple final : public CUIArtefactDetectorBase
{
	using inherited = CUIArtefactDetectorBase;

	CSimpleDetector* m_parent = nullptr;
	u16	m_flash_bone = BI_NONE;
	u16	m_on_off_bone = BI_NONE;
	u32	m_turn_off_flash_time = BI_NONE;

	float m_fFlash_light_range = 0.0f;
	float m_fOnOff_light_range = 0.0f;

	ref_light m_flash_light = nullptr;
	ref_light m_on_off_light = nullptr;
	CLAItem* m_pOnOfLAnim = nullptr;
	CLAItem* m_pFlashLAnim = nullptr;
	void setup_internals();
public:
	~CUIArtefactDetectorSimple() override;
	void update() override;
	void Flash(bool bOn, float fRelPower);

	void construct(CSimpleDetector* p);
};

class CUIArtefactDetectorElite final : public CUIArtefactDetectorBase, public CUIAttachedPlaneWnd
{
	using inherited = CUIArtefactDetectorBase;

	CUIWindow* m_wrk_area = nullptr;

	xr_map<shared_str, CUIStatic*> m_palette = {};

	struct SDrawOneItem
	{
		SDrawOneItem(CUIStatic* s, const Fvector& p, u8 typeId, void* ppiRadarTargetKey = nullptr)
			: pStatic(s), pos(p), ppiType(typeId), ppiRadarTargetKey(ppiRadarTargetKey)
		{
		}
		CUIStatic* pStatic = nullptr;
		Fvector	pos = zero_vel;
		u8 ppiType = 0;
		// Stable key for PPI sweep state (e.g. CArtefact*, CAnomalyZone*). Cleared each frame from m_items_to_draw only.
		void* ppiRadarTargetKey = nullptr;
	};
	xr_vector<SDrawOneItem>	m_items_to_draw = {};
	CEliteDetector* m_parent = nullptr;

	CUIStatic* m_ppiBaseStatic = nullptr;
	bool m_ppiBlipsInShader = false;
	static constexpr u32 kPpiMaxPoints = 32;

	// PPI: last Device.fTimeGlobal when sweep first latched this world target (-1 in map = never).
	xr_map<void*, float> m_ppiSweepLastHitTime;
	// PPI: Device.fTimeGlobal when PPI blips became active (-1 = reset, e.g. holster).
	float m_ppiSweepArmStartTime = -1.0f;

private:
	void resetPpiSweepState();
	float& ppiSweepLastHitTimeRef(void* key);
	void erasePpiSweepEntry(void* key);
	void prunePpiSweepLastHitTimeMap();
	void UpdatePpiData();

public:
	void update() override;
	void Draw() override;

	void construct(CEliteDetector* p);
	void Clear();
	void RegisterItemToDraw(const Fvector& p, const shared_str& palette_idx, void* ppiRadarTargetKey = nullptr);

	virtual CUIWindow* ui_cast_window() { return this; }
};

class CUIArtefactDetectorAdv final : public CUIArtefactDetectorBase
{
	using inherited = CUIArtefactDetectorBase;

	CAdvancedDetector* m_parent = nullptr;
	Fvector	m_target_dir = zero_vel;
	float m_cur_y_rot = 0.0f;
	float m_curr_ang_speed = 0.0f;
	u16	m_bid = BI_NONE;

public:
	~CUIArtefactDetectorAdv() override = default;
	void update() override;
	void construct(CAdvancedDetector* p);
	void SetValue(const float v1, const Fvector& v2);
	float CurrentYRotation() const;
	void ResetBoneCallbacks();
	void SetBoneCallbacks();
};
