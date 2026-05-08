////////////////////////////////////////////////////////////////////////////
//	Module 		: UILogsWnd.cpp
//	Created 	: 25.04.2008
//	Author		: Evgeniy Sokolov
//	Description : UI Logs (PDA) window class implementation
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "UILogsWnd.h"

#include "../../xrUI/UIXmlInit.h"
#include "../../xrUI/Widgets/UIProgressBar.h"
#include "../../xrUI/Widgets/UIFrameLineWnd.h"
#include "../../xrUI/Widgets/UIFrameWindow.h"
#include "../../xrUI/Widgets/UIScrollBar.h"
#include "../../xrUI/Widgets/UIFixedScrollBar.h"
#include "../../xrUI/Widgets/UIScrollView.h"
#include "../../xrUI/Widgets/UICheckButton.h"
#include "../../xrUI/Widgets/UIStackPanel.h"
#include "../../xrUI/UIHelper.h"
#include "UICharacterInfo.h"
#include "UIInventoryUtilities.h"
#include "CUITimeLine.h"

#include "../Actor.h"
#include "../game_news.h"
#include "../alife_time_manager.h"
#include "../alife_registry_wrappers.h"
#include "../../xrEngine/string_table.h"
#include "UINewsItemWnd.h"
#include "../../xrEngine/xr_input.h"

#define PDA_LOGS_XML "pda_logs.xml"

extern ENGINE_API void split_time(u64 time, u32 &years, u32 &months, u32 &days, u32 &hours, u32 &minutes, u32 &seconds, u32 &milliseconds);

u64 const day2ms			= u64( 24 * 60 * 60 * 1000 );

namespace
{
bool IsLeapYear(u32 year)
{
    return (year % 400 == 0) || ((year % 4 == 0) && (year % 100 != 0));
}

u32 GetDaysInMonth(u32 year, u32 month)
{
    switch (month)
    {
        case 1:
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
            return 31;
        case 4:
        case 6:
        case 9:
        case 11:
            return 30;
        case 2:
            return IsLeapYear(year) ? 29 : 28;
        default:
            return 0;
    }
}

bool IsLogVisibleByFilter(const GAME_NEWS_DATA& newsData, bool filterNews, bool filterTalk)
{
    if (newsData.m_type == GAME_NEWS_DATA::eNews)
    {
        return filterNews;
    }

    if (newsData.m_type == GAME_NEWS_DATA::eTalk)
    {
        return filterTalk;
    }

    return false;
}
}

CUILogsWnd::CUILogsWnd()
{
	m_actor_ch_info			= nullptr;
	m_previous_time			= Device.dwTimeGlobal;
	m_selected_period		= 0;
    m_filter_news           = nullptr;
    m_filter_talk           = nullptr;
    m_date_caption          = nullptr;
    m_date                  = nullptr;
    m_period_caption        = nullptr;
    m_period                = nullptr;
    m_prev_period           = nullptr;
    m_next_period           = nullptr;
}

CUILogsWnd::~CUILogsWnd()
{
	m_list->Clear			();
	delete_data				(m_items_cache);
}


void CUILogsWnd::Show( bool status )
{
	m_ctrl_press = false;
	if ( status )
	{
		if (m_actor_ch_info)
			m_actor_ch_info->InitCharacter(Actor());
		m_selected_period = GetShiftPeriod( Level().GetGameTime(), 0 );
		m_need_reload = true;
        SyncTimelineState();
		Update();
	}
	inherited::Show( status );
}

void CUILogsWnd::Update()
{
	inherited::Update();
	if( m_need_reload )
		ReLoadNews();
	if (IsShown() && m_date && m_date_caption)
	{
		if (Device.dwTimeGlobal - m_previous_time > 1000)
		{
			m_previous_time = Device.dwTimeGlobal;
			m_date->SetText(InventoryUtilities::Get_GameTimeAndDate_AsString().c_str());

			m_date_caption->AdjustWidthToText();
			Fvector2 pos = m_date_caption->GetWndPos();
			pos.x = m_date->GetWndPos().x - m_date_caption->GetWidth() - 5.0f;
			m_date_caption->SetWndPos(pos);
		}
	}
	if(!m_items_ready.empty())
	{
		WINDOW_LIST::iterator it = m_items_ready.begin();
		WINDOW_LIST::iterator it_e = m_items_ready.end();
		for(; it!=it_e; ++it)
			m_list->AddWindow			(*it, true);
		
		m_items_ready.clear();
	}
}

void CUILogsWnd::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
	inherited::SendMessage( pWnd, msg, pData );
	CUIWndCallback::OnEvent( pWnd, msg, pData );
}

void CUILogsWnd::Init()
{
	m_uiXml.Load( CONFIG_PATH, UI_PATH, PDA_LOGS_XML );

	CUIXmlInit::InitWindow( m_uiXml, "main_wnd", 0, this );

	m_background = UIHelper::CreateFrameWindow(m_uiXml, "background", this, false);
	if (!m_background)
		m_background2 = UIHelper::CreateFrameLine(m_uiXml, "background", this, false);
	m_center_background = UIHelper::CreateFrameWindow(m_uiXml, "center_background", this, false);

	if (m_uiXml.NavigateToNode("actor_ch_info"))
	{
		m_actor_ch_info = new CUICharacterInfo();
		m_actor_ch_info->SetAutoDelete(true);
		AttachChild(m_actor_ch_info);
		m_actor_ch_info->InitCharacterInfo(&m_uiXml, "actor_ch_info");
	}

	if (!m_center_background && m_uiXml.NavigateToNode("center_background"))
		m_center_background2 = UIHelper::CreateStatic(m_uiXml, "center_background", this);

	m_center_caption	= UIHelper::CreateStatic( m_uiXml, "center_caption", this );

	string256 buf;
	xr_strcpy( buf, sizeof(buf), m_center_caption->GetText() );
	xr_strcat( buf, sizeof(buf), g_pStringTable->translate("ui_logs_center_caption").c_str() );
	m_center_caption->SetText( buf );

	CUIFixedScrollBar* tmp_scroll = new CUIFixedScrollBar();
	m_list = new CUIScrollView(tmp_scroll);
	m_list->SetAutoDelete( true );
	AttachChild( m_list );
	CUIXmlInit::InitScrollView( m_uiXml, "logs_list", 0, m_list);

	if (m_uiXml.NavigateToNode("filter_news"))
    {
        m_filter_news = UIHelper::CreateCheck(m_uiXml, "filter_news", this);
        if (m_filter_news)
        {
            m_filter_news->SetCheck(true);
        }
    }
    if (m_uiXml.NavigateToNode("filter_talk"))
    {
        m_filter_talk = UIHelper::CreateCheck(m_uiXml, "filter_talk", this);
        if (m_filter_talk)
        {
            m_filter_talk->SetCheck(true);
        }
    }

	if (m_uiXml.NavigateToNode("date_caption"))
		m_date_caption = UIHelper::CreateStatic(m_uiXml, "date_caption", this);

	if (m_uiXml.NavigateToNode("date"))
		m_date = UIHelper::CreateStatic(m_uiXml, "date", this);

	if (m_date || m_date_caption)
	{
		R_ASSERT3(m_date && m_date_caption,
			"Please, provide both [date] and [date_caption] tags in xml file", m_uiXml.m_xml_file_name);
	}

	if (m_uiXml.NavigateToNode("period_caption"))
    {
        m_period_caption = UIHelper::CreateStatic(m_uiXml, "period_caption", this);
    }
    if (m_uiXml.NavigateToNode("period"))
    {
        m_period = UIHelper::CreateStatic(m_uiXml, "period", this);
    }

    if (m_uiXml.NavigateToNode("btn_prev_period"))
    {
        m_prev_period = UIHelper::Create3tButton(m_uiXml, "btn_prev_period", this);
    }
    if (m_uiXml.NavigateToNode("btn_next_period"))
    {
        m_next_period = UIHelper::Create3tButton(m_uiXml, "btn_next_period", this);
    }

    if (m_uiXml.NavigateToNode("pda_timeline"))
    {
        m_timeline = new CUITimeLine();
        m_timeline->SetAutoDelete(true);
        if (m_timeline->InitFromXml(m_uiXml, "pda_timeline"))
        {
            m_timeline->SetOnNodeSelected(xr_delegate<void(u32)>(this, &CUILogsWnd::OnTimelineNodeSelected));
            AttachChild(m_timeline);
        }
        else
        {
            xr_delete(m_timeline);
            Msg("! [Timeline] pda_timeline init failed. Logs page keeps legacy behavior.");
        }
    }

	m_gamepad_legend = UIHelper::CreateGamepadLegend( m_uiXml, "gamepad_legend", this, false );

	if (m_filter_news)
    {
        Register(m_filter_news);
        AddCallback(m_filter_news, BUTTON_CLICKED, CUIWndCallback::void_function(this, &CUILogsWnd::UpdateChecks));
    }
    if (m_filter_talk)
    {
        Register(m_filter_talk);
        AddCallback(m_filter_talk, BUTTON_CLICKED, CUIWndCallback::void_function(this, &CUILogsWnd::UpdateChecks));
    }
    if (m_prev_period)
    {
        Register(m_prev_period);
        AddCallback(m_prev_period, BUTTON_CLICKED, CUIWndCallback::void_function(this, &CUILogsWnd::PrevPeriod));
    }
    if (m_next_period)
    {
        Register(m_next_period);
        AddCallback(m_next_period, BUTTON_CLICKED, CUIWndCallback::void_function(this, &CUILogsWnd::NextPeriod));
    }

	m_start_game_time = Level().GetStartGameTime();
	m_start_game_time = GetShiftPeriod( m_start_game_time, 0 );
    SyncTimelineState();
}
void itemToCache(CUIWindow* w)
{
	w->SetAutoDelete	(false);
	w->SetParent		(nullptr);
}

void CUILogsWnd::ReLoadNews() {
	m_news_in_queue.clear();
	CActor* pActor = Actor();

	if(pActor == nullptr) {
		m_need_reload = false;
		return;
	}

	const char* date_str = InventoryUtilities::GetDateAsString(m_selected_period, InventoryUtilities::edpDateToDay).c_str();
    if (m_period)
    {
        m_period->TextItemControl()->SetText(date_str);
    }
    if (m_period && m_period_caption && m_prev_period)
    {
        Fvector2 pos = m_period_caption->GetWndPos();
        pos.x = m_period->GetWndPos().x - m_period_caption->GetWidth() - m_prev_period->GetWidth() - 5.0f;
        m_period_caption->SetWndPos(pos);
    }

	ALife::_TIME_ID end_period = GetShiftPeriod(m_selected_period, 1);

	GAME_NEWS_VECTOR& news_vector = pActor->game_news_registry->registry().objects();

	bool filter_news = m_filter_news ? m_filter_news->GetCheck() : true;
	bool filter_talk = m_filter_talk ? m_filter_talk->GetCheck() : true;

	GAME_NEWS_VECTOR::iterator ib = news_vector.begin();
	GAME_NEWS_VECTOR::iterator ie = news_vector.end();
	for(u32 idx = 0; ib != ie; ++ib, ++idx) {
		bool add = false;
		GAME_NEWS_DATA& gn = (*ib);
		if(gn.m_type == GAME_NEWS_DATA::eNews && filter_news) {
			add = true;
		}
		else if(gn.m_type == GAME_NEWS_DATA::eTalk && filter_talk) {
			add = true;
		}
		if(gn.receive_time < m_selected_period || end_period < gn.receive_time) {
			add = false;
		}

		if(add) {
			m_news_in_queue.push_back(idx);
		}
	}
	m_need_reload = false;

	if(!m_list->Empty()) {
		xrCriticalSectionGuard guard(m_list->csUi);

		m_items_cache.insert(m_items_cache.end(), m_list->Items().begin(), m_list->Items().end());
		m_list->Items().clear();

		std::for_each(m_items_cache.begin(), m_items_cache.end(), itemToCache);
	}
	PerformWork();
}

void CUILogsWnd::PerformWork()
{
	if(!m_news_in_queue.empty())
	{
		u32 count = std::min(30u, (u32)m_news_in_queue.size());
		for(u32 i=0; i<count;++i)
		{
			GAME_NEWS_VECTOR& news_vector = Actor()->game_news_registry->registry().objects();
			u32 idx						= m_news_in_queue.back();
			m_news_in_queue.pop_back	();
			GAME_NEWS_DATA& gn			= news_vector[idx];
			
			AddNewsItem					( gn );
		}
	}
}

CUIWindow*	CUILogsWnd::CreateItem()
{
	CUINewsItemWnd* itm_res;
	itm_res = new CUINewsItemWnd();
	itm_res->Init(m_uiXml, "logs_item");
	return itm_res;
}

//void CUILogsWnd::ItemToCache(CUIWindow* w)
//{
//	CUINewsItemWnd* itm = smart_cast<CUINewsItemWnd*>(w);
//	VERIFY				(w);
//	m_items_cache.push_back(itm);
//}

CUIWindow* CUILogsWnd::ItemFromCache()
{
	CUIWindow* itm_res;
	if(m_items_cache.empty())
	{
		itm_res = CreateItem();
	}else
	{
		itm_res		= m_items_cache.back();
		m_items_cache.pop_back	();
	}
	return			itm_res;
}

void CUILogsWnd::AddNewsItem(GAME_NEWS_DATA& news_data)
{
	CUIWindow* news_itm_w		= ItemFromCache();
	CUINewsItemWnd*	news_itm	= smart_cast<CUINewsItemWnd*>(news_itm_w);
	news_itm->Setup				(news_data);
	
	m_items_ready.push_back		(news_itm);
}

void CUILogsWnd::UpdateChecks( CUIWindow* w, void* d )
{
	m_need_reload = true;
    SyncTimelineState();
}

void CUILogsWnd::PrevPeriod( CUIWindow* w, void* d )
{
	ALife::_TIME_ID	current_period = m_selected_period;
	m_selected_period = GetShiftPeriod( m_selected_period, -1 );
	if ( m_selected_period < m_start_game_time )
	{
		m_selected_period = m_start_game_time;
	}
	if(current_period != m_selected_period)
		m_need_reload = true;
    SyncTimelineState();
}

void CUILogsWnd::NextPeriod( CUIWindow* w, void* d )
{
	ALife::_TIME_ID	current_period = m_selected_period;
	m_selected_period = GetShiftPeriod( m_selected_period, 1 ); // +1
	ALife::_TIME_ID game_time = GetShiftPeriod( Level().GetGameTime(), 0 );
	if ( m_selected_period > game_time  )
	{
		m_selected_period = game_time;
	}
	if(current_period != m_selected_period)
		m_need_reload = true;
    SyncTimelineState();
}

ALife::_TIME_ID CUILogsWnd::GetShiftPeriod( ALife::_TIME_ID datetime, int shift_day )
{
	datetime -= (datetime % day2ms);
	datetime += (u64)shift_day * day2ms;
	return datetime;
}

void CUILogsWnd::OnTimelineNodeSelected(u32 day)
{
    if (!m_timeline || day == 0)
    {
        return;
    }

    u32 year = 0;
    u32 month = 0;
    u32 oldDay = 0;
    u32 hours = 0;
    u32 minutes = 0;
    u32 seconds = 0;
    u32 milliseconds = 0;
    split_time(m_selected_period, year, month, oldDay, hours, minutes, seconds, milliseconds);
    if (day > GetDaysInMonth(year, month))
    {
        return;
    }

    const ALife::_TIME_ID candidatePeriod = GetShiftPeriod(generate_time(year, month, day, 0, 0, 0, 0), 0);
    const ALife::_TIME_ID minPeriod = m_start_game_time;
    const ALife::_TIME_ID maxPeriod = GetShiftPeriod(Level().GetGameTime(), 0);

    ALife::_TIME_ID clampedPeriod = candidatePeriod;
    if (clampedPeriod < minPeriod)
    {
        clampedPeriod = minPeriod;
    }
    if (clampedPeriod > maxPeriod)
    {
        clampedPeriod = maxPeriod;
    }

    if (clampedPeriod != m_selected_period)
    {
        m_selected_period = clampedPeriod;
        m_need_reload = true;
    }

    SyncTimelineState();
}

void CUILogsWnd::SyncTimelineState()
{
    if (!m_timeline)
    {
        return;
    }

    u32 currentYear = 0;
    u32 currentMonth = 0;
    u32 currentDay = 0;
    u32 selectedYear = 0;
    u32 selectedMonth = 0;
    u32 selectedDay = 0;
    u32 hours = 0;
    u32 minutes = 0;
    u32 seconds = 0;
    u32 milliseconds = 0;
    split_time(
        Level().GetGameTime(),
        currentYear,
        currentMonth,
        currentDay,
        hours,
        minutes,
        seconds,
        milliseconds);
    split_time(
        m_selected_period,
        selectedYear,
        selectedMonth,
        selectedDay,
        hours,
        minutes,
        seconds,
        milliseconds);

    const u32 nodeCount = m_timeline->GetNodeCount();
    xr_vector<u8> hasMessagesByDay;
    hasMessagesByDay.resize(nodeCount + 1, 0);

    const bool filterNews = m_filter_news ? m_filter_news->GetCheck() : true;
    const bool filterTalk = m_filter_talk ? m_filter_talk->GetCheck() : true;
    CActor* actor = Actor();
    if (actor)
    {
        GAME_NEWS_VECTOR& newsVector = actor->game_news_registry->registry().objects();
        for (GAME_NEWS_VECTOR::iterator it = newsVector.begin(); it != newsVector.end(); ++it)
        {
            GAME_NEWS_DATA& newsData = (*it);
            if (!IsLogVisibleByFilter(newsData, filterNews, filterTalk))
            {
                continue;
            }

            u32 newsYear = 0;
            u32 newsMonth = 0;
            u32 newsDay = 0;
            split_time(
                newsData.receive_time,
                newsYear,
                newsMonth,
                newsDay,
                hours,
                minutes,
                seconds,
                milliseconds);
            if (newsYear == selectedYear &&
                newsMonth == selectedMonth &&
                newsDay > 0 &&
                newsDay <= nodeCount)
            {
                hasMessagesByDay[newsDay] = 1;
            }
        }
    }

    const u32 daysInMonth = GetDaysInMonth(selectedYear, selectedMonth);
    const ALife::_TIME_ID currentPeriod = GetShiftPeriod(Level().GetGameTime(), 0);

    for (u32 day = 1; day <= nodeCount; ++day)
    {
        bool hasMessages = false;
        ETimelineState state = ETimelineState::Future;

        if (day <= daysInMonth)
        {
            const ALife::_TIME_ID dayPeriod = GetShiftPeriod(
                generate_time(selectedYear, selectedMonth, day, 0, 0, 0, 0),
                0);
            if (dayPeriod >= m_start_game_time && dayPeriod <= currentPeriod)
            {
                hasMessages = hasMessagesByDay[day] != 0;
                state = ETimelineState::Empty;
                if (selectedYear == currentYear &&
                    selectedMonth == currentMonth &&
                    day == currentDay)
                {
                    state = ETimelineState::Present;
                }
                else if (hasMessages)
                {
                    state = ETimelineState::Archive;
                }
            }
        }

        m_timeline->SetNodeState(day, state, hasMessages);
    }

    if (selectedDay <= nodeCount)
    {
        m_timeline->SetSelectedDay(selectedDay);
    }
}

bool CUILogsWnd::OnKeyboardAction( int dik, EUIMessages keyboard_action )
{
	if ( keyboard_action == WINDOW_KEY_PRESSED )
	{
		switch ( dik )
		{
		case SDL_SCANCODE_UP:
		case SDL_SCANCODE_DOWN:
		case SDL_SCANCODE_PAGEUP:
		case SDL_SCANCODE_PAGEDOWN:
			{
				on_scroll_keys( dik );
				return true;
			}break;
		case SDL_SCANCODE_RCTRL:
		case SDL_SCANCODE_LCTRL:
			{
				m_ctrl_press = true;
				return true;
			}break;		
		}
	}
	m_ctrl_press = false;
	return inherited::OnKeyboardAction( dik, keyboard_action );
}

bool CUILogsWnd::OnKeyboardHold( int dik )
{
	switch ( dik )
	{
	case SDL_SCANCODE_UP:
	case SDL_SCANCODE_DOWN:
	case SDL_SCANCODE_PAGEUP:
	case SDL_SCANCODE_PAGEDOWN:
		{
			on_scroll_keys( dik );
			return true;
		}break;
	}
	return inherited::OnKeyboardHold( dik );
}

bool CUILogsWnd::OnGamepadKeyAction(int key, EUIMessages gamepad_action)
{
	if (WINDOW_KEY_PRESSED == gamepad_action)
	{
		switch (get_binded_action(key, agUILogMenu))
		{
			case kPDA_LOG_TO_START:
			{
				m_list->ScrollToBegin();
				break;
			}
			case kPDA_LOG_TO_END:
			{
				m_list->ScrollToEnd();
				break;
			}
			case kPDA_LOG_DATE_PREV:
			{
                if (m_prev_period)
                {
                    m_prev_period->OnClick();
                }
				break;
			}
			case kPDA_LOG_DATE_NEXT:
			{
                if (m_next_period)
                {
                    m_next_period->OnClick();
                }
				break;
			}
			case kPDA_LOG_SHOW_DIALOGS:
			{
                if (m_filter_talk)
                {
                    m_filter_talk->SetCheck(!m_filter_talk->GetCheck());
                    m_filter_talk->SendClickCallback();
                }
				break;
			}
			case kPDA_LOG_SHOW_NEWS:
			{
                if (m_filter_news)
                {
                    m_filter_news->SetCheck(!m_filter_news->GetCheck());
                    m_filter_news->SendClickCallback();
                }
				break;
			}
			case kPDA_LOG_SCROLL_UP:
			{
				on_scroll_keys(SDL_SCANCODE_UP, 64);
				break;
			}
			case kPDA_LOG_SCROLL_DOWN:
			{
				on_scroll_keys(SDL_SCANCODE_DOWN, 64);
				break;
			}
			return true;
		}
	}

	return inherited::OnGamepadKeyAction(key, gamepad_action);
}

bool CUILogsWnd::OnGamepadKeyHold(int key)
{
	switch (get_binded_action(key, agUILogMenu))
	{
		case kPDA_LOG_SCROLL_UP:
		{
			on_scroll_keys(SDL_SCANCODE_UP, 64);
			break;
		}
		case kPDA_LOG_SCROLL_DOWN:
		{
			on_scroll_keys(SDL_SCANCODE_DOWN, 64);
			break;
		}
		return true;
	}

	return inherited::OnGamepadKeyHold(key);
}

void CUILogsWnd::on_scroll_keys( int dik, int step )
{
	VERIFY( m_list && m_list->ScrollBar() );

	switch ( dik )
	{
	case SDL_SCANCODE_UP:
		{
			int orig = m_list->ScrollBar()->GetStepSize();
			m_list->ScrollBar()->SetStepSize( step );
			m_list->ScrollBar()->TryScrollDec();
			m_list->ScrollBar()->SetStepSize( orig );
			break;
		}	
	case SDL_SCANCODE_DOWN:
		{
			int orig = m_list->ScrollBar()->GetStepSize();
			m_list->ScrollBar()->SetStepSize( step );
			m_list->ScrollBar()->TryScrollInc();
			m_list->ScrollBar()->SetStepSize( orig );
			break;
		}
	case SDL_SCANCODE_PAGEUP:
		{
			if ( m_ctrl_press )
			{
				m_list->ScrollToBegin();
				break;
			}
			m_list->ScrollBar()->TryScrollDec();
			break;
		}
	case SDL_SCANCODE_PAGEDOWN:
		{
			if ( m_ctrl_press )
			{
				m_list->ScrollToEnd();
				break;
			}
			m_list->ScrollBar()->TryScrollInc();
			break;
		}		
	}// switch

}