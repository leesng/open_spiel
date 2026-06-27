#include "5dc_interface.h"
#include "game.h"

// ===================== EngineInterface - Delayed Initialization =====================

const EngineInterface::piece_t EngineInterface::NO_PIECE = ::NO_PIECE;
const EngineInterface::piece_t EngineInterface::WALL_PIECE = ::WALL_PIECE;
const EngineInterface::piece_t EngineInterface::KING_UW = ::KING_UW;
const EngineInterface::piece_t EngineInterface::ROOK_UW = ::ROOK_UW;
const EngineInterface::piece_t EngineInterface::PAWN_UW = ::PAWN_UW;
const EngineInterface::piece_t EngineInterface::KING_UB = ::KING_UB;
const EngineInterface::piece_t EngineInterface::ROOK_UB = ::ROOK_UB;
const EngineInterface::piece_t EngineInterface::PAWN_UB = ::PAWN_UB;
const EngineInterface::piece_t EngineInterface::KING_W = ::KING_W;
const EngineInterface::piece_t EngineInterface::QUEEN_W = ::QUEEN_W;
const EngineInterface::piece_t EngineInterface::BISHOP_W = ::BISHOP_W;
const EngineInterface::piece_t EngineInterface::KNIGHT_W = ::KNIGHT_W;
const EngineInterface::piece_t EngineInterface::ROOK_W = ::ROOK_W;
const EngineInterface::piece_t EngineInterface::PAWN_W = ::PAWN_W;
const EngineInterface::piece_t EngineInterface::UNICORN_W = ::UNICORN_W;
const EngineInterface::piece_t EngineInterface::DRAGON_W = ::DRAGON_W;
const EngineInterface::piece_t EngineInterface::BRAWN_W = ::BRAWN_W;
const EngineInterface::piece_t EngineInterface::PRINCESS_W = ::PRINCESS_W;
const EngineInterface::piece_t EngineInterface::ROYAL_QUEEN_W = ::ROYAL_QUEEN_W;
const EngineInterface::piece_t EngineInterface::COMMON_KING_W = ::COMMON_KING_W;
const EngineInterface::piece_t EngineInterface::KING_B = ::KING_B;
const EngineInterface::piece_t EngineInterface::QUEEN_B = ::QUEEN_B;
const EngineInterface::piece_t EngineInterface::BISHOP_B = ::BISHOP_B;
const EngineInterface::piece_t EngineInterface::KNIGHT_B = ::KNIGHT_B;
const EngineInterface::piece_t EngineInterface::ROOK_B = ::ROOK_B;
const EngineInterface::piece_t EngineInterface::PAWN_B = ::PAWN_B;
const EngineInterface::piece_t EngineInterface::UNICORN_B = ::UNICORN_B;
const EngineInterface::piece_t EngineInterface::DRAGON_B = ::DRAGON_B;
const EngineInterface::piece_t EngineInterface::BRAWN_B = ::BRAWN_B;
const EngineInterface::piece_t EngineInterface::PRINCESS_B = ::PRINCESS_B;
const EngineInterface::piece_t EngineInterface::ROYAL_QUEEN_B = ::ROYAL_QUEEN_B;
const EngineInterface::piece_t EngineInterface::COMMON_KING_B = ::COMMON_KING_B;

const EngineInterface::match_status_t EngineInterface::PLAYING = (EngineInterface::match_status_t)::match_status_t::PLAYING;
const EngineInterface::match_status_t EngineInterface::WHITE_WINS = (EngineInterface::match_status_t)::match_status_t::WHITE_WINS;
const EngineInterface::match_status_t EngineInterface::BLACK_WINS = (EngineInterface::match_status_t)::match_status_t::BLACK_WINS;
const EngineInterface::match_status_t EngineInterface::STALEMATE = (EngineInterface::match_status_t)::match_status_t::STALEMATE;

const decltype(EngineInterface::v4_string) EngineInterface::v4_string = [](vec4_t &pos) {
	return ::vec4(std::get<3>(pos), std::get<2>(pos), std::get<1>(pos), std::get<0>(pos)).to_string();
};

const decltype(EngineInterface::em_string) EngineInterface::em_string = [](ext_move_t &em) {
	return ::ext_move(
		::vec4(std::get<3>(std::get<0>(em)), std::get<2>(std::get<0>(em)), std::get<1>(std::get<0>(em)), std::get<0>(std::get<0>(em))),
		::vec4(std::get<3>(std::get<1>(em)), std::get<2>(std::get<1>(em)), std::get<1>(std::get<1>(em)), std::get<0>(std::get<1>(em))),
		::piece_t(std::get<2>(em))
	).to_string();
};

const decltype(EngineInterface::act_string) EngineInterface::act_string = [](action_t &act) {
	std::string ret;
	for (auto &mv : act) {
		ret += em_string(mv); 
	}
	return ret;
};

const decltype(EngineInterface::from_pgn) EngineInterface::from_pgn = [](const std::string &input) { 
    //return std::any();
    return std::any(std::make_shared<game>(game::from_pgn(input)));
};
const decltype(EngineInterface::get_board_size) EngineInterface::get_board_size = [](std::any &ei) {
    //return std::pair<int, int>{8, 8}; 
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    return g->get_board_size();
};

const decltype(EngineInterface::get_current_present) EngineInterface::get_current_present = [](std::any &ei) { 
    //return std::pair<int, bool>{1, false}; 
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    return g->get_current_present();
};

const decltype(EngineInterface::get_current_timeline_status) EngineInterface::get_current_timeline_status = [](std::any &ei) { 
    //return std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> {{0}, {-1}, {-2}}; 
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    return g->get_current_timeline_status();
};

const decltype(EngineInterface::get_current_boards) EngineInterface::get_current_boards = [](std::any &ei) { 
    //return std::vector<std::tuple<int, int, bool, std::string>> {
    //    {0, 0, 1, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR"},
    //    {0, 1, 0, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR"},
    //};
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    return g->get_current_boards();
};

const decltype(EngineInterface::get_movable_pieces) EngineInterface::get_movable_pieces = [](std::any &ei) { 

    //return std::vector<vec4_t>  {
    //    {0, 1, 7, 1},
    //    {0, 1, 7, 6},
    //};

    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    std::vector<::vec4> res =  g->get_movable_pieces();
    std::vector<vec4_t> ret;
    for (const auto& it : res) {
        ret.push_back(std::make_tuple(it.l(), it.t(), it.y(), it.x()));
    }
    return ret;
};

const decltype(EngineInterface::gen_move_if_playable) EngineInterface::gen_move_if_playable = [](std::any &ei, vec4_t pos) { 
    //return std::vector<vec4_t> {
    //    {0, 1, 5, 0},
    //    {0, 1, 5, 2},
    //};
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    std::vector<::vec4> res = g->gen_move_if_playable(
        ::vec4(std::get<3>(pos), std::get<2>(pos), std::get<1>(pos), std::get<0>(pos))
    );
    std::vector<vec4_t> ret;
    for (const auto& it : res) {
        ret.push_back(std::make_tuple(it.l(), it.t(), it.y(), it.x()));
    }
    return ret;
};

const decltype(EngineInterface::get_current_checks) EngineInterface::get_current_checks = [](std::any &ei) { 
    //return std::vector<std::pair<vec4_t, vec4_t>> {
    //    {{0, 1, 7, 1}, {0, 0, 0, 4}},
    //    {{0, 1, 7, 1}, {0, 1, 0, 4}},
    //    {{0, 1, 7, 1}, {0, 0, 7, 0}},
    //    {{0, 1, 7, 1}, {0, 1, 7, 7}},
    //    {{0, 1, 7, 1}, {0, 1, 7, 0}},
    //};
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    std::vector<std::pair<::vec4, ::vec4>> res = g->get_current_checks();
    std::vector<std::pair<vec4_t, vec4_t>> ret;
    for (const auto& it : res) {
        ret.push_back(
            std::make_pair(std::make_tuple(it.first.l(), it.first.t(), it.first.y(), it.first.x()),
                           std::make_tuple(it.second.l(), it.second.t(), it.second.y(), it.second.x())
            )
        );
    }
    return ret;
};

const decltype(EngineInterface::get_phantom_boards_and_checks) EngineInterface::get_phantom_boards_and_checks = [](std::any &ei) { 
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
	auto res = g->get_phantom_boards_and_checks();
	
	std::vector<std::tuple<int, int, bool, std::string>> ph_boards;
	std::vector<std::pair<vec4_t, vec4_t>> ph_checks;
	ph_boards = res.first;
	for (const auto &it : res.second) {
		ph_checks.push_back(
			std::make_pair(std::make_tuple(it.from.l(), it.from.t(), it.from.y(), it.from.x()),
						std::make_tuple(it.to.l(), it.to.t(), it.to.y(), it.to.x()))
		);
	}
    return std::make_pair(ph_boards, ph_checks);
};

const decltype(EngineInterface::apply_move) EngineInterface::apply_move = [](std::any &ei, ext_move_t em) { 
    //return bool(false);
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    return g->apply_move(
        ::ext_move(
            ::vec4(std::get<3>(std::get<0>(em)), std::get<2>(std::get<0>(em)), std::get<1>(std::get<0>(em)), std::get<0>(std::get<0>(em))),
            ::vec4(std::get<3>(std::get<1>(em)), std::get<2>(std::get<1>(em)), std::get<1>(std::get<1>(em)), std::get<0>(std::get<1>(em))),
            ::piece_t(std::get<2>(em))
        )
    );
};

const decltype(EngineInterface::can_undo) EngineInterface::can_undo = [](std::any &ei) { 
    //return bool(false);
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    return g->can_undo();
};
const decltype(EngineInterface::can_redo) EngineInterface::can_redo = [](std::any &ei) { 
    //return bool(false);
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    return g->can_redo();
};
const decltype(EngineInterface::can_submit) EngineInterface::can_submit = [](std::any &ei) { 
    //return bool(false);
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    return g->can_submit();
};
const decltype(EngineInterface::undo) EngineInterface::undo = [](std::any &ei) { 
    //return bool(false);
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    return g->undo();
};
const decltype(EngineInterface::redo) EngineInterface::redo = [](std::any &ei) { 
    //return bool(false);
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    return g->redo();
};
const decltype(EngineInterface::submit) EngineInterface::submit = [](std::any &ei) { 
    //return bool(false);
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    return g->submit();
};
const decltype(EngineInterface::currently_check) EngineInterface::currently_check = [](std::any &ei) { 
    //return bool(false);
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    return g->currently_check();
};
const decltype(EngineInterface::suggest_action) EngineInterface::suggest_action = [](std::any &ei) { 
    //return bool(false);
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    return g->suggest_action();
};
const decltype(EngineInterface::visit_parent) EngineInterface::visit_parent = [](std::any &ei) { 
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    g->visit_parent();
	return;
};
const decltype(EngineInterface::visit_child) EngineInterface::visit_child = [](std::any &ei, action_t &ui_act) { 
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
	std::vector<ext_move> mvs;
	for (auto &em : ui_act) {
		mvs.push_back(::ext_move(
		::vec4(std::get<3>(std::get<0>(em)), std::get<2>(std::get<0>(em)), std::get<1>(std::get<0>(em)), std::get<0>(std::get<0>(em))),
		::vec4(std::get<3>(std::get<1>(em)), std::get<2>(std::get<1>(em)), std::get<1>(std::get<1>(em)), std::get<0>(std::get<1>(em))),
		::piece_t(std::get<2>(em))
		));
	}
	auto us = g->get_unmoved_state();
	auto ei_act = action::from_vector(mvs, us);
	
	g->visit_child(ei_act);
	return;
};

const decltype(EngineInterface::get_cached_moves) EngineInterface::get_cached_moves = [](std::any &ei) { 
    auto g = std::any_cast<std::shared_ptr<game>>(ei);
    std::vector<::ext_move> res =  g->get_cached_moves();
	std::vector<ext_move_t> ret;
	
	for (const auto& it : res) {
		ret.push_back(
			std::make_tuple(
				std::make_tuple(it.get_from().l(), it.get_from().t(), it.get_from().y(), it.get_from().x()),
				std::make_tuple(it.get_to().l(), it.get_to().t(), it.get_to().y(), it.get_to().x()),
				it.get_promote()
			)
		);
	}
	return ret;
};

const decltype(EngineInterface::get_child_actions) EngineInterface::get_child_actions = [](std::any &ei) { 
	auto g = std::any_cast<std::shared_ptr<game>>(ei);
	std::vector<std::tuple<::action, std::string>> res0 = g->get_child_actions();
	std::vector<std::tuple<EngineInterface::action_t, std::string>> ret0;
	
	for (const auto& it0 : res0) {
		std::vector<ext_move_t> ret;
		for (const auto& it : std::get<0>(it0).get_moves()) {
			ret.push_back(
				std::make_tuple(
					std::make_tuple(it.get_from().l(), it.get_from().t(), it.get_from().y(), it.get_from().x()),
					std::make_tuple(it.get_to().l(), it.get_to().t(), it.get_to().y(), it.get_to().x()),
					it.get_promote()
				)
			);
		}
		ret0.push_back(std::make_tuple(ret, std::get<1>(it0)));
	}
	return ret0;
};

const decltype(EngineInterface::get_historical_actions) EngineInterface::get_historical_actions = [](std::any &ei) { 
	auto g = std::any_cast<std::shared_ptr<game>>(ei);
	std::vector<std::tuple<::action, std::string>> res0 = g->get_historical_actions(state::SHOW_CAPTURE | state::SHOW_PROMOTION | state::SHOW_MATE);
	std::vector<std::tuple<EngineInterface::action_t, std::string>> ret0;

	for (const auto& it0 : res0) {
		std::vector<ext_move_t> ret;
		for (const auto& it : std::get<0>(it0).get_moves()) {
			ret.push_back(
				std::make_tuple(
					std::make_tuple(it.get_from().l(), it.get_from().t(), it.get_from().y(), it.get_from().x()),
					std::make_tuple(it.get_to().l(), it.get_to().t(), it.get_to().y(), it.get_to().x()),
					it.get_promote()
				)
			);
		}
		ret0.push_back(std::make_tuple(ret, std::get<1>(it0)));
	}
	return ret0;
};

const decltype(EngineInterface::get_cached_action) EngineInterface::get_cached_action = [](std::any &ei) {
	auto g = std::any_cast<std::shared_ptr<game>>(ei);
	auto cm = g->get_cached_moves();
	auto us = g->get_unmoved_state();
	auto act = action::from_vector(cm, us);
	
	EngineInterface::action_t r_act;
	for (const auto& it : act.get_moves()) {
		r_act.push_back(
			std::make_tuple(
				std::make_tuple(it.get_from().l(), it.get_from().t(), it.get_from().y(), it.get_from().x()),
				std::make_tuple(it.get_to().l(), it.get_to().t(), it.get_to().y(), it.get_to().x()),
				it.get_promote()
			)
		);
	}
	std::string r_txt = us.pretty_action(act, state::SHOW_CAPTURE | state::SHOW_PROMOTION | state::SHOW_MATE);
	
	return make_tuple(r_act, r_txt);
};

const decltype(EngineInterface::get_following_actions) EngineInterface::get_following_actions = [](std::any &ei) { 
	auto g = std::any_cast<std::shared_ptr<game>>(ei);
	std::vector<std::tuple<::action, std::string>> res0 = g->get_following_actions(state::SHOW_CAPTURE | state::SHOW_PROMOTION | state::SHOW_MATE);
	std::vector<std::tuple<EngineInterface::action_t, std::string>> ret0;

	for (const auto& it0 : res0) {
		std::vector<ext_move_t> ret;
		for (const auto& it : std::get<0>(it0).get_moves()) {
			ret.push_back(
				std::make_tuple(
					std::make_tuple(it.get_from().l(), it.get_from().t(), it.get_from().y(), it.get_from().x()),
					std::make_tuple(it.get_to().l(), it.get_to().t(), it.get_to().y(), it.get_to().x()),
					it.get_promote()
				)
			);
		}
		ret0.push_back(std::make_tuple(ret, std::get<1>(it0)));
	}
	return ret0;
};

const decltype(EngineInterface::get_current_level) EngineInterface::get_current_level = [](std::any& ei) {
	auto g = std::any_cast<std::shared_ptr<game>>(ei);
	return g->get_current_level();
};

const decltype(EngineInterface::get_current_boards_edges) EngineInterface::get_current_boards_edges = [](std::any& ei) {
	auto g = std::any_cast<std::shared_ptr<game>>(ei);
	std::optional<std::vector<std::pair<int,std::vector<uint64_t>>>> out_all_boards = std::nullopt;
	std::vector<std::pair<int, int>> edges = g->get_current_boards_edges(out_all_boards);
	std::vector<std::pair<std::tuple<int, int, int, int>, std::tuple<int, int, int, int>>> result;

	for (auto it : edges) {
		auto [u0, v0] = DecodeBoardId(it.first);
		auto [u1, v1] = DecodeBoardId(it.second);
		result.push_back(std::make_pair(std::make_tuple(u_to_l(u0), (v0), 3, 3),
		                                std::make_tuple(u_to_l(u1), (v1), 3, 3)));
	}
	return result;
};

const decltype(EngineInterface::get_operable_boards_moves_and_match_status) EngineInterface::get_operable_boards_moves_and_match_status = [](std::any& ei, match_status_t &ms) {
	auto g = std::any_cast<std::shared_ptr<game>>(ei);
	::match_status_t ms0;
	std::vector<std::pair<int,std::vector<uint64_t>>> operable_boards = g->get_operable_boards_moves_and_match_status(ms0);
	ms = (EngineInterface::match_status_t)ms0;
	std::vector<std::vector<ext_move_t>> move_table;
	for (const auto & [bid, mvs] : operable_boards) {
		std::vector<ext_move_t> move_table_per_board;
		for (const auto &  moveid : mvs) {
			auto [u0, v0, y0, x0, u1, v1, y1, x1, pto, flags] = DecodeMoveId(moveid);
			move_table_per_board.push_back(
				std::make_tuple(
					std::make_tuple(u_to_l(u0), v_to_tc(v0).first, y0, x0),
					std::make_tuple(u_to_l(u1), v_to_tc(v1).first, y1, x1),
					piece_t("QNRB"[pto])
				)
			);
		}
		move_table.push_back(move_table_per_board);
	}
	return move_table;
};

constexpr int kMaxMovesPerBoard = 256; // Maximum moves per single board (256)
constexpr int kMaxOperableBoards = 128;   // Maximum operable boards (128)

const decltype(EngineInterface::action_id_to_ext_move) EngineInterface::action_id_to_ext_move = [](std::any& ei, int action) {
	auto g = std::any_cast<std::shared_ptr<game>>(ei);
	::match_status_t ms0;
	std::vector<std::pair<int,std::vector<uint64_t>>> operable_boards = g->get_operable_boards_moves_and_match_status(ms0);

	int board_idx = action / kMaxMovesPerBoard;
	int move_idx = action % kMaxMovesPerBoard;
	
	if (board_idx < std::min(kMaxOperableBoards, (int)operable_boards.size())) {
		if (move_idx < std::min(kMaxMovesPerBoard, (int)operable_boards[board_idx].second.size())) {
			auto moveid = operable_boards[board_idx].second[move_idx];
			auto [u0, v0, y0, x0, u1, v1, y1, x1, pto, flags] = DecodeMoveId(moveid);
			ext_move_t em = std::make_tuple(
					std::make_tuple(u_to_l(u0), v_to_tc(v0).first, y0, x0),
					std::make_tuple(u_to_l(u1), v_to_tc(v1).first, y1, x1),
					piece_t("QNRB"[pto])
			);
			return em;
		}
	}
	std::cerr << "action_id_to_ext_move failed! " << std::endl;
	return ext_move_t{};
};

const decltype(EngineInterface::ext_move_to_action_id) EngineInterface::ext_move_to_action_id = [](std::any& ei, ext_move_t em0) {
	auto g = std::any_cast<std::shared_ptr<game>>(ei);
	::match_status_t ms0;
	std::vector<std::pair<int,std::vector<uint64_t>>> operable_boards = g->get_operable_boards_moves_and_match_status(ms0);

	for (int board_idx = 0; board_idx < std::min(kMaxOperableBoards, (int)operable_boards.size()); board_idx++) {
		const auto& [board_id, move_list] = operable_boards[board_idx];
		for (int move_idx = 0; move_idx < std::min(kMaxMovesPerBoard, (int)move_list.size()); move_idx++) {
		  	auto moveid = operable_boards[board_idx].second[move_idx];
			auto [u0, v0, y0, x0, u1, v1, y1, x1, pto, flags] = DecodeMoveId(moveid);
			ext_move_t em = std::make_tuple(
					std::make_tuple(u_to_l(u0), v_to_tc(v0).first, y0, x0),
					std::make_tuple(u_to_l(u1), v_to_tc(v1).first, y1, x1),
					piece_t("QNRB"[pto])
			);
			if (em0 == em) {
				return (board_idx * kMaxMovesPerBoard + move_idx);
			}
		}
	}

	std::cerr << "ext_move_to_action_id failed! " << std::endl;
	return int{-1};
};

const decltype(EngineInterface::big_round_over) EngineInterface::big_round_over = [](std::any &ei,  bool &can_pass) {
    return std::any_cast<std::shared_ptr<game>>(ei)->big_round_over(can_pass);
};

const decltype(EngineInterface::get_match_status) EngineInterface::get_match_status = [](std::any &ei) { 
	auto g = std::any_cast<std::shared_ptr<game>>(ei);
	return (EngineInterface::match_status_t)g->get_match_status();
};

const decltype(EngineInterface::show_pgn) EngineInterface::show_pgn = [](std::any &ei) { 
	auto g = std::any_cast<std::shared_ptr<game>>(ei);
	return g->show_pgn();
};






