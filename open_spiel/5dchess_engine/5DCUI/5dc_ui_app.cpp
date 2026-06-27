
#define TS []() -> const char* { \
    static thread_local char buf[24]; \
    auto now = std::chrono::system_clock::now(); \
    std::time_t t = std::chrono::system_clock::to_time_t(now); \
    std::strftime(buf, sizeof(buf), "[%F %T] ", std::localtime(&t)); \
    return buf; \
}()

#include "5dc_alpha_zero_torch.h"
#include "5dc_ui_app.h"

// Nanosvg SVG rasterizer
#define NANOSVG_ALL_COLOR_KEYWORDS
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

#define MINIFILEDIALOG_IMPLEMENTATION
#include "minifiledialog.h"

// ===================== FiveDChessTexture - Implementation =====================

sf::Texture FiveDChessTexture::loadSVGFromPath(const std::string& svgPath, int targetSize) {
    const sf::Vector2u texSize(static_cast<unsigned int>(targetSize), static_cast<unsigned int>(targetSize));

    NSVGimage* svg = nsvgParseFromFile(svgPath.c_str(), "px", 96.0f);
    if (!svg) {
        std::cout << TS << "Failed to load SVG: " << svgPath << std::endl;
        sf::Image defaultImage(texSize, sf::Color(200, 200, 200));
        return sf::Texture(defaultImage);
    }

    float svgWidth = static_cast<float>(svg->width);
    float svgHeight = static_cast<float>(svg->height);
    float scale = static_cast<float>(targetSize) / std::max(svgWidth, svgHeight);

    std::vector<unsigned char> pixels(static_cast<size_t>(texSize.x) * static_cast<size_t>(texSize.y) * 4, 0);

    NSVGrasterizer* rast = nsvgCreateRasterizer();
    nsvgRasterize(rast, svg, 0.0f, 0.0f, scale, pixels.data(),
                  static_cast<int>(texSize.x), static_cast<int>(texSize.y),
                  static_cast<int>(texSize.x * 4));

    sf::Image svgImage(texSize, pixels.data());
    sf::Texture texture(svgImage);
    texture.setSmooth(true);

    nsvgDeleteRasterizer(rast);
    nsvgDelete(svg);
    return texture;
}

sf::Texture FiveDChessTexture::loadSVGFromStr(const std::string& svgContent, int targetSize) {
    const sf::Vector2u texSize(static_cast<unsigned int>(targetSize), static_cast<unsigned int>(targetSize));

    if (svgContent.empty()) {
        std::cout << TS << "SVG content pointer is null!" << std::endl;
        sf::Image defaultImage(texSize, sf::Color(200, 200, 200));
        return sf::Texture(defaultImage);
    }

    std::string svgCopy(svgContent);
    NSVGimage* svg = nsvgParse(svgCopy.data(), "px", 96.0f);
    if (!svg) {
        std::cout << TS << "Failed to parse SVG string!" << std::endl;
        sf::Image defaultImage(texSize, sf::Color(200, 200, 200));
        return sf::Texture(defaultImage);
    }

    float svgWidth = static_cast<float>(svg->width);
    float svgHeight = static_cast<float>(svg->height);
    float scale = static_cast<float>(targetSize) / std::max(svgWidth, svgHeight);

    std::vector<unsigned char> pixels(static_cast<size_t>(texSize.x) * static_cast<size_t>(texSize.y) * 4, 0);

    NSVGrasterizer* rast = nsvgCreateRasterizer();
    nsvgRasterize(rast, svg, 0.0f, 0.0f, scale, pixels.data(),
                  static_cast<int>(texSize.x), static_cast<int>(texSize.y),
                  static_cast<int>(texSize.x * 4));

    sf::Image svgImage(texSize, pixels.data());
    sf::Texture texture(svgImage);
    texture.setSmooth(true);

    nsvgDeleteRasterizer(rast);
    nsvgDelete(svg);
    return texture;
}

// ===================== Utility Functions - Coordinate Conversion =====================

sf::Vector2f screenToWorld(const sf::Vector2i& screenPos, const sf::Vector2f& windowSize,
                           const sf::Vector2f& cameraPos, float cameraZoom) {
    sf::Vector2f normalizedPos(
        static_cast<float>(screenPos.x) - windowSize.x / 2.0f,
        static_cast<float>(screenPos.y) - windowSize.y / 2.0f
    );
    normalizedPos /= cameraZoom;
    return cameraPos + normalizedPos;
}

// World position -> Cell tuple (yIdx,xIdx,y,x) | Return nullopt if out of valid board range
std::optional<std::tuple<int, int, int, int>> worldToCellTuple(FiveDChessData& m_data, const sf::Vector2f& worldPos) {
    int xIdx = static_cast<int>(std::floor(worldPos.x / m_data.cellSizeX));
    int yIdx = static_cast<int>(std::floor(worldPos.y / m_data.cellSizeY));

    sf::Vector2f localInBigCell(
        worldPos.x - static_cast<float>(xIdx) * m_data.cellSizeX,
        worldPos.y - static_cast<float>(yIdx) * m_data.cellSizeY
    );

    float smallBoardOffsetX = (m_data.cellSizeX - m_data.smallCellSizeX * static_cast<float>(m_data.boardGridX)) / 2.0f;
    float smallBoardOffsetY = (m_data.cellSizeY - m_data.smallCellSizeY * static_cast<float>(m_data.boardGridY)) / 2.0f;

    sf::Vector2f localInSmallBoard(localInBigCell.x - smallBoardOffsetX, localInBigCell.y - smallBoardOffsetY);
    
    if (localInSmallBoard.x < 0 || localInSmallBoard.y < 0 ||
        localInSmallBoard.x > m_data.smallCellSizeX * m_data.boardGridX ||
        localInSmallBoard.y > m_data.smallCellSizeY * m_data.boardGridY)
        return std::nullopt;
    
    int x = static_cast<int>(std::floor(localInSmallBoard.x / m_data.smallCellSizeX));
    int y = static_cast<int>(std::floor(localInSmallBoard.y / m_data.smallCellSizeY));
    
    if (x < 0 || x >= m_data.boardGridX || y < 0 || y >= m_data.boardGridY)
        return std::nullopt;
    
    return std::make_tuple(yIdx, xIdx, y, x);
}

// ===================== Mouse Event Callbacks =====================

void onMouseClick(FiveDChessData &m_data, const sf::Vector2f& worldPos, const sf::Vector2i& screenPos) {
	
	if (m_data.player_is_ai[m_data.presentColor]) {
		return;
	}
	
    auto cellTupleOpt = worldToCellTuple(m_data, worldPos);
    if (!cellTupleOpt.has_value()) {
		return;
	}
	// handle_click
	auto [LL, TC, BY, BX] = cellTupleOpt.value();
	// FLIP: L Y X when not flipped (game expects top-to-bottom 0-7)
	auto [L, T, C, Y, X] = std::make_tuple(
		m_data.flip ? -LL : LL, 
		(TC >> 1), (bool)(TC & 1),
		m_data.flip ? BY : (m_data.boardGridY -1 - BY), 
		m_data.flip ? (m_data.boardGridX -1 - BX) : BX);
	EngineInterface::vec4_t pos = std::make_tuple(L, T, Y, X);
	auto it1 = std::find(m_data.movablePositions.begin(), m_data.movablePositions.end(), pos);
	
	if (it1 != m_data.movablePositions.end()) {
		EngineInterface::ext_move_t efm = std::make_tuple(m_data.slectedPosition, pos, EngineInterface::QUEEN_W);
        auto a = EngineInterface::ext_move_to_action_id(m_data.engine, efm);
		m_data.applyEnable = EngineInterface::apply_move(m_data.engine, efm);
		if (m_data.applyEnable) {
			m_data.human_played_actions.push_back(a);
			std::cout << TS << "Human action:" << a << ", applied:" << EngineInterface::em_string(efm) << std::endl;
		}			
		m_data.slectedPosition = {0, 0, 0, 0};
	} else if (C == (m_data.presentColor)) {
		m_data.slectedPosition = pos;
	} else {
		m_data.slectedPosition = {0, 0, 0, 0};
	}
	m_data.selectEnable = true;
    
}

void onMouseRightClick(FiveDChessData &m_data, const sf::Vector2f& worldPos, const sf::Vector2i& screenPos) {
	
	if (m_data.player_is_ai[m_data.presentColor]) {
		return;
	}
	
	// canceled click
    if (m_data.slectedPosition != std::tuple(0, 0, 0, 0)) {
        m_data.slectedPosition = {0, 0, 0, 0};
        std::cout << TS << "deselect done" << std::endl;
    } else {
		// undo
        m_data.undoEnable = EngineInterface::undo(m_data.engine);
        if (m_data.undoEnable) {
            std::cout << TS << "undo done" << std::endl;
			m_data.human_played_actions.pop_back();
        }
    }
}

void onMouseDblClick(FiveDChessData &m_data, const sf::Vector2f& worldPos, const sf::Vector2i& screenPos) {
	
	if (m_data.player_is_ai[m_data.presentColor]) {
		return;
	}
	
    // submit
    if (EngineInterface::currently_check(m_data.engine)) {
		return;
	}
	
	bool can_pass;
	bool round_over = EngineInterface::big_round_over(m_data.engine, can_pass); 
	if (round_over || can_pass) {
        if (!round_over && can_pass) {
            // push PASS to AZ, then do submit
            m_data.human_played_actions.push_back(0);
            std::cout << TS << "Human action: PASS." << std::endl;
        }
		auto c_act = EngineInterface::get_cached_action(m_data.engine);
        m_data.submitEnable = EngineInterface::submit(m_data.engine);
		if (m_data.submitEnable) {
			std::cout << TS << "submit: {" << std::get<1>(c_act) << "}" << std::endl;
			// focus next
			m_data.focusIndex = 0;
			m_data.focusEnable = true;
			// has submited cached action
			m_data.submited_action = c_act;
		}
	}
}

void onMiddleClick(FiveDChessData& m_data, const sf::Vector2f& worldPos, const sf::Vector2i& screenPos) {
    m_data.focusEnable = true;
}

void FiveDChessEvent::handleAiEvent(const sf::Event& event, const sf::Vector2f& windowSize) {
	
	if (!m_data.player_is_ai[m_data.presentColor]) {
		return;
	}
	
	// if current_player is az 
	// push cached human action to az, than pull az action
	if (!m_data.human_played_actions.empty()) {
		int a = m_data.human_played_actions.front();
		if (TryPushHumanAction(!m_data.presentColor, a)) {
			m_data.human_played_actions.pop_front();
		}
		return;
	}
	
	int ai_act;
    if (TryPopAIAction(m_data.presentColor, &ai_act)) {
        auto efm = EngineInterface::action_id_to_ext_move(m_data.engine, ai_act);
        bool is_pass_move = std::get<1>(efm) == std::make_tuple(0, 0, 0, 0);
        if (is_pass_move) {
            m_data.applyEnable = true;  // PASS MOVE
        }
        else {
            m_data.applyEnable = EngineInterface::apply_move(m_data.engine, efm);
        }
		if (m_data.applyEnable) {
			std::cout << TS << "AI action:" << ai_act << ", applied:" << EngineInterface::em_string(efm) << std::endl;
			
			// try to submit 
			bool can_pass;
			bool round_over = EngineInterface::big_round_over(m_data.engine, can_pass); 
			if (round_over || can_pass && is_pass_move) {
				auto c_act = EngineInterface::get_cached_action(m_data.engine);
				m_data.submitEnable = EngineInterface::submit(m_data.engine);
				if (m_data.submitEnable) {
					std::cout << TS << "AI submit: {" << std::get<1>(c_act) << "}" << std::endl;
					// focus next
					m_data.focusIndex = 0;
					m_data.focusEnable = true;
					// has submited cached action
					m_data.submited_action = c_act;
				}
			}
		}	
	}
}
// ===================== FiveDChessEvent - Implementation =====================

void FiveDChessEvent::handleHumanEvent(const sf::Event& event, const sf::Vector2f& windowSize) {
    // Mouse button pressed
    if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mousePressed->button == sf::Mouse::Button::Left) {
            m_isDragging = true;
            m_leftPressStartPos = m_lastMousePos = mousePressed->position;

            // Double click check: Time + position tolerance
            TimePoint now = std::chrono::steady_clock::now();
            auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastLeftClickTime).count();
            int posDiffX = std::abs(mousePressed->position.x - m_lastLeftClickPos.x);
            int posDiffY = std::abs(mousePressed->position.y - m_lastLeftClickPos.y);

            if (timeDiff <= DBL_CLICK_INTERVAL && posDiffX <= DBL_CLICK_OFFSET && posDiffY <= DBL_CLICK_OFFSET) {
                if (m_dblClickCb) {
                    sf::Vector2f worldPos = screenToWorld(mousePressed->position, windowSize, m_data.cameraPos, m_data.cameraZoom);
                    m_dblClickCb(m_data, worldPos, mousePressed->position);
                }
                m_isDblClickTriggered = true;
            } else {
                m_isDblClickTriggered = false;
            }
            m_lastLeftClickTime = now;
            m_lastLeftClickPos = mousePressed->position;
        } else if (mousePressed->button == sf::Mouse::Button::Right) {
            if (m_rightClickCb) {
                sf::Vector2f worldPos = screenToWorld(mousePressed->position, windowSize, m_data.cameraPos, m_data.cameraZoom);
                m_rightClickCb(m_data, worldPos, mousePressed->position);
            }
        } else if (mousePressed->button == sf::Mouse::Button::Middle) {
            if (m_middleClickCb) {
                sf::Vector2f worldPos = screenToWorld(mousePressed->position, windowSize, m_data.cameraPos, m_data.cameraZoom);
                m_middleClickCb(m_data, worldPos, mousePressed->position);
            }
        }
        return;
    }

    // Mouse button released
    if (const auto* mouseReleased = event.getIf<sf::Event::MouseButtonReleased>()) {
        if (mouseReleased->button == sf::Mouse::Button::Left) {
            m_isDragging = false; 
            int moveDiffX = std::abs(mouseReleased->position.x - m_leftPressStartPos.x);
            int moveDiffY = std::abs(mouseReleased->position.y - m_leftPressStartPos.y);
            bool isDragRelease = (moveDiffX > DRAG_MIN_OFFSET) || (moveDiffY > DRAG_MIN_OFFSET);
            if (isDragRelease == false) {
                if (!m_isDblClickTriggered && m_clickCb) {
                    sf::Vector2f worldPos = screenToWorld(mouseReleased->position, windowSize, m_data.cameraPos, m_data.cameraZoom);
                    m_clickCb(m_data, worldPos, mouseReleased->position);
                }
            }
        }
        return;
    }

    // Mouse move: Camera drag
    if (const auto* mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
        if (m_isDragging) {
            m_data.cameraPos -= sf::Vector2f(mouseMoved->position - m_lastMousePos) / m_data.cameraZoom;
            m_lastMousePos = mouseMoved->position;
        }
        return;
    }

    // Mouse wheel: Camera zoom (clamped 0.1~10x)
    if (const auto* mouseWheel = event.getIf<sf::Event::MouseWheelScrolled>()) {
        if (mouseWheel->wheel == sf::Mouse::Wheel::Vertical) {
            m_data.cameraZoom = std::clamp(m_data.cameraZoom * (mouseWheel->delta > 0 ? 1.1f : 0.9f), 0.1f, 10.f);
        }
        return;
    }
}

// ===================== FiveDChessRenderer - Implementation =====================

void FiveDChessRenderer::drawArrowOne(sf::RenderWindow& window, const sf::Vector2f& startCenter,
    const sf::Vector2f& endCenter, const sf::Transform& transform,
    float cameraZoom, const sf::Color& arrowColor) {
    
    // World base constants [1:1 zoom]
    const float BASE_ARROW_HEAD_SIZE = 15.0f;
    const float BASE_ARROW_LINE_THICK = 10.0f;
    const float MIN_WORLD_ARROW_LENGTH = 30.0f;
    const float ARROW_HEAD_OVERHANG = 1.1f;
    const float NATURAL_BEND = 0.12f;
    const int SMOOTH_SEGMENTS = 16;

    // World length check (zoom-independent) - skip short arrows
    const sf::Vector2f worldDir = endCenter - startCenter;
    const float worldArrowLength = std::sqrt(worldDir.x * worldDir.x + worldDir.y * worldDir.y);
    if (worldArrowLength < MIN_WORLD_ARROW_LENGTH) {
        return;
    }
    const sf::Vector2f worldDirNormal = worldDir / worldArrowLength;

    const sf::Vector2f perpWorldDir = sf::Vector2f(-worldDirNormal.y, worldDirNormal.x);
    const sf::Vector2f midPoint = (startCenter + endCenter) / 2.0f;
    const sf::Vector2f controlPoint = midPoint + perpWorldDir * worldArrowLength * NATURAL_BEND;

    sf::Vector2f endTangent = endCenter - controlPoint;
    const float tangentLen = std::max(std::sqrt(endTangent.x * endTangent.x + endTangent.y * endTangent.y), 0.001f);
    endTangent /= tangentLen;
    const sf::Vector2f arrowPerp = sf::Vector2f(-endTangent.y, endTangent.x);

    const float headPerpOffset = BASE_ARROW_LINE_THICK * ARROW_HEAD_OVERHANG;
    const sf::Vector2f curveRealEnd = endCenter - endTangent * BASE_ARROW_HEAD_SIZE;
    const sf::Vector2f lineEndL = curveRealEnd - arrowPerp * BASE_ARROW_LINE_THICK * 0.5f;
    const sf::Vector2f lineEndR = curveRealEnd + arrowPerp * BASE_ARROW_LINE_THICK * 0.5f;

    const sf::Vector2f arrowTip = endCenter;
    const sf::Vector2f arrowHeadL = curveRealEnd - arrowPerp * headPerpOffset;
    const sf::Vector2f arrowHeadR = curveRealEnd + arrowPerp * headPerpOffset;

    // Draw bezier shaft (TriangleStrip) - seamless joint
    sf::VertexArray curveLine(sf::PrimitiveType::TriangleStrip, SMOOTH_SEGMENTS * 2);
    for (int i = 0; i < SMOOTH_SEGMENTS; ++i) {
        const float t = static_cast<float>(i) / (SMOOTH_SEGMENTS - 1);
        const sf::Vector2f currPos = (i == SMOOTH_SEGMENTS - 1) ? curveRealEnd
            : (1 - t) * (1 - t) * startCenter + 2 * (1 - t) * t * controlPoint + t * t * curveRealEnd;

        curveLine[i * 2].position = currPos + arrowPerp * BASE_ARROW_LINE_THICK * 0.5f;
        curveLine[i * 2 + 1].position = currPos - arrowPerp * BASE_ARROW_LINE_THICK * 0.5f;
        curveLine[i * 2].color = arrowColor;
        curveLine[i * 2 + 1].color = arrowColor;
    }
    // Force seamless shaft-arrowhead joint
    curveLine[(SMOOTH_SEGMENTS - 1) * 2].position = lineEndR;
    curveLine[(SMOOTH_SEGMENTS - 1) * 2 + 1].position = lineEndL;

    // Draw shaft + triangular arrowhead
    window.draw(curveLine, transform);
    sf::VertexArray arrowHead(sf::PrimitiveType::Triangles, 3);
    arrowHead[0].position = arrowTip;
    arrowHead[1].position = arrowHeadL;
    arrowHead[2].position = arrowHeadR;
    for (std::size_t i = 0; i < 3; ++i) {
        arrowHead[i].color = arrowColor;
    }
    window.draw(arrowHead, transform);
}

void FiveDChessRenderer::render(sf::RenderWindow& window, const sf::Vector2f& windowSize) {
    window.clear(sf::Color::White);

    // Calculate visible range (perf opt: only draw in view)
    sf::Vector2f visibleSize = windowSize / m_data.cameraZoom;
    sf::Vector2f visibleMin = m_data.cameraPos - visibleSize / 2.f;
    sf::Vector2f visibleMax = m_data.cameraPos + visibleSize / 2.f;
    int startX = static_cast<int>(std::floor(visibleMin.x / m_data.cellSizeX) - 1);
    int endX = static_cast<int>(std::ceil(visibleMax.x / m_data.cellSizeX) + 1);
    int startY = static_cast<int>(std::floor(visibleMin.y / m_data.cellSizeY) - 1);
    int endY = static_cast<int>(std::ceil(visibleMax.y / m_data.cellSizeY) + 1);

    // Global camera transform (calculate once per frame)
    m_transform = sf::Transform::Identity;
    m_transform.translate(windowSize / 2.f);
    m_transform.scale(sf::Vector2f(m_data.cameraZoom, m_data.cameraZoom));
    m_transform.translate(-m_data.cameraPos);

    // Draw in layer order: Background -> Boards/Pieces -> Arrows
    drawBackgroundGrid(window, startX, endX, startY, endY, m_transform);
    drawBoardsAndPieces(window, startX, endX, startY, endY, m_transform, m_data.cameraZoom);
    drawArrowList(window, startX, endX, startY, endY, m_transform, m_data.cameraZoom);
}

void FiveDChessRenderer::drawBackgroundGrid(sf::RenderWindow& window, int startX, int endX, int startY, int endY,
                                            const sf::Transform& transform) {
    const float cellW = m_data.cellSizeX;
    const float cellH = m_data.cellSizeY;
    sf::RenderStates states(transform);

    // 1. Base checkerboard: nested loops (easy to read) + VertexArray (1 draw)
    sf::VertexArray grid(sf::PrimitiveType::Triangles);
    grid.resize((endX - startX + 1) * (endY - startY + 1) * 6);
    std::size_t idx = 0;

    for (int xIdx = startX; xIdx <= endX; ++xIdx) {
        for (int yIdx = startY; yIdx <= endY; ++yIdx) {
            const sf::Color color = ((((xIdx >> 1) + yIdx) % 2 == 0) ? m_data.COLOR_DARK : m_data.COLOR_LIGHT);
            const float x = xIdx * cellW;
            const float y = yIdx * cellH;

            grid[idx++] = sf::Vertex({x, y}, color);
            grid[idx++] = sf::Vertex({x + cellW, y}, color);
            grid[idx++] = sf::Vertex({x + cellW, y + cellH}, color);
            grid[idx++] = sf::Vertex({x, y}, color);
            grid[idx++] = sf::Vertex({x + cellW, y + cellH}, color);
            grid[idx++] = sf::Vertex({x, y + cellH}, color);
        }
    }
    window.draw(grid, states);

    // 2. Row highlights
    sf::RectangleShape rowShape({(endX - startX + 1) * cellW, cellH});
    auto drawRow = [&](int y, const sf::Color& c) {
        if (y < startY || y > endY) return;
		y = m_data.flip ? -y : y;
        rowShape.setPosition({startX * cellW, y * cellH});
        rowShape.setFillColor(c);
        window.draw(rowShape, states);
    };
    for (int y : m_data.unplayableLines) drawRow(y, m_data.COLOR_UNPLAYABLE);
    for (int y : m_data.optionalLines) drawRow(y, m_data.COLOR_OPTIONAL);
    for (int y : m_data.mandatoryLines) drawRow(y, m_data.COLOR_MANDATORY);

    // 3. Column highlight
    const int presentCol = ((m_data.presentTurn << 1) | static_cast<int>(m_data.presentColor));
    if (presentCol >= startX && presentCol <= endX) {
        sf::RectangleShape colShape({cellW, (endY - startY + 1) * cellH});
        colShape.setPosition({presentCol * cellW, startY * cellH});
        colShape.setFillColor(m_data.HIGHLIGHT_PRESENT_COLOR);
        window.draw(colShape, states);
    }

    // 4. Coordinate text
    m_coordText.setCharacterSize(cellW * 0.09f);
    if (startX <= 0 && 0 <= endX) {
        for (int yIdx = startY; yIdx <= endY; ++yIdx) {
            m_coordText.setFillColor((((0 >> 1) + yIdx) % 2 != 0) ? m_data.COLOR_DARK : m_data.COLOR_LIGHT);
            m_coordText.setString("L" + std::to_string(m_data.flip ? -yIdx : yIdx));
            m_coordText.setPosition({0 * cellW, (yIdx + 0.45f) * cellH});
            window.draw(m_coordText, states);
        }
    }
    if (startY <= 0 && 0 <= endY) {
        for (int xIdx = startX; xIdx <= endX; ++xIdx) {
            if (xIdx % 2 != 1) continue;
            m_coordText.setFillColor((((xIdx >> 1) + 0) % 2 != 0) ? m_data.COLOR_DARK : m_data.COLOR_LIGHT);
            m_coordText.setString("T" + std::to_string(xIdx / 2));
            m_coordText.setPosition({(xIdx - 0.05f) * cellW, (0 + 0.9f) * cellH});
            window.draw(m_coordText, states);
        }
    }
}

void FiveDChessRenderer::drawBoardsAndPieces(sf::RenderWindow& window, int startX, int endX, int startY, int endY,
                                             const sf::Transform& transform, float cameraZoom) {
    std::vector<sf::Vertex> batchBoardEdges;
    std::vector<sf::Vertex> batchSmallCells;
    std::vector<sf::Vertex> batchHighlights;
    std::unordered_map<const sf::Texture*, std::vector<sf::Vertex>> batchPieces;
    batchPieces.reserve(32);

    const float pieceWorldMaxSize = std::max(m_data.pieceSizeX, m_data.pieceSizeY);
    const float pieceScreenPixelSize = std::max(0.0f, pieceWorldMaxSize * cameraZoom);
    const auto& lodTextures = FiveDChessTexture::chooseLOD(pieceScreenPixelSize);
    const int presentColorInt = (int)m_data.presentColor;

    // pre reserve size
    const std::size_t totalBoards = m_data.boardList.size() + m_data.phantomBoardList.size();
    const std::size_t totalCells = totalBoards * m_data.boardGridX * m_data.boardGridY;
    
    batchBoardEdges.reserve(totalBoards * 6);
    batchSmallCells.reserve(totalCells * 6);
    batchHighlights.reserve((m_data.movablePieces.size() + m_data.movablePositions.size() + 1) * 6);

    auto processList = [&](const auto& list, bool isPhantom) {
        const sf::Color phC = isPhantom ? m_data.COLOR_PHANTOM_MUL : m_data.COLOR_NORMAL_MUL;
        
        const float cellSizeX = m_data.cellSizeX;
        const float cellSizeY = m_data.cellSizeY;
        const float boardEdgeSizeX = m_data.boardEdgeSizeX;
        const float boardEdgeSizeY = m_data.boardEdgeSizeY;
        const float smallCellSizeX = m_data.smallCellSizeX;
        const float smallCellSizeY = m_data.smallCellSizeY;
        const int boardGridX = m_data.boardGridX;
        const int boardGridY = m_data.boardGridY;
        const float pieceSizeX = m_data.pieceSizeX;
        const float pieceSizeY = m_data.pieceSizeY;

        for (size_t i = 0; i < list.size(); ++i) {
            const auto& tpl = list[i];
            const int l = std::get<0>(tpl);
            const int t_val = std::get<1>(tpl);
            const int c = std::get<2>(tpl);
            const std::string& b = std::get<3>(tpl);

            const int yIdx = m_data.flip ? -l : l;
            const int xIdx = ((t_val) << 1) | (int)(c);

			// A . drawing edges
            {
                sf::Vector2f edgePos = {
                    xIdx * cellSizeX + (cellSizeX - boardEdgeSizeX) / 2,
                    yIdx * cellSizeY + (cellSizeY - boardEdgeSizeY) / 2
                };
                sf::Color edgeColor = (c ? m_data.BOARD_EDGE_BLACK : m_data.BOARD_EDGE_WHITE) * phC;
                sf::Vector2f size(boardEdgeSizeX, boardEdgeSizeY);
                
                sf::Vector2f p1 = edgePos;
                sf::Vector2f p2 = edgePos + sf::Vector2f(size.x, 0);
                sf::Vector2f p3 = edgePos + size;
                sf::Vector2f p4 = edgePos + sf::Vector2f(0, size.y);
                
                batchBoardEdges.emplace_back(p1, edgeColor);
                batchBoardEdges.emplace_back(p2, edgeColor);
                batchBoardEdges.emplace_back(p3, edgeColor);
                batchBoardEdges.emplace_back(p1, edgeColor);
                batchBoardEdges.emplace_back(p3, edgeColor);
                batchBoardEdges.emplace_back(p4, edgeColor);
            }

            // B . drawing small cells
            float smallGridOffsetX = xIdx * cellSizeX + (cellSizeX - smallCellSizeX * boardGridX) / 2;
            float smallGridOffsetY = yIdx * cellSizeY + (cellSizeY - smallCellSizeY * boardGridY) / 2;
            
            for (int x = 0; x < boardGridX; x++) {
                const float baseX = smallGridOffsetX + x * smallCellSizeX;
                for (int y = 0; y < boardGridY; y++) {
                    sf::Vector2f cellPos(baseX, smallGridOffsetY + y * smallCellSizeY);
                    sf::Color cellColor = (((x + y) % 2 == 1) ? m_data.SMALL_BOARD_DARK : m_data.SMALL_BOARD_LIGHT) * phC;
                    sf::Vector2f size(smallCellSizeX, smallCellSizeY);
                    
                    sf::Vector2f p1 = cellPos;
                    sf::Vector2f p2 = cellPos + sf::Vector2f(size.x, 0);
                    sf::Vector2f p3 = cellPos + size;
                    sf::Vector2f p4 = cellPos + sf::Vector2f(0, size.y);
                    
                    batchSmallCells.emplace_back(p1, cellColor);
                    batchSmallCells.emplace_back(p2, cellColor);
                    batchSmallCells.emplace_back(p3, cellColor);
                    batchSmallCells.emplace_back(p1, cellColor);
                    batchSmallCells.emplace_back(p3, cellColor);
                    batchSmallCells.emplace_back(p4, cellColor);
                }
            }

			// C. for drawing Pieces
            int board_x = 0;
            int board_y = 0;
            for (char ch : b) {
                if (ch == '/') {
                    board_y++;
                    board_x = 0;
                    if (board_y >= boardGridY) break;
                    continue;
                }
                if (isdigit(static_cast<unsigned char>(ch))) {
                    board_x += (ch - '0');
                } else {
				    // drawing sgv
                    if (board_x >= 0 && board_x < boardGridX && board_y >= 0 && board_y < boardGridY) {
                        auto texIt = lodTextures.find(ch);
                        if (texIt != lodTextures.end()) {
                            const sf::Texture& tex = texIt->second;
                            sf::Vector2f texSize(tex.getSize());
                            
                            int render_x = m_data.flip ? m_data.boardGridX - 1 - board_x : board_x;
							int render_y = m_data.flip ? m_data.boardGridY - 1 - board_y : board_y;
                            
                            sf::Vector2f smallCellCenterPos(
                                smallGridOffsetX + render_x * smallCellSizeX + smallCellSizeX / 2,
                                smallGridOffsetY + render_y * smallCellSizeY + smallCellSizeY / 2
                            );
                            sf::Vector2f drawSize(pieceSizeX, pieceSizeY);
                            sf::Vector2f drawPos = smallCellCenterPos - drawSize / 2.0f;
                            sf::Color pieceColor = phC;

                            auto& vec = batchPieces[&tex];
                            
                            sf::Vector2f p1 = drawPos;
                            sf::Vector2f p2 = drawPos + sf::Vector2f(drawSize.x, 0);
                            sf::Vector2f p3 = drawPos + drawSize;
                            sf::Vector2f p4 = drawPos + sf::Vector2f(0, drawSize.y);
                            sf::Vector2f t1(0, 0);
                            sf::Vector2f t2(texSize.x, 0);
                            sf::Vector2f t3(texSize.x, texSize.y);
                            sf::Vector2f t4(0, texSize.y);

                            vec.emplace_back(p1, pieceColor, t1);
                            vec.emplace_back(p2, pieceColor, t2);
                            vec.emplace_back(p3, pieceColor, t3);
                            vec.emplace_back(p1, pieceColor, t1);
                            vec.emplace_back(p3, pieceColor, t3);
                            vec.emplace_back(p4, pieceColor, t4);
                        }
                    }
                    board_x++;
                }
            }
        }
    };

	// boards fill
    processList(m_data.boardList, false);
    processList(m_data.phantomBoardList, true);

	// hight light fill
    auto addHighlight = [&](const auto& posTuple, const sf::Color& color) {
        int l = m_data.flip ? -std::get<0>(posTuple) : std::get<0>(posTuple);
        int t = std::get<1>(posTuple);
        int localX = m_data.flip ? m_data.boardGridX - 1 - std::get<3>(posTuple) : std::get<3>(posTuple);
        int localY = m_data.flip ? std::get<2>(posTuple) : m_data.boardGridY - 1 - std::get<2>(posTuple);

        if (localX < 0 || localX >= m_data.boardGridX || localY < 0 || localY >= m_data.boardGridY) return;

        int yIdx = l;
        int xIdx = ((t) << 1) | presentColorInt;

        float smallGridOffsetX = xIdx * m_data.cellSizeX + (m_data.cellSizeX - m_data.smallCellSizeX * m_data.boardGridX) / 2;
        float smallGridOffsetY = yIdx * m_data.cellSizeY + (m_data.cellSizeY - m_data.smallCellSizeY * m_data.boardGridY) / 2;
        
        sf::Vector2f cellPos(smallGridOffsetX + localX * m_data.smallCellSizeX, smallGridOffsetY + localY * m_data.smallCellSizeY);
        sf::Vector2f size(m_data.smallCellSizeX, m_data.smallCellSizeY);
        
        sf::Vector2f p1 = cellPos;
        sf::Vector2f p2 = cellPos + sf::Vector2f(size.x, 0);
        sf::Vector2f p3 = cellPos + size;
        sf::Vector2f p4 = cellPos + sf::Vector2f(0, size.y);
        
        batchHighlights.emplace_back(p1, color);
        batchHighlights.emplace_back(p2, color);
        batchHighlights.emplace_back(p3, color);
        batchHighlights.emplace_back(p1, color);
        batchHighlights.emplace_back(p3, color);
        batchHighlights.emplace_back(p4, color);
    };

    for (const auto& mp : m_data.movablePieces) addHighlight(mp, m_data.COLOR_MOVABLE_PIECE);
    for (const auto& mv : m_data.movablePositions) addHighlight(mv, m_data.COLOR_MOVABLE_POS);
    addHighlight(m_data.slectedPosition, m_data.COLOR_SELECTED_POS);

	// l1 draw edges
    if (!batchBoardEdges.empty()) {
        window.draw(batchBoardEdges.data(), batchBoardEdges.size(), sf::PrimitiveType::Triangles, transform);
    }
	// l2 draw small cell
    if (!batchSmallCells.empty()) {
        window.draw(batchSmallCells.data(), batchSmallCells.size(), sf::PrimitiveType::Triangles, transform);
    }
	// l3 draw hightlight
    if (!batchHighlights.empty()) {
        window.draw(batchHighlights.data(), batchHighlights.size(), sf::PrimitiveType::Triangles, transform);
    }
	// l4 draw Pieces
    for (auto& [texPtr, vec] : batchPieces) {
        if (vec.empty()) continue;
        sf::RenderStates states;
        states.transform = transform;
        states.texture = texPtr;
        window.draw(vec.data(), vec.size(), sf::PrimitiveType::Triangles, states);
    }
}

void FiveDChessRenderer::drawArrowList(sf::RenderWindow& window, int startX, int endX, int startY, int endY,
                                    const sf::Transform& transform, float cameraZoom) {
    auto processArrows = [&](const auto& arrowList, bool isCheck, bool isGlobaled = false) {
        for (const auto& arrow : arrowList) {
            const auto& startTuple = arrow.first;
            const auto& endTuple = arrow.second;

            const int presentColor = isCheck ? (int)!m_data.presentColor : (int)m_data.presentColor;
            const sf::Color& color = isCheck ? m_data.COLOR_CHECK_ARROW_CELL : m_data.COLOR_TRACE_ARROW_CELL;

            int sYIdx = m_data.flip ? -std::get<0>(startTuple) : std::get<0>(startTuple);
            int sXIdx = ((std::get<1>(startTuple) << 1) | presentColor);
            int eYIdx = m_data.flip ? -std::get<0>(endTuple) : std::get<0>(endTuple);
            int eXIdx = ((std::get<1>(endTuple) << 1) | presentColor);
			
			if (isGlobaled) {
				sXIdx = std::get<1>(startTuple);
				eXIdx = std::get<1>(endTuple);
			}

            sf::Vector2f sCellPos(sXIdx * m_data.cellSizeX, sYIdx * m_data.cellSizeY);
            sf::Vector2f startCenter(
                sCellPos.x + (m_data.cellSizeX - m_data.smallCellSizeX * m_data.boardGridX) / 2 + (m_data.flip ? m_data.boardGridX - 1 - std::get<3>(startTuple) : std::get<3>(startTuple)) * m_data.smallCellSizeX + m_data.smallCellSizeX / 2,
                sCellPos.y + (m_data.cellSizeY - m_data.smallCellSizeY * m_data.boardGridY) / 2 + (m_data.flip ? std::get<2>(startTuple) : m_data.boardGridY - 1 - std::get<2>(startTuple)) * m_data.smallCellSizeY + m_data.smallCellSizeY / 2
            );

            sf::Vector2f eCellPos(eXIdx * m_data.cellSizeX, eYIdx * m_data.cellSizeY);
            sf::Vector2f endCenter(
                eCellPos.x + (m_data.cellSizeX - m_data.smallCellSizeX * m_data.boardGridX) / 2 + (m_data.flip ? m_data.boardGridX - 1 - std::get<3>(endTuple) : std::get<3>(endTuple)) * m_data.smallCellSizeX + m_data.smallCellSizeX / 2,
                eCellPos.y + (m_data.cellSizeY - m_data.smallCellSizeY * m_data.boardGridY) / 2 + (m_data.flip ? std::get<2>(endTuple) : m_data.boardGridY - 1 - std::get<2>(endTuple)) * m_data.smallCellSizeY + m_data.smallCellSizeY / 2
            );

            drawArrowOne(window, startCenter, endCenter, transform, cameraZoom, color);
        }
    };

    processArrows(m_data.checkArrowsList, true);
    processArrows(m_data.phantomCheckArrowsList, true);
    processArrows(m_data.traceArrowsList, false);
	processArrows(m_data.edgesArrowsList, false, true);
}

// ===================== FiveDChessUI - Implementation =====================

void FiveDChessUI::run() {
	
	//AIZeroConfig cfg;
	//cfg.player0_is_ai = m_data.player_is_ai[0];
	//cfg.player1_is_ai = m_data.player_is_ai[1];
	//StartAI(cfg);

    while (m_window.isOpen()) {
        m_windowSize = sf::Vector2f(
            static_cast<float>(m_window.getSize().x), 
            static_cast<float>(m_window.getSize().y)
        );

        std::optional<sf::Event> eventOpt;
        while ((eventOpt = m_window.pollEvent())) {
            if (eventOpt->is<sf::Event::Closed>()) {
                m_window.close();
                continue;
            }
            if (const auto* resized = eventOpt->getIf<sf::Event::Resized>()) {
                sf::FloatRect visibleArea(
                    sf::Vector2f(0.f, 0.f),
                    sf::Vector2f(static_cast<float>(resized->size.x), static_cast<float>(resized->size.y))
                );
                m_window.setView(sf::View(visibleArea));
                continue;
            }

            if (m_controls.handleUiEvent(m_window, *eventOpt) == false) {
                m_inputHandler.handleHumanEvent(*eventOpt, m_windowSize);
				m_inputHandler.handleAiEvent(*eventOpt, m_windowSize);
            }
			
            if (m_data.hasAnyFlag()) {
                m_data.update();
                m_controls.update();
                m_data.cleanAllflags();
            }
        }
		
        if (m_controls.m_mover.refresh(m_clock.restart().asSeconds()) == true) {
            m_data.cameraPos = m_controls.m_mover.position;
        }

        m_renderer.render(m_window, m_windowSize);

        m_controls.m_btnList.draw(m_window);
        m_controls.m_historyBox.draw(m_window);
        m_controls.m_branchBox.draw(m_window);
		
        m_window.display();
    }
	
	//StopAI();
}

// ===================== Program Entry Point =====================

int main() {
    //std::ofstream log_file("5DCUI.log", std::ios::out | std::ios::trunc);
    //if (log_file.is_open()) {
    //    log_file.rdbuf()->pubsetbuf(nullptr, 0);
    //    std::cout.rdbuf(log_file.rdbuf());
    //}
	
    FiveDChessUI fDCUI;
    fDCUI.run();
    return 0;
}