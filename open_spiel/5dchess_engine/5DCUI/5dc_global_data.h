#pragma once
#include <any>
#include <deque>
// ===================== Global Configuration Constants =====================
struct FiveDChessData {

	// Core engine object
    std::any engine = {};
	
	//size define 
	int boardGridX = 8;
	int boardGridY = 8;

	// Large board cell size (X/Y)
	float cellSizeX = 400.f;
	float cellSizeY = 400.f;

	float smallCellSizeX = cellSizeX / (boardGridX + 2);
	float smallCellSizeY = cellSizeY / (boardGridY + 2);
	float boardEdgeSizeX = smallCellSizeX * (boardGridX + 1);
	float boardEdgeSizeY = smallCellSizeY * (boardGridY + 1);
	float pieceSizeX = smallCellSizeX * 0.85f;
	float pieceSizeY = smallCellSizeY * 0.85f;

    sf::Vector2f cameraPos = sf::Vector2f((cellSizeX * 2.5f),(cellSizeY * 0.5f));
    float cameraZoom = (1.f);
	
	// Global vertical flip control (true = flip L Y X axis, false = original)
	// true: Black at bottom, White at top | false: White at bottom, Black at top
	bool flip = true;

    // Board highlight - Row (priority) + Column | Tuple: {T, C, Row, Col}
    std::vector<int> mandatoryLines = {0};
    std::vector<int> optionalLines = {-1};
    std::vector<int> unplayableLines = {-2};
	
	bool selectEnable = false;
	bool applyEnable = false;
	bool undoEnable = false;
	bool submitEnable = false;
					
	bool hintEnable = false;
	bool prevEnable = false;
	bool nextEnable = false;
	bool loadEnable = false;
	bool saveEnable = false;
	bool focusEnable = false;

	int focusIndex = 0;
	int currentLevel = 0;
    int presentTurn = 1;
    bool presentColor = false;
	EngineInterface::match_status_t matchStatus = EngineInterface::PLAYING;

    // Board data | Tuple: {L, T, C, FENString}
    std::vector<std::tuple<int, int, bool, std::string>> boardList = {
    };

    // Movable pieces (L T Y X)  Add presentColor to draw
    std::vector<std::tuple<int, int, int, int>> movablePieces = {
    };

	// Selected piece position (L T Y X)  Add presentColor to draw
    std::tuple<int, int, int, int> slectedPosition = {0, 0, 0, 0};  
    std::vector<std::tuple<int, int, int, int>> movablePositions = {
    };

    // Normal arrow list | Pair: {StartPos(L T Y X), EndPos(L T Y X)}  Add presentColor to draw
    std::vector<std::pair<std::tuple<int, int, int, int>, std::tuple<int, int, int, int>>> checkArrowsList = {
    };

    // Trace arrow list | Pair: {StartPos(L T Y X), EndPos(L T Y X)}  Add presentColor to draw
    std::vector<std::pair<std::tuple<int, int, int, int>, std::tuple<int, int, int, int>>> traceArrowsList = {
    };
    std::vector<std::pair<std::tuple<int, int, int, int>, std::tuple<int, int, int, int>>> edgesArrowsList = {
    };
	
	std::vector<std::tuple<int, int, bool, std::string>> phantomBoardList = {
    };
	std::vector<std::pair<std::tuple<int, int, int, int>, std::tuple<int, int, int, int>>> phantomCheckArrowsList = {
    };
	
	std::vector<std::tuple<EngineInterface::action_t, std::string>> child_actions = {
	};
	std::vector<std::tuple<EngineInterface::action_t, std::string>> history_actions = {
	};

	std::vector<std::tuple<EngineInterface::action_t, std::string>> following_actions = {
	};

	std::tuple<EngineInterface::action_t, std::string> submited_action = {
	};
	std::tuple<EngineInterface::action_t, std::string> nexted_action = {
	};
	
	//default t0 fen
	const std::string T0_FEN = ""
	"[Size \"8x8\"]\n"
	"[Board \"custom\"]\n"
	"[r*nbqk*bnr*/p*p*p*p*p*p*p*p*/8/8/8/8/P*P*P*P*P*P*P*P*/R*NBQK*BNR*:0:0:b]\n"
	"[r*nbqk*bnr*/p*p*p*p*p*p*p*p*/8/8/8/8/P*P*P*P*P*P*P*P*/R*NBQK*BNR*:0:1:w]\n";

	// Background grid colors
    const sf::Color COLOR_LIGHT = sf::Color(250, 250, 250, 255);
    const sf::Color COLOR_DARK = sf::Color(235, 235, 235, 255);

    // Small board colors
    const sf::Color SMALL_BOARD_LIGHT = sf::Color(200, 200, 200, 255);
    const sf::Color SMALL_BOARD_DARK = sf::Color(100, 100, 100, 255);

    // Board edge colors (RGBA, full opacity)
    const sf::Color BOARD_EDGE_WHITE = sf::Color(255, 255, 255, 255);
    const sf::Color BOARD_EDGE_BLACK = sf::Color(0, 0, 0, 255);

    const sf::Color COLOR_MANDATORY = sf::Color(180, 200, 255, 120);
    const sf::Color COLOR_OPTIONAL = sf::Color(200, 255, 200, 120);
    const sf::Color COLOR_UNPLAYABLE = sf::Color(255, 200, 200, 120);
	
	const sf::Color HIGHLIGHT_PRESENT_COLOR = sf::Color(255, 220, 180, 120);

    // Highlight colors (RGBA: last = opacity 0-255, semi-transparent)
    const sf::Color COLOR_MOVABLE_PIECE = sf::Color(128, 0, 128, 120);
    const sf::Color COLOR_MOVABLE_POS = sf::Color(0, 128, 0, 120);
    const sf::Color COLOR_SELECTED_POS = sf::Color(255, 182, 193, 120);
    const sf::Color COLOR_CHECK_ARROW_CELL = sf::Color(255, 0, 0, 120);
    const sf::Color COLOR_TRACE_ARROW_CELL = sf::Color(139, 69, 19, 120);
	const sf::Color COLOR_NORMAL_MUL = sf::Color(255, 255, 255, 255);
	const sf::Color COLOR_PHANTOM_MUL = sf::Color(255, 255, 255, 50);

	FiveDChessData() {
		//init engine 
		engine = EngineInterface::from_pgn(T0_FEN);
		// init game variables
		std::tie(presentTurn, presentColor) = EngineInterface::get_current_present(engine);
		std::tie(mandatoryLines, optionalLines, unplayableLines) = EngineInterface::get_current_timeline_status(engine);

		matchStatus = EngineInterface::get_match_status(engine);
		boardList = EngineInterface::get_current_boards(engine);
		movablePieces = EngineInterface::get_movable_pieces(engine);
	}

	void update() {
		
		//update size variables
		std::tie(boardGridX, boardGridY) = EngineInterface::get_board_size(engine);
		cellSizeX = 400.f;
		cellSizeY = 400.f;

		smallCellSizeX = cellSizeX / (boardGridX + 2);
		smallCellSizeY = cellSizeY / (boardGridY + 2);
		boardEdgeSizeX = smallCellSizeX * (boardGridX + 1);
		boardEdgeSizeY = smallCellSizeY * (boardGridY + 1);
		pieceSizeX = smallCellSizeX * 0.85f;
		pieceSizeY = smallCellSizeY * 0.85f;

		// update game variables
		std::tie(presentTurn, presentColor) = EngineInterface::get_current_present(engine);
		std::tie(mandatoryLines, optionalLines, unplayableLines) = EngineInterface::get_current_timeline_status(engine);
		std::tie(phantomBoardList, phantomCheckArrowsList) = EngineInterface::get_phantom_boards_and_checks(engine);
		
		matchStatus = EngineInterface::get_match_status(engine);
		currentLevel = EngineInterface::get_current_level(engine);
		boardList = EngineInterface::get_current_boards(engine);
		movablePieces = EngineInterface::get_movable_pieces(engine);
		movablePositions = EngineInterface::gen_move_if_playable(engine, slectedPosition);
		checkArrowsList = EngineInterface::get_current_checks(engine);
		traceArrowsList = {};
		edgesArrowsList = EngineInterface::get_current_boards_edges(engine);
		
		//child_actions = EngineInterface::get_child_actions(engine);
		//history_actions = EngineInterface::get_historical_actions(engine);

	}
	
	bool hasAnyFlag() {
		return selectEnable
		|| applyEnable
		|| undoEnable
		|| submitEnable

		|| hintEnable
		|| prevEnable
		|| nextEnable
		|| loadEnable
		|| saveEnable
		|| focusEnable 
		|| playerAichange
		;
	}
	
	void cleanAllflags() {
		selectEnable = false;
		applyEnable = false;
		undoEnable = false;
		submitEnable = false;

		hintEnable = false;
		prevEnable = false;
		nextEnable = false;
		loadEnable = false;
		saveEnable = false;
		focusEnable = false;
		playerAichange = false;
	}
	
	bool playerAichange = false;
	bool player_is_ai[2] = {false, false};
	std::deque<int> human_played_actions;

};