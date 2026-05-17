#pragma once

#include "../../xrUI/Widgets/UIWindow.h"

class CUIXml;
class CUIStatic;

class UIMapZoomScale final : public CUIWindow
{
private:
    using inherited = CUIWindow;

    enum class EZoomCrossAlign : u8
    {
        Left = 0,
        Center = 1,
        Right = 2,
    };

    struct ThumbLayout final
    {
        Fvector2 pos = { 0.f, 0.f };
        Fvector2 size = { 0.f, 0.f };
    };

public:
    UIMapZoomScale() = default;
    ~UIMapZoomScale() override;

    void InitFromXml(CUIXml& xml, const char* path);
    void SyncFromMap(float minZoom, float maxZoom, float currentZoom);
    void Update() override;

private:
    void ReadConfigAttribs(CUIXml& xml, const char* path);
    void InitRailAndThumb(CUIXml& xml, const char* path);
    void InitLabelTemplates(CUIXml& xml);

    void EnsureLabels();
    void ApplyLabelStyle(CUIStatic* label, CUIStatic* styleTemplate, const Fvector2& labelSize) const;
    void UpdateBoundLabels();
    void UpdateValueLabel();
    void PlaceLabelAtRatio(CUIStatic* label, float ratio, const Fvector2& crossOffset) const;
    float GetLabelAlongHalfExtent(const CUIStatic* label) const;
    void FormatRatioLabel(string32& buffer, float ratio) const;

    float GetMaxRatio() const;
    // trackT = log2(ratio) / log2(maxRatio)
    float GetTrackNormalized(float ratio) const;
    float GetRailAlongSize() const;
    float GetRailCrossSize() const;
    float RatioToAlongLocal(float ratio) const;
    float RatioToAlongParent(float ratio) const;
    void GetRailLocalRect(Frect& out) const;

    static void ApplyCrossAlignToCoord(
        float& crossPos,
        float crossSize,
        float parentCrossSize,
        EZoomCrossAlign align);
    void ApplyRailAlignInParent();
    void ApplyCrossAlignThumb(Fvector2& thumbPos, const Fvector2& size, const Frect& railLocalRect) const;
    void ClampThumbToRail(Fvector2& thumbPos, Fvector2& size) const;
    void ApplyThumbOffset(Fvector2& thumbPos) const;
    void InitThumbOffsetFromXml(CUIXml& xml);

    // thumb_scaling: fill rail from min edge to RatioToAlongLocal (log curve via GetTrackNormalized).
    ThumbLayout ComputeThumbFillLayout(float displayRatio) const;
    ThumbLayout ComputeThumbMarkerLayout(float displayRatio) const;
    void ApplyThumbLayout(const ThumbLayout& layout);
    void UpdateThumbFill();
    void UpdateThumbPosition();

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
    Fvector2 _thumbOffset = { 0.f, 0.f };
    string64 _labelFormat = {};
    shared_str _boundLabelMinId;
    shared_str _boundLabelMaxId;
    bool _isHorizontal = false;
    bool _boundLabelsMinMax = false;
    bool _thumbScaleWithZoom = false;
    EZoomCrossAlign _railCrossAlign = EZoomCrossAlign::Left;
    EZoomCrossAlign _thumbCrossAlign = EZoomCrossAlign::Left;
    bool _isInitialized = false;
};
