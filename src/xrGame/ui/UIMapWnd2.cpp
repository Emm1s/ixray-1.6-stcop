#include "StdAfx.h"
#include "UIMapWnd.h"
#include "UIMap.h"
#include "../../xrUI/UIXmlInit.h"
#include "../../xrEngine/xr_input.h"
#include "../../xrUI/Widgets/UI3tButton.h"
#include "../../xrUI/UIHelper.h"
#include "UITaskWnd.h"
#include "PdaConstants.h"

namespace
{
bool WindowNameEquals(const char* windowName, const char* candidate)
{
	return windowName && candidate && xr_strcmp(windowName, candidate) == 0;
}
} // namespace

void CUIMapWnd::RegisterNavButtonByName(CUI3tButton* btn)
{
	if (!btn)
	{
		return;
	}

	const shared_str windowName = btn->WindowName();
	const char* n = windowName.c_str();
	if (!n || !n[0])
	{
		return;
	}

	if (WindowNameEquals(n, PdaNavButton::Legend))
	{
		AddCallback(btn, BUTTON_DOWN, CUIWndCallback::void_function(this, &CUIMapWnd::OnBtnLegend_Push));
	}
	else if (WindowNameEquals(n, PdaNavButton::ZoomIn))
	{
		AddCallback(btn, BUTTON_DOWN, CUIWndCallback::void_function(this, &CUIMapWnd::OnBtnZoomMore_Push));
	}
	else if (WindowNameEquals(n, PdaNavButton::Center) || WindowNameEquals(n, "btn_nav_actor"))
	{
		AddCallback(btn, BUTTON_DOWN, CUIWndCallback::void_function(this, &CUIMapWnd::OnBtnActor_Push));
	}
	else if (WindowNameEquals(n, PdaNavButton::ZoomOut))
	{
		AddCallback(btn, BUTTON_DOWN, CUIWndCallback::void_function(this, &CUIMapWnd::OnBtnZoomLess_Push));
	}
	else if (WindowNameEquals(n, PdaNavButton::ZoomReset))
	{
		AddCallback(btn, BUTTON_DOWN, CUIWndCallback::void_function(this, &CUIMapWnd::OnBtnZoomReset_Push));
	}
	else if (WindowNameEquals(n, PdaNavButton::PersonalSpot))
	{
		AddCallback(btn, BUTTON_DOWN, CUIWndCallback::void_function(this, &CUIMapWnd::OnBtnPersonalSpot_Push));
	}
	else if (WindowNameEquals(n, "global_map_btn"))
	{
		AddCallback(btn, BUTTON_DOWN, CUIWndCallback::void_function(this, &CUIMapWnd::OnBtnZoomReset_Push));
	}
	else if (WindowNameEquals(n, "actor_btn"))
	{
		AddCallback(btn, BUTTON_DOWN, CUIWndCallback::void_function(this, &CUIMapWnd::OnBtnActor_Push));
	}
	else if (WindowNameEquals(n, "zoom_in_btn"))
	{
		AddCallback(btn, BUTTON_DOWN, CUIWndCallback::void_function(this, &CUIMapWnd::OnBtnZoomMore_Push));
	}
	else if (WindowNameEquals(n, "zoom_out_btn"))
	{
		AddCallback(btn, BUTTON_DOWN, CUIWndCallback::void_function(this, &CUIMapWnd::OnBtnZoomLess_Push));
	}
}

void CUIMapWnd::init_xml_nav(CUIXml& xml, const char* start_from)
{
	if (xml.NavigateToNode("btn_nav_parent"))
	{
		m_btn_nav_parent = UIHelper::CreateStatic(xml, "btn_nav_parent", this);

		VERIFY(hint_wnd);

		string64 buf;
		for (u8 i = 0; i < max_btn_nav; ++i)
		{
			xr_sprintf(buf, "btn_nav_parent:btn_nav_%d", i);

			if (!xml.NavigateToNode(buf))
			{
				m_btn_nav[i] = nullptr;
				continue;
			}

			m_btn_nav[i] = UIHelper::Create3tButton(xml, buf, m_btn_nav_parent);
			Register(m_btn_nav[i]);
			RegisterNavButtonByName(m_btn_nav[i]);
		}
	}
	else
	{
		string512 pth;
		xr_strconcat(pth, start_from, ":main_wnd:map_header_frame_line:tool_bar");

		string512 temp;
		m_btn_nav[btn_zoom_reset] = UIHelper::Create3tButton(xml, xr_strconcat(temp, pth, ":global_map_btn"), UIMainMapHeader);
		Register(m_btn_nav[btn_zoom_reset]);
		m_btn_nav[btn_actor] = UIHelper::Create3tButton(xml, xr_strconcat(temp, pth, ":actor_btn"), UIMainMapHeader);
		Register(m_btn_nav[btn_actor]);
		m_btn_nav[btn_zoom_more] = UIHelper::Create3tButton(xml, xr_strconcat(temp, pth, ":zoom_in_btn"), UIMainMapHeader);
		Register(m_btn_nav[btn_zoom_more]);
		m_btn_nav[btn_zoom_less] = UIHelper::Create3tButton(xml, xr_strconcat(temp, pth, ":zoom_out_btn"), UIMainMapHeader);
		Register(m_btn_nav[btn_zoom_less]);

		RegisterNavButtonByName(m_btn_nav[btn_zoom_reset]);
		RegisterNavButtonByName(m_btn_nav[btn_actor]);
		RegisterNavButtonByName(m_btn_nav[btn_zoom_more]);
		RegisterNavButtonByName(m_btn_nav[btn_zoom_less]);
	}
}

void CUIMapWnd::UpdateNav()
{
	if (m_btn_nav_parent)
	{
		m_btn_nav_parent->Show(!pInput->GetControllerMode());
	}
	if (Device.dwTimeGlobal - m_nav_timing < 10)
	{
		return;
	}
	m_nav_timing = Device.dwTimeGlobal;

	if (m_btn_nav[btn_up] && m_btn_nav[btn_up]->CursorOverWindow() && m_btn_nav[btn_up]->GetButtonState() == CUIButton::BUTTON_PUSHED)
	{
		MoveMap(Fvector2().set(0.0f, m_map_move_step));
	}
	else if (m_btn_nav[btn_left] && m_btn_nav[btn_left]->CursorOverWindow() && m_btn_nav[btn_left]->GetButtonState() == CUIButton::BUTTON_PUSHED)
	{
		MoveMap(Fvector2().set(m_map_move_step, 0.0f));
	}
	else if (m_btn_nav[btn_right] && m_btn_nav[btn_right]->CursorOverWindow() && m_btn_nav[btn_right]->GetButtonState() == CUIButton::BUTTON_PUSHED)
	{
		MoveMap(Fvector2().set(-m_map_move_step, 0.0f));
	}
	else if (m_btn_nav[btn_down] && m_btn_nav[btn_down]->CursorOverWindow() && m_btn_nav[btn_down]->GetButtonState() == CUIButton::BUTTON_PUSHED)
	{
		MoveMap(Fvector2().set(0.0f, -m_map_move_step));
	}
}

void CUIMapWnd::OnBtnLegend_Push(CUIWindow*, void*)
{
	CUITaskWnd* parent_wnd = smart_cast<CUITaskWnd*>(m_pParentWnd);
	if (parent_wnd)
	{
		parent_wnd->Switch_ShowMapLegend();
	}
}

void CUIMapWnd::OnBtnZoomMore_Push(CUIWindow*, void*)
{
	ViewZoomIn();
}

void CUIMapWnd::OnBtnActor_Push(CUIWindow*, void*)
{
	ViewActor();
}

void CUIMapWnd::OnBtnZoomLess_Push(CUIWindow*, void*)
{
	ViewZoomOut();
}

void CUIMapWnd::OnBtnZoomReset_Push(CUIWindow*, void*)
{
	ViewGlobalMap();
}

void CUIMapWnd::OnBtnPersonalSpot_Push(CUIWindow*, void*)
{
	SetPersonalSpotPlacement(!m_personalSpotPlacement);
}
