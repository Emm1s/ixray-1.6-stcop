#pragma once
#include "CustomDevice.h"
#include "AnomalyZone.h"
#include "CustomDetectorZones.h"
#include "ui/ArtefactDetectorUI.h"
#include "IPowerManager.h"

class CUIArtefactDetectorBase;

class CCustomDetector : public CCustomDevice, public IPowerManager
{
	using inherited = CCustomDevice;

	float m_fAfVisRadius = 0.0f;
	float m_fAfDetectRadius = 0.0f;
	bool m_ppiBlipsInShader = false;
	// Optional PPI radar CPU/shader sync (see devices.ltx keys). Defaults match legacy behaviour.
	float m_ppiRadarSweepPhaseScale = 0.6f;
	float m_ppiRadarSweepArmSec = 0.0f;
	float m_ppiRadarBeamTouchAngleRad = 0.10f;
	float m_ppiRadarMarkerAppearSec = 0.055f;
	float m_ppiRadarMarkerPeakSec = 0.035f;
	float m_ppiRadarMarkerFadeSec = 0.14f;
	float m_ppiRadarMarkerLingerAlpha = 0.22f;
	float m_ppiRadarMarkerSectorSoftRad = 0.07f;
	float m_ppiRadarMarkerWaveFreq = 52.0f;
	float m_ppiRadarMarkerWaveAmp = 0.11f;
	float m_ppiRadarMarkerWaveDecay = 8.0f;
	float m_ppiRadarTouchThreshold = 0.22f;
	float m_ppiRadarTouchBlend = 0.92f;
protected:
	CUIArtefactDetectorBase* m_ui = nullptr;
	CAfList m_artefacts;
public:
	bool m_need_refresh = false;
public:
	CCustomDetector() = default;
	~CCustomDetector() override;

	bool IsNeedReloadUI() { return m_bWorking && m_need_refresh; }
	void Load(LPCSTR section) override;
	void OnH_B_Independent(bool just_before_destroy) override;
	void shedule_Update(u32 dt) override;
	void TurnDetectorInternal(bool b) final override;

	float AfVisibleRadius() const { return m_fAfVisRadius; }
	float AfDetectRadius() const { return m_fAfDetectRadius; }
	bool PpiBlipsInShader() const { return m_ppiBlipsInShader; }

	float PpiRadarSweepPhaseScale() const { return m_ppiRadarSweepPhaseScale; }
	float PpiRadarSweepArmDurationSec() const;
	float PpiRadarBeamTouchAngleRad() const { return m_ppiRadarBeamTouchAngleRad; }
	float PpiRadarMarkerAppearSec() const { return m_ppiRadarMarkerAppearSec; }
	float PpiRadarMarkerPeakSec() const { return m_ppiRadarMarkerPeakSec; }
	float PpiRadarMarkerFadeSec() const { return m_ppiRadarMarkerFadeSec; }
	float PpiRadarMarkerLingerAlpha() const { return m_ppiRadarMarkerLingerAlpha; }
	float PpiRadarMarkerSectorSoftRad() const { return m_ppiRadarMarkerSectorSoftRad; }
	float PpiRadarMarkerWaveFreq() const { return m_ppiRadarMarkerWaveFreq; }
	float PpiRadarMarkerWaveAmp() const { return m_ppiRadarMarkerWaveAmp; }
	float PpiRadarMarkerWaveDecay() const { return m_ppiRadarMarkerWaveDecay; }
	float PpiRadarTouchThreshold() const { return m_ppiRadarTouchThreshold; }
	float PpiRadarTouchBlend() const { return m_ppiRadarTouchBlend; }

	virtual CCustomDetector* cast_custom_detector() { return this; }
	virtual CCustomDevice* cast_custom_device() { return this; }

	void save(NET_Packet& output_packet) override;
	void load(IReader& input_packet) override;

protected:
	void UpdateWork() override;
	virtual void UpdateAf() {};
	virtual void CreateUI() {};
};
