#pragma once
#include <any>
#include <functional>
#include <string>
#include <map>

// ===================== Engine Interface Constants =====================
// Final scoped interface: Static function callbacks only, instantiation disabled
struct EngineInterface final {
	
	using piece_t = unsigned char;
	static const piece_t NO_PIECE;
	static const piece_t WALL_PIECE;
	static const piece_t KING_UW;
	static const piece_t ROOK_UW;
	static const piece_t PAWN_UW;
	static const piece_t KING_UB;
	static const piece_t ROOK_UB;
	static const piece_t PAWN_UB;
	static const piece_t KING_W;
	static const piece_t QUEEN_W;
	static const piece_t BISHOP_W;
	static const piece_t KNIGHT_W;
	static const piece_t ROOK_W;
	static const piece_t PAWN_W;
	static const piece_t UNICORN_W;
	static const piece_t DRAGON_W;
	static const piece_t BRAWN_W;
	static const piece_t PRINCESS_W;
	static const piece_t ROYAL_QUEEN_W;
	static const piece_t COMMON_KING_W;
	static const piece_t KING_B;
	static const piece_t QUEEN_B;
	static const piece_t BISHOP_B;
	static const piece_t KNIGHT_B;
	static const piece_t ROOK_B;
	static const piece_t PAWN_B;
	static const piece_t UNICORN_B;
	static const piece_t DRAGON_B;
	static const piece_t BRAWN_B;
	static const piece_t PRINCESS_B;
	static const piece_t ROYAL_QUEEN_B;
	static const piece_t COMMON_KING_B;

	using match_status_t = int;
	static const match_status_t PLAYING;
	static const match_status_t WHITE_WINS;
	static const match_status_t BLACK_WINS;
	static const match_status_t STALEMATE;

	// vec4
	using vec4_t = std::tuple<int, int, int, int>;
	static const std::function<std::string(vec4_t &)> v4_string;
	
	// ext_move
	using ext_move_t = std::tuple<std::tuple<int, int, int, int>, std::tuple<int, int, int, int>, piece_t>;
	static const std::function<std::string(ext_move_t &)> em_string;

	using action_t = std::vector<ext_move_t>;
	static const std::function<std::string(action_t &)> act_string;

	// engine
	static const std::function<std::map<std::string, std::string>& (std::any&)> metadata; 
	static const std::function<std::any(const std::string &input)> from_pgn;
    static const std::function<std::pair<int, int>(std::any &)> get_board_size;
	
	static const std::function<std::pair<int, bool>(std::any &)> get_current_present;
	static const std::function<std::vector<std::tuple<int, int, bool, std::string>>(std::any &)> get_current_boards;
	static const std::function<std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> (std::any &)> get_current_timeline_status;
	static const std::function<std::vector<std::pair<vec4_t, vec4_t>>(std::any &)> get_current_checks;
	static const std::function<std::pair<std::vector<std::tuple<int, int, bool, std::string>>,
	std::vector<std::pair<vec4_t, vec4_t>>>(std::any &)> get_phantom_boards_and_checks;

	static const std::function<std::vector<vec4_t>(std::any &, vec4_t)> gen_move_if_playable;
	static const std::function<match_status_t(std::any &)> get_match_status;
	static const std::function<std::vector<vec4_t> (std::any &)> get_movable_pieces;
	static const std::function<bool (std::any &, vec4_t)> is_playable;
	
	static const std::function<bool(std::any &)> can_undo;
	static const std::function<bool(std::any &)> can_redo;
	static const std::function<bool(std::any &)> can_submit;
	static const std::function<bool(std::any &)> undo;
	static const std::function<bool(std::any &)> redo;
	static const std::function<bool(std::any &, ext_move_t)> apply_move;
	static const std::function<bool(std::any &)> submit;
	static const std::function<bool(std::any &)> currently_check;
	static const std::function<bool(std::any &)> suggest_action;
	static const std::function<bool(std::any &, bool &can_pass)> big_round_over;

	using comments_t = std::vector<std::string>;
	static const std::function<comments_t (std::any &)> get_comments;
	static const std::function<bool(std::any &)> has_parent;
	static const std::function<void(std::any &)> visit_parent;
	static const std::function<std::string(std::any &)> show_pgn;

	static const std::function<void (std::any &, action_t&)> visit_child;
	static const std::function<std::vector<ext_move_t> (std::any &) > get_cached_moves;
	static const std::function<std::tuple<action_t, std::string> (std::any &) > get_cached_action;
	static const std::function<std::vector<std::tuple<action_t, std::string>> (std::any &) > get_child_actions;
	static const std::function<std::vector<std::tuple<action_t, std::string>> (std::any &) > get_historical_actions;
	static const std::function<std::vector<std::tuple<action_t, std::string>> (std::any &) > get_following_actions;
	static const std::function<int (std::any&) > get_current_level;
	static const std::function<std::vector<std::pair<std::tuple<int, int, int, int>, std::tuple<int, int, int, int>>> (std::any&) > get_current_boards_edges;
	static const std::function<std::vector<std::vector<ext_move_t>> (std::any&, match_status_t &) > get_operable_boards_moves_and_match_status;
	static const std::function<ext_move_t (std::any&, int) > action_id_to_ext_move;
	static const std::function<int (std::any&, ext_move_t) > ext_move_to_action_id;

    EngineInterface() = delete;
};