#pragma once
#include <iostream>
#include <fstream>
#include "5dc_widgets.h"
#include "minifiledialog.h"

struct FiveDChessUiControls {
	
	FiveDChessData& m_data;
	SmoothMover m_mover;
	Button m_btnList{FiveDChessTexture::m_font};
	ScrollTextBox m_historyBox{FiveDChessTexture::m_font};
	ScrollTextBox m_branchBox{FiveDChessTexture::m_font};
	
	FiveDChessUiControls(FiveDChessData& data) : m_data(data) {
		
		m_btnList.setGrid({10, 10}, {350,50,50,50,50,50, 50,50,50,50}, {50}, 36);

		m_historyBox.setRect({10, 70}, {800, 90}, 36);
		m_branchBox.setRect({850, 10}, {400, 150}, 36);
		
		m_historyBox.setSplitRule(".", "/");
		
		m_branchBox.setEnabled(false);
		m_historyBox.setEnabled(false);
		
		// m_data.presentColor has inited
		m_btnList.setText(0, std::string(m_data.player_is_ai[m_data.presentColor] ? "AI " : "HU ") + std::string(m_data.presentColor ? "Black's Move" : "White's Move"));
		m_btnList.setText(1, "\xE2\x97\x81");      // prev
		m_btnList.setText(2, "\xE2\x96\xB7");      // next
		m_btnList.setText(3, "\xF0\x9F\x92\xA1");  // hint
		m_btnList.setText(4, "\xF0\x9F\x93\xA4");  // upload
		m_btnList.setText(5, "\xF0\x9F\x93\xA5");  // loadload

		m_btnList.setText(6, (m_data.player_is_ai[0] ? "AI" : "HU"));  // player0 is az
		m_btnList.setText(7, (m_data.player_is_ai[1] ? "AI" : "HU"));  // player1 is az
		m_btnList.setText(8, "N");  // new az thread
		m_btnList.setText(9, "P");  // stop az thread

	}
	
	bool handleUiEvent(sf::RenderWindow &window, const sf::Event& event)
	{
		sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
		bool retValue = (m_btnList.contains(mousePos) || m_btnList.isHandlingDrag())
		|| (m_historyBox.contains(mousePos) || m_historyBox.isHandlingDrag()) && m_historyBox.isEnabled()
		|| (m_branchBox.contains(mousePos) || m_branchBox.isHandlingDrag()) && m_branchBox.isEnabled();

		// event process
		m_btnList.handleEvent(event, mousePos);
		m_historyBox.handleEvent(event, mousePos);
		m_branchBox.handleEvent(event, mousePos);

		//if (event.is<sf::Event::MouseButtonPressed>()) {
		if (event.is<sf::Event::MouseButtonReleased>()) {

			auto btnIndex = m_btnList.getIndexAt(mousePos) ;

			if (btnIndex == 0) { // btnMatchStatus
				m_data.flip = !m_data.flip;
				std::cout << TS << "m_btnMatchStatus Click : " << std::endl;
			}

			if (btnIndex == 1) { // btnPrev
				m_data.prevEnable = true;
				EngineInterface::visit_parent(m_data.engine);
				std::cout << TS << "m_btnPrev Click : " << std::endl;
			}
			if (btnIndex == 2) { // btnNext
				
				int nextIndex = m_historyBox.getSelectedIndex() + 1; //(m_data.presentTurn << 1 | (int)m_data.presentColor) - 2;
				int childIdx = m_branchBox.getSelectedIndex();
				if ((m_data.nextEnable = (childIdx < m_data.child_actions.size()))) {

					EngineInterface::visit_child(m_data.engine, std::get<0>(m_data.child_actions[childIdx]));
					if ((nextIndex >= m_data.history_actions.size())
						|| (std::get<0>(m_data.child_actions[childIdx]) != std::get<0>(m_data.history_actions[nextIndex]))) {
						m_data.following_actions = EngineInterface::get_following_actions(m_data.engine);
						// assign {begin - nextIndex}
						if (nextIndex < m_data.history_actions.size()) {
							m_data.history_actions.assign(m_data.history_actions.begin(), m_data.history_actions.begin() + nextIndex);
						}
						// refresh all m_historyBox
						m_data.history_actions.insert(m_data.history_actions.end(), m_data.following_actions.begin(), m_data.following_actions.end());
						m_historyBox.clear(0);
						for (int i = 0; i < m_data.history_actions.size(); ++i) {
							if (!(i & 1)) {
								m_historyBox.addLine(std::to_string((i >> 1) + 1) + ". " + std::get<1>(m_data.history_actions[i]));
							} else {
								m_historyBox.appendToLine(" / " + std::get<1>(m_data.history_actions[i]));
							}
						}
						std::cout << TS << "refresh history by selected child action." << std::endl;
					}
				}
				std::cout << TS << "m_btnNext Click : level=" << nextIndex << " child=" << childIdx << std::endl;
			}
				
			if (btnIndex == 3) { // btnHint
				m_data.hintEnable = EngineInterface::suggest_action(m_data.engine);
				std::cout << TS << "m_btnHint Click : " << std::endl;
			}
			
			if (btnIndex == 4) { // btnLoad
				std::cout << TS << "m_btnLoad Click : " << std::endl;
				
				std::string path = minifiledialog::open_file(
					"5DCUI Load File",
					".",
					{{"5dPgn Files", "*.5dpgn"}, {"All Files", "*"}}
				);
				if ((m_data.loadEnable = !path.empty())) {
					std::ifstream inFile(path);
					std::string content = std::string((std::istreambuf_iterator<char>(inFile)), 
									  std::istreambuf_iterator<char>());
					auto t0 = std::chrono::system_clock::now();
					m_data.engine = EngineInterface::from_pgn(content);
					auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - t0).count();
					std::cout << TS << "Open from file :" << path << ", from_pgn=" << ms << "ms." << std::endl;
					
					// refresh all m_historyBox
					//t0 = std::chrono::system_clock::now();
					m_data.history_actions = EngineInterface::get_historical_actions(m_data.engine);
					//ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - t0).count();
					//std::cout << TS << "get_historical_actions = " << ms << "ms."<< std::endl;
					m_historyBox.clear(0);
					for (int i = 0; i < m_data.history_actions.size(); ++i) {
						if (!(i & 1)) {
							m_historyBox.addLine(std::to_string((i >> 1) + 1) + ". " + std::get<1>(m_data.history_actions[i]));
						} else {
							m_historyBox.appendToLine(" / " + std::get<1>(m_data.history_actions[i]));
						}
					}

				}	
			}
			if (btnIndex == 5) {  // btnSave
				std::cout << TS << "m_btnSave Click : " << std::endl;
				
				std::string path = minifiledialog::save_file(
					"5DCUI Save File",
					".",
					std::format("5DC_{:%Y%m%d_%H%M%S}.5dpgn", std::chrono::system_clock::now()),// default name
					{{"5dPgn Files", "*.5dpgn"}, {"All Files", "*"}}
				);
				if ((m_data.saveEnable = !path.empty())) {
					auto t0 = std::chrono::system_clock::now();
					std::string content = EngineInterface::show_pgn(m_data.engine);
					auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - t0).count();
					std::cout << TS << "Save to file : " << path << ", show_pgn=" << ms << "ms" << std::endl;
					std::ofstream outFile(path);
					outFile << content;
				}
			}
			
			if (btnIndex == 6) {  // player 0 is az
				m_data.playerAichange = true;
				if (m_data.presentTurn == 1 && m_data.presentColor == false) {
					m_data.player_is_ai[0] = !m_data.player_is_ai[0];
				}
				std::cout << TS << "m_btnPlayer0 Click : TC = " << m_data.presentTurn << "," <<  m_data.presentColor << std::endl;
			}
			if (btnIndex == 7) {  // player 1 is az
			    m_data.playerAichange = true;
				if (m_data.presentTurn == 1 && m_data.presentColor == false) {
					m_data.player_is_ai[1] = !m_data.player_is_ai[1];
				}
				std::cout << TS << "m_btnPlayer1 Click : TC = " << m_data.presentTurn << "," <<  m_data.presentColor << std::endl;
			}
			if (btnIndex == 8) {  // new az
				m_data.playerAichange = true;
				if (m_data.presentTurn == 1 && m_data.presentColor == false) {
					AIZeroConfig cfg;
					cfg.player0_is_ai = m_data.player_is_ai[0];
					cfg.player1_is_ai = m_data.player_is_ai[1];
					StartAI(cfg);
				}
				std::cout << TS << "m_btnNew Click : TC = " << m_data.presentTurn << "," <<  m_data.presentColor << std::endl;
			}
			if (btnIndex == 9) {  // stop is az
				m_data.playerAichange = true;
				StopAI();
				std::cout << TS << "m_btnStop Click : " << std::endl;
			}
			
			auto historyActionIndex = m_historyBox.getClickedGlobalIndex(mousePos) ;
			if (historyActionIndex >= 0) {
				std::cout << TS << "m_historyBox.getClickedGlobalIndex() : " << historyActionIndex << std::endl;
			}
			auto childActionIndex = m_branchBox.getClickedGlobalIndex(mousePos) ;
			if (childActionIndex >= 0) {
				m_branchBox.setSelectedIndex(childActionIndex);
				std::cout << TS << "m_branchBox.setSelectedIndex() : " << childActionIndex << std::endl;
			}
		}

		return retValue;
	}
	
	void update() {
		// show status update
		auto isInChecking = !m_data.phantomCheckArrowsList.empty();
		
		if (m_data.matchStatus == EngineInterface::PLAYING) {
			if (isInChecking) m_btnList.setText(0, std::string(m_data.player_is_ai[!m_data.presentColor] ? "AI " : "HU ") 
				                                 + std::string(!m_data.presentColor ? "Black Checking" : "White Checking")); 
			else m_btnList.setText(0, std::string(m_data.player_is_ai[m_data.presentColor] ? "AI " : "HU ") 
			                        + std::string(m_data.presentColor ? "Black's Move" : "White's Move"));
		} else if (m_data.matchStatus == EngineInterface::STALEMATE) {
			m_btnList.setText(0, std::string("Stalemate"));
		} else {
			m_btnList.setText(0, std::string(m_data.player_is_ai[!m_data.presentColor] ? "AI " : "HU ")
                               + std::string(!m_data.presentColor ? "Black CheckMate!" : "White CheckMate!"));
		}
		m_btnList.setText(1, "\xE2\x97\x81");      // prev
		m_btnList.setText(2, "\xE2\x96\xB7");      // next
		m_btnList.setText(3, "\xF0\x9F\x92\xA1");  // hint
		m_btnList.setText(4, "\xF0\x9F\x93\xA4");  // upload
		m_btnList.setText(5, "\xF0\x9F\x93\xA5");  // loadload
		
		m_btnList.setText(6, (m_data.player_is_ai[0] ? "AI" : "HU"));  // player0 is az
		m_btnList.setText(7, (m_data.player_is_ai[1] ? "AI" : "HU"));  // player1 is az
		m_btnList.setText(8, "N");  // new az thread
		m_btnList.setText(9, "P");  // stop az thread
		
		//post-processing for events
		int preIdx = m_data.currentLevel - 1;
		if (m_data.submitEnable) {
			std::cout << TS << "currentLevel = " << m_data.currentLevel << std::endl;
			if (preIdx < m_data.history_actions.size()) {
				std::cout << TS << "history[" << preIdx << "] has " << std::get<1>(m_data.history_actions[preIdx]) << std::endl;
				m_data.history_actions.erase(m_data.history_actions.begin() + preIdx, m_data.history_actions.end());
				m_historyBox.clear(preIdx);
			}
			m_data.history_actions.push_back(m_data.submited_action);
			// Addline to m_historyBox
			if (m_data.presentColor) {
				m_historyBox.addLine(std::to_string((preIdx >> 1) + 1) + ". " + std::get<1>(m_data.submited_action));
			} else {
				m_historyBox.appendToLine(" / " + std::get<1>(m_data.submited_action));
			}
			//m_historyBox.setSelectedIndex(prevIndex);
			m_data.submited_action = {};
		}
		
		// file dialog process
		if (m_data.loadEnable) {
			// nothing Load post-processing to do	  
		}
		if (m_data.saveEnable) {
			// nothing Save post-processing to do
		}
		if (m_data.prevEnable) {
			// nothing Prev post-processing to do
		}
		if (m_data.nextEnable) {
			// nothing Next post-processing to do
			m_data.nexted_action = {};
		}
		// refresh m_branchBox list and history box status
		if (m_data.submitEnable || m_data.prevEnable || m_data.nextEnable ||  m_data.hintEnable || m_data.loadEnable) {
			m_branchBox.clear(0);
			m_data.child_actions = EngineInterface::get_child_actions(m_data.engine);
			for (int i = 0; i < m_data.child_actions.size(); ++i) {
				m_branchBox.addLine(std::to_string(i+1) + ". " + std::get<1>(m_data.child_actions[i]));

				if ((preIdx + 1) < m_data.history_actions.size()) {
					if (std::get<0>(m_data.child_actions[i]) == std::get<0>(m_data.history_actions[preIdx + 1])) {
						m_branchBox.setSelectedIndex(i);
					}
				}
			}
			
			m_branchBox.setEnabled(m_branchBox.getTotalLines() > 0);//(m_data.hintEnable? 0: 1));
			m_historyBox.setSelectedIndex(preIdx);
			m_historyBox.setEnabled(m_historyBox.getTotalLines() > 0);
		}
		
		if (m_data.focusEnable == true) {
			//focus next
			auto focusL = m_data.mandatoryLines.empty() ? 0 : m_data.mandatoryLines[m_data.focusIndex++ % m_data.mandatoryLines.size()];
			focusL = m_data.flip ? -focusL : focusL;
			auto focusTC = ((m_data.presentTurn << 1) | (int)m_data.presentColor);
			auto isInChecking = !m_data.phantomCheckArrowsList.empty();
			m_mover.setup(m_data.cameraPos, {(focusTC + 0.5f) * m_data.cellSizeX, (focusL + 0.5f) * m_data.cellSizeY}, isInChecking);
		}
		
		// clean all flags for Events node
		// -- donnot do it here

	}

};
