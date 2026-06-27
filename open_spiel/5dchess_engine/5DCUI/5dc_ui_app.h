#pragma once
#include <SFML/Graphics.hpp>
#include "5dc_interface.h"
#include "5dc_global_data.h"
#include "5dc_event.h"
#include "5dc_texture.h"
#include "5dc_renderer.h"
#include "5dc_ui_controls.h"

// Mouse event callbacks
void onMouseClick(FiveDChessData& m_data, const sf::Vector2f& worldPos, const sf::Vector2i& screenPos);
void onMouseRightClick(FiveDChessData& m_data, const sf::Vector2f& worldPos, const sf::Vector2i& screenPos);
void onMouseDblClick(FiveDChessData& m_data, const sf::Vector2f& worldPos, const sf::Vector2i& screenPos);
void onMiddleClick(FiveDChessData& m_data, const sf::Vector2f& worldPos, const sf::Vector2i& screenPos);

// ===================== 5D Chess Core UI Controller =====================
class FiveDChessUI
{
public:
    // Init core modules, SFML window, camera params and register mouse callbacks
    FiveDChessUI()
	{
		m_window.create(
			sf::VideoMode(sf::Vector2u(1280, 720)),
			"5D Chess UI V0.1",
			sf::Style::Default
		);
		m_window.setFramerateLimit(60);
		m_windowSize = sf::Vector2f(
			static_cast<float>(m_window.getSize().x), 
			static_cast<float>(m_window.getSize().y)
		);

        m_inputHandler.setClickCallback(onMouseClick);
        m_inputHandler.setRightClickCallback(onMouseRightClick);
        m_inputHandler.setDblClickCallback(onMouseDblClick);
        m_inputHandler.setMiddleClickCallback(onMiddleClick);
    }

    // Main program loop entry
    void run();

private:
    // SFML Window & Core Modules
    sf::RenderWindow m_window;       // Main SFML window
	sf::Vector2f m_windowSize;       // Window size cache (perf opt)
	sf::Clock m_clock;
    
	FiveDChessData m_data;           // Global config & data (owner)
    FiveDChessEvent m_inputHandler{m_data};  // Input event handler
    FiveDChessRenderer m_renderer{m_data};   // Graphic renderer
	FiveDChessUiControls m_controls{m_data};

};