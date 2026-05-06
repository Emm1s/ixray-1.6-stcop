#include "StdAfx.h"
#include "UIPdaWnd.h"
#include "../PDA.h"
#include "UIPdaAux.h"
#include "../../xrUI/xrUIXmlParser.h"
#include "../../xrUI/UIXmlInit.h"
#include "UIInventoryUtilities.h"
#include "../../xrEngine/xr_input.h"
#include "../Level.h"
#include "UIGameCustom.h"
#include "UIStalkersRankingWnd.h"
#include "../../xrUI/Widgets/UIStatic.h"
#include "../../xrUI/Widgets/UIFrameWindow.h"
#include "../../xrUI/Widgets/UITabControl.h"
#include "UIMapWnd.h"
#include "../../xrUI/Widgets/UIFrameLineWnd.h"
#include "object_broker.h"
#include "UIMessagesWindow.h"
#include "UIMainIngameWnd.h"
#include "../../xrUI/Widgets/UITabButton.h"
#include "../../xrUI/Widgets/UIAnimatedStatic.h"
#include "UIEventsWnd.h"
#include "../../xrUI/UIHelper.h"
#include "../../xrUI/Widgets/UIHint.h"
#include "../../xrUI/Widgets/UIBtnHint.h"
#include "UITaskWnd.h"
#include "UIRankingWnd.h"
#include "UILogsWnd.h"
#include "UIFactionWarWnd.h"
#include "UIScriptWnd.h"
#include "UIPdaContactsWnd.h"
#include "UIEncyclopediaWnd.h"
#include "UIActorInfo.h"
#include "UIDiaryWnd.h"
#include "../../xrUI/UICursor.h"
#include "PdaConstants.h"
#include "PdaState.h"
#include "PdaScriptBridge.h"

u32 g_pda_info_state = 0;

namespace
{
struct LegacyTabIdEntry
{
    const char* legacyId;
    const char* tabId;
};

constexpr LegacyTabIdEntry g_legacyTabIds[] = {
    {PdaLegacyTabId::Legacy0, PdaSectionId::Quests},
    {PdaLegacyTabId::Legacy1, PdaSectionId::Map},
    {PdaLegacyTabId::Legacy2, PdaSectionId::Diary},
    {PdaLegacyTabId::Legacy3, PdaSectionId::Contacts},
    {PdaLegacyTabId::Legacy4, PdaSectionId::RankingGlobal},
    {PdaLegacyTabId::Legacy5, PdaSectionId::ActorStatistic},
    {PdaLegacyTabId::Legacy6, PdaSectionId::Encyclopedia},
};

// Canonical section ids that ResolveKnownTabId may normalize through [pda_tab_aliases].
constexpr const char* g_knownPdaSectionIds[] = {
    PdaSectionId::Tasks,
    PdaSectionId::Quests,
    PdaSectionId::FractionWar,
    PdaSectionId::Contacts,
    PdaSectionId::Ranking,
    PdaSectionId::RankingGlobal,
    PdaSectionId::Logs,
    PdaSectionId::Encyclopedia,
    PdaSectionId::ActorStatistic,
    PdaSectionId::Diary,
    PdaSectionId::Map,
};

const char* ResolveTabId(const char* sectionId)
{
    return PdaSectionId::Resolve(sectionId);
}

shared_str ResolveKnownTabId(const shared_str& sectionId)
{
    for (const char* canonicalId : g_knownPdaSectionIds)
    {
        if (PdaSectionId::Equals(sectionId, canonicalId))
        {
            return ResolveTabId(canonicalId);
        }
    }
    return sectionId;
}

bool TryGetPdaUpdateSection(const shared_str& sectionId, pda_section::part& updateSection)
{
    if (PdaSectionId::Equals(sectionId, PdaSectionId::Tasks))
    {
        updateSection = pda_section::quests;
        return true;
    }
    if (PdaSectionId::Equals(sectionId, PdaSectionId::Quests))
    {
        updateSection = pda_section::quests;
        return true;
    }
    if (PdaSectionId::Equals(sectionId, PdaSectionId::Contacts))
    {
        updateSection = pda_section::contacts;
        return true;
    }
    if (PdaSectionId::Equals(sectionId, PdaSectionId::Ranking))
    {
        updateSection = pda_section::ranking;
        return true;
    }
    if (PdaSectionId::Equals(sectionId, PdaSectionId::RankingGlobal))
    {
        updateSection = pda_section::ranking;
        return true;
    }
    if (PdaSectionId::Equals(sectionId, PdaSectionId::Logs))
    {
        updateSection = pda_section::news;
        return true;
    }
    if (PdaSectionId::Equals(sectionId, PdaSectionId::Encyclopedia))
    {
        updateSection = pda_section::encyclopedia;
        return true;
    }
    if (PdaSectionId::Equals(sectionId, PdaSectionId::ActorStatistic))
    {
        updateSection = pda_section::statistics;
        return true;
    }
    if (PdaSectionId::Equals(sectionId, PdaSectionId::Diary))
    {
        updateSection = pda_section::diary;
        return true;
    }
    if (PdaSectionId::Equals(sectionId, PdaSectionId::Map))
    {
        updateSection = pda_section::map;
        return true;
    }
    return false;
}

// Same time + legacy date format as PDA timer_frame_line (InventoryUtilities).
shared_str BuildPdaGameDateTimeString()
{
    xr_string gameDateTime = *InventoryUtilities::GetGameTimeAsString(InventoryUtilities::etpTimeToMinutes);
    gameDateTime += " ";
    gameDateTime += *InventoryUtilities::GetDateAsStringLegacy(Level().GetGameTime(), InventoryUtilities::edpDateToDay);
    return shared_str(gameDateTime.c_str());
}

} // namespace

void RearrangeTabButtons(CUITabControl* pTab);
void RearrangeTabButtonsLegacy(CUITabControl* pTab, xr_vector<Fvector2>& vec_sign_places);

CUIPdaWnd::CUIPdaWnd()
{
	LoadCallbackGlobals(m_isSetActiveSubdialog, m_onSetActiveSubdialog, PdaScript::OnSetActiveSubdialog);

	pUITaskWnd       = nullptr;
	pUIFactionWarWnd = nullptr;
	pUIRankingWnd    = nullptr;
	pUILogsWnd       = nullptr;
	UIPdaContactsWnd = nullptr;
	pUIEventsWnd       = nullptr;
	pUIStalkersRankingWnd = nullptr;
	pUIEncyclopediaWnd = nullptr;
	pUIActorInfoWnd	 = nullptr;
	pUIDiaryWnd		 = nullptr;
	pUIMapWnd		 = nullptr;

	m_hint_wnd       = nullptr;
	m_caption		 = nullptr;
	m_caption_const	 = "";
	m_clock			 = nullptr;
	m_pTabBgLayer     = nullptr;
	m_pCurrentTabBackground = nullptr;
	UIMainButtonsBackground = nullptr;
	UITimerBackground = nullptr;
	UINoice			 = nullptr;
	m_btn_close		 = nullptr;
	m_updatedSectionImage = nullptr;
	m_oldSectionImage = nullptr;
	m_sign_places_main.clear();

	last_cursor_pos.set(UI_BASE_WIDTH / 2.f, UI_BASE_HEIGHT / 2.f);

	Init();
	ActionRepeaters()->Register(this, kUI_TAB_LEFT);
	ActionRepeaters()->Register(this, kUI_TAB_RIGHT);
	ActionRepeaters()->Register(this, kUI_TAB_SECONDARY_LEFT);
	ActionRepeaters()->Register(this, kUI_TAB_SECONDARY_RIGHT);
}

CUIPdaWnd::~CUIPdaWnd()
{
	ActionRepeaters()->UnregisterOwner(this);
	delete_data( pUITaskWnd );
	delete_data( pUIFactionWarWnd );
	delete_data( UIPdaContactsWnd );
	delete_data( pUIRankingWnd );
	delete_data( pUILogsWnd );
	delete_data( pUIEventsWnd );
	delete_data( pUIStalkersRankingWnd );
	delete_data( pUIEncyclopediaWnd );
	delete_data( pUIActorInfoWnd );
	delete_data( pUIDiaryWnd );
	delete_data( pUIMapWnd );

	delete_data( m_hint_wnd );
	delete_data( UINoice );
	delete_data( m_updatedSectionImage );
	delete_data( m_oldSectionImage );
}

void CUIPdaWnd::Init()
{
	CUIXml					uiXml;
	uiXml.Load				(CONFIG_PATH, UI_PATH, PdaXml::Main);

	m_pActiveDialog			= nullptr;
	m_sActiveSection		= "";

	CUIXmlInit::InitWindow	(uiXml, "main", 0, this);

	UIMainPdaFrame			= UIHelper::CreateStatic	( uiXml, "background_static", this );
	m_pTabBgLayer = new CUIWindow();
	m_pTabBgLayer->SetAutoDelete(true);
	UIMainPdaFrame->AttachChild(m_pTabBgLayer);
	if (uiXml.NavigateToNode("caption_static"))
	{
		m_caption				= UIHelper::CreateStatic	( uiXml, "caption_static", this );
		m_caption_const			= ( m_caption->TextItemControl()->GetText() );
		// game_datetime: 1 = show game time + legacy date in caption; 0 = default (prefix + active tab, vanilla PDA).
		m_captionGameDateTime	= uiXml.ReadAttribInt("caption_static", 0, "game_datetime", 0) != 0;
	}

	if (uiXml.NavigateToNode("clock_wnd"))
		m_clock					= UIHelper::CreateStatic	( uiXml, "clock_wnd", this );

	CUIWindow* tabControlParent = this;
	if (uiXml.NavigateToNode("mbbackground_frame_line"))
	{
		UIMainButtonsBackground = UIHelper::CreateFrameLine	( uiXml, "mbbackground_frame_line", UIMainPdaFrame);
		tabControlParent = UIMainPdaFrame;
	}
	
	if (uiXml.NavigateToNode("timer_frame_line"))
		UITimerBackground = UIHelper::CreateFrameLine(uiXml, "timer_frame_line", UIMainPdaFrame);

	if (uiXml.NavigateToNode("anim_static"))
	{
		m_anim_static = new CUIAnimatedStatic();
		AttachChild(m_anim_static);
		m_anim_static->SetAutoDelete(true);
		CUIXmlInit::InitAnimatedStatic(uiXml, "anim_static", 0, m_anim_static);
	}
	if (uiXml.NavigateToNode("close_button"))
		m_btn_close				= UIHelper::Create3tButton( uiXml, "close_button", this );

	if (uiXml.NavigateToNode("hint_wnd"))
		m_hint_wnd				= UIHelper::CreateHint( uiXml, "hint_wnd" );

	UITabControl					= new CUITabControl();
	UITabControl->SetAutoDelete		(true);
	tabControlParent->AttachChild	(UITabControl);
	CUIXmlInit::InitTabControl		(uiXml, "tab", 0, UITabControl);
	UITabControl->SetMessageTarget	(this);

	for (u32 i = 0; i < UITabControl->GetTabsCount(); i++)
	{
		CUITabButton* btn = UITabControl->GetButtonByIndex(i);
		if (!btn || !btn->IsIdDefaultAssigned())
			continue;

		for (const LegacyTabIdEntry& entry : g_legacyTabIds)
		{
			if (btn->m_btn_id == entry.legacyId)
			{
				btn->m_btn_id = ResolveTabId(entry.tabId);
				break;
			}
		}
	}
	InitTabBackgrounds(uiXml);

	const auto tabPresentLambda = [this](const char* sectionId)
	{
		return UITabControl->GetButtonById(ResolveTabId(sectionId)) != nullptr;
	};

	if (tabPresentLambda(PdaSectionId::Tasks) && !pUITaskWnd)
	{
		pUITaskWnd = new CUITaskWnd();
		pUITaskWnd->hint_wnd = m_hint_wnd;
		pUITaskWnd->Init();
	}
	if (tabPresentLambda(PdaSectionId::Quests) && !pUIEventsWnd)
	{
		pUIEventsWnd = new CUIEventsWnd();
		pUIEventsWnd->Init();
	}
	if (tabPresentLambda(PdaSectionId::FractionWar) && !pUIFactionWarWnd)
	{
		pUIFactionWarWnd = new CUIFactionWarWnd();
		pUIFactionWarWnd->hint_wnd = m_hint_wnd;
		pUIFactionWarWnd->Init();
	}
	if (tabPresentLambda(PdaSectionId::Contacts) && !UIPdaContactsWnd)
	{
		UIPdaContactsWnd = new CUIPdaContactsWnd();
		UIPdaContactsWnd->Init();
	}
	if (tabPresentLambda(PdaSectionId::Ranking) && !pUIRankingWnd)
	{
		pUIRankingWnd = new CUIRankingWnd();
		pUIRankingWnd->Init();
	}
	if (tabPresentLambda(PdaSectionId::RankingGlobal) && !pUIStalkersRankingWnd)
	{
		pUIStalkersRankingWnd = new CUIStalkersRankingWnd();
		pUIStalkersRankingWnd->Init();
	}
	if (tabPresentLambda(PdaSectionId::Logs) && !pUILogsWnd)
	{
		pUILogsWnd = new CUILogsWnd();
		pUILogsWnd->Init();
	}
	if (tabPresentLambda(PdaSectionId::Encyclopedia) && !pUIEncyclopediaWnd)
	{
		pUIEncyclopediaWnd = new CUIEncyclopediaWnd();
		pUIEncyclopediaWnd->Init();
	}
	if (tabPresentLambda(PdaSectionId::ActorStatistic) && !pUIActorInfoWnd)
	{
		pUIActorInfoWnd = new CUIActorInfoWnd();
		pUIActorInfoWnd->Init();
	}
	if (tabPresentLambda(PdaSectionId::Diary) && !pUIDiaryWnd)
	{
		pUIDiaryWnd = new CUIDiaryWnd();
		pUIDiaryWnd->Init();
	}
	if (tabPresentLambda(PdaSectionId::Map) && !pUIMapWnd)
	{
		pUIMapWnd = new CUIMapWnd();
		pUIMapWnd->Init(PdaXml::Map, PdaConfig::MapSubdialogWindowName);
	}

	if (uiXml.NavigateToNode("noice_static"))
	{
		UINoice					= new CUIStatic();
		UINoice->SetAutoDelete	( true );
		CUIXmlInit::InitStatic	( uiXml, "noice_static", 0, UINoice );
	}

	if (uiXml.NavigateToNode("updated_section_static"))
	{
		m_updatedSectionImage = new CUIStatic();
		CUIXmlInit::InitStatic(uiXml, "updated_section_static", 0, m_updatedSectionImage);
	}

	if (uiXml.NavigateToNode("old_section_static"))
	{
		m_oldSectionImage = new CUIStatic();
		CUIXmlInit::InitStatic(uiXml, "old_section_static", 0, m_oldSectionImage);
	}

	const static bool shouldRearrangeButtons = EngineExternal()[EEngineExternalUI::PdaRearrangeTabButtons];
	if (shouldRearrangeButtons)
	{
		if (m_updatedSectionImage && m_oldSectionImage)
			RearrangeTabButtonsLegacy(UITabControl, m_sign_places_main);
		else
			RearrangeTabButtons		(UITabControl);
	}
}

void CUIPdaWnd::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
	switch ( msg )
	{
	case TAB_CHANGED:
		{
			if ( pWnd == UITabControl )
			{
				SetActiveSubdialog			(UITabControl->GetActiveId());
			}
			break;
		}
	case BUTTON_CLICKED:
		{
			if (m_btn_close && pWnd == m_btn_close )
			{
				HideDialog();
			}
			break;
		}
	default:
		{
		if (m_pActiveDialog)
			m_pActiveDialog->SendMessage	(pWnd, msg, pData);
		}
	};
}

void CUIPdaWnd::Show(bool status)
{
	inherited::Show						(status);
	if(status)
	{
		InventoryUtilities::SendInfoToActor	(PdaActorInfo::Show);
		
		if (m_sActiveSection == nullptr || strcmp(m_sActiveSection.c_str(), "") == 0)
		{
			if (UITabControl->GetButtonById(ResolveTabId(PdaSectionId::Tasks)))
			{
				SetActiveSubdialog(ResolveTabId(PdaSectionId::Tasks));
				UITabControl->SetActiveTab(ResolveTabId(PdaSectionId::Tasks));
			}
			else
			{
				SetActiveSubdialog(ResolveTabId(PdaSectionId::Quests));
				UITabControl->SetActiveTab(ResolveTabId(PdaSectionId::Quests));
			}
		}
		else
			SetActiveSubdialog(m_sActiveSection);
	}else
	{
		InventoryUtilities::SendInfoToActor	(PdaActorInfo::Hide);
		CurrentGameUI()->UIMainIngameWnd->SetFlashIconState_(CUIMainIngameWnd::efiPdaTask, false);
		if (m_pActiveDialog)
		{
			m_pActiveDialog->Show				(false);
			if (pUITaskWnd)
			{
				// HACK: Restore native task/journal container after script-owned PDA page is hidden.
				m_pActiveDialog = pUITaskWnd;
			}
			else
			{
				m_pActiveDialog = pUIEventsWnd;
			}
		}
		g_btnHint->Discard					();
		g_statHint->Discard					();
	}
}

void CUIPdaWnd::UpdateDateTime()
{
	const bool hasTimerTarget = UITimerBackground != nullptr;
	const bool hasCaptionTarget = m_caption && m_captionGameDateTime;
	if (!hasTimerTarget && !hasCaptionTarget)
	{
		return;
	}

	static shared_str prevDateTimeValue;
	const shared_str gameDateTime = BuildPdaGameDateTimeString();
	if (prevDateTimeValue.equal(gameDateTime))
	{
		return;
	}

	prevDateTimeValue = gameDateTime;
	if (hasTimerTarget)
	{
		UITimerBackground->UITitleText.SetText(gameDateTime.c_str());
	}
	if (hasCaptionTarget)
	{
		SetCaption(gameDateTime.c_str());
	}
}

void CUIPdaWnd::Update()
{
	inherited::Update();
	if (m_pActiveDialog)
		m_pActiveDialog->Update();
	if (m_clock)
		m_clock->SetText(InventoryUtilities::GetGameTimeAsString(InventoryUtilities::etpTimeToMinutes).c_str());
	UpdateDateTime();

	if (pUILogsWnd)
		Device.seqParallel.push_back(xr_make_delegate(pUILogsWnd, &CUILogsWnd::PerformWork));
}

void CUIPdaWnd::SetActiveSubdialog(const shared_str& section)
{
	const shared_str resolvedSection = ResolveKnownTabId(section);

	if ( m_pActiveDialog )
	{
		if (UIMainPdaFrame->IsChild(m_pActiveDialog))
		{
			UIMainPdaFrame->DetachChild( m_pActiveDialog );
		}
		m_pActiveDialog->Show( false );
	}

	pda_section::part updateSection = pda_section::quests;
	const bool hasUpdateSection = TryGetPdaUpdateSection(resolvedSection, updateSection);

	m_pActiveDialog = nullptr;
	if (PdaSectionId::Equals(resolvedSection, PdaSectionId::Tasks))
	{
		m_pActiveDialog = pUITaskWnd;
	}
	else if (PdaSectionId::Equals(resolvedSection, PdaSectionId::Quests))
	{
		m_pActiveDialog = pUIEventsWnd;
	}
	else if (PdaSectionId::Equals(resolvedSection, PdaSectionId::FractionWar))
	{
		m_pActiveDialog = pUIFactionWarWnd;
	}
	else if (PdaSectionId::Equals(resolvedSection, PdaSectionId::Contacts))
	{
		if (UIPdaContactsWnd)
		{
			m_pActiveDialog = UIPdaContactsWnd;
		}
		else
		{
			m_pActiveDialog = pUITaskWnd;
		}
	}
	else if (PdaSectionId::Equals(resolvedSection, PdaSectionId::Ranking))
	{
		if (IsGameTypeSingle())
		{
			m_pActiveDialog = pUIRankingWnd;
		}
	}
	else if (PdaSectionId::Equals(resolvedSection, PdaSectionId::RankingGlobal))
	{
		m_pActiveDialog = pUIStalkersRankingWnd;
	}
	else if (PdaSectionId::Equals(resolvedSection, PdaSectionId::Logs))
	{
		m_pActiveDialog = pUILogsWnd;
	}
	else if (PdaSectionId::Equals(resolvedSection, PdaSectionId::Encyclopedia))
	{
		m_pActiveDialog = pUIEncyclopediaWnd;
	}
	else if (PdaSectionId::Equals(resolvedSection, PdaSectionId::ActorStatistic))
	{
		m_pActiveDialog = pUIActorInfoWnd;
	}
	else if (PdaSectionId::Equals(resolvedSection, PdaSectionId::Diary))
	{
		m_pActiveDialog = pUIDiaryWnd;
	}
	else if (PdaSectionId::Equals(resolvedSection, PdaSectionId::Map))
	{
		if (pUIMapWnd)
		{
			m_pActiveDialog = pUIMapWnd;
		}
		else
		{
			m_pActiveDialog = pUITaskWnd;
		}
	}

	if (hasUpdateSection)
	{
		PdaState::Clear(updateSection);
	}

	if (m_isSetActiveSubdialog)
	{
		CUIDialogWndEx* ret = nullptr;
		PdaScriptBridge::TryCall(m_onSetActiveSubdialog, (const char*)resolvedSection.c_str(), ret);
		CUIWindow* pScriptWnd = ret ? smart_cast<CUIWindow*>(ret) : nullptr;
		if (pScriptWnd)
		{
			m_pActiveDialog = pScriptWnd;
		}

		if (m_pActiveDialog)
		{
			if (!UIMainPdaFrame->IsChild(m_pActiveDialog))
			{
				UIMainPdaFrame->AttachChild(m_pActiveDialog);
			}
			m_pActiveDialog->Show(true);
			m_sActiveSection = resolvedSection;
			SetActiveTabBackground(m_sActiveSection);
			SetActiveCaption();
		}
		else
		{
			m_sActiveSection = "";
			SetActiveTabBackground(m_sActiveSection);
		}
	}
	else
	{
		if (m_pActiveDialog && !UIMainPdaFrame->IsChild(m_pActiveDialog))
		{
			UIMainPdaFrame->AttachChild(m_pActiveDialog);
		}
		if (m_pActiveDialog)
		{
			m_pActiveDialog->Show(true);
		}

		if (UITabControl->GetActiveId() != section)
		{
			UITabControl->SetActiveTab(resolvedSection);
		}
		m_sActiveSection = resolvedSection;
		SetActiveTabBackground(m_sActiveSection);
		SetActiveCaption();
	}

}

void CUIPdaWnd::InitTabBackgrounds(CUIXml& xml)
{
	if (!m_pTabBgLayer || !UITabControl)
	{
		return;
	}

	const int tabsCount = UITabControl->GetTabsCount();
	for (int i = 0; i < tabsCount; ++i)
	{
		CUITabButton* tabButton = UITabControl->GetButtonByIndex(i);
		if (!tabButton)
		{
			continue;
		}

		const shared_str tabId = ResolveKnownTabId(tabButton->m_btn_id);
		string256 autoStaticPath;
		xr_sprintf(autoStaticPath, "tab_backgrounds:%s:auto_static", tabId.c_str());

		bool hasAutoStatic = xml.NavigateToNode(autoStaticPath, 0) != nullptr;
		if (!hasAutoStatic)
		{
			xr_sprintf(autoStaticPath, "pda:tab_backgrounds:%s:auto_static", tabId.c_str());
			hasAutoStatic = xml.NavigateToNode(autoStaticPath, 0) != nullptr;
		}

		if (!hasAutoStatic)
		{
			continue;
		}

		CUIStatic* tabBackground = new CUIStatic();
		tabBackground->SetAutoDelete(true);
		CUIXmlInit::InitStatic(xml, autoStaticPath, 0, tabBackground, false);
		tabBackground->Show(false);
		m_pTabBgLayer->AttachChild(tabBackground);
		m_tabBackgrounds[tabId] = tabBackground;
	}
}

void CUIPdaWnd::SetActiveTabBackground(const shared_str& sectionId)
{
	if (m_pCurrentTabBackground)
	{
		m_pCurrentTabBackground->Show(false);
	}

	m_pCurrentTabBackground = nullptr;

	const shared_str resolvedSection = ResolveKnownTabId(sectionId);
	const xr_map<shared_str, CUIStatic*>::const_iterator backgroundIt = m_tabBackgrounds.find(resolvedSection);
	if (backgroundIt != m_tabBackgrounds.end())
	{
		m_pCurrentTabBackground = backgroundIt->second;
		m_pCurrentTabBackground->Show(true);
	}
}

void CUIPdaWnd::SetActiveCaption()
{
	if (m_captionGameDateTime)
		return;

	TABS_VECTOR*	btn_vec		= UITabControl->GetButtonsVector();
	TABS_VECTOR::iterator it_b	= btn_vec->begin();
	TABS_VECTOR::iterator it_e	= btn_vec->end();
	for ( ; it_b != it_e; ++it_b )
	{
		if ( (*it_b)->m_btn_id == m_sActiveSection )
		{
			const char* cur = (*it_b)->TextItemControl()->GetText();
			string256 buf;
			xr_strconcat(buf, m_caption_const.c_str(), cur );
			SetCaption( buf );
			return;
		}
	}
}

void CUIPdaWnd::Show_SecondTaskWnd( bool status )
{
	if (!pUITaskWnd)
		return;

	if ( status )
	{
		SetActiveSubdialog(ResolveTabId(PdaSectionId::Tasks));
	}
	pUITaskWnd->Show_TaskListWnd( status );
}

void CUIPdaWnd::Show_MapLegendWnd( bool status )
{
	if (!pUITaskWnd)
		return;

	if ( status )
	{
		SetActiveSubdialog(ResolveTabId(PdaSectionId::Tasks));
	}
	pUITaskWnd->ShowMapLegend( status );
}

static u32 g_pdaRenderFrame = 0;

void CUIPdaWnd::Draw()
{
	if (g_pdaRenderFrame == Device.dwFrame)
	{
		return;
	}

	g_pdaRenderFrame = Device.dwFrame;

	inherited::Draw();
	DrawUpdatedSections();
	DrawHint();
	if (UINoice)
		UINoice->Draw(); // over all
}

void CUIPdaWnd::DrawHint()
{
	if (PdaSectionId::Equals(m_sActiveSection, PdaSectionId::Tasks))
	{
		pUITaskWnd->DrawHint();
	}
	else if (PdaSectionId::Equals(m_sActiveSection, PdaSectionId::Quests))
	{
		pUIEventsWnd->DrawHint();
	}
	else if (PdaSectionId::Equals(m_sActiveSection, PdaSectionId::Map))
	{
		pUIMapWnd->DrawHint();
	}
	else if (PdaSectionId::Equals(m_sActiveSection, PdaSectionId::FractionWar))
	{
		// m_hint_wnd->Draw();
	}
	else if (PdaSectionId::Equals(m_sActiveSection, PdaSectionId::Ranking))
	{
		pUIRankingWnd->DrawHint();
	}
	else if (PdaSectionId::Equals(m_sActiveSection, PdaSectionId::Logs))
	{
	}
	else if (PdaSectionId::Equals(m_sActiveSection, PdaSectionId::Contacts))
	{
		UIPdaContactsWnd->DrawHint();
	}
	else if (PdaSectionId::Equals(m_sActiveSection, PdaSectionId::RankingGlobal))
	{
		pUIStalkersRankingWnd->DrawHint();
	}
	if (m_hint_wnd)
	{
		m_hint_wnd->Draw();
	}
}

void CUIPdaWnd::UpdatePda()
{
	if (pUILogsWnd)
		pUILogsWnd->UpdateNews();

	if (PdaSectionId::Equals(m_sActiveSection, PdaSectionId::Tasks))
	{
		pUITaskWnd->ReloadTaskInfo();
	}
}

void CUIPdaWnd::UpdateRankingWnd()
{
	if (pUIRankingWnd)
		pUIRankingWnd->Update();
}

void CUIPdaWnd::PdaContentsChanged	(pda_section::part type)
{
	bool b = true;

	if (type == pda_section::encyclopedia && pUIEncyclopediaWnd)
	{
		pUIEncyclopediaWnd->ReloadArticles	();
	}
	else if (type == pda_section::news && pUIDiaryWnd)
	{
		pUIDiaryWnd->AddNews();
		pUIDiaryWnd->MarkNewsAsRead(pUIDiaryWnd->IsShown());
	}
	else if (type == pda_section::quests && pUIEventsWnd)
	{
		pUIEventsWnd->Reload				();
	}
	else if (type == pda_section::contacts)
	{
		if (UIPdaContactsWnd)
			UIPdaContactsWnd->Reload			();
		b = false;
	}
	else
	{
		b = false;
	}

	if(b)
	{
		PdaState::MarkUpdated(type);
		CurrentGameUI()->UIMainIngameWnd->SetFlashIconState_(CUIMainIngameWnd::efiPdaTask, true);
	}

}
void draw_sign		(CUIStatic* s, Fvector2& pos)
{
	s->SetWndPos		(pos);
	s->Draw				();
}

void CUIPdaWnd::DrawUpdatedSections				()
{
	if (!m_updatedSectionImage || !m_oldSectionImage)
		return;

	m_updatedSectionImage->Update				();
	m_oldSectionImage->Update					();
	
	Fvector2									tab_pos;
	UITabControl->GetAbsolutePos				(tab_pos);

	Fvector2 pos;

	if (m_sign_places_main.size() < 7)
		return;

	pos = m_sign_places_main[0];
	pos.add(tab_pos);
	if (PdaState::HasUpdates(pda_section::quests))
		draw_sign								(m_updatedSectionImage, pos);
	else
		draw_sign								(m_oldSectionImage, pos);

	pos = m_sign_places_main[1];
	pos.add(tab_pos);
	if (PdaState::HasUpdates(pda_section::map))
		draw_sign								(m_updatedSectionImage, pos);
	else
		draw_sign								(m_oldSectionImage, pos);

	pos = m_sign_places_main[2];
	pos.add(tab_pos);
	if (PdaState::HasUpdates(pda_section::diary))
		draw_sign								(m_updatedSectionImage, pos);
	else
		draw_sign								(m_oldSectionImage, pos);

	pos = m_sign_places_main[3];
	pos.add(tab_pos);
	if (PdaState::HasUpdates(pda_section::contacts))
		draw_sign								(m_updatedSectionImage, pos);
	else
		draw_sign								(m_oldSectionImage, pos);

	pos = m_sign_places_main[4];
	pos.add(tab_pos);
	if (PdaState::HasUpdates(pda_section::ranking))
		draw_sign								(m_updatedSectionImage, pos);
	else
		draw_sign								(m_oldSectionImage, pos);

	pos = m_sign_places_main[5];
	pos.add(tab_pos);
	if (PdaState::HasUpdates(pda_section::statistics))
		draw_sign								(m_updatedSectionImage, pos);
	else
		draw_sign								(m_oldSectionImage, pos);

	pos = m_sign_places_main[6];
	pos.add(tab_pos);
	if (PdaState::HasUpdates(pda_section::encyclopedia))
		draw_sign								(m_updatedSectionImage, pos);
	else
		draw_sign								(m_oldSectionImage, pos);
	
}

void CUIPdaWnd::Reset()
{
	inherited::ResetAll		();

	if ( pUIEventsWnd )		
		pUIEventsWnd->ResetAll();

	if ( pUITaskWnd )		
		pUITaskWnd->ResetAll();

	if ( pUIFactionWarWnd )	
		pUIFactionWarWnd->ResetAll();

	if ( UIPdaContactsWnd )	
		UIPdaContactsWnd->ResetAll();

	if ( pUIRankingWnd )	
		pUIRankingWnd->ResetAll();

	if ( pUIStalkersRankingWnd )	
		pUIStalkersRankingWnd->ResetAll();

	if ( pUILogsWnd )		
		pUILogsWnd->ResetAll();

	if ( pUIEncyclopediaWnd )		
		pUIEncyclopediaWnd->ResetAll();

	if ( pUIActorInfoWnd )	
		pUIActorInfoWnd->ResetAll();

	if ( pUIDiaryWnd )	
		pUIDiaryWnd->ResetAll();
	
	if ( pUIMapWnd )	
		pUIMapWnd->ResetAll();
}

void CUIPdaWnd::SetCaption( const char* text )
{
	if (m_caption)
		m_caption->TextItemControl()->SetText( text );
}

void RearrangeTabButtons(CUITabControl* pTab)
{
	TABS_VECTOR *	btn_vec		= pTab->GetButtonsVector();
	TABS_VECTOR::iterator it	= btn_vec->begin();
	TABS_VECTOR::iterator it_e	= btn_vec->end();

	Fvector2					pos;
	pos.set						((*it)->GetWndPos());
	float						size_x;

	for ( ; it != it_e; ++it )
	{
		(*it)->SetWndPos		(pos);
		(*it)->AdjustWidthToText();
		size_x					= (*it)->GetWndSize().x + 30.0f;
		(*it)->SetWidth			(size_x);
		pos.x					+= size_x - 6.0f;
	}
	
	pTab->SetWidth( pos.x + 5.0f );
	pos.x = pTab->GetWndPos().x - pos.x;
	pos.y = pTab->GetWndPos().y;
	pTab->SetWndPos( pos );
}

bool CUIPdaWnd::OnKeyboardAction(int dik, EUIMessages keyboard_action)
{
	if (is_binded(kACTIVE_JOBS, dik))
	{
		if (WINDOW_KEY_PRESSED == keyboard_action)
		{
			HideDialog();
		}

		return true;
	}

	return inherited::OnKeyboardAction(dik, keyboard_action);
}

bool CUIPdaWnd::OnGamepadKeyAction(int key, EUIMessages gamepad_action)
{
	if (WINDOW_KEY_PRESSED == gamepad_action)
	{
		switch (get_binded_action(key))
		{
			case kACTIVE_JOBS:
			{
				HideDialog();
				break;
			}
			return true;
		}
		switch (get_binded_action(key, agUIGeneral))
		{
			case kUI_BACK:
			{
				HideDialog();
				break;
			}
			case kUI_TAB_LEFT:
			{
				ActionRepeaters()->SetActionStarted(this, kUI_TAB_LEFT);
				UITabControl->PrevTab(true);
				break;
			}
			case kUI_TAB_RIGHT:
			{
				ActionRepeaters()->SetActionStarted(this, kUI_TAB_RIGHT);
				UITabControl->NextTab(true);
				break;
			}
			case kUI_TAB_SECONDARY_LEFT:
			{
				if (m_pActiveDialog == pUIDiaryWnd)
				{
					ActionRepeaters()->SetActionStarted(this, kUI_TAB_SECONDARY_LEFT);
					pUIDiaryWnd->m_FilterTab->PrevTab(true);
				}
				break;
			}
			case kUI_TAB_SECONDARY_RIGHT:
			{
				if (m_pActiveDialog == pUIDiaryWnd)
				{
					ActionRepeaters()->SetActionStarted(this, kUI_TAB_SECONDARY_RIGHT);
					pUIDiaryWnd->m_FilterTab->NextTab(true);
				}
				break;
			}
			return true;
		}
		switch (get_binded_action(key, agUITaskMenu))
		{
			case kPDA_TASKS_MAP_SHOW_ME:
			{
				if (m_pActiveDialog == pUIMapWnd)
				{
					pUIMapWnd->ViewActor();
					return true;
				}
				break;
			}
		}
	}

	return inherited::OnGamepadKeyAction(key, gamepad_action);
}

bool CUIPdaWnd::OnGamepadKeyHold(int key)
{
	switch (get_binded_action(key, agUIGeneral))
	{
		case kUI_TAB_LEFT:
		{
			if (ActionRepeaters()->CanRepeatActionNow(this, kUI_TAB_LEFT) && !any_binded_key_for_action_pressed_c(kUI_TAB_RIGHT))
			{
				UITabControl->PrevTab();
				return true;
			}
			break;
		}
		case kUI_TAB_RIGHT:
		{
			if (ActionRepeaters()->CanRepeatActionNow(this, kUI_TAB_RIGHT) && !any_binded_key_for_action_pressed_c(kUI_TAB_LEFT))
			{
				UITabControl->NextTab();
				return true;
			}
			break;
		}
		case kUI_TAB_SECONDARY_LEFT:
		{
			if (m_pActiveDialog == pUIDiaryWnd && ActionRepeaters()->CanRepeatActionNow(this, kUI_TAB_SECONDARY_LEFT) && !any_binded_key_for_action_pressed_c(kUI_TAB_SECONDARY_RIGHT))
			{
				pUIDiaryWnd->m_FilterTab->PrevTab();
				return true;
			}
			break;
		}
		case kUI_TAB_SECONDARY_RIGHT:
		{
			if (m_pActiveDialog == pUIDiaryWnd && ActionRepeaters()->CanRepeatActionNow(this, kUI_TAB_SECONDARY_LEFT) && !any_binded_key_for_action_pressed_c(kUI_TAB_SECONDARY_RIGHT))
			{
				pUIDiaryWnd->m_FilterTab->NextTab();
				return true;
			}
			break;
		}
	}

	return inherited::OnGamepadKeyHold(key);
}

//void CUIPdaWnd::Enable(bool status)
//{
//	if (status)
//		ResetCursor();
//	else
//	{
//		g_player_hud->reset_thumb(false);
//		ResetJoystick(false);
//		bButtonL = false;
//		bButtonR = false;
//	}
//
//	inherited::Enable(status);
//}

void CUIPdaWnd::HideDialog()
{
	if (!IsShown())
	{
		return;
	}

	CObject* current_entity = Level().CurrentControlEntity();

	CHudPdaAnimator* pda_animator = current_entity != nullptr ? current_entity->cast_actor()->HudAnimator()->PdaAnimator() : nullptr;

	if (pda_animator != nullptr
		&& pda_animator->GetState() != CHudStateAnimator::EAnimatorStates::eHidden
		&& pda_animator->GetState() != CHudStateAnimator::EAnimatorStates::eHiding)
	{
		pda_animator->SetState(CHudStateAnimator::EAnimatorStates::eHiding);
	}

	GetHolder()->StopDialog(this);
}

void RearrangeTabButtonsLegacy(CUITabControl* pTab, xr_vector<Fvector2>& vec_sign_places)
{
	TABS_VECTOR *	btn_vec		= pTab->GetButtonsVector();
	TABS_VECTOR::iterator it	= btn_vec->begin();
	TABS_VECTOR::iterator it_e	= btn_vec->end();
	vec_sign_places.clear		();
	vec_sign_places.resize		(btn_vec->size());

	Fvector2					pos;
	pos.set						((*it)->GetWndPos());
	Fvector2					sign_sz;
	sign_sz.set					(9.0f+3.0f, 11.0f);
	u32 idx						= 0;
	float	btn_text_len		= 0.0f;
	CUIStatic* st				= nullptr;

	for(;it!=it_e;++it,++idx)
	{
		if(idx!=0)
		{
			st = new CUIStatic(); st->SetAutoDelete(true);pTab->AttachChild(st);
			st->SetFont((*it)->GetFont());
			st->SetTextColor	(color_rgba(90,90,90,255));
			st->SetText("//");
			st->SetWndSize		((*it)->GetWndSize());
			st->AdjustWidthToText();
			st->SetWndPos		(pos);
			pos.x				+= st->GetWndSize().x;
		}

		vec_sign_places[idx].set(pos);
		vec_sign_places[idx].y	+= iFloor(((*it)->GetWndSize().y - sign_sz.y)/2.0f);
		vec_sign_places[idx].y	= (float)iFloor(vec_sign_places[idx].y);
		pos.x					+= sign_sz.x * UI().get_current_kx();

		(*it)->SetWndPos		(pos);
		(*it)->AdjustWidthToText();
		btn_text_len			= (*it)->GetWndSize().x;
		pos.x					+= btn_text_len+3.0f;
	}

}

bool CUIPdaWnd::StopAnyMove() 
{ 
	return pInput->GetControllerMode(); 
}

bool CUIPdaWnd::OnMouseAction(float x, float y, EUIMessages mouse_action)
{
	CObject* current_entity = Level().CurrentControlEntity();
	CHudPdaAnimator* pda_animator = current_entity != nullptr ? current_entity->cast_actor()->HudAnimator()->PdaAnimator() : nullptr;
	if (pda_animator != nullptr)
	{
		pda_animator->OnMouseAction(x, y, mouse_action);
	}
	CUIDialogWnd::OnMouseAction(x, y, mouse_action);
	return true; //always true because StopAnyMove() == false
}

void CUIPdaWnd::ResetCursor()
{
	if (!last_cursor_pos.similar({ 0.f, 0.f }))
	{
		GetUICursor().SetUICursorPosition(last_cursor_pos);
	}
}