#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <numeric>

class Button
{
public:
    Button(const sf::Font& font) : m_font(font) {}

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    void setStyle(sf::Color textColor, sf::Color bgColor, sf::Color disabledBgColor)
    {
        m_textColor = textColor;
        m_bgColor = bgColor;
        m_disabledColor = disabledBgColor;
    }

    void setGrid(sf::Vector2f position, const std::vector<float>& widths, const std::vector<float>& heights, unsigned int fontSize)
    {
        m_position = position;
        m_widths = widths;
        m_heights = heights;
        m_fontSize = fontSize;
        m_texts.clear();
    }

    void setText(size_t index, const std::string& str)
    {
        if (m_widths.empty() || m_heights.empty()) return;
        const size_t total = m_widths.size() * m_heights.size();
        if (index >= total) return;

        if (m_texts.size() < total)
            m_texts.resize(total, sf::Text(m_font));

        m_texts[index].setCharacterSize(m_fontSize);
        m_texts[index].setStyle(sf::Text::Bold);
        m_texts[index].setString(sf::String::fromUtf8(str.begin(), str.end())); 
        const auto bounds = m_texts[index].getLocalBounds();
        m_texts[index].setOrigin({ bounds.position.x + bounds.size.x / 2, bounds.position.y + bounds.size.y / 2 });
    }

    int getIndexAt(sf::Vector2f mouse) const
    {
        if (!contains(mouse)) return -1;

        const float localX = mouse.x - m_position.x;
        const float localY = mouse.y - m_position.y;

        size_t col = 0;
        float accX = 0.0f;
        for (size_t i = 0; i < m_widths.size(); ++i)
        {
            accX += m_widths[i];
            if (localX < accX)
            {
                col = i;
                break;
            }
        }

        size_t row = 0;
        float accY = 0.0f;
        for (size_t i = 0; i < m_heights.size(); ++i)
        {
            accY += m_heights[i];
            if (localY < accY)
            {
                row = i;
                break;
            }
        }

        return static_cast<int>(row * m_widths.size() + col);
    }

    bool contains(sf::Vector2f mouse) const
    {
        const float totalWidth = std::accumulate(m_widths.begin(), m_widths.end(), 0.0f);
        const float totalHeight = std::accumulate(m_heights.begin(), m_heights.end(), 0.0f);
        return sf::FloatRect(m_position, { totalWidth, totalHeight }).contains(mouse);
    }

    void handleEvent(const sf::Event& e, sf::Vector2f mouse)
    {
        if (!m_enabled) return;

        if (e.is<sf::Event::MouseButtonPressed>()) {
            if (contains(mouse)) {
                m_dragStartPos  = m_position;
                m_dragStartMouse= mouse;
                m_isDragging    = false;
                m_isPressed     = true;
            }
        }
        else if (e.is<sf::Event::MouseButtonReleased>()) {
            if (m_isPressed) {
                m_isPressed = false;
                m_isDragging = false;
            }
        }
        else if (e.is<sf::Event::MouseMoved>()) {
            if (!m_isPressed) return;
            if (!m_isDragging) {
                float dx = std::abs(mouse.x - m_dragStartMouse.x);
                float dy = std::abs(mouse.y - m_dragStartMouse.y);
                m_isDragging = (dx > 3 || dy > 3);
            }
            if (m_isDragging) {
                m_position.x = m_dragStartPos.x + (mouse.x - m_dragStartMouse.x);
                m_position.y = m_dragStartPos.y + (mouse.y - m_dragStartMouse.y);
            }
        }
    }

    bool isHandlingDrag() const {
        return m_isPressed;
    }

    void draw(sf::RenderWindow& win) const
    {
        if (m_widths.empty() || m_heights.empty()) return;

        constexpr float gap = 2.0f;
        float accX = 0.0f;
        for (size_t col = 0; col < m_widths.size(); ++col)
        {
            float accY = 0.0f;
            for (size_t row = 0; row < m_heights.size(); ++row)
            {
                sf::RectangleShape bg;
                bg.setSize({ m_widths[col] - 2 * gap, m_heights[row] - 2 * gap });
                bg.setPosition({ m_position.x + accX + gap, m_position.y + accY + gap });
                bg.setFillColor(m_enabled ? m_bgColor : m_disabledColor);
                win.draw(bg);
                accY += m_heights[row];
            }
            accX += m_widths[col];
        }

        for (size_t i = 0; i < m_texts.size(); ++i)
        {
            auto text = m_texts[i];

            const size_t col = i % m_widths.size();
            const size_t row = i / m_widths.size();

            float x = m_position.x;
            for (size_t c = 0; c < col; ++c) x += m_widths[c];
            float y = m_position.y;
            for (size_t r = 0; r < row; ++r) y += m_heights[r];
            const float w = m_widths[col];
            const float h = m_heights[row];

            text.setPosition({ x + w / 2, y + h / 2 });
            text.setFillColor(m_textColor);
            win.draw(text);
        }
    }

private:
    const sf::Font& m_font;
    unsigned int m_fontSize = 0;
    sf::Vector2f m_position;
    std::vector<float> m_widths, m_heights;
    sf::Color m_textColor{ sf::Color::White }, m_bgColor{ 80,80,80 }, m_disabledColor{ 120,120,120 };
    std::vector<sf::Text> m_texts;
    bool m_enabled = true;
    bool m_isPressed = false, m_isDragging = false;
    sf::Vector2f m_dragStartMouse, m_dragStartPos;
};

class ScrollTextBox
{
public:
    ScrollTextBox(const sf::Font& font) : m_font(font) {}

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    
    void setStyle(sf::Color textColor, sf::Color bgColor, sf::Color highlightColor)
    {
        m_textColor = textColor;
        m_bgColor = bgColor;
        m_highlightColor = highlightColor;
    }

    void setRect(sf::Vector2f position, sf::Vector2f size, unsigned int fontSize) {
        m_box.position = position;
        m_box.size     = size;
        m_fontSize = fontSize;
    }

    bool contains(sf::Vector2f p) const { return m_box.contains(p); }

    void clear(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_globalIndexMap.size()))
        {
            m_originLines.clear();
        }
        else
        {
            auto [lineIdx, logSegIdx] = m_globalIndexMap[index];

            if (lineIdx >= static_cast<int>(m_originLines.size()))
            {
                m_originLines.clear();
            }
            else
            {
                const std::string& originalLine = m_originLines[lineIdx];
                auto logicalSegs = splitLogicalLine(originalLine);

                std::string newLine;
                for (int i = 0; i < logSegIdx; ++i)
                {
                    newLine += logicalSegs[i];
                }

                if (newLine.empty())
                {
                    m_originLines.erase(m_originLines.begin() + lineIdx, m_originLines.end());
                }
                else
                {
                    m_originLines[lineIdx] = newLine;
                    m_originLines.erase(m_originLines.begin() + lineIdx + 1, m_originLines.end());
                }
            }
        }

        m_dirty = true;

    }

    void addLine(const std::string& text) {
        m_originLines.push_back(text);
        m_dirty = true;
    }

	void setSelectedIndex(int globalIndex) {
	    refreshAllLines(); 

	    if (globalIndex < 0 || globalIndex >= (int)m_globalIndexMap.size())
	        globalIndex = -1;
        
	    m_selectedGlobalIndex = globalIndex;

	    if (globalIndex != -1) {
	        auto [lineIdx, logSegIdx] = m_globalIndexMap[globalIndex];
	        ensureVisible(lineIdx);
	    }
	}


    int getSelectedIndex() const {
        return m_selectedGlobalIndex;
    }

    bool isHandlingDrag() const {
        return m_drag != DragNone;
    }

    size_t getTotalLines() const {
        return m_originLines.size();
    }

    void setSplitRule(const std::string& prefix, const std::string& delimiter) {
        m_prefixStr = prefix;
        m_delimiterStr = delimiter;
    }

    void appendToLine(const std::string& text) {
        if (m_originLines.empty())
            return;
        
        int index = m_originLines.size() - 1;
        m_originLines[index] += text;
        m_dirty = true;
    }

    void handleEvent(const sf::Event& e, sf::Vector2f mouse)
    {
        if (!m_enabled) return;
        const float resizeZone = 16;

        if (e.is<sf::Event::MouseButtonPressed>()) {
            if (!contains(mouse)) return;
            m_drag = DragNone;

            bool resizeCorner = 
                mouse.x >= m_box.position.x + m_box.size.x - resizeZone &&
                mouse.y >= m_box.position.y + m_box.size.y - resizeZone;

            bool borderArea = 
                !(mouse.x > m_box.position.x + leftPadding &&
                  mouse.x < m_box.position.x + m_box.size.x - leftPadding - scrollWidth &&
                  mouse.y > m_box.position.y + topPadding &&
                  mouse.y < m_box.position.y + m_box.size.y - bottomPadding);

            bool scrollBarArea =
                mouse.x >= m_box.position.x + m_box.size.x - scrollWidth &&
                mouse.x <= m_box.position.x + m_box.size.x;

            if (resizeCorner) {
                m_drag = DragResize;
                m_startMouse = mouse;
                m_startBox = m_box;
            }
            else if (scrollBarArea) {
                m_drag = DragScrollBar;
                updateScrollFromMouseY(mouse.y);
            }
            else if (borderArea) {
                m_drag = DragMove;
                m_startMouse = mouse;
                m_startBox = m_box;
            }
        }
        else if (e.is<sf::Event::MouseButtonReleased>()) {
            m_drag = DragNone;
        }
        else if (e.is<sf::Event::MouseMoved>()) {
            if (m_drag == DragMove) {
                m_box.position.x = m_startBox.position.x + (mouse.x - m_startMouse.x);
                m_box.position.y = m_startBox.position.y + (mouse.y - m_startMouse.y);
            }
            else if (m_drag == DragResize) {
                float w = m_startBox.size.x + (mouse.x - m_startMouse.x);
                float h = m_startBox.size.y + (mouse.y - m_startMouse.y);
                m_box.size.x = std::max(100.f, w);
                m_box.size.y = std::max(100.f, h);
                m_dirty = true;
            }
            else if (m_drag == DragScrollBar) {
                updateScrollFromMouseY(mouse.y);
            }
        }

    }

    void draw(sf::RenderWindow& win) /*const*/
    {
        if (!m_enabled) return;
		
		refreshAllLines();

        sf::RectangleShape bg;
        bg.setSize({m_box.size.x, m_box.size.y});
        bg.setPosition({m_box.position.x, m_box.position.y});
        bg.setFillColor(m_bgColor);
        bg.setOutlineColor(sf::Color(80,80,80));
        bg.setOutlineThickness(1);
        win.draw(bg);

        auto p1 = win.mapCoordsToPixel({m_box.position.x, m_box.position.y});
        auto p2 = win.mapCoordsToPixel({m_box.position.x + m_box.size.x, m_box.position.y + m_box.size.y});
        glEnable(GL_SCISSOR_TEST);
        glScissor(p1.x, win.getSize().y - p2.y, p2.x-p1.x, p2.y-p1.y);
        drawContent(win);
        glDisable(GL_SCISSOR_TEST);

        drawScrollBar(win);
        drawResizeTriangle(win);
    }

    int getClickedGlobalIndex(sf::Vector2f mouse) const {
        if (!contains(mouse)) return -1;

        bool isTextArea =
            mouse.x >= m_box.position.x + leftPadding &&
            mouse.x <= m_box.position.x + m_box.size.x - rightPadding - scrollWidth &&
            mouse.y >= m_box.position.y + topPadding &&
            mouse.y <= m_box.position.y + m_box.size.y - bottomPadding;

        if (!isTextArea) return -1;

        float localY = mouse.y - m_box.position.y + m_scrollY - topPadding;
        float curY = 0, lh = getLineHeight();

        for (int lineIdx = 0; lineIdx < (int)m_paragraphs.size(); ++lineIdx) {
            auto& physicalLines = m_paragraphs[lineIdx];
            float paraH = physicalLines.size() * lh;

            for (int physIdx = 0; physIdx < (int)physicalLines.size(); ++physIdx) {
                float physTop = curY + physIdx * lh;
                float physBot = physTop + lh;

                if (localY >= physTop && localY < physBot) {
                    auto& ranges = m_physicalSegmentRanges[lineIdx][physIdx];
                    float localX = mouse.x - m_box.position.x - leftPadding;

                    sf::Text lineText(m_font);
                    lineText.setCharacterSize(m_fontSize);
                    lineText.setString(physicalLines[physIdx]);
                    lineText.setPosition({m_box.position.x + leftPadding, m_box.position.y + topPadding - m_scrollY + physTop});

                    for (const auto& range : ranges) {
                        auto [logSegIdx, _, __, startInLine, endInLine] = range;
                        sf::Vector2f startPos = lineText.findCharacterPos(startInLine);
                        sf::Vector2f endPos = lineText.findCharacterPos(endInLine);

                        if (localX >= (startPos.x - m_box.position.x - leftPadding) && localX < (endPos.x - m_box.position.x - leftPadding)) {
                            return m_reverseGlobalIndexMap[lineIdx][logSegIdx];
                        }
                    }
                    return -1;
                }
            }
            curY += paraH + paragraphGap;
        }
        return -1;
    }

private:
    enum DragType { DragNone, DragMove, DragResize, DragScrollBar } m_drag = DragNone;

    const sf::Font& m_font;
    sf::FloatRect   m_box;
    std::vector<std::string>          m_originLines;
    mutable std::vector<std::vector<std::string>> m_paragraphs;
    mutable std::vector<std::vector<std::vector<std::tuple<int, size_t, size_t, size_t, size_t>>>> m_physicalSegmentRanges;
    mutable std::vector<std::vector<std::string>> m_logicalSegments;
    mutable std::vector<std::pair<int, int>> m_globalIndexMap;
    mutable std::vector<std::vector<int>> m_reverseGlobalIndexMap;
    mutable float           m_scrollY = 0;
    mutable bool            m_dirty = true;

    bool            m_enabled = true;
    sf::Vector2f    m_startMouse;
    sf::FloatRect   m_startBox;

    unsigned int    m_fontSize = 40;
    static constexpr float paragraphGap    = 14;
    static constexpr float topPadding      = 8;
    static constexpr float bottomPadding   = 8;
    static constexpr float leftPadding     = 8;
    static constexpr float rightPadding    = 8;
    static constexpr float scrollWidth     = 12;

    std::string m_prefixStr;
    std::string m_delimiterStr;
    int m_selectedGlobalIndex = -1;

    sf::Color m_textColor = sf::Color::Black;
    sf::Color m_bgColor = sf::Color::White;
    sf::Color m_highlightColor = sf::Color(0, 120, 215, 150);

    float getLineHeight() const { return m_font.getLineSpacing(m_fontSize); }

    void clampScroll() const {
        float total = getTotalHeight();
        float viewH = m_box.size.y - topPadding - bottomPadding;
        float maxScroll = std::max(0.f, total - viewH);
        m_scrollY = std::clamp(m_scrollY, 0.f, maxScroll);
    }

    void ensureVisible(int idx) const {
        float cur = 0, lh = getLineHeight();
        for (int i=0; i<idx; ++i) 
			cur += m_paragraphs[i].size()*lh + paragraphGap;
        float top = cur, bot = cur + m_paragraphs[idx].size()*lh;
        float viewH = m_box.size.y - topPadding - bottomPadding;
        if (bot - top > viewH) 
			m_scrollY = top;
        else if (top < m_scrollY) 
			m_scrollY = top;
        else if (bot > m_scrollY + viewH) 
			m_scrollY = bot - viewH;
        clampScroll();
    }

    float getTotalHeight() const {
        float h = 0, lh = getLineHeight();
        for (auto& p : m_paragraphs) h += p.size()*lh;
        h += paragraphGap * (m_paragraphs.empty() ? 0 : m_paragraphs.size()-1);
        return h + topPadding + bottomPadding;
    }

    void updateScrollFromMouseY(float mouseY) const {
        float totalH = getTotalHeight();
        float viewH = m_box.size.y - topPadding - bottomPadding;
        float trackHeight = m_box.size.y;

        if (totalH <= viewH) { 
            m_scrollY = 0; 
            return; 
        }

        float barHeight = (viewH / totalH) * trackHeight;
        float availableTrack = trackHeight - barHeight;
        if (availableTrack <= 0) {
            m_scrollY = 0;
            return;
        }

        float mouseOffsetInTrack = mouseY - m_box.position.y;
        float ratio = std::clamp(mouseOffsetInTrack / availableTrack, 0.f, 1.f);
        
        m_scrollY = ratio * (totalH - viewH);
        clampScroll();
    }

    void drawScrollBar(sf::RenderWindow& win) const {
        float totalH = getTotalHeight();
        float viewH  = m_box.size.y - topPadding - bottomPadding;
        if (totalH <= viewH) return;

        float barH = (viewH / totalH) * m_box.size.y;
        float barY = (m_scrollY / totalH) * m_box.size.y;
        barH = std::min(barH, m_box.size.y - 4);
        barY = std::min(barY, m_box.size.y - barH);

        sf::RectangleShape bar({scrollWidth, barH});
        bar.setPosition({m_box.position.x + m_box.size.x - scrollWidth, m_box.position.y + barY});
        bar.setFillColor(sf::Color(180,180,180));
        win.draw(bar);
    }

    void drawResizeTriangle(sf::RenderWindow& win) const {
        float size = 10;
        float x = m_box.position.x + m_box.size.x - size - 2;
        float y = m_box.position.y + m_box.size.y - size - 2;

        sf::ConvexShape triangle(3);
        triangle.setPoint(0, {x,     y+size});
        triangle.setPoint(1, {x+size,y+size});
        triangle.setPoint(2, {x+size,y});
        triangle.setFillColor(sf::Color(120,120,120));
        win.draw(triangle);
    }

    void buildGlobalIndexMap() const {
        m_globalIndexMap.clear();
        m_reverseGlobalIndexMap.clear();
        m_reverseGlobalIndexMap.resize(m_logicalSegments.size());

        int globalIdx = 0;
        for (int lineIdx = 0; lineIdx < (int)m_logicalSegments.size(); ++lineIdx) {
            m_reverseGlobalIndexMap[lineIdx].resize(m_logicalSegments[lineIdx].size());
            for (int logSegIdx = 0; logSegIdx < (int)m_logicalSegments[lineIdx].size(); ++logSegIdx) {
                m_globalIndexMap.emplace_back(lineIdx, logSegIdx);
                m_reverseGlobalIndexMap[lineIdx][logSegIdx] = globalIdx;
                globalIdx++;
            }
        }
    }

    void drawContent(sf::RenderWindow& win) const {
        float x = m_box.position.x + leftPadding;
        float y = m_box.position.y + topPadding - m_scrollY;
        float lh = getLineHeight();
        float cw = m_box.size.x - leftPadding - rightPadding - scrollWidth;

        for (int i=0; i<(int)m_paragraphs.size(); ++i) {
            auto& lines = m_paragraphs[i];
            for (auto& line : lines) {
                sf::Text t(m_font);
                t.setString(line);
                t.setCharacterSize(m_fontSize);
                t.setFillColor(m_textColor);
                t.setPosition({x, y});
                win.draw(t);
                y += lh;
            }
            if (i != (int)m_paragraphs.size()-1) {
                sf::RectangleShape line({cw, 1.0f});
                line.setPosition({x, y + 2});
                line.setFillColor(sf::Color(220,220,220));
                win.draw(line);
                y += paragraphGap;
            }
        }

        drawSegmentHighlights(win, x, m_box.position.y + topPadding - m_scrollY, lh, cw);
    }

    std::vector<std::string> splitLogicalLine(const std::string& line) const {
        std::vector<std::string> segs;

        if (m_delimiterStr.empty()) {
            segs.push_back(line);
            return segs;
        }

        size_t delimiterPos = line.find(m_delimiterStr);
        if (delimiterPos == std::string::npos) {
            segs.push_back(line);
            return segs;
        }

        segs.push_back(line.substr(0, delimiterPos));
        segs.push_back(line.substr(delimiterPos));
        return segs;
    }

    std::pair<std::vector<std::string>, std::vector<std::vector<std::tuple<int, size_t, size_t, size_t, size_t>>>>
    splitLineWithSyncRange(int lineIdx) const {
        std::vector<std::string> physicalLines;
        std::vector<std::vector<std::tuple<int, size_t, size_t, size_t, size_t>>> physicalRanges;

        const float maxW = m_box.size.x - leftPadding - rightPadding - scrollWidth;
        if (maxW <= 0) {
            physicalLines.push_back(m_originLines[lineIdx]);
            physicalRanges.push_back({std::make_tuple(0, 0, m_logicalSegments[lineIdx][0].size(), 0, m_logicalSegments[lineIdx][0].size())});
            return {physicalLines, physicalRanges};
        }

        struct CharWithSeg {
            char c;
            int segIdx;
            size_t charInSeg;
        };
        std::vector<CharWithSeg> charStream;

        const auto& logSegs = m_logicalSegments[lineIdx];
        for (int segIdx = 0; segIdx < (int)logSegs.size(); ++segIdx) {
            const std::string& seg = logSegs[segIdx];
            for (size_t charIdx = 0; charIdx < seg.size(); ++charIdx) {
                charStream.push_back({seg[charIdx], segIdx, charIdx});
            }
        }

        sf::Text t(m_font);
        t.setCharacterSize(m_fontSize);

        std::string currentLine;
        std::vector<std::tuple<int, size_t, size_t, size_t, size_t>> currentRanges;
        float currentLineW = 0;
        size_t charIdx = 0;

        int currentSegIdx = -1;
        size_t currentSegStart = 0;
        size_t currentLineCharCount = 0;
        size_t rangeStartInLine = 0;

        while (charIdx < charStream.size()) {
            const auto& currChar = charStream[charIdx];
            std::string tempLine = currentLine + currChar.c;
            t.setString(tempLine);
            float tempW = t.getLocalBounds().size.x;

            if (tempW <= maxW * 0.98f || currentLine.empty()) {
                currentLine = tempLine;
                currentLineW = tempW;
                currentLineCharCount++;

                if (currentSegIdx == -1) {
                    currentSegIdx = currChar.segIdx;
                    currentSegStart = currChar.charInSeg;
                    rangeStartInLine = currentLineCharCount - 1;
                } else if (currentSegIdx != currChar.segIdx) {
                    currentRanges.emplace_back(
                        currentSegIdx, 
                        currentSegStart, 
                        charStream[charIdx-1].charInSeg + 1,
                        rangeStartInLine,
                        currentLineCharCount - 1
                    );
                    currentSegIdx = currChar.segIdx;
                    currentSegStart = currChar.charInSeg;
                    rangeStartInLine = currentLineCharCount - 1;
                }

                charIdx++;
            } else {
                if (currentSegIdx != -1) {
                    currentRanges.emplace_back(
                        currentSegIdx, 
                        currentSegStart, 
                        charStream[charIdx-1].charInSeg + 1,
                        rangeStartInLine,
                        currentLineCharCount
                    );
                }
                physicalLines.push_back(currentLine);
                physicalRanges.push_back(currentRanges);
                
                currentLine.clear();
                currentRanges.clear();
                currentLineW = 0;
                currentSegIdx = -1;
                currentLineCharCount = 0;
                rangeStartInLine = 0;
            }
        }

        if (!currentLine.empty()) {
            if (currentSegIdx != -1) {
                currentRanges.emplace_back(
                    currentSegIdx, 
                    currentSegStart, 
                    charStream.back().charInSeg + 1,
                    rangeStartInLine,
                    currentLineCharCount
                );
            }
            physicalLines.push_back(currentLine);
            physicalRanges.push_back(currentRanges);
        }

        return {physicalLines, physicalRanges};
    }

    void drawSegmentHighlights(sf::RenderWindow& win, float x, float startY, float lh, float cw) const {
        if (m_selectedGlobalIndex == -1) return;
        auto [selLineIdx, selLogSegIdx] = m_globalIndexMap[m_selectedGlobalIndex];
        
        if (selLineIdx < 0 || selLineIdx >= (int)m_paragraphs.size()) return;
        if (selLogSegIdx < 0 || selLogSegIdx >= (int)m_logicalSegments[selLineIdx].size()) return;
        if (m_physicalSegmentRanges.size() <= selLineIdx) return;

        float lineBaseY = startY;
        for (int i = 0; i < selLineIdx; ++i) {
            lineBaseY += m_paragraphs[i].size() * lh + paragraphGap;
        }

        sf::Text lineText(m_font);
        lineText.setCharacterSize(m_fontSize);
        const auto& physicalLines = m_paragraphs[selLineIdx];
        const auto& allLineRanges = m_physicalSegmentRanges[selLineIdx];
        
        for (int physIdx = 0; physIdx < (int)physicalLines.size(); ++physIdx) {
            if (physIdx >= (int)allLineRanges.size()) continue;
            
            const std::string& physLine = physicalLines[physIdx];
            const auto& lineRanges = allLineRanges[physIdx];
            if (lineRanges.empty()) continue;

            lineText.setString(physLine);
            lineText.setPosition({x, lineBaseY + physIdx * lh});

            for (const auto& range : lineRanges) {
                auto [logSegIdx, _, __, startInLine, endInLine] = range;
                if (logSegIdx != selLogSegIdx) continue;

                sf::Vector2f charStartPos = lineText.findCharacterPos(startInLine);
                sf::Vector2f charEndPos = lineText.findCharacterPos(endInLine);
                
                const float highlightOffset = 2.f;
                sf::RectangleShape highlight;
                highlight.setSize({charEndPos.x - charStartPos.x, lh});
                highlight.setPosition({charStartPos.x, lineBaseY + physIdx * lh + highlightOffset});
                highlight.setFillColor(m_highlightColor);
                win.draw(highlight);
            }
        }
    }

	void refreshAllLines() {
	    if (m_dirty) {
	        m_paragraphs.clear();
	        m_physicalSegmentRanges.clear();
	        m_logicalSegments.clear();
	        for (const auto& line : m_originLines) {
	            m_logicalSegments.push_back(splitLogicalLine(line));
	        }
	        for (int lineIdx = 0; lineIdx < (int)m_originLines.size(); ++lineIdx) {
	            auto [physLines, physRanges] = splitLineWithSyncRange(lineIdx);
	            m_paragraphs.push_back(physLines);
	            m_physicalSegmentRanges.push_back(physRanges);
	        }
	        buildGlobalIndexMap();
	        m_dirty = false;

	        clampScroll(); 
	    }

	}

};

struct SmoothMover {
	sf::Vector2f position;
	sf::Vector2f target;
	bool  enableShake = false;

	int   state = 0;

	float smoothFactor = 0.12f;
	float initShakeAmp = 15.0f;
	float shakeFreq = 18.0f;
	float shakeDecay = 0.92f;
	float currentShakeAmp = 0.0f;
	float time = 0.0f;

	static constexpr float ARRIVE = 0.5f;
	static constexpr float SHAKE_STOP = 0.5f;

	void setup(const sf::Vector2f& pos, const sf::Vector2f& tgt, bool shake) {
		position = pos;
		target = tgt;
		enableShake = shake;
		state = 1;
		time = 0.0f;
		currentShakeAmp = initShakeAmp;
	}

	bool refresh(float dt) {
		if (state == 0) return false;

		if (state == 1) {
			sf::Vector2f diff = target - position;
			float dist = std::sqrt(diff.x*diff.x + diff.y*diff.y);

			if (dist < ARRIVE) {
				position = target;
				if (enableShake) {
					state = 2;
					currentShakeAmp = initShakeAmp;
				} else {
					state = 0;
				}
				return state == 2;
			}
			position += diff * smoothFactor;
			return true;
		}

		if (state == 2) {
			time += dt;
			float shake = std::sin(time * shakeFreq) * currentShakeAmp;
			position.y = target.y + shake;

			currentShakeAmp *= shakeDecay;
			if (currentShakeAmp < SHAKE_STOP) {
				position = target;
				state = 0;
				return false;
			}
			return true;
		}
		return false;
	}
};