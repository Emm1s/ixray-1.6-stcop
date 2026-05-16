////////////////////////////////////////////////////////////////////////////
//	Module 		: UILogsWnd.h
//	Created 	: 25.04.2008
//	Author		: Evgeniy Sokolov
//	Description : UI Logs (PDA) window class
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../../xrUI/Widgets/UIWindow.h"
#include "../../xrUI/Widgets/UIWndCallback.h"

#include "../ai_space.h"
#include "../../xrEngine/AI/alife_space.h"
#include "../../xrUI/xrUIXmlParser.h"

class CUIStatic;
class CUIXml;
class CUIProgressBar;
class CUIFrameLineWnd;
class CUIFrameWindow;
class CUICharacterInfo;
class CUIScrollView;
class CUI3tButton;
class CUICheckButton;
struct GAME_NEWS_DATA;
class CUINewsItemWnd;
class CUIGamepadLegend;
class CUICalendar;

class CUILogsWnd final : public CUIWindow, public CUIWndCallback
{
private:
	using inherited = CUIWindow;

	CUIFrameWindow*		m_background;
	CUIFrameLineWnd*	m_background2;
	CUIFrameWindow*		m_center_background;
	CUIStatic*			m_center_background2;

	CUIStatic*			m_center_caption;
	CUICharacterInfo*	m_actor_ch_info;

	CUICheckButton*		m_filter_news = nullptr;
	CUICheckButton*		m_filter_talk = nullptr;

	CUIStatic*			m_date_caption;
	CUIStatic*			m_date;

	CUIStatic*			m_period_caption = nullptr;
	CUIStatic*			m_period = nullptr;

	ALife::_TIME_ID		m_start_game_time;
	ALife::_TIME_ID		m_selected_period;

	CUI3tButton*		m_prev_period = nullptr;
	CUI3tButton*		m_next_period = nullptr;
	CUI3tButton*		m_btn_calendar = nullptr;
	CUICalendar*		m_calendar = nullptr;
	bool				m_ctrl_press;

	CUIScrollView*		m_list;
	u32					m_previous_time;
	bool				m_need_reload;
	WINDOW_LIST			m_items_cache;
	WINDOW_LIST			m_items_ready;
	xr_vector<u32>		m_news_in_queue;

	CUIWindow*			CreateItem			();
	CUIWindow*			ItemFromCache		();
//	void				ItemToCache			(CUIWindow* w);
	CUIXml				m_uiXml;
	CUIGamepadLegend*	m_gamepad_legend = nullptr;

public:
						CUILogsWnd			();
						~CUILogsWnd			() override;

			void		Init				();

	void 				Show				( bool status ) override;
	void				Update				() override;
	void				SendMessage			( CUIWindow* pWnd, s16 msg, void* pData ) override;

	bool				OnKeyboardAction	(int dik, EUIMessages keyboard_action) override;
	bool				OnKeyboardHold		(int dik) override;
	bool				OnGamepadKeyAction	(int key, EUIMessages gamepad_action) override;
	bool				OnGamepadKeyHold	(int key) override;

	IC		void		UpdateNews			()
	{
		m_need_reload = true;
		SyncCalendarState();
	}
	void		PerformWork			();

	CUIWindow* ui_cast_window() override { return this; }

protected:
			void		ReLoadNews			();
			void		AddNewsItem			( GAME_NEWS_DATA& news_data );
	ALife::_TIME_ID		GetShiftPeriod		( ALife::_TIME_ID datetime, int shift_day );
			void		SyncCalendarState	();
			void		OnCalendarDaySelected( ALife::_TIME_ID period );

			void 	UpdateChecks	( CUIWindow* w, void* d);
			void 	PrevPeriod		( CUIWindow* w, void* d);
			void 	NextPeriod		( CUIWindow* w, void* d);
			void	ToggleCalendarPopup	( CUIWindow* w, void* d );

			void 		on_scroll_keys		( int dik, int step = 1 );

/*
protected:
	void		add_faction			( CUIXml& xml, shared_str const& faction_id );
	void		clear_all_factions		();
	bool		SortingLessFunction		( CUIWindow* left, CUIWindow* right );
*/
}; // class CUILogsWnd
