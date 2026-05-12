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

void UIMapZoomScale::InitFromXml(CUIXml& xml, const char* path)
{
    CUIXmlInit::InitWindow(xml, path, 0, this);

    _inertion = xml.ReadAttribFlt(path, 0, "inertion", 0.85f);
    _smoothingScale = xml.ReadAttribFlt(path, 0, "smoothing_scale", 20.f);
    xr_strcpy(_labelFormat, xml.ReadAttrib(path, 0, "label_format", "x%.1f"));
    _boundLabelsMinMax = (xml.ReadAttribInt(path, 0, "bound_labels", 0) != 0);
    _boundLabelMinId = xml.ReadAttrib(path, 0, "bound_label_min", "ui_map_zoom_min");
    _boundLabelMaxId = xml.ReadAttrib(path, 0, "bound_label_max", "ui_map_zoom_max");
    // 0: vertical rail (thumb moves along Y, min zoom at bottom). 1: horizontal (thumb along X, min zoom at left).
    _isHorizontal = (xml.ReadAttribInt(path, 0, "horizontal", 0) != 0);
    _thumbScaleWithZoom = (xml.ReadAttribInt(path, 0, "thumb_scaling", 0) != 0);

    XML_NODE* storedRoot = xml.GetLocalRoot();
    XML_NODE* nodeRoot = xml.NavigateToNode(path, 0);
    xml.SetLocalRoot(nodeRoot);

    _rail = UIHelper::CreateStatic(xml, "rail", this, false);
    if (_rail)
    {
        _rail->Enable(false);
    }

    _thumb = UIHelper::CreateStatic(xml, "thumb", this, false);
    if (_thumb)
    {
        _thumb->Enable(false);
        _thumbBaseSize.set(_thumb->GetWidth(), _thumb->GetHeight());
    }

    const bool hasBoundTemplate = InitTickLabelTemplate(
        xml, "tick_label_bound", _tickLabelBoundTemplate, _boundLabelOffset, _boundLabelSize);
    const bool hasValueTemplate = InitTickLabelTemplate(
        xml, "tick_label_value", _tickLabelValueTemplate, _valueLabelOffset, _valueLabelSize);

    if (xml.NavigateToNode("tick_label", 0))
    {
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

    xml.SetLocalRoot(storedRoot);

    _isInitialized = (_rail != nullptr && _thumb != nullptr);
    _displayRatio = 1.f;
    _targetRatio = 1.f;

    if (_isInitialized)
    {
        EnsureLabels();
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
        UpdateThumbScale();
    }

    _targetRatio = targetRatio;
}

void UIMapZoomScale::Update()
{
    inherited::Update();

    if (!_isInitialized)
    {
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

    UpdateThumbScale();
    UpdateThumbPosition();
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

float UIMapZoomScale::RatioToTrackPos(float ratio) const
{
    R_ASSERT(_rail);

    const float normalizedTrack = GetTrackNormalized(ratio);
    if (_isHorizontal)
    {
        return _rail->GetWndPos().x + normalizedTrack * _rail->GetWidth();
    }

    return _rail->GetWndPos().y + (1.f - normalizedTrack) * _rail->GetHeight();
}

void UIMapZoomScale::GetRailRect(Frect& out) const
{
    R_ASSERT(_rail);
    const Fvector2& pos = _rail->GetWndPos();
    out.set(pos.x, pos.y, pos.x + _rail->GetWidth(), pos.y + _rail->GetHeight());
}

void UIMapZoomScale::UpdateThumbScale()
{
    if (!_thumb || !_thumbScaleWithZoom || !_rail)
    {
        return;
    }

    const float trackT = GetTrackNormalized(_displayRatio);
    Fvector2 size = _thumbBaseSize;

    if (_isHorizontal)
    {
        const float railLen = _rail->GetWidth();
        size.x = size.x + trackT * (railLen - size.x);
    }
    else
    {
        const float railLen = _rail->GetHeight();
        size.y = size.y + trackT * (railLen - size.y);
    }

    _thumb->SetWndSize(size);
}

void UIMapZoomScale::UpdateThumbPosition()
{
    if (!_thumb || !_rail)
    {
        return;
    }

    Frect railRect;
    GetRailRect(railRect);

    Fvector2 thumbPos = _thumb->GetWndPos();
    if (_isHorizontal)
    {
        thumbPos.x = RatioToTrackPos(_displayRatio) - _thumb->GetWidth() * 0.5f;
        thumbPos.x = clampr(thumbPos.x, railRect.x1, railRect.x2 - _thumb->GetWidth());
    }
    else
    {
        thumbPos.y = RatioToTrackPos(_displayRatio) - _thumb->GetHeight() * 0.5f;
        thumbPos.y = clampr(thumbPos.y, railRect.y1, railRect.y2 - _thumb->GetHeight());
    }
    _thumb->SetWndPos(thumbPos);
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

    if (_isHorizontal)
    {
        const float labelY = _rail->GetWndPos().y + _boundLabelOffset.y;
        _minLabel->SetWndPos(
            Fvector2().set(RatioToTrackPos(1.f) - _minLabel->GetWidth() * 0.5f, labelY));
        _maxLabel->SetWndPos(
            Fvector2().set(RatioToTrackPos(maxRatio) - _maxLabel->GetWidth() * 0.5f, labelY));
    }
    else
    {
        const float labelX = _rail->GetWndPos().x + _boundLabelOffset.x;
        _minLabel->SetWndPos(
            Fvector2().set(labelX, RatioToTrackPos(1.f) - _minLabel->GetHeight() * 0.5f));
        _maxLabel->SetWndPos(
            Fvector2().set(labelX, RatioToTrackPos(maxRatio) - _maxLabel->GetHeight() * 0.5f));
    }
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

    if (_isHorizontal)
    {
        const float labelY = _rail->GetWndPos().y + _valueLabelOffset.y;
        _valueLabel->SetWndPos(Fvector2().set(
            RatioToTrackPos(_displayRatio) - _valueLabel->GetWidth() * 0.5f, labelY));
    }
    else
    {
        const float labelX = _rail->GetWndPos().x + _valueLabelOffset.x;
        const float labelY = RatioToTrackPos(_displayRatio) - _valueLabel->GetHeight() * 0.5f;
        _valueLabel->SetWndPos(Fvector2().set(labelX, labelY));
    }
}
