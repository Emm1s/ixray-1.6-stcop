#pragma once

#include "../../xrUI/Widgets/UIWindow.h"

class CUIXml;
class CUIStatic;

class UIMapZoomScale final : public CUIWindow
{
private:
    using inherited = CUIWindow;

public:
    UIMapZoomScale() = default;
    ~UIMapZoomScale() override;

    void InitFromXml(CUIXml& xml, const char* path);
    void SyncFromMap(float minZoom, float maxZoom, float currentZoom);
    void Update() override;

private:
    void EnsureLabels();
    void ApplyLabelStyle(CUIStatic* label, CUIStatic* styleTemplate, const Fvector2& labelSize) const;
    void UpdateBoundLabels();
    void UpdateValueLabel();
    void FormatRatioLabel(string32& buffer, float ratio) const;
    float GetTrackNormalized(float ratio) const;
    float RatioToTrackPos(float ratio) const;
    void GetRailRect(Frect& out) const;
    void UpdateThumbScale();
    void UpdateThumbPosition();
    float GetMaxRatio() const;

    CUIStatic* GetBoundLabelStyleTemplate() const;
    CUIStatic* GetValueLabelStyleTemplate() const;

    CUIStatic* _rail = nullptr;
    CUIStatic* _thumb = nullptr;
    CUIStatic* _minLabel = nullptr;
    CUIStatic* _maxLabel = nullptr;
    CUIStatic* _valueLabel = nullptr;
    CUIStatic* _tickLabelTemplate = nullptr;
    CUIStatic* _tickLabelBoundTemplate = nullptr;
    CUIStatic* _tickLabelValueTemplate = nullptr;

    float _minZoom = 1.f;
    float _maxZoom = 1.f;
    float _displayRatio = 1.f;
    float _targetRatio = 1.f;
    float _inertion = 0.85f;
    float _smoothingScale = 20.f;
    Fvector2 _boundLabelOffset = { 0.f, 0.f };
    Fvector2 _valueLabelOffset = { 0.f, 0.f };
    Fvector2 _boundLabelSize = { 0.f, 0.f };
    Fvector2 _valueLabelSize = { 0.f, 0.f };
    Fvector2 _thumbBaseSize = { 0.f, 0.f };
    string64 _labelFormat = {};
    shared_str _boundLabelMinId;
    shared_str _boundLabelMaxId;
    bool _isHorizontal = false;
    bool _boundLabelsMinMax = false;
    bool _thumbScaleWithZoom = false;
    bool _isInitialized = false;
};
