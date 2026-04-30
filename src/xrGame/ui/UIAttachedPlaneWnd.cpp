#include "StdAfx.h"

#include "UIAttachedPlaneWnd.h"

#include "../player_hud.h"

void CUIAttachedPlaneWnd::SetupAttachOffset(LPCSTR section)
{
    Fvector mapAttachPos = pSettings->r_fvector3(section, "ui_p");
    Fvector mapAttachRot = pSettings->r_fvector3(section, "ui_r");
    SetAttachBoneName(READ_IF_EXISTS(pSettings, r_string, section, "ui_attach_bone", "cover"));

    mapAttachRot.mul(PI / 180.f);
    m_mapAttachOffset.setHPB(mapAttachRot.x, mapAttachRot.y, mapAttachRot.z);
    m_mapAttachOffset.translate_over(mapAttachPos);
}

void CUIAttachedPlaneWnd::SetAttachBoneName(LPCSTR boneName)
{
    if (boneName == nullptr || boneName[0] == 0)
    {
        m_attachBoneName = "cover";
        return;
    }

    m_attachBoneName = boneName;
}

bool CUIAttachedPlaneWnd::BuildAttachMatrix(attachable_hud_item* hudItem, Fmatrix& outMatrix) const
{
    if (hudItem == nullptr || hudItem->m_model == nullptr)
    {
        return false;
    }

    IKinematics* kin = hudItem->m_model;
    Fmatrix trans = hudItem->m_item_transform;
    u16 boneId = kin->LL_BoneID(GetAttachBoneName());
    if (boneId == BI_NONE)
    {
        boneId = kin->LL_BoneID("cover");
    }
    if (boneId == BI_NONE)
    {
        return false;
    }
    Fmatrix coverBone = kin->LL_GetTransform(boneId);
    outMatrix.mul(trans, coverBone);
    outMatrix.mulB_43(m_mapAttachOffset);
    return true;
}

