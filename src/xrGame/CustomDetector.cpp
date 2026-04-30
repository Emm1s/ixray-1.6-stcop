#include "StdAfx.h"
#include "CustomDetector.h"
#include "Inventory.h"
#include "Actor.h"

CCustomDetector::~CCustomDetector()
{
	m_artefacts.destroy();
	TurnDetectorInternal(false);
	xr_delete(m_ui);
}

void CCustomDetector::Load(LPCSTR section)
{
	m_animation_slot = 7;
	inherited::Load(section);

	m_fAfDetectRadius = READ_IF_EXISTS(pSettings, r_float, section, "af_radius", 30.0f);
	m_fAfVisRadius = READ_IF_EXISTS(pSettings, r_float, section, "af_vis_radius", 2.0f);
	m_ppiBlipsInShader = READ_IF_EXISTS(pSettings, r_bool, section, "ppi_blips_in_shader", false);
	m_ppiRadarSweepPhaseScale = READ_IF_EXISTS(pSettings, r_float, section, "ppi_radar_sweep_phase_scale", 0.6f);
	m_ppiRadarSweepArmSec = READ_IF_EXISTS(pSettings, r_float, section, "ppi_radar_sweep_arm_sec", 0.0f);
	m_ppiRadarBeamTouchAngleRad = READ_IF_EXISTS(pSettings, r_float, section, "ppi_radar_beam_touch_angle_rad", 0.10f);
	m_ppiRadarMarkerAppearSec = READ_IF_EXISTS(pSettings, r_float, section, "ppi_radar_marker_appear_sec", 0.055f);
	m_ppiRadarMarkerPeakSec = READ_IF_EXISTS(pSettings, r_float, section, "ppi_radar_marker_peak_sec", 0.035f);
	m_ppiRadarMarkerFadeSec = READ_IF_EXISTS(pSettings, r_float, section, "ppi_radar_marker_fade_sec", 0.14f);
	m_ppiRadarMarkerLingerAlpha = READ_IF_EXISTS(pSettings, r_float, section, "ppi_radar_marker_linger_alpha", 0.22f);
	m_ppiRadarMarkerSectorSoftRad = READ_IF_EXISTS(pSettings, r_float, section, "ppi_radar_marker_sector_soft_rad", 0.07f);
	m_ppiRadarMarkerWaveFreq = READ_IF_EXISTS(pSettings, r_float, section, "ppi_radar_marker_wave_freq", 52.0f);
	m_ppiRadarMarkerWaveAmp = READ_IF_EXISTS(pSettings, r_float, section, "ppi_radar_marker_wave_amp", 0.11f);
	m_ppiRadarMarkerWaveDecay = READ_IF_EXISTS(pSettings, r_float, section, "ppi_radar_marker_wave_decay", 8.0f);
	m_ppiRadarTouchThreshold = READ_IF_EXISTS(pSettings, r_float, section, "ppi_radar_touch_threshold", 0.22f);
	m_ppiRadarTouchBlend = READ_IF_EXISTS(pSettings, r_float, section, "ppi_radar_touch_blend", 0.92f);
	m_artefacts.load(section, "af");

	SpatialComponent->spatial.type |= ESPATIAL_TYPE::ANOMALY_DETECTOR;

	IPowerManager::SetSelfObject(cast_inventory_item(), H_Parent());
	IPowerManager::Load(section, cast_inventory_item());
}

float CCustomDetector::PpiRadarSweepArmDurationSec() const
{
	if (m_ppiRadarSweepArmSec > 0.0f)
	{
		return m_ppiRadarSweepArmSec;
	}
	const float scale = m_ppiRadarSweepPhaseScale;
	if (fis_zero(scale))
	{
		return 1.0f / 0.6f;
	}
	return 1.0f / scale;
}

void CCustomDetector::shedule_Update(u32 dt)
{
	PROF_EVENT(__FUNCTION__)

	inherited::shedule_Update(dt);

	IPowerManager::SetEnabled(m_bWorking);

	bool isInHands = false;
	if (attachable_hud_item* itm = g_player_hud->attached_item(1))
	{
		if (itm->m_parent_hud_item == this)
		{
			isInHands = true;
		}
	}

	if (isInHands && IPowerManager::IsAllow())
	{
		if (IPowerManager::GetLeftPowerValue() <= 0)
		{
			m_need_refresh = true;
			m_bWorking = false;
		}
		else
		{
			if (m_need_refresh)
			{
				m_bWorking = true;
				m_need_refresh = false;
			}
		}
	}

	if (!IsWorking())
	{
		return;
	}

	Fvector P;
	P.set(H_Parent()->Position());
	m_artefacts.feel_touch_update(P, m_fAfDetectRadius);
}

void CCustomDetector::UpdateWork()
{
	if (!IsWorking() || !m_ui)
	{
		return;
	}

	UpdateAf();

	m_ui->update();
}

void CCustomDetector::OnH_B_Independent(bool just_before_destroy)
{
	inherited::OnH_B_Independent(just_before_destroy);

	m_artefacts.clear();
}

void CCustomDetector::TurnDetectorInternal(bool b)
{
	m_bWorking = b;

	if (IPowerManager::IsAllow() && IPowerManager::GetLeftPowerValue() <= 0)
	{
		m_bWorking = false;
	}

	if (b && m_ui == nullptr)
	{
		CreateUI();
	}
}

void CCustomDetector::save(NET_Packet& output_packet)
{
	inherited::save(output_packet);
	IPowerManager::net_save(output_packet);
}

void CCustomDetector::load(IReader& input_packet)
{
	inherited::load(input_packet);
	IPowerManager::net_load(input_packet);
}
