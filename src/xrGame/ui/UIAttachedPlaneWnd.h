#pragma once

#include "../../xrUI/Widgets/UIWindow.h"

struct attachable_hud_item;

class CUIAttachedPlaneWnd : public CUIWindow
{
    using inherited = CUIWindow;

protected:
    Fmatrix m_mapAttachOffset;
    shared_str m_attachBoneName = "cover";

    void SetupAttachOffset(LPCSTR section);
    void SetAttachBoneName(LPCSTR boneName);
    LPCSTR GetAttachBoneName() const { return m_attachBoneName.c_str(); }
    bool BuildAttachMatrix(attachable_hud_item* hudItem, Fmatrix& outMatrix) const;
};

