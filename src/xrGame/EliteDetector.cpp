#include "StdAfx.h"
#include "EliteDetector.h"
#include "player_hud.h"
#include "../Include/xrRender/UIRender.h"
#include "../../xrUI/UIXmlInit.h"
#include "../../xrUI/xrUIXmlParser.h"
#include "../../xrUI/Widgets/UIStatic.h"
#include "ui/ArtefactDetectorUI.h"
#include "ui/PpiRadarRuntime.h"

namespace
{
static const float kPpiHoldRotEps = 1e-4f;
} // namespace

CEliteDetector::CEliteDetector()
{
	m_artefacts.m_af_rank = 3;
	m_ui_xml_tag = "elite";
}

void CEliteDetector::Load(LPCSTR section)
{
	inherited::Load(section);
	m_ui_xml_tag = READ_IF_EXISTS(pSettings, r_string, section, "ui_xml_tag", m_ui_xml_tag);
}

void CEliteDetector::CreateUI()
{
	R_ASSERT(nullptr == m_ui);

	m_ui = new CUIArtefactDetectorElite();
	ui().construct(this);
}

CUIArtefactDetectorElite& CEliteDetector::ui()
{
	return *((CUIArtefactDetectorElite*)m_ui);
}

void CEliteDetector::UpdateAf()
{
	if (m_ui == nullptr)
	{
		return;
	}

	ui().Clear();

	if (m_artefacts.m_ItemInfos.empty())
	{
		return;
	}

	CAfList::ItemsMapIt it_b = m_artefacts.m_ItemInfos.begin();
	CAfList::ItemsMapIt it_e = m_artefacts.m_ItemInfos.end();
	CAfList::ItemsMapIt it = it_b;

	Fvector detector_pos = Position();

	for (; it_b != it_e; ++it_b)
	{
		CArtefact* pAf = it_b->first;

		if (pAf->H_Parent())
		{
			continue;
		}

		ui().RegisterItemToDraw(pAf->Position(), "af_sign", pAf);

		if (pAf->CanBeInvisible())
		{
			float d = detector_pos.distance_to(pAf->Position());
			if (d < AfVisibleRadius())
			{
				pAf->SwitchVisibility(true);
			}
		}
	}
}

bool CEliteDetector::render_item_3d_ui_query()
{
	return IsWorking();
}

void CEliteDetector::render_item_3d_ui()
{
	R_ASSERT(HudItemData());
	inherited::render_item_3d_ui();
	ui().Draw();
	// Restore cull mode
	UIRender->CacheSetCullMode(ERHI_CULLMODE::BACK);
}

void fix_ws_wnd_size(CUIWindow* w, float kx)
{
	Fvector2 p = w->GetWndSize();
	p.x /= kx;
	w->SetWndSize(p);

	p = w->GetWndPos();
	p.x /= kx;
	w->SetWndPos(p);

	xrCriticalSectionGuard guard(w->csUi);

	for (auto& child : w->GetChildWndList())
	{
		fix_ws_wnd_size(child, kx);
	}
}

void CUIArtefactDetectorElite::construct(CEliteDetector* p)
{
	m_parent = p;

	CUIXml uiXml;
	uiXml.Load(CONFIG_PATH, UI_PATH, "ui_detector_artefact.xml");

	m_ppiBlipsInShader = p->PpiBlipsInShader();
	if (m_ppiBlipsInShader)
	{
		string512 texPath = {};
		// Shader for <auto_static> is read from the nested <texture> node (CUIXmlInit::InitTexture).
		xr_sprintf(texPath, "%s:wrk_area:auto_static:texture", p->ui_xml_tag());
		LPCSTR shaderName = uiXml.ReadAttrib(texPath, 0, "shader", nullptr);
		const bool shaderMatch = PpiRadarRuntime::IsDetectorPpiShaderName(shaderName);
		if (!shaderMatch)
			m_ppiBlipsInShader = false;
	}

	CUIXmlInit xml_init;
	string512 buff = {};
	xr_strcpy(buff, p->ui_xml_tag());

	xml_init.InitWindow(uiXml, buff, 0, this);

	m_wrk_area = new CUIWindow();

	xr_sprintf(buff, "%s:wrk_area", p->ui_xml_tag());

	xml_init.InitWindow(uiXml, buff, 0, m_wrk_area);
	m_wrk_area->SetAutoDelete(true);
	AttachChild(m_wrk_area);

	if (m_ppiBlipsInShader)
	{
		float bestArea = 0.0f;
		m_ppiBaseStatic = nullptr;

		for (CUIWindow* child : m_wrk_area->GetChildWndList())
		{
			if (child == nullptr)
				continue;

			CUIStatic* candidate = child->ui_cast_static();
			if (candidate == nullptr)
				continue;

			Fvector2 sz = candidate->GetWndSize();
			float area = sz.x * sz.y;
			if (area > bestArea)
			{
				bestArea = area;
				m_ppiBaseStatic = candidate;
			}
		}

		if (m_ppiBaseStatic == nullptr)
			m_ppiBlipsInShader = false;
	}

	xr_sprintf(buff, "%s", p->ui_xml_tag());
	int num = uiXml.GetNodesNum(buff, 0, "palette");
	XML_NODE* pStoredRoot = uiXml.GetLocalRoot();
	uiXml.SetLocalRoot(uiXml.NavigateToNode(buff, 0));
	for (int idx = 0; idx < num; ++idx)
	{
		CUIStatic* S = new CUIStatic();
		shared_str name = uiXml.ReadAttrib("palette", idx, "id");
		m_palette[name] = S;
		xml_init.InitStatic(uiXml, "palette", idx, S);
		S->SetAutoDelete(true);
		m_wrk_area->AttachChild(S);
		S->SetCustomDraw(true);
	}
	uiXml.SetLocalRoot(pStoredRoot);

	SetupAttachOffset(m_parent->cNameSect().c_str());
}

float& CUIArtefactDetectorElite::ppiSweepLastHitTimeRef(void* key)
{
	R_ASSERT(key != nullptr);
	auto it = m_ppiSweepLastHitTime.find(key);
	if (it == m_ppiSweepLastHitTime.end())
	{
		it = m_ppiSweepLastHitTime.insert(std::make_pair(key, -1.0f)).first;
	}
	return it->second;
}

void CUIArtefactDetectorElite::erasePpiSweepEntry(void* key)
{
	if (key == nullptr)
	{
		return;
	}
	m_ppiSweepLastHitTime.erase(key);
}

void CUIArtefactDetectorElite::resetPpiSweepState()
{
	m_ppiSweepArmStartTime = -1.0f;
	m_ppiSweepLastHitTime.clear();
}

void CUIArtefactDetectorElite::prunePpiSweepLastHitTimeMap()
{
	for (auto mapIt = m_ppiSweepLastHitTime.begin(); mapIt != m_ppiSweepLastHitTime.end();)
	{
		void* mapKey = mapIt->first;
		bool keep = false;
		for (const auto& item : m_items_to_draw)
		{
			if (item.ppiRadarTargetKey == mapKey)
			{
				keep = true;
				break;
			}
		}
		if (!keep)
		{
			mapIt = m_ppiSweepLastHitTime.erase(mapIt);
		}
		else
		{
			++mapIt;
		}
	}
}

void CUIArtefactDetectorElite::update()
{
	if (m_parent != nullptr && !m_parent->IsWorking())
	{
		resetPpiSweepState();
	}
	inherited::update();
	CUIWindow::Update();
}

void CUIArtefactDetectorElite::UpdatePpiData()
{
	if (!m_ppiBlipsInShader || m_ppiBaseStatic == nullptr || Render == nullptr)
	{
		return;
	}

	CCustomDetector* const parentDet = m_parent;
	R_ASSERT(parentDet != nullptr);
	RDEVICE.detectorPpiShaderParams.sweepPhaseScale = parentDet->PpiRadarSweepPhaseScale();
	RDEVICE.detectorPpiShaderParams.beamTouchAngleRad = parentDet->PpiRadarBeamTouchAngleRad();

	if (!parentDet->IsWorking())
	{
		resetPpiSweepState();
		return;
	}

	Fvector2 wrk_sz = m_wrk_area->GetWndSize();
	Fvector2 rp;
	m_wrk_area->GetAbsolutePos(rp);

	Fmatrix M;
	PpiRadarRuntime::BuildInvViewMapFromCameraYaw(M);

	float baseX = m_ppiBaseStatic->GetWndPos().x;
	float baseY = m_ppiBaseStatic->GetWndPos().y;
	Fvector2 baseSize = m_ppiBaseStatic->GetWndSize();
	float baseW = baseSize.x;
	float baseH = baseSize.y;

	float detectRadius = m_parent->AfDetectRadius();
	if (fis_zero(detectRadius))
	{
		return;
	}

	prunePpiSweepLastHitTimeMap();

	const float gameTime = Device.fTimeGlobal;
	const float sweepPhaseScale = parentDet->PpiRadarSweepPhaseScale();
	const float armDurationSec = parentDet->PpiRadarSweepArmDurationSec();
	if (m_ppiSweepArmStartTime < 0.0f)
	{
		m_ppiSweepArmStartTime = gameTime;
		// New PPI session: drop latched hits so markers wait for sweep again (stale map caused instant blips).
		m_ppiSweepLastHitTime.clear();
	}
	const bool armUseRot = !fis_zero(sweepPhaseScale);
	const float armHoldRot = armUseRot ? (armDurationSec * sweepPhaseScale) : 0.0f;
	const float armRotSince = armUseRot ? ((gameTime - m_ppiSweepArmStartTime) * sweepPhaseScale) : 0.0f;
	const bool hasPpiSweepArmed = armUseRot ? (armRotSince + kPpiHoldRotEps >= armHoldRot)
											: ((gameTime - m_ppiSweepArmStartTime) >= armDurationSec);

	const float beamTouchAngleRad = parentDet->PpiRadarBeamTouchAngleRad();
	const float markerFlashSec = parentDet->PpiRadarMarkerAppearSec();
	const float peakSec = parentDet->PpiRadarMarkerPeakSec();
	const float fadeSec = parentDet->PpiRadarMarkerFadeSec();
	const float lingerAlpha = parentDet->PpiRadarMarkerLingerAlpha();
	const float sectorSoftRad = parentDet->PpiRadarMarkerSectorSoftRad();
	const float waveFreq = parentDet->PpiRadarMarkerWaveFreq();
	const float waveAmp = parentDet->PpiRadarMarkerWaveAmp();
	const float waveDecay = parentDet->PpiRadarMarkerWaveDecay();
	const float touchThreshold = parentDet->PpiRadarTouchThreshold();
	const float touchBlend = parentDet->PpiRadarTouchBlend();
	SPpiRadarMarkerParams markerParams = {};
	markerParams.sweepPhaseScale = sweepPhaseScale;
	markerParams.beamTouchAngleRad = beamTouchAngleRad;
	markerParams.markerFlashSec = markerFlashSec;
	markerParams.peakSec = peakSec;
	markerParams.fadeSec = fadeSec;
	markerParams.lingerAlpha = lingerAlpha;
	markerParams.sectorSoftRad = sectorSoftRad;
	markerParams.waveFreq = waveFreq;
	markerParams.waveAmp = waveAmp;
	markerParams.waveDecay = waveDecay;
	markerParams.touchThreshold = touchThreshold;
	markerParams.touchBlend = touchBlend;

	float kz = wrk_sz.y / detectRadius;

	u8 data[kPpiMaxPoints * 4] = {};
	u32 slot = 0;

	for (auto& item : m_items_to_draw)
	{
		if (slot >= kPpiMaxPoints)
		{
			break;
		}

		Fvector p_ = item.pos;
		Fvector pt3d;
		M.transform_tiny(pt3d, p_);
		pt3d.x *= kz;
		pt3d.z *= kz;
		pt3d.x += wrk_sz.x / 2.0f;
		pt3d.z -= wrk_sz.y;

		Fvector2 pos;
		pos.set(pt3d.x, -pt3d.z);
		pos.sub(rp);

		float nx = (pos.x - baseX) / baseW;
		float ny = (pos.y - baseY) / baseH;
		if (nx < 0.0f || nx > 1.0f || ny < 0.0f || ny > 1.0f)
		{
			if (item.ppiRadarTargetKey != nullptr)
			{
				erasePpiSweepEntry(item.ppiRadarTargetKey);
			}
			continue;
		}

		float cx = nx * 2.0f - 1.0f;
		float cy = ny * 2.0f - 1.0f;
		if ((cx * cx + cy * cy) > 1.0f)
		{
			if (item.ppiRadarTargetKey != nullptr)
			{
				erasePpiSweepEntry(item.ppiRadarTargetKey);
			}
			continue;
		}

		if (item.ppiRadarTargetKey == nullptr)
		{
			continue;
		}

		const float targetAngleRad = atan2f(cy, cx);
		float markerVis = 0.0f;
		if (hasPpiSweepArmed)
		{
			float& lastHitRef = ppiSweepLastHitTimeRef(item.ppiRadarTargetKey);
			markerVis = PpiRadarRuntime::ComputeMarkerVisibility(gameTime, targetAngleRad, lastHitRef, markerParams);
		}

		data[slot * 4 + 0] = u8(clampr(nx, 0.0f, 1.0f) * 255.0f);
		data[slot * 4 + 1] = u8(clampr(ny, 0.0f, 1.0f) * 255.0f);
		data[slot * 4 + 2] = item.ppiType ? 255 : 0;
		data[slot * 4 + 3] = u8(clampr(markerVis, 0.0f, 1.0f) * 255.0f);

		++slot;
	}

	Render->UpdateDetectorPpiData(data, kPpiMaxPoints, 1);
}

void CUIArtefactDetectorElite::Draw()
{
	Fmatrix LM;
	if (!BuildAttachMatrix(m_parent->HudItemData(), LM))
	{
		return;
	}

	IUIRender::ePointType bk = UI().m_currentPointType;

	UI().m_currentPointType = IUIRender::pttLIT;

	UIRender->CacheSetXformWorld(LM);
	UIRender->CacheSetCullMode(ERHI_CULLMODE::NONE);

	if (m_ppiBlipsInShader)
	{
		UpdatePpiData();
	}

	CUIWindow::Draw();

	Fvector2 wrk_sz = m_wrk_area->GetWndSize();
	Fvector2 rp;
	m_wrk_area->GetAbsolutePos(rp);

	Fmatrix M;
	PpiRadarRuntime::BuildInvViewMapFromCameraYaw(M);

	UI().ScreenFrustumLIT().CreateFromRect(Frect().set(rp.x, rp.y, wrk_sz.x, wrk_sz.y));

	// Palette uses SetCustomDraw(true); parent Draw skips it. Legacy (no PPI): draw XML palette markers.
	// With PPI + hud\detector_ppi (addon), markers are drawn in the radar PS from s_data — skip palette here.
	if (!m_ppiBlipsInShader)
	{
		const float detectRadius = m_parent->AfDetectRadius();
		if (!fis_zero(detectRadius))
		{
			const float kz = wrk_sz.y / detectRadius;
			for (const auto& item : m_items_to_draw)
			{
				Fvector p_ = item.pos;
				Fvector pt3d;
				M.transform_tiny(pt3d, p_);
				pt3d.x *= kz;
				pt3d.z *= kz;

				pt3d.x += wrk_sz.x / 2.0f;
				pt3d.z -= wrk_sz.y;

				Fvector2 pos;
				pos.set(pt3d.x, -pt3d.z);
				pos.sub(rp);

				item.pStatic->SetWndPos(pos);
				item.pStatic->Draw();
			}
		}
	}

	UI().m_currentPointType = bk;
}

void CUIArtefactDetectorElite::Clear()
{
	m_items_to_draw.clear();
}

void CUIArtefactDetectorElite::RegisterItemToDraw(const Fvector& p, const shared_str& palette_idx, void* ppiRadarTargetKey)
{
	xr_map<shared_str, CUIStatic*>::iterator it = m_palette.find(palette_idx);
	if (it == m_palette.end())
	{
		Msg("! RegisterItemToDraw. static not found for [%s]", palette_idx.c_str());
		return;
	}

	CUIStatic* S = m_palette[palette_idx];
	const char* id = palette_idx.c_str();
	const bool isZone = id != nullptr && 0 == xr_strncmp(id, "zone_", 5);
	const u8 typeId = isZone ? u8(1) : u8(0);
	SDrawOneItem itm(S, p, typeId, ppiRadarTargetKey);
	m_items_to_draw.push_back(itm);
}

CScientificDetector::CScientificDetector()
{
	m_artefacts.m_af_rank = 3;
	m_ui_xml_tag = "scientific";
}

CScientificDetector::~CScientificDetector()
{
	m_zones.destroy();
}

void CScientificDetector::Load(LPCSTR section)
{
	inherited::Load(section);
	m_zones.load(section, "zone");
}

void CScientificDetector::UpdateWork()
{
	ui().Clear();

	if (!m_bWorking)
	{
		return;
	}

	CAfList::ItemsMapIt ait_b = m_artefacts.m_ItemInfos.begin();
	CAfList::ItemsMapIt ait_e = m_artefacts.m_ItemInfos.end();
	CAfList::ItemsMapIt ait = ait_b;
	Fvector detector_pos = Position();

	for (; ait_b != ait_e; ++ait_b)
	{
		CArtefact* pAf = ait_b->first;

		if (pAf->H_Parent())
		{
			continue;
		}

		ui().RegisterItemToDraw(pAf->Position(), pAf->cNameSect(), pAf);

		if (pAf->CanBeInvisible())
		{
			float d = detector_pos.distance_to(pAf->Position());
			if (d < AfVisibleRadius())
			{
				pAf->SwitchVisibility(true);
			}
		}
	}

	CZoneList::ItemsMapIt zit_b = m_zones.m_ItemInfos.begin();
	CZoneList::ItemsMapIt zit_e = m_zones.m_ItemInfos.end();
	CZoneList::ItemsMapIt zit = zit_b;

	for (; zit_b != zit_e; ++zit_b)
	{
		CAnomalyZone* pZone = zit_b->first;
		ui().RegisterItemToDraw(pZone->Position(), pZone->cNameSect(), pZone);
	}

	m_ui->update();
}

void CScientificDetector::shedule_Update(u32 dt)
{
	inherited::shedule_Update(dt);

	if (!H_Parent() || !m_bWorking)
	{
		return;
	}

	Fvector P;
	P.set(H_Parent()->Position());
	m_zones.feel_touch_update(P, AfDetectRadius());
}

void CScientificDetector::OnH_B_Independent(bool just_before_destroy)
{
	inherited::OnH_B_Independent(just_before_destroy);

	m_zones.clear();
}
