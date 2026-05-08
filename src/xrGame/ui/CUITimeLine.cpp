#include "StdAfx.h"
#include "CUITimeLine.h"

#include "../../xrUI/UIXmlInit.h"
#include "../../xrUI/Widgets/UIStatic.h"
#include "../../xrEngine/XR_IOConsole.h"

namespace
{
bool ReportMissingTimelineValue(const char* path, const char* valueName)
{
    Msg(
        "! [Timeline] Required XML value '%s' is missing in '%s'. Timeline is disabled.",
        valueName,
        path);
    return false;
}

bool ReadRequiredFloat(CUIXml& xml, const char* path, const char* attribute, float& outValue)
{
    if (!xml.NavigateToNode(path, 0) || !xml.ReadAttrib(path, 0, attribute, nullptr))
    {
        return ReportMissingTimelineValue(path, attribute);
    }

    outValue = xml.ReadAttribFlt(path, 0, attribute);
    return true;
}

bool ReadRequiredU32(CUIXml& xml, const char* path, const char* attribute, u32& outValue)
{
    if (!xml.NavigateToNode(path, 0) || !xml.ReadAttrib(path, 0, attribute, nullptr))
    {
        return ReportMissingTimelineValue(path, attribute);
    }

    const int value = xml.ReadAttribInt(path, 0, attribute);
    if (value < 0)
    {
        Msg(
            "! [Timeline] XML value '%s' in '%s' must be non-negative. Timeline is disabled.",
            attribute,
            path);
        return false;
    }

    outValue = (u32)value;
    return true;
}

bool ReadRequiredBool(CUIXml& xml, const char* path, const char* attribute, bool& outValue)
{
    if (!xml.NavigateToNode(path, 0) || !xml.ReadAttrib(path, 0, attribute, nullptr))
    {
        return ReportMissingTimelineValue(path, attribute);
    }

    outValue = xml.ReadAttribBool(path, 0, attribute);
    return true;
}

bool ReadRequiredText(CUIXml& xml, const char* path, xr_string& outValue)
{
    if (!xml.NavigateToNode(path, 0))
    {
        return ReportMissingTimelineValue(path, "text");
    }

    const char* value = xml.Read(path, 0, nullptr);
    if (!value || !value[0])
    {
        return ReportMissingTimelineValue(path, "text");
    }

    outValue = value;
    return true;
}

}

bool CUITimeLine::InitFromXml(CUIXml& xml, const char* path)
{
    if (!xml.NavigateToNode(path, 0))
    {
        Msg("! [Timeline] XML node '%s' is missing. Timeline is disabled.", path);
        return false;
    }

    if (!CUIXmlInit::InitWindow(xml, path, 0, this, false))
    {
        Msg("! [Timeline] Cannot init window from '%s'. Timeline is disabled.", path);
        return false;
    }

    string_path nodesPath;
    string_path animationPath;
    string_path labelPath;
    string_path spacingPath;
    string_path lineTexturePath;
    string_path nodeEmptyTexturePath;
    string_path nodeArchiveTexturePath;
    string_path nodePresentTexturePath;
    string_path nodeFutureTexturePath;
    string_path nodeHoverTexturePath;
    string_path nodeSelectedTexturePath;
    string_path markerTexturePath;
    xr_sprintf(nodesPath, "%s:nodes", path);
    xr_sprintf(animationPath, "%s:animation", path);
    xr_sprintf(labelPath, "%s:label", path);
    xr_sprintf(spacingPath, "%s:spacing", path);
    xr_sprintf(lineTexturePath, "%s:textures:line", path);
    xr_sprintf(nodeEmptyTexturePath, "%s:textures:node_empty", path);
    xr_sprintf(nodeArchiveTexturePath, "%s:textures:node_archive", path);
    xr_sprintf(nodePresentTexturePath, "%s:textures:node_present", path);
    xr_sprintf(nodeFutureTexturePath, "%s:textures:node_future", path);
    xr_sprintf(nodeHoverTexturePath, "%s:textures:node_hover", path);
    xr_sprintf(nodeSelectedTexturePath, "%s:textures:node_selected", path);
    xr_sprintf(markerTexturePath, "%s:textures:marker", path);

    u32 maxNodeCount = 0;
    if (!ReadRequiredU32(xml, nodesPath, "count", _nodeCount) ||
        !ReadRequiredU32(xml, nodesPath, "max_count", maxNodeCount) ||
        !ReadRequiredFloat(xml, nodesPath, "size", _nodeSize) ||
        !ReadRequiredFloat(xml, nodesPath, "line_height", _lineHeight) ||
        !ReadRequiredFloat(xml, nodesPath, "hit_padding", _hitPadding) ||
        !ReadRequiredBool(xml, nodesPath, "show_future", _showFutureNodes))
    {
        return false;
    }

    if (_nodeCount == 0 || maxNodeCount == 0 || _nodeCount > maxNodeCount)
    {
        Msg("! [Timeline] Invalid node count: %u. Timeline is disabled.", _nodeCount);
        return false;
    }

    if (_nodeSize <= 0.0f || _lineHeight <= 0.0f || _hitPadding < 0.0f)
    {
        Msg("! [Timeline] Invalid node metrics in '%s'. Timeline is disabled.", nodesPath);
        return false;
    }

    if (!ReadRequiredFloat(xml, animationPath, "smoothing", _smoothing) ||
        !ReadRequiredFloat(xml, animationPath, "snap_threshold", _snapThreshold))
    {
        return false;
    }

    if (_smoothing <= 0.0f || _snapThreshold < 0.0f)
    {
        Msg("! [Timeline] Invalid animation settings in '%s'. Timeline is disabled.", animationPath);
        return false;
    }

    if (!ReadRequiredFloat(xml, labelPath, "offset_y", _labelOffsetY) ||
        !ReadRequiredFloat(xml, labelPath, "width", _labelSize.x) ||
        !ReadRequiredFloat(xml, labelPath, "height", _labelSize.y))
    {
        return false;
    }

    if (_labelSize.x <= 0.0f || _labelSize.y <= 0.0f)
    {
        Msg("! [Timeline] Invalid label size in '%s'. Timeline is disabled.", labelPath);
        return false;
    }

    bool hasAutoSpacing = false;
    if (!ReadRequiredBool(xml, spacingPath, "auto", hasAutoSpacing) ||
        !ReadRequiredText(xml, lineTexturePath, _lineTexture) ||
        !ReadRequiredText(xml, nodeEmptyTexturePath, _nodeEmptyTexture) ||
        !ReadRequiredText(xml, nodeArchiveTexturePath, _nodeArchiveTexture) ||
        !ReadRequiredText(xml, nodePresentTexturePath, _nodePresentTexture) ||
        !ReadRequiredText(xml, nodeFutureTexturePath, _nodeFutureTexture) ||
        !ReadRequiredText(xml, nodeHoverTexturePath, _nodeHoverTexture) ||
        !ReadRequiredText(xml, nodeSelectedTexturePath, _nodeSelectedTexture) ||
        !ReadRequiredText(xml, markerTexturePath, _markerTexture))
    {
        return false;
    }

    _backgroundLine = new CUIStatic();
    _backgroundLine->SetAutoDelete(true);
    _backgroundLine->SetWndPos(Fvector2().set(0.0f, (GetHeight() - _lineHeight) * 0.5f));
    _backgroundLine->SetWndSize(Fvector2().set(GetWidth(), _lineHeight));
    _backgroundLine->SetStretchTexture(true);
    if (!_lineTexture.empty())
    {
        _backgroundLine->InitTexture(_lineTexture.c_str(), false);
    }
    AttachChild(_backgroundLine);

    _marker = new CUIStatic();
    _marker->SetAutoDelete(true);
    _marker->SetWndSize(Fvector2().set(_nodeSize, _nodeSize));
    _marker->SetStretchTexture(true);
    if (!_markerTexture.empty())
    {
        _marker->InitTexture(_markerTexture.c_str(), false);
    }
    AttachChild(_marker);

    _label = new CUIStatic();
    _label->SetAutoDelete(true);
    _label->SetWndSize(_labelSize);
    if (xml.NavigateToNode(labelPath, 0))
    {
        CUIXmlInit::InitText(xml, labelPath, 0, _label);
    }
    AttachChild(_label);

    if (hasAutoSpacing)
    {
        _nodeSpacing = _nodeCount > 1 ? (GetWidth() - _nodeSize) / float(_nodeCount - 1) : 0.0f;
    }
    else
    {
        if (!ReadRequiredFloat(xml, spacingPath, "value", _nodeSpacing))
        {
            return false;
        }

        if (_nodeSpacing <= 0.0f)
        {
            Msg("! [Timeline] Invalid spacing value in '%s'. Timeline is disabled.", spacingPath);
            return false;
        }
    }

    BuildNodes();

    _isInitialized = true;
    return true;
}

u32 CUITimeLine::GetNodeCount() const
{
    return _nodeCount;
}

void CUITimeLine::BuildNodes()
{
    _nodes.clear();
    _nodeWidgets.clear();
    _nodes.resize(_nodeCount);
    _nodeWidgets.resize(_nodeCount, nullptr);

    const float centerY = GetHeight() * 0.5f;
    for (u32 i = 0; i < _nodeCount; ++i)
    {
        STimelineNode& node = _nodes[i];
        node.day = i + 1;
        node.state = ETimelineState::Empty;
        node.hasMessages = false;
        node.isClickable = false;
        node.position.set(_nodeSpacing * i, centerY - _nodeSize * 0.5f);

        CUIStatic* nodeStatic = new CUIStatic();
        nodeStatic->SetAutoDelete(true);
        nodeStatic->SetWndSize(Fvector2().set(_nodeSize, _nodeSize));
        nodeStatic->SetWndPos(node.position);
        nodeStatic->SetStretchTexture(true);
        AttachChild(nodeStatic);
        _nodeWidgets[i] = nodeStatic;
        ApplyNodeVisual(i);
    }
}

void CUITimeLine::ApplyNodeVisual(u32 index)
{
    if (index >= _nodes.size() || index >= _nodeWidgets.size())
    {
        return;
    }

    STimelineNode& node = _nodes[index];
    CUIStatic* nodeStatic = _nodeWidgets[index];
    if (!nodeStatic)
    {
        return;
    }

    const xr_string* textureName = nullptr;
    if (node.day == _selectedDay)
    {
        textureName = &_nodeSelectedTexture;
    }
    else if (index == _hoveredNodeIndex && node.isClickable)
    {
        textureName = &_nodeHoverTexture;
    }
    else
    {
        switch (node.state)
        {
            case ETimelineState::Archive:
                textureName = &_nodeArchiveTexture;
                break;
            case ETimelineState::Present:
                textureName = &_nodePresentTexture;
                break;
            case ETimelineState::Future:
                textureName = &_nodeFutureTexture;
                break;
            case ETimelineState::Empty:
            default:
                textureName = &_nodeEmptyTexture;
                break;
        }
    }

    if (textureName && !textureName->empty())
    {
        nodeStatic->InitTexture(textureName->c_str(), false);
    }

    nodeStatic->Show(_showFutureNodes || node.state != ETimelineState::Future);
}

void CUITimeLine::ApplyDayVisual(u32 day)
{
    if (!IsDayValid(day))
    {
        return;
    }

    ApplyNodeVisual(DayToIndex(day));
}

void CUITimeLine::Draw()
{
    if (!_isInitialized)
    {
        return;
    }

    inherited::Draw();
}

void CUITimeLine::Update()
{
    inherited::Update();
    if (!_isInitialized || !_isAnimating)
    {
        return;
    }

    _currentMarkerX += (_targetMarkerX - _currentMarkerX) * _smoothing * Device.fTimeDelta;
    if (fsimilar(_targetMarkerX, _currentMarkerX, _snapThreshold))
    {
        _currentMarkerX = _targetMarkerX;
        _isAnimating = false;
    }

    UpdateMarkerAndLabelPosition();
}

bool CUITimeLine::OnMouseAction(float x, float y, EUIMessages mouseAction)
{
    if (!_isInitialized)
    {
        return inherited::OnMouseAction(x, y, mouseAction);
    }

    u32 nodeIndex = _kInvalidNodeIndex;
    const bool hasHoveredNode = HitTestNode(x, y, nodeIndex);

    if (mouseAction == WINDOW_MOUSE_MOVE)
    {
        const u32 previousHoveredNodeIndex = _hoveredNodeIndex;

        if (hasHoveredNode)
        {
            _hoveredNodeIndex = nodeIndex;
            if (previousHoveredNodeIndex != _hoveredNodeIndex && !_onNodeHover.empty())
            {
                _onNodeHover(_nodes[nodeIndex].day);
            }
        }
        else
        {
            _hoveredNodeIndex = _kInvalidNodeIndex;
        }

        if (previousHoveredNodeIndex != _hoveredNodeIndex)
        {
            ApplyNodeVisual(previousHoveredNodeIndex);
            ApplyNodeVisual(_hoveredNodeIndex);
        }
    }
    else if (mouseAction == WINDOW_LBUTTON_DOWN)
    {
        if (!hasHoveredNode)
        {
            return inherited::OnMouseAction(x, y, mouseAction);
        }

        STimelineNode& node = _nodes[nodeIndex];
        if (node.state == ETimelineState::Future || !node.isClickable || !node.hasMessages)
        {
            return true;
        }
        if (node.day == _selectedDay)
        {
            return true;
        }

        if (!_onNodePressed.empty())
        {
            _onNodePressed(node.day);
        }

        SelectNodeByIndex(nodeIndex);
        return true;
    }

    return inherited::OnMouseAction(x, y, mouseAction);
}

bool CUITimeLine::HitTestNode(float x, float y, u32& outIndex) const
{
    for (u32 i = 0; i < _nodes.size(); ++i)
    {
        const STimelineNode& node = _nodes[i];
        Frect rect;
        rect.lt.set(node.position.x - _hitPadding, node.position.y - _hitPadding);
        rect.rb.set(
            node.position.x + _nodeSize + _hitPadding,
            node.position.y + _nodeSize + _hitPadding);
        if (rect.in(x, y))
        {
            outIndex = i;
            return true;
        }
    }

    return false;
}

bool CUITimeLine::SelectNodeByIndex(u32 index)
{
    if (index >= _nodes.size())
    {
        return false;
    }

    const u32 day = _nodes[index].day;
    if (!IsDayValid(day))
    {
        return false;
    }

    const u32 previousSelectedDay = _selectedDay;
    _selectedDay = day;
    _targetMarkerX = MarkerTargetXByIndex(index);
    _isAnimating = true;
    UpdateLabelText();
    ApplyDayVisual(previousSelectedDay);
    ApplyDayVisual(_selectedDay);
    if (!_onNodeSelected.empty())
    {
        _onNodeSelected(day);
    }
    LogDebugSelection(day);
    return true;
}

void CUITimeLine::SetCurrentDay(u32 day)
{
    if (!IsDayValid(day))
    {
        return;
    }

    _currentDay = day;
    for (u32 i = 0; i < _nodes.size(); ++i)
    {
        STimelineNode& node = _nodes[i];
        if (node.day < _currentDay)
        {
            node.state = node.hasMessages ? ETimelineState::Archive : ETimelineState::Empty;
            node.isClickable = node.hasMessages;
        }
        else if (node.day == _currentDay)
        {
            node.state = ETimelineState::Present;
            node.isClickable = node.hasMessages;
        }
        else
        {
            node.state = ETimelineState::Future;
            node.isClickable = false;
        }
        ApplyNodeVisual(i);
    }
}

void CUITimeLine::SetSelectedDay(u32 day)
{
    if (!IsDayValid(day))
    {
        return;
    }

    const u32 previousSelectedDay = _selectedDay;
    _selectedDay = day;
    const u32 index = DayToIndex(day);
    _targetMarkerX = MarkerTargetXByIndex(index);
    _currentMarkerX = _targetMarkerX;
    _isAnimating = false;
    UpdateLabelText();
    UpdateMarkerAndLabelPosition();
    ApplyDayVisual(previousSelectedDay);
    ApplyDayVisual(_selectedDay);
}

void CUITimeLine::SetNodeState(u32 day, ETimelineState state, bool hasMessages)
{
    if (!IsDayValid(day))
    {
        return;
    }

    const u32 index = DayToIndex(day);
    STimelineNode& node = _nodes[index];
    node.state = state;
    node.hasMessages = hasMessages;
    node.isClickable = hasMessages && state != ETimelineState::Future;
    ApplyNodeVisual(index);
}

void CUITimeLine::SetOnNodeHover(const TimelineNodeCallback& callback)
{
    _onNodeHover = callback;
}

void CUITimeLine::SetOnNodePressed(const TimelineNodeCallback& callback)
{
    _onNodePressed = callback;
}

void CUITimeLine::SetOnNodeSelected(const TimelineNodeCallback& callback)
{
    _onNodeSelected = callback;
}

float CUITimeLine::MarkerTargetXByIndex(u32 index) const
{
    if (index >= _nodes.size())
    {
        return 0.0f;
    }

    const STimelineNode& node = _nodes[index];
    return node.position.x + _nodeSize * 0.5f;
}

bool CUITimeLine::IsDayValid(u32 day) const
{
    return day > 0 && day <= _nodeCount;
}

u32 CUITimeLine::DayToIndex(u32 day) const
{
    return day > 0 ? day - 1 : 0;
}

void CUITimeLine::UpdateMarkerAndLabelPosition()
{
    if (!_marker || !_label)
    {
        return;
    }

    Fvector2 markerPosition;
    markerPosition.set(
        _currentMarkerX - _marker->GetWidth() * 0.5f,
        (GetHeight() - _marker->GetHeight()) * 0.5f);
    _marker->SetWndPos(markerPosition);

    Fvector2 labelPosition;
    labelPosition.set(_currentMarkerX - _label->GetWidth() * 0.5f, _labelOffsetY);
    _label->SetWndPos(labelPosition);
}

void CUITimeLine::UpdateLabelText()
{
    if (!_label)
    {
        return;
    }

    string32 text;
    xr_sprintf(text, sizeof(text), "%02u", _selectedDay);
    _label->SetText(text);
}

void CUITimeLine::LogDebugSelection(u32 day) const
{
    if (Console && Console->GetBool("ui_timeline_debug"))
    {
        Msg("[Timeline] Selected day: %d", day);
    }
}

