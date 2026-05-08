#pragma once

#include "../../xrCore/xr_delegate.h"
#include "../../xrUI/Widgets/UIWindow.h"

class CUIStatic;
class CUIXml;

enum class ETimelineState : u8
{
    Empty,
    Archive,
    Present,
    Future
};

struct STimelineNode
{
    u32 day = 0;
    ETimelineState state = ETimelineState::Empty;
    Fvector2 position = Fvector2().set(0.0f, 0.0f);
    bool hasMessages = false;
    bool isClickable = false;
};

class CUITimeLine final :
    public CUIWindow
{
    using inherited = CUIWindow;

public:
    using TimelineNodeCallback = xr_delegate<void(u32)>;

    CUITimeLine() = default;
    ~CUITimeLine() override = default;

    bool InitFromXml(CUIXml& xml, const char* path);
    u32 GetNodeCount() const;

    void Draw() override;
    void Update() override;
    bool OnMouseAction(float x, float y, EUIMessages mouseAction) override;

    void SetCurrentDay(u32 day);
    void SetSelectedDay(u32 day);
    void SetNodeState(u32 day, ETimelineState state, bool hasMessages);

    void SetOnNodeHover(const TimelineNodeCallback& callback);
    void SetOnNodePressed(const TimelineNodeCallback& callback);
    void SetOnNodeSelected(const TimelineNodeCallback& callback);

private:
    static constexpr u32 _kInvalidNodeIndex = (u32)-1;

    void BuildNodes();
    void ApplyNodeVisual(u32 index);
    void ApplyDayVisual(u32 day);
    bool SelectNodeByIndex(u32 index);
    bool HitTestNode(float x, float y, u32& outIndex) const;
    float MarkerTargetXByIndex(u32 index) const;
    bool IsDayValid(u32 day) const;
    u32 DayToIndex(u32 day) const;
    void UpdateMarkerAndLabelPosition();
    void UpdateLabelText();
    void LogDebugSelection(u32 day) const;

private:
    xr_vector<STimelineNode> _nodes;
    xr_vector<CUIStatic*> _nodeWidgets;

    CUIStatic* _backgroundLine = nullptr;
    CUIStatic* _marker = nullptr;
    CUIStatic* _label = nullptr;

    xr_string _lineTexture;
    xr_string _nodeEmptyTexture;
    xr_string _nodeArchiveTexture;
    xr_string _nodePresentTexture;
    xr_string _nodeFutureTexture;
    xr_string _nodeHoverTexture;
    xr_string _nodeSelectedTexture;
    xr_string _markerTexture;

    u32 _nodeCount = 0;
    u32 _currentDay = 0;
    u32 _selectedDay = 0;
    u32 _hoveredNodeIndex = _kInvalidNodeIndex;

    float _currentMarkerX = 0.0f;
    float _targetMarkerX = 0.0f;
    float _smoothing = 0.0f;
    float _snapThreshold = 0.0f;
    float _nodeSize = 0.0f;
    float _lineHeight = 0.0f;
    float _labelOffsetY = 0.0f;
    float _nodeSpacing = 0.0f;
    float _hitPadding = 0.0f;

    Fvector2 _labelSize = Fvector2().set(0.0f, 0.0f);

    bool _showFutureNodes = false;
    bool _isAnimating = false;
    bool _isInitialized = false;

    TimelineNodeCallback _onNodeHover;
    TimelineNodeCallback _onNodePressed;
    TimelineNodeCallback _onNodeSelected;
};

