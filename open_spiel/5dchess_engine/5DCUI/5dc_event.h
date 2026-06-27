#pragma once

// ===================== 5D Chess Input Event Handler =====================
class FiveDChessEvent
{
public:
    // Mouse event callback types | Params: [GlobalData, WorldPos, ScreenPos]
    using MouseCallback = void(*)(FiveDChessData& m_data, const sf::Vector2f& worldPos, const sf::Vector2i& screenPos);
    // Steady clock time point for time calculation
    using TimePoint = std::chrono::steady_clock::time_point;

    // Constructor: Bind global chess data, init left-click state
    FiveDChessEvent(FiveDChessData& data)
        : m_data(data)
        , m_isDragging(false)
        , m_lastMousePos(0, 0)
        , m_lastLeftClickTime(std::chrono::steady_clock::now())
        , m_lastLeftClickPos(sf::Vector2i(0, 0))
    {

    }

    // Register mouse event callbacks
    void setClickCallback(MouseCallback cb) { m_clickCb = cb; }
    void setRightClickCallback(MouseCallback cb) { m_rightClickCb = cb; }
    void setDblClickCallback(MouseCallback cb) { m_dblClickCb = cb; }
    void setMiddleClickCallback(MouseCallback cb) { m_middleClickCb = cb; }

    // Process SFML input events: Update camera/drag status, trigger business callbacks
    void handleHumanEvent(const sf::Event& event, const sf::Vector2f& windowSize);
	void handleAiEvent(const sf::Event& event, const sf::Vector2f& windowSize);

private:
    FiveDChessData& m_data; // Reference to global chess config & data

    TimePoint m_lastLeftClickTime;   // Last left mouse click timestamp
    sf::Vector2i m_lastLeftClickPos; // Last left mouse click screen position
    bool m_isDblClickTriggered = false; // Double click trigger flag
    const int DBL_CLICK_INTERVAL = 300; // Double click time threshold (ms)
    const int DBL_CLICK_OFFSET = 3;     // Double click position tolerance (px)
    const int DRAG_MIN_OFFSET = 3;

    bool m_isDragging;               // drag flag
    sf::Vector2i m_lastMousePos;     // Last frame mouse screen position
    sf::Vector2i m_leftPressStartPos;

    // Mouse event callback instances
    MouseCallback m_clickCb = nullptr;
    MouseCallback m_rightClickCb = nullptr;
    MouseCallback m_dblClickCb = nullptr;
    MouseCallback m_middleClickCb = nullptr;
};