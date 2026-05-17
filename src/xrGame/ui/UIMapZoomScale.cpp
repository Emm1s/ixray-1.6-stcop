#include "StdAfx.h"
#include "UIMapZoomScale.h"

#include "../../xrUI/xrUIXmlParser.h"
#include "../../xrUI/UIXmlInit.h"
#include "../../xrUI/UIHelper.h"
#include "../../xrUI/Widgets/UIStatic.h"

#include "../../xrEngine/Device.h"
#include "../../xrEngine/string_table.h"

namespace
{
float Log2Positive(float value)
{
    if (value <= 0.f)
    {
        return 0.f;
    }

    return logf(value) / logf(2.f);
}

float ClampRatio(float ratio, float maxRatio)
{
    if (maxRatio <= 1.f)
    {
        return 1.f;
    }

    return clampr(ratio, 1.f, maxRatio);
}

float MinFloat(float a, float b)
{
    return a < b ? a : b;
}

bool InitTickLabelTemplate(
    CUIXml& xml,
    const char* node,
    CUIStatic*& outTemplate,
    Fvector2& outOffset,
    Fvector2& outSize)
{
    if (!xml.NavigateToNode(node, 0))
    {
        return false;
    }

    outTemplate = new CUIStatic();
    CUIXmlInit::InitStatic(xml, node, 0, outTemplate);
    outOffset.x = xml.ReadAttribFlt(node, 0, "x", 0.f);
    outOffset.y = xml.ReadAttribFlt(node, 0, "y", 0.f);
    outSize.x = outTemplate->GetWidth();
    outSize.y = outTemplate->GetHeight();
    return true;
}

u8 ParseLcrAlignStr(const char* alignStr)
{
    if (alignStr == nullptr || alignStr[0] == 0)
    {
        return 0;
    }

    if (_stricmp(alignStr, "c") == 0 || _stricmp(alignStr, "center") == 0)
    {
        return 1;
    }
    if (_stricmp(alignStr, "r") == 0 || _stricmp(alignStr, "right") == 0)
    {
        return 2;
    }
    return 0;
}

const char* ResolveThumbXmlPath(CUIXml& xml)
{
    if (xml.NavigateToNode("rail:thumb", 0))
    {
        return "rail:thumb";
    }
    if (xml.NavigateToNode("thumb", 0))
    {
        return "thumb";
    }
    return nullptr;
}

CUIStatic* CreateZoomScaleThumb(CUIXml& xml, CUIWindow* railParent)
{
    if (const char* thumbPath = ResolveThumbXmlPath(xml))
    {
        return UIHelper::CreateStatic(xml, thumbPath, railParent, false);
    }
    return nullptr;
}

u8 ParseThumbCrossAlign(CUIXml& xml, const char* zoomScalePath)
{
    shared_str alignStr;

    if (const char* thumbPath = ResolveThumbXmlPath(xml))
    {
        alignStr = xml.ReadAttrib(thumbPath, 0, "thumb_align", nullptr);
        if (!alignStr.size())
        {
            alignStr = xml.ReadAttrib(thumbPath, 0, "align", nullptr);
        }
    }

    if (!alignStr.size())
    {
        alignStr = xml.ReadAttrib(zoomScalePath, 0, "thumb_align", nullptr);
    }
    if (!alignStr.size())
    {
        alignStr = xml.ReadAttrib(zoomScalePath, 0, "align", "l");
    }

    return ParseLcrAlignStr(alignStr.c_str());
}

u8 ParseRailCrossAlign(CUIXml& xml)
{
    shared_str alignStr = xml.ReadAttrib("rail", 0, "rail_align", nullptr);
    if (!alignStr.size())
    {
        alignStr = xml.ReadAttrib("rail", 0, "align", "l");
    }
    return ParseLcrAlignStr(alignStr.c_str());
}
} // namespace

UIMapZoomScale::~UIMapZoomScale()
{
    xr_delete(_tickLabelTemplate);
    xr_delete(_tickLabelBoundTemplate);
    xr_delete(_tickLabelValueTemplate);
    xr_delete(_minLabel);
    xr_delete(_maxLabel);
    xr_delete(_valueLabel);
}

void UIMapZoomScale::ReadConfigAttribs(CUIXml& xml, const char* path)
{
    _inertion = xml.ReadAttribFlt(path, 0, "inertion", 0.85f);
    _smoothingScale = xml.ReadAttribFlt(path, 0, "smoothing_scale", 20.f);
    xr_strcpy(_labelFormat, xml.ReadAttrib(path, 0, "label_format", "x%.1f"));
    _boundLabelsMinMax = (xml.ReadAttribInt(path, 0, "bound_labels", 0) != 0);
    _boundLabelMinId = xml.ReadAttrib(path, 0, "bound_label_min", "ui_map_zoom_min");
    _boundLabelMaxId = xml.ReadAttrib(path, 0, "bound_label_max", "ui_map_zoom_max");
    // 0: vertical rail (min zoom at bottom). 1: horizontal (min zoom at left).
    _isHorizontal = (xml.ReadAttribInt(path, 0, "horizontal", 0) != 0);
    _thumbScaleWithZoom = (xml.ReadAttribInt(path, 0, "thumb_scaling", 0) != 0);
}

void UIMapZoomScale::InitRailAndThumb(CUIXml& xml, const char* path)
{
    _thumbCrossAlign = (EZoomCrossAlign)ParseThumbCrossAlign(xml, path);

    _rail = UIHelper::CreateStatic(xml, "rail", this, false);
    if (_rail)
    {
        _rail->Enable(false);
        _railCrossAlign = (EZoomCrossAlign)ParseRailCrossAlign(xml);
        ApplyRailAlignInParent();
    }

    if (!_rail)
    {
        return;
    }

    _thumb = CreateZoomScaleThumb(xml, _rail);
    if (!_thumb)
    {
        return;
    }

    _thumb->Enable(false);
    _thumb->Show(true);
    _thumbBaseSize.set(_thumb->GetWidth(), _thumb->GetHeight());
    InitThumbOffsetFromXml(xml);
}

void UIMapZoomScale::InitLabelTemplates(CUIXml& xml)
{
    const bool hasBoundTemplate = InitTickLabelTemplate(
        xml, "tick_label_bound", _tickLabelBoundTemplate, _boundLabelOffset, _boundLabelSize);
    const bool hasValueTemplate = InitTickLabelTemplate(
        xml, "tick_label_value", _tickLabelValueTemplate, _valueLabelOffset, _valueLabelSize);

    if (!xml.NavigateToNode("tick_label", 0))
    {
        return;
    }

    _tickLabelTemplate = new CUIStatic();
    CUIXmlInit::InitStatic(xml, "tick_label", 0, _tickLabelTemplate);
    const Fvector2 legacyOffset = {
        xml.ReadAttribFlt("tick_label", 0, "x", 0.f),
        xml.ReadAttribFlt("tick_label", 0, "y", 0.f)};
    const Fvector2 legacySize = {_tickLabelTemplate->GetWidth(), _tickLabelTemplate->GetHeight()};

    if (!hasBoundTemplate)
    {
        _boundLabelOffset = legacyOffset;
        _boundLabelSize = legacySize;
    }

    if (!hasValueTemplate)
    {
        _valueLabelOffset = legacyOffset;
        _valueLabelSize = legacySize;
    }
}

void UIMapZoomScale::InitFromXml(CUIXml& xml, const char* path)
{
    CUIXmlInit::InitWindow(xml, path, 0, this);
    ReadConfigAttribs(xml, path);

    XML_NODE* storedRoot = xml.GetLocalRoot();
    XML_NODE* nodeRoot = xml.NavigateToNode(path, 0);
    xml.SetLocalRoot(nodeRoot);

    InitRailAndThumb(xml, path);
    InitLabelTemplates(xml);

    xml.SetLocalRoot(storedRoot);

    _isInitialized = (_rail != nullptr && _thumb != nullptr);
    _displayRatio = 1.f;
    _targetRatio = 1.f;

    if (_isInitialized)
    {
        EnsureLabels();
        if (_thumbScaleWithZoom)
        {
            UpdateThumbFill();
        }
    }
}

void UIMapZoomScale::SyncFromMap(float minZoom, float maxZoom, float currentZoom)
{
    if (!_isInitialized)
    {
        return;
    }

    const bool boundsChanged = !fsimilar(_minZoom, minZoom, EPS_L) || !fsimilar(_maxZoom, maxZoom, EPS_L);
    _minZoom = minZoom;
    _maxZoom = maxZoom;

    const float maxRatio = GetMaxRatio();
    const float targetRatio = fis_zero(_minZoom, EPS_L) ? 1.f : ClampRatio(currentZoom / _minZoom, maxRatio);

    if (boundsChanged)
    {
        EnsureLabels();
        UpdateBoundLabels();
        _displayRatio = targetRatio;
    }

    _targetRatio = targetRatio;

    if (_thumbScaleWithZoom)
    {
        UpdateThumbFill();
    }
}

void UIMapZoomScale::Update()
{
    if (!_isInitialized)
    {
        inherited::Update();
        return;
    }

    if (!fsimilar(_displayRatio, _targetRatio, EPS_L))
    {
        const float diff = _targetRatio - _displayRatio;
        const float step = diff * (1.f - _inertion) * Device.fTimeDelta * _smoothingScale;

        if (fabsf(step) >= fabsf(diff))
        {
            _displayRatio = _targetRatio;
        }
        else
        {
            _displayRatio += step;
        }
    }

    if (_thumbScaleWithZoom)
    {
        UpdateThumbFill();
    }
    else
    {
        UpdateThumbPosition();
    }

    inherited::Update();

    UpdateValueLabel();
}

float UIMapZoomScale::GetMaxRatio() const
{
    if (fis_zero(_minZoom, EPS_L))
    {
        return 1.f;
    }

    const float maxRatio = _maxZoom / _minZoom;
    if (maxRatio <= 1.f)
    {
        return 1.f;
    }

    return maxRatio;
}

float UIMapZoomScale::GetTrackNormalized(float ratio) const
{
    const float maxRatio = GetMaxRatio();
    ratio = ClampRatio(ratio, maxRatio);

    if (maxRatio <= 1.f)
    {
        return 0.f;
    }

    float trackT = Log2Positive(ratio) / Log2Positive(maxRatio);
    clamp(trackT, 0.f, 1.f);
    return trackT;
}

float UIMapZoomScale::GetRailAlongSize() const
{
    R_ASSERT(_rail);
    return _isHorizontal ? _rail->GetWidth() : _rail->GetHeight();
}

float UIMapZoomScale::GetRailCrossSize() const
{
    R_ASSERT(_rail);
    return _isHorizontal ? _rail->GetHeight() : _rail->GetWidth();
}

float UIMapZoomScale::RatioToAlongLocal(float ratio) const
{
    const float normalizedTrack = GetTrackNormalized(ratio);
    const float alongSize = GetRailAlongSize();

    if (_isHorizontal)
    {
        return normalizedTrack * alongSize;
    }

    return (1.f - normalizedTrack) * alongSize;
}

float UIMapZoomScale::RatioToAlongParent(float ratio) const
{
    R_ASSERT(_rail);

    const Fvector2& railPos = _rail->GetWndPos();
    const float alongOrigin = _isHorizontal ? railPos.x : railPos.y;
    return alongOrigin + RatioToAlongLocal(ratio);
}

void UIMapZoomScale::GetRailLocalRect(Frect& out) const
{
    R_ASSERT(_rail);
    out.set(0.f, 0.f, _rail->GetWidth(), _rail->GetHeight());
}

void UIMapZoomScale::ApplyCrossAlignToCoord(
    float& crossPos,
    float crossSize,
    float parentCrossSize,
    EZoomCrossAlign align)
{
    switch (align)
    {
    case EZoomCrossAlign::Center:
        crossPos = (parentCrossSize - crossSize) * 0.5f;
        break;

    case EZoomCrossAlign::Right:
        crossPos = parentCrossSize - crossSize;
        break;

    case EZoomCrossAlign::Left:
    default:
        break;
    }
}

void UIMapZoomScale::ClampThumbToRail(Fvector2& thumbPos, Fvector2& size) const
{
    Frect railRect;
    GetRailLocalRect(railRect);

    const float railWidth = railRect.width();
    const float railHeight = railRect.height();

    if (railWidth > 0.f)
    {
        size.x = clampr(size.x, 0.f, railWidth);
    }
    if (railHeight > 0.f)
    {
        size.y = clampr(size.y, 0.f, railHeight);
    }

    thumbPos.x = clampr(thumbPos.x, railRect.x1, railRect.x2 - size.x);
    thumbPos.y = clampr(thumbPos.y, railRect.y1, railRect.y2 - size.y);
}

void UIMapZoomScale::ApplyRailAlignInParent()
{
    if (!_rail)
    {
        return;
    }

    Fvector2 pos = _rail->GetWndPos();
    const Fvector2 size = _rail->GetWndSize();

    if (_isHorizontal)
    {
        ApplyCrossAlignToCoord(pos.y, size.y, GetHeight(), _railCrossAlign);
    }
    else
    {
        ApplyCrossAlignToCoord(pos.x, size.x, GetWidth(), _railCrossAlign);
    }

    _rail->SetWndPos(pos);
}

void UIMapZoomScale::ApplyCrossAlignThumb(
    Fvector2& thumbPos,
    const Fvector2& size,
    const Frect& railLocalRect) const
{
    const float crossSize = _isHorizontal ? size.y : size.x;
    const float parentCrossSize = _isHorizontal ? railLocalRect.height() : railLocalRect.width();
    float& crossPos = _isHorizontal ? thumbPos.y : thumbPos.x;

    ApplyCrossAlignToCoord(crossPos, crossSize, parentCrossSize, _thumbCrossAlign);
}

void UIMapZoomScale::InitThumbOffsetFromXml(CUIXml& xml)
{
    _thumbOffset.set(0.f, 0.f);

    if (const char* thumbPath = ResolveThumbXmlPath(xml))
    {
        _thumbOffset.x = xml.ReadAttribFlt(thumbPath, 0, "x", 0.f);
        _thumbOffset.y = xml.ReadAttribFlt(thumbPath, 0, "y", 0.f);
    }
}

void UIMapZoomScale::ApplyThumbOffset(Fvector2& thumbPos) const
{
    thumbPos.x += _thumbOffset.x;
    thumbPos.y += _thumbOffset.y;
}

UIMapZoomScale::ThumbLayout UIMapZoomScale::ComputeThumbFillLayout(float displayRatio) const
{
    ThumbLayout layout;
    layout.size = _thumbBaseSize;
    layout.pos = _thumb->GetWndPos();

    const float trackPos = RatioToAlongLocal(displayRatio);
    const float alongSize = GetRailAlongSize();
    const float crossSize = GetRailCrossSize();

    if (_isHorizontal)
    {
        const float fillLen = clampr(trackPos, 0.f, alongSize);
        layout.pos.x = 0.f;
        layout.size.x = (fillLen >= _thumbBaseSize.x) ? fillLen : _thumbBaseSize.x;
        layout.size.y = MinFloat(_thumbBaseSize.y, crossSize > 0.f ? crossSize : _thumbBaseSize.y);
    }
    else
    {
        const float fillLen = clampr(alongSize - trackPos, 0.f, alongSize);
        layout.size.x = MinFloat(_thumbBaseSize.x, crossSize > 0.f ? crossSize : _thumbBaseSize.x);
        if (fillLen >= _thumbBaseSize.y)
        {
            layout.size.y = fillLen;
            layout.pos.y = trackPos;
        }
        else
        {
            layout.size.y = _thumbBaseSize.y;
            layout.pos.y = alongSize - layout.size.y;
        }
    }

    return layout;
}

UIMapZoomScale::ThumbLayout UIMapZoomScale::ComputeThumbMarkerLayout(float displayRatio) const
{
    ThumbLayout layout;
    layout.pos = _thumb->GetWndPos();
    layout.size = _thumb->GetWndSize();

    const float alongLocal = RatioToAlongLocal(displayRatio);
    const float halfAlong = _isHorizontal ? layout.size.x * 0.5f : layout.size.y * 0.5f;

    if (_isHorizontal)
    {
        layout.pos.x = alongLocal - halfAlong;
    }
    else
    {
        layout.pos.y = alongLocal - halfAlong;
    }

    return layout;
}

void UIMapZoomScale::ApplyThumbLayout(const ThumbLayout& layout)
{
    Fvector2 thumbPos = layout.pos;
    Fvector2 size = layout.size;

    Frect railLocalRect;
    GetRailLocalRect(railLocalRect);

    ApplyCrossAlignThumb(thumbPos, size, railLocalRect);
    ApplyThumbOffset(thumbPos);
    ClampThumbToRail(thumbPos, size);

    _thumb->SetWndSize(size);
    _thumb->SetWndPos(thumbPos);
}

void UIMapZoomScale::UpdateThumbFill()
{
    if (!_thumb || !_rail)
    {
        return;
    }

    _thumb->SetStretchTexture(true);
    ApplyThumbLayout(ComputeThumbFillLayout(_displayRatio));
}

void UIMapZoomScale::UpdateThumbPosition()
{
    if (!_thumb || !_rail || _thumbScaleWithZoom)
    {
        return;
    }

    ApplyThumbLayout(ComputeThumbMarkerLayout(_displayRatio));
}

CUIStatic* UIMapZoomScale::GetBoundLabelStyleTemplate() const
{
    return _tickLabelBoundTemplate ? _tickLabelBoundTemplate : _tickLabelTemplate;
}

CUIStatic* UIMapZoomScale::GetValueLabelStyleTemplate() const
{
    return _tickLabelValueTemplate ? _tickLabelValueTemplate : _tickLabelTemplate;
}

void UIMapZoomScale::EnsureLabels()
{
    if (!_rail)
    {
        return;
    }

    CUIStatic* boundStyleTemplate = GetBoundLabelStyleTemplate();
    CUIStatic* valueStyleTemplate = GetValueLabelStyleTemplate();

    const auto attachLabelIfNullLambda = [this](
        CUIStatic*& slot, CUIStatic* styleTemplate, const Fvector2& labelSize)
    {
        if (slot || !styleTemplate)
        {
            return;
        }
        slot = new CUIStatic();
        ApplyLabelStyle(slot, styleTemplate, labelSize);
        AttachChild(slot);
    };

    attachLabelIfNullLambda(_minLabel, boundStyleTemplate, _boundLabelSize);
    attachLabelIfNullLambda(_maxLabel, boundStyleTemplate, _boundLabelSize);
    attachLabelIfNullLambda(_valueLabel, valueStyleTemplate, _valueLabelSize);
}

void UIMapZoomScale::ApplyLabelStyle(
    CUIStatic* label,
    CUIStatic* styleTemplate,
    const Fvector2& labelSize) const
{
    if (!label || !styleTemplate)
    {
        return;
    }

    label->SetWndSize(labelSize);
    if (CGameFont* tickFont = styleTemplate->GetFont())
    {
        label->SetFont(tickFont);
    }
    label->TextItemControl()->SetTextAlignment(styleTemplate->TextItemControl()->GetTextAlignment());
    label->SetTextColor(styleTemplate->TextItemControl()->GetTextColor());
    label->Enable(false);
}

void UIMapZoomScale::FormatRatioLabel(string32& buffer, float ratio) const
{
    xr_sprintf(buffer, _labelFormat, ratio);
}

float UIMapZoomScale::GetLabelAlongHalfExtent(const CUIStatic* label) const
{
    return _isHorizontal ? label->GetWidth() * 0.5f : label->GetHeight() * 0.5f;
}

void UIMapZoomScale::PlaceLabelAtRatio(
    CUIStatic* label,
    float ratio,
    const Fvector2& crossOffset) const
{
    const Fvector2& railPos = _rail->GetWndPos();
    const float alongCenter = RatioToAlongParent(ratio) - GetLabelAlongHalfExtent(label);

    if (_isHorizontal)
    {
        label->SetWndPos(Fvector2().set(alongCenter, railPos.y + crossOffset.y));
    }
    else
    {
        label->SetWndPos(Fvector2().set(railPos.x + crossOffset.x, alongCenter));
    }
}

void UIMapZoomScale::UpdateBoundLabels()
{
    if (!_rail || !_minLabel || !_maxLabel)
    {
        return;
    }

    const float maxRatio = GetMaxRatio();

    if (_boundLabelsMinMax)
    {
        _minLabel->SetText(g_pStringTable->translate(_boundLabelMinId.c_str()).c_str());
        _maxLabel->SetText(g_pStringTable->translate(_boundLabelMaxId.c_str()).c_str());
    }
    else
    {
        string32 labelText = {};

        FormatRatioLabel(labelText, 1.f);
        _minLabel->SetText(labelText);
        FormatRatioLabel(labelText, maxRatio);
        _maxLabel->SetText(labelText);
    }

    PlaceLabelAtRatio(_minLabel, 1.f, _boundLabelOffset);
    PlaceLabelAtRatio(_maxLabel, maxRatio, _boundLabelOffset);
}

void UIMapZoomScale::UpdateValueLabel()
{
    if (!_rail || !_valueLabel)
    {
        return;
    }

    string32 labelText = {};
    FormatRatioLabel(labelText, _displayRatio);
    _valueLabel->SetText(labelText);

    PlaceLabelAtRatio(_valueLabel, _displayRatio, _valueLabelOffset);
}
