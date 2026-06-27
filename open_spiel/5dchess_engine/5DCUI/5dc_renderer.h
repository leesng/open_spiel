#pragma once

// ===================== 5D Chess Renderer =====================
class FiveDChessRenderer
{
public:
    // Bind global data, init drawable shapes, load textures
    FiveDChessRenderer(FiveDChessData &data)
    : m_data(data), m_coordText(FiveDChessTexture::m_font)
    {
        FiveDChessTexture::loadAll();
        //highlightShape.setOutlineThickness(0.f);
    }

    // Core render: Draw all visible elements
    void render(sf::RenderWindow& window, const sf::Vector2f& windowSize);

private:
    FiveDChessData& m_data; // Global chess data ref

    sf::Transform m_transform;            // Camera transform matrix
	
	sf::Text m_coordText;
	
    // Private draw functions
    void drawBackgroundGrid(sf::RenderWindow& window, int startX, int endX, int startY, int endY, const sf::Transform& transform);
    void drawBoardsAndPieces(sf::RenderWindow& window, int startX, int endX, int startY, int endY, const sf::Transform& transform, float cameraZoom);
    void drawArrowList(sf::RenderWindow& window, int startX, int endX, int startY, int endY, const sf::Transform& transform, float cameraZoom);
    void drawArrowOne(sf::RenderWindow& window, const sf::Vector2f& startCenter, const sf::Vector2f& endCenter, const sf::Transform& transform, float cameraZoom, const sf::Color& arrowColor);
};