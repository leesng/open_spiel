#include "multiverse_base.h"
#include "utils.h"
#include "magic.h"
#include <regex>
#include <sstream>
#include <algorithm>
#include <limits>
#include <iostream>
#include <utility>
#include <initializer_list>
#include <cassert>

multiverse::multiverse(std::vector<std::tuple<int, int, bool, std::string>> bds, int size_x, int size_y)
: size_x(size_x), size_y(size_y), l_min(0), l_max(0)
{
    if(bds.empty())
        throw std::runtime_error("multiverse(): Empty input");
    for(const auto& [l, t, c, fen] : bds)
    {
        insert_board_impl(l, t, c, std::make_shared<board>(fen, size_x, size_y));
    }
    for(int l = l_min; l <= l_max; l++)
    {
        int u = l_to_u(l);
        if(boards[u].empty())
            throw std::runtime_error("multiverse(): There is a gap between timelines.");
        for(int v = timeline_start[u]; v <= timeline_end[u]; v++)
        {
            if(boards[u][v] == nullptr)
            {
                throw std::runtime_error("multiverse(): There is a gap between boards on timeline L"
                    + std::to_string(u_to_l(u)) + ".");
            }
        }
    }
}

turn_t multiverse::get_present() const
{
    int present_v = std::numeric_limits<int>::max();
    for(int l = active_min; l <= active_max; l++)
    {
        present_v = std::min(present_v, timeline_end[l_to_u(l)]);
    }
    return v_to_tc(present_v);
}

std::pair<int, int> multiverse::get_board_size() const
{
    return std::make_pair(size_x, size_y);
}

std::pair<int, int> multiverse::get_lines_range() const
{
    return std::make_pair(l_min, l_max);
}

std::pair<int, int> multiverse::get_active_range() const
{
    return std::make_pair(active_min, active_max);
}

turn_t multiverse::get_timeline_start(int l) const
{
    return v_to_tc(timeline_start.at(l_to_u(l)));
    //return v_to_tc(timeline_start[l_to_u(l)]);
}

turn_t multiverse::get_timeline_end(int l) const
{
    return v_to_tc(timeline_end.at(l_to_u(l)));
    //return v_to_tc(timeline_end[l_to_u(l)]);
}

std::shared_ptr<board> multiverse::get_board(int l, int t, bool c) const
{
    try
    {
        return this->boards.at(l_to_u(l)).at(tc_to_v(t,c));
    }
    catch(const std::out_of_range& ex)
    {
		std::cerr << ex.what() << std::endl;
        std::cerr << "In this multiverse object:\n" << to_string();
        std::cerr << "Error: Out of range in multiverse::get_board("
        << l << ", " << t << ", " << c << ")"<< std::endl;
		throw std::runtime_error("Error: Out of range in multiverse::get_board(" + std::to_string(l) + ", " + std::to_string(t) + ", " + std::to_string(c) + ")");
        return nullptr;
    }
}

void multiverse::append_board(int l, const std::shared_ptr<board>& b_ptr)
{
    int u = l_to_u(l);
    boards[u].push_back(b_ptr);
    timeline_end[u]++;
}

void multiverse::insert_board_impl(int l, int t, bool c, const std::shared_ptr<board>& b_ptr)
{
    int u = l_to_u(l);
    int v = tc_to_v(t, c);

    // if u is too large, resize this->board to accommodate new board
    // and fill any missing row with empty vector
    if(u >= static_cast<int>(this->boards.size()))
    {
        this->boards.resize(u+1, std::vector<std::shared_ptr<board>>());
        this->timeline_start.resize(u+1, std::numeric_limits<int>::max());
        this->timeline_end.resize(u+1, std::numeric_limits<int>::min());
    }
    l_min = std::min(l_min, l);
    l_max = std::max(l_max, l);
    std::vector<std::shared_ptr<board>> &timeline = this->boards[u];
    // do the same for v
    if(v >= static_cast<int>(timeline.size()))
    {
        timeline.resize(v+1, nullptr);
    }
    else if(v < 0)
    {
        throw std::runtime_error("multiverse::insert_board_impl(): Negative time is not supported.");
    }
    if(timeline[v] != nullptr)
    {
        throw std::runtime_error("multiverse::insert_board_impl(): Duplicate definition of the board on L="+std::to_string(l)+" (plain notation), T="+std::to_string(t)+" C="+std::string(c?"b":"w"));
    }
    timeline[v] = b_ptr;
    timeline_start[u] = std::min(timeline_start[u], v);
    timeline_end[u]   = std::max(timeline_end[u],   v);
}

void multiverse::insert_board(int l, int t, bool c, const std::shared_ptr<board> &b_ptr)
{
    insert_board_impl(l, t, c, b_ptr);
    // recalculate active range since there is probably a new line
    const auto [l0_min, l0_max] = get_initial_lines_range();
    int whites_lines = l_max - l0_max;
    int blacks_lines = l0_min - l_min;
    if(l > l0_max && whites_lines <= blacks_lines + 1 && l > active_max)
    {
        active_max++;
        if(l_min < active_min) // check reactivate
        {
            active_min--;
        }
    }
    else if(l < l0_min && blacks_lines <= whites_lines + 1 && l < active_min)
    {
        active_min--;
        if(l_max > active_max)
        {
            active_max++;
        }
    }
    assert(std::make_pair(active_min, active_max) == calculate_active_range());
}

void multiverse::update_active_range()
{
    std::tie(active_min, active_max) = calculate_active_range();
}

template<bool SHOW_UMOVE>
std::vector<std::tuple<int,int,bool,std::string>> multiverse::get_boards() const
{
    std::vector<std::tuple<int,int,bool,std::string>> result;
    for(int u = 0; u < static_cast<int>(boards.size()); u++)
    {
        const auto& timeline = this->boards[u];
        int l = u_to_l(u);
        for(int v = 0; v < static_cast<int>(timeline.size()); v++)
        {
            const auto [t, c] = v_to_tc(v);
            if(timeline[v] != nullptr)
            {
                result.push_back(std::make_tuple(l,t,c,timeline[v]->get_fen<SHOW_UMOVE>()));
            }
        }
    }
    return result;
}

std::vector<std::shared_ptr<board>> multiverse::get_newboard_by_move(vec4 p, vec4 q, bool player, piece_t pto) const
{
	const std::shared_ptr<board>& b_ptr = get_board(p.l(), p.t(), player);
	piece_t pic = static_cast<piece_t>(piece_name(b_ptr->get_piece(p.xy())));
	const std::shared_ptr<board>& x_ptr = get_board(q.l(), q.t(), player);
	std::shared_ptr<board> new_board = nullptr;
	std::shared_ptr<board> new_board2 = nullptr;
	std::vector<std::shared_ptr<board>> result;

	// promotion (only brawns can do)
	if ((b_ptr->lrawn() & pmask(p.xy())) && (q.y() == 0 || q.y() == size_y - 1)) {
		pic = player ? to_black(pto) : pto;
	}
	
	if (b_ptr == x_ptr) {
		vec4 d = q - p;
		if((b_ptr->lpawn() & pmask(p.xy())) && d.x()!=0 && b_ptr->get_piece(q.xy()) == NO_PIECE) {
			// en passant
			new_board = b_ptr->replace_piece(ppos(q.x(), p.y()), NO_PIECE);
		} else if((b_ptr->king() & pmask(p.xy())) && abs(d.x()) > 1) {
			// castling
			int rook_x1 = d.x() < 0 ? 0 : (size_x - 1); //rook's original x coordinate
			int rook_x2 = q.x() + (d.x() < 0 ? 1 : -1); //rook's new x coordinate
			new_board = b_ptr->move_piece(ppos(rook_x1, p.y()), ppos(rook_x2, q.y()));
		} else {
			// normal move
			new_board = b_ptr;
		}
		new_board = new_board->replace_piece(p.xy(), NO_PIECE);
		new_board = new_board->replace_piece(q.xy(), pic);
		result.push_back(new_board);
	} else  {
		// superphysical move
		new_board = b_ptr->replace_piece(p.xy(), NO_PIECE);
		new_board2 = x_ptr->replace_piece(q.xy(), pic);
		result.push_back(new_board);
		result.push_back(new_board2);
	}

	return result;
}

//#define MAKE_BOARD_ID(u,v) ((u) << 8 | (v))
//#define MAKE_MOVE_ID(u0,v0,y0,x0,u1,v1,y1,x1,promote_to,flags) ( \
//(u0) << 44 | (v0) << 36 | (y0) << 33 | (x0) << 30 | (u1) << 22 | (v1) << 14 | (y1) << 11 | (x1) << 8 | \
//(promote_to) << 4 | (flags))  // bit 0 is valid bit; bit 1 is is branching move
std::tuple<std::vector<std::pair<int,std::vector<uint64_t>>>,
           std::vector<std::pair<int,int>>> multiverse::get_boards_and_edges() const
{
    std::vector<std::pair<int,int>> boards_edges;
	std::vector<std::pair<int,std::vector<uint64_t>>> all_boards;

	//auto present_tc = get_present();
    for(int u = 0; u < static_cast<int>(boards.size()); u++) {
        const auto& timeline = boards[u];
        //int l = u_to_l(u);
        for(int v = 0; v < static_cast<int>(timeline.size()); v++) {
            const auto [t, c] = v_to_tc(v);
            if(timeline[v] != nullptr) {
				// get board edges
				int parent_u = l_to_u(((timeline[v]->contact() >> 8 & 0xFF) ^ 0x80) - 0x80);
				int travel_u = l_to_u(((timeline[v]->contact() >> 24 & 0xFF) ^ 0x80) - 0x80);
				int travel_v = tc_to_v(((timeline[v]->contact() >> 16 & 0xFF) ^ 0x80) - 0x80, c);
				if (parent_u > 0 || v - 1 > 0) {
					boards_edges.push_back(std::make_pair(EncodeBoardId(u,v), EncodeBoardId(parent_u, v - 1)));
					//std::cout << "parent==" << u << v << parent_u << v - 1 << std::endl;
				}
				if (travel_u > 0 || travel_v > 1) {
					boards_edges.push_back(std::make_pair(EncodeBoardId(u,v), EncodeBoardId(travel_u, travel_v)));
					//std::cout << "travel==" << u << v << travel_u << travel_v << std::endl;
				}
				//get board planes
				std::vector<uint64_t> board_planes;
				board_planes.push_back(timeline[v]->white());
				board_planes.push_back(timeline[v]->black());
				board_planes.push_back(timeline[v]->royal());
				board_planes.push_back(timeline[v]->lking());
				board_planes.push_back(timeline[v]->lknight());
				board_planes.push_back(timeline[v]->lpawn());
				board_planes.push_back(timeline[v]->lrawn());
				board_planes.push_back(timeline[v]->lrook());
				board_planes.push_back(timeline[v]->lbishop());
				board_planes.push_back(timeline[v]->lunicorn());
				board_planes.push_back(timeline[v]->ldragon());
				board_planes.push_back(timeline[v]->umove());
                all_boards.push_back(std::make_pair(EncodeBoardId(u,v), board_planes));
				
            }
        }

    }

    return std::make_tuple(all_boards, boards_edges);
}

template <bool COLOR> std::vector<std::pair<int,std::vector<uint64_t>>> multiverse::get_operable_boards_moves(bool allow_pass) const
{
	std::vector<std::pair<int,std::vector<uint64_t>>> operable_boards;

	auto present_tc = get_present();
    for(int u = 0; u < static_cast<int>(boards.size()); u++) {
		if (boards[u].empty()) {
			continue;
		}
        int l = u_to_l(u);
		int v = timeline_end[u];
        auto [t, c] = v_to_tc(v);
		if (c != COLOR) {
			continue;
		}
		//////////////////begin getting operted boards and moves///////////////////////////////////
		std::vector<uint64_t> board_moves;
		// can pass ?
		bool cannot_pass_it = (active_min <= l && l <= active_max && v_to_tc(v) == present_tc);
		bool cannot_submit_it = (COLOR == present_tc.second);
		if (allow_pass && !cannot_pass_it && !cannot_submit_it) {
			board_moves.push_back(EncodeMoveId(u,v,0,0, 0,0,0,0,0, 1)); // pass valid only in optional-lines
		}
		// generate all moves
		bitboard_t b_pieces = boards[u][v]->friendly<COLOR>() & ~boards[u][v]->wall();
		for (int src_pos : marked_pos(b_pieces)) {
			vec4 p = vec4(src_pos, vec4(0,0,t,l));
			auto gen = gen_moves<COLOR>(p);
			for (const auto& [r, bb] : gen) {
				for(int dst_pos : marked_pos(bb)) {
					vec4 q = vec4(dst_pos, r);
					// is branching move
					bool non_branching = (std::make_pair(q.t(), COLOR) == get_timeline_end(q.l()));
					// out of range???
					bool out_of_range = outofrange(p, q, COLOR);
					if (out_of_range) {
						continue;
					}

					// been attacked
					auto nbs = get_newboard_by_move(p, q, COLOR);
					bool been_attacked = false;
					for (int i = 0; been_attacked == false && i < static_cast<int>(nbs.size()); ++i) {
						for(int pos2 : marked_pos(nbs[i]->royal() & nbs[i]->friendly<COLOR>())) {
							if(nbs[i]->is_under_attack(pos2, COLOR)) {
								//std:: count << "physical check" << std::endl;
								been_attacked = true;
								break;
							}
						}
					}
					if (been_attacked) {
						continue;
					}

					bool is_ptomote_move = (boards[u][v]->lrawn() & pmask(p.xy())) && (q.y() == 0 || q.y() == size_y - 1);
					// make move id code
					uint64_t mid = EncodeMoveId(
					l_to_u(p.l()), tc_to_v(p.t(), c), p.y(), p.x(),
					l_to_u(q.l()), tc_to_v(q.t(), c), q.y(), q.x(), 
					0, is_ptomote_move << 5 | !cannot_pass_it << 2 | !non_branching << 1 | 1);
					
					board_moves.push_back(mid); // promote to QUEEN , default
					if (is_ptomote_move /*(boards[u][v]->lrawn() & pmask(p.xy())) && (q.y() == 0 || q.y() == size_y - 1)*/) {
						board_moves.push_back(mid | 1 << 8); // KNIGHT
						board_moves.push_back(mid | 2 << 8); // ROOK
						board_moves.push_back(mid | 3 << 8); // BISHOP
					}
				}
			}
		}

		std::sort(board_moves.begin(), board_moves.end());
		//for (int i = 0; i < static_cast<int>(board_moves.size()); ++i) {
		//	std::cout << "sorted end mv[" << std::dec << i << "] = 0x" << std::hex << board_moves[i] << std::dec << std::endl;
		//}
		operable_boards.push_back(std::make_pair(EncodeBoardId(u,v), board_moves));
		//////////////////end of getting operted boards and moves///////////////////////////////////
    }

    return operable_boards;
}

std::string multiverse::to_string() const
{
    std::stringstream sstm;
	int all_max_v = 0;
	for (int l = l_min; l <= l_max; l++) {
		if (all_max_v < timeline_end[l_to_u(l)])
			all_max_v = timeline_end[l_to_u(l)];
	}
	for (int i = 0; i < (l_max + 1 - l_min) * (size_y + 1); i++) {
		for (int j = 0; j < (all_max_v + 1 - 0) * (size_x + 1); j++) {
			int l = l_min + i / (size_y + 1);
			int u = l_to_u(l);
			int v = 0 + j / (size_x + 1);
			auto [t, c] = v_to_tc(v);
			int y = (size_y + 1) - 1 - i % (size_y + 1);
			int x = j % (size_x + 1);
			char ch = 0;
			if (y < size_y && x < size_x) {
				const auto& timeline = this->boards[u];
				if (v < timeline.size() && timeline[v] != nullptr) {
					piece_t pic = timeline[v]->get_piece(vec4(x,y,t,l).xy());
					ch = pic == NO_PIECE ? '.' : (char)pic;
				}
			}
			if (ch == 0) {
				sstm << " " << ' ';
			} else if (ch == '.') {
				sstm << '.' << ' ';
			} else {
				sstm << ch << '\'';
			}
		}
		sstm << std::endl;
	}
	/*
    auto [present, player] = get_present();
    sstm << "Multiverse present: T" << present << (player?'b':'w') << "\n";
    sstm << "lines range:" << get_lines_range() << "\t";
    sstm << "active range:" << get_active_range() << "\n";
    for(int u = 0; u < static_cast<int>(this->boards.size()); u++)
    {
        const auto& timeline = this->boards[u];
        int l = u_to_l(u);
        for(int v = 0; v < static_cast<int>(timeline.size()); v++)
        {
            const auto [t, c] = v_to_tc(v);
            if(timeline[v] != nullptr)
            {
                sstm << "L" << l << "T" << t << (c ? 'b' : 'w');
                sstm << "  aka." << pretty_lt(vec4(0,0,t,l)) << "\n";
                sstm << timeline[v]->to_string();
            }
        }
    }*/
    return sstm.str();
}

bool multiverse::inbound(vec4 a, bool color) const
{
    int l = a.l(), u = l_to_u(l), v = tc_to_v(a.t(), color);
    if(a.outbound() || l < l_min || l > l_max)
        return false;
    return timeline_start[u] <= v && v <= timeline_end[u];
}

bool multiverse::outofrange(vec4 p, vec4 q, bool color) const
{
	int max_new_l = 60;
	int max_new_t = 120;
	
	if (tc_to_v(p.t(), color) > tc_to_v(max_new_t, 1) || tc_to_v(q.t(), color) > tc_to_v(max_new_t, 1)) {
		return true;
	}
	if (tc_to_v(q.t(), color) != timeline_end[l_to_u(q.l())]) {		
		if (color ? l_min - 1 < -max_new_l :  l_max + 1 > max_new_l - 1) {
			return true;
		}
	}
    return false;
}	

piece_t multiverse::get_piece(vec4 a, bool color) const
{
    return boards[l_to_u(a.l())][tc_to_v(a.t(), color)]->get_piece(a.xy());
}

bool multiverse::get_umove_flag(vec4 a, bool color) const
{
    return boards[l_to_u(a.l())][tc_to_v(a.t(), color)]->umove() & pmask(ppos(a.x(),a.y()));
}


/************************************************************************************\
*     ***     **        ****    ***      ** *******    *****     ******* **      **  *
*    ****    ****     ***   **  ***      ** ***       ***   **   ***     ***     **  *
*   *** **  *** **   ***     **  ***    **  ***      ***         ***     ****    **  *
*  ***   *****   **  ***     **  ***    **  *******  ***   ****  ******* *****   **  *
*  ***   *****   **  ***     **   ***  **   ***      ***     **  ***     *** **  **  *
* ***     ***     ** ***     **   ***  **   ***      ***     **  ***     ***  ** **  *
* ***     ***     **  ***   **     *****    ***       ***   **   ***     ***   ****  *
* ***     ***     **   ******       ***     *******    *****     ******* ***    ***  *
\************************************************************************************/
template<bool C>
bitboard_t multiverse::gen_physical_moves(vec4 p) const
{
    std::shared_ptr<board> b_ptr = get_board(p.l(), p.t(), C);
    piece_t p_piece = b_ptr->get_piece(p.xy());
    if (b_ptr->umove() & pmask(p.xy()))
    {
        p_piece = static_cast<piece_t>(p_piece | 0x80);
    }
    switch (p_piece)
    {
#define GENERATE_MOVES_CASE(PIECE) \
        case PIECE: \
            return gen_physical_moves_impl<PIECE, C>(p);

        GENERATE_MOVES_CASE(KING_W)
        GENERATE_MOVES_CASE(KING_B)
        GENERATE_MOVES_CASE(KING_UW)
        GENERATE_MOVES_CASE(KING_UB)
        GENERATE_MOVES_CASE(COMMON_KING_W)
        GENERATE_MOVES_CASE(COMMON_KING_B)
        GENERATE_MOVES_CASE(ROOK_W)
        GENERATE_MOVES_CASE(ROOK_B)
        GENERATE_MOVES_CASE(ROOK_UW)
        GENERATE_MOVES_CASE(ROOK_UB)
        GENERATE_MOVES_CASE(BISHOP_W)
        GENERATE_MOVES_CASE(BISHOP_B)
        GENERATE_MOVES_CASE(QUEEN_W)
        GENERATE_MOVES_CASE(QUEEN_B)
        GENERATE_MOVES_CASE(PRINCESS_W)
        GENERATE_MOVES_CASE(PRINCESS_B)
        GENERATE_MOVES_CASE(PAWN_W)
        GENERATE_MOVES_CASE(BRAWN_W)
        GENERATE_MOVES_CASE(PAWN_B)
        GENERATE_MOVES_CASE(BRAWN_B)
        GENERATE_MOVES_CASE(PAWN_UW)
        GENERATE_MOVES_CASE(BRAWN_UW)
        GENERATE_MOVES_CASE(PAWN_UB)
        GENERATE_MOVES_CASE(BRAWN_UB)
        GENERATE_MOVES_CASE(KNIGHT_W)
        GENERATE_MOVES_CASE(KNIGHT_B)
        GENERATE_MOVES_CASE(UNICORN_W)
        GENERATE_MOVES_CASE(UNICORN_B)
        GENERATE_MOVES_CASE(DRAGON_W)
        GENERATE_MOVES_CASE(DRAGON_B)
#undef GENERATE_MOVES_CASE
    default:
        throw std::runtime_error("gen_superphysical_moves: Unknown piece " + std::string({ (char)piece_name(p_piece) }) + (p_piece & 0x80 ? "*" : "") + "\n");
        break;
    }
}
template<bool C>
movegen_t multiverse::gen_superphysical_moves(vec4 p) const
{
    std::shared_ptr<board> b_ptr = get_board(p.l(), p.t(), C);
    piece_t p_piece = b_ptr->get_piece(p.xy());
    if (b_ptr->umove() & pmask(p.xy()))
    {
        p_piece = static_cast<piece_t>(p_piece | 0x80);
    }
    switch (p_piece)
    {
#define GENERATE_MOVES_CASE(PIECE) \
        case PIECE: \
            return gen_moves_impl<PIECE, C, true>(p);

        GENERATE_MOVES_CASE(KING_W)
        GENERATE_MOVES_CASE(KING_B)
        GENERATE_MOVES_CASE(KING_UW)
        GENERATE_MOVES_CASE(KING_UB)
        GENERATE_MOVES_CASE(COMMON_KING_W)
        GENERATE_MOVES_CASE(COMMON_KING_B)
        GENERATE_MOVES_CASE(ROOK_W)
        GENERATE_MOVES_CASE(ROOK_B)
        GENERATE_MOVES_CASE(ROOK_UW)
        GENERATE_MOVES_CASE(ROOK_UB)
        GENERATE_MOVES_CASE(BISHOP_W)
        GENERATE_MOVES_CASE(BISHOP_B)
        GENERATE_MOVES_CASE(QUEEN_W)
        GENERATE_MOVES_CASE(QUEEN_B)
        GENERATE_MOVES_CASE(ROYAL_QUEEN_W)
        GENERATE_MOVES_CASE(ROYAL_QUEEN_B)
        GENERATE_MOVES_CASE(PRINCESS_W)
        GENERATE_MOVES_CASE(PRINCESS_B)
        GENERATE_MOVES_CASE(PAWN_W)
        GENERATE_MOVES_CASE(BRAWN_W)
        GENERATE_MOVES_CASE(PAWN_B)
        GENERATE_MOVES_CASE(BRAWN_B)
        GENERATE_MOVES_CASE(PAWN_UW)
        GENERATE_MOVES_CASE(BRAWN_UW)
        GENERATE_MOVES_CASE(PAWN_UB)
        GENERATE_MOVES_CASE(BRAWN_UB)
        GENERATE_MOVES_CASE(KNIGHT_W)
        GENERATE_MOVES_CASE(KNIGHT_B)
        GENERATE_MOVES_CASE(UNICORN_W)
        GENERATE_MOVES_CASE(UNICORN_B)
        GENERATE_MOVES_CASE(DRAGON_W)
        GENERATE_MOVES_CASE(DRAGON_B)
#undef GENERATE_MOVES_CASE
    default:
        throw std::runtime_error("gen_superphysical_moves: Unknown piece " + std::string({ (char)piece_name(p_piece) }) + (p_piece & 0x80 ? "*" : "") + "\n");
        break;
    }
}
template<bool C>
movegen_t multiverse::gen_moves(vec4 p) const
{
    std::shared_ptr<board> b_ptr = get_board(p.l(), p.t(), C);
    piece_t p_piece = b_ptr->get_piece(p.xy());
    if (b_ptr->umove() & pmask(p.xy()))
    {
        p_piece = static_cast<piece_t>(p_piece | 0x80);
    }
    switch (p_piece)
    {
#define GENERATE_MOVES_CASE(PIECE) \
        case PIECE: \
            return gen_moves_impl<PIECE, C, false>(p);

        GENERATE_MOVES_CASE(KING_W)
        GENERATE_MOVES_CASE(KING_B)
        GENERATE_MOVES_CASE(KING_UW)
        GENERATE_MOVES_CASE(KING_UB)
        GENERATE_MOVES_CASE(COMMON_KING_W)
        GENERATE_MOVES_CASE(COMMON_KING_B)
        GENERATE_MOVES_CASE(ROOK_W)
        GENERATE_MOVES_CASE(ROOK_B)
        GENERATE_MOVES_CASE(ROOK_UW)
        GENERATE_MOVES_CASE(ROOK_UB)
        GENERATE_MOVES_CASE(BISHOP_W)
        GENERATE_MOVES_CASE(BISHOP_B)
        GENERATE_MOVES_CASE(QUEEN_W)
        GENERATE_MOVES_CASE(QUEEN_B)
        GENERATE_MOVES_CASE(ROYAL_QUEEN_W)
        GENERATE_MOVES_CASE(ROYAL_QUEEN_B)
        GENERATE_MOVES_CASE(PRINCESS_W)
        GENERATE_MOVES_CASE(PRINCESS_B)
        GENERATE_MOVES_CASE(PAWN_W)
        GENERATE_MOVES_CASE(BRAWN_W)
        GENERATE_MOVES_CASE(PAWN_B)
        GENERATE_MOVES_CASE(BRAWN_B)
        GENERATE_MOVES_CASE(PAWN_UW)
        GENERATE_MOVES_CASE(BRAWN_UW)
        GENERATE_MOVES_CASE(PAWN_UB)
        GENERATE_MOVES_CASE(BRAWN_UB)
        GENERATE_MOVES_CASE(KNIGHT_W)
        GENERATE_MOVES_CASE(KNIGHT_B)
        GENERATE_MOVES_CASE(UNICORN_W)
        GENERATE_MOVES_CASE(UNICORN_B)
        GENERATE_MOVES_CASE(DRAGON_W)
        GENERATE_MOVES_CASE(DRAGON_B)
#undef GENERATE_MOVES_CASE
    case NO_PIECE:
        throw std::runtime_error("gen_moves: applied on NO_PIECE\n");
        break;
    default:
        throw std::runtime_error("gen_moves: Unknown piece " + std::string({ (char)piece_name(p_piece) }) + (p_piece & 0x80 ? "*" : "") + "\n");
        break;
    }
}

generator<vec4> multiverse::gen_piece_move(vec4 p, bool board_color) const
{
    movegen_t gen = board_color ? gen_moves<true>(p) : gen_moves<false>(p);
    for (const auto& [r, bb] : gen)
    {
        for(int pos : marked_pos(bb))
        {
            vec4 q = vec4(pos, r);
			if (!outofrange(p, q, board_color))
			co_yield q;
        }
    }
}

constexpr std::initializer_list<vec4> orthogonal_dtls = {
    vec4(0, 0, 0, 1),
    vec4(0, 0, 0, -1),
    vec4(0, 0, -1, 0)
};

constexpr std::initializer_list<vec4> diagonal_dtls = {
    vec4(0, 0, 1, 1),
    vec4(0, 0, 1, -1),
    vec4(0, 0, -1, 1),
    vec4(0, 0, -1, -1)
};

constexpr std::initializer_list<vec4> both_dtls = {
    vec4(0, 0, 0, 1),
    vec4(0, 0, 0, -1),
    vec4(0, 0, -1, 0),
    vec4(0, 0, 1, 1),
    vec4(0, 0, 1, -1),
    vec4(0, 0, -1, 1),
    vec4(0, 0, -1, -1)
};

constexpr std::initializer_list<vec4> double_dtls = {
    vec4(0, 0, 0, 2),
    vec4(0, 0, 0, -2),
    vec4(0, 0, -2, 0)
};

template<bool C>
std::vector<std::pair<vec4, bitboard_t>> multiverse::gen_purely_sp_rook_moves(vec4 p0) const
{
    std::vector<std::pair<vec4, bitboard_t>> result;
    std::shared_ptr<board> b0_ptr = get_board(p0.l(), p0.t(), C);
    bitboard_t lrook = b0_ptr->lrook() & b0_ptr->friendly<C>();
    for(auto d : orthogonal_dtls)
    {
        bitboard_t remaining = lrook;
        for(vec4 p1 = p0 + d; remaining && inbound(p1, C); p1 = p1 + d)
        {
            std::shared_ptr<board> b1_ptr = get_board(p1.l(), p1.t(), C);
            remaining &= ~b1_ptr->friendly<C>();
            if(remaining)
            {
                //result[p1.tl()] |= remaining;
                result.push_back(std::make_pair(p1.tl(), remaining));
                remaining &= ~b1_ptr->hostile<C>();
            }
        }
    }
    return result;
}


template<bool C>
std::vector<std::pair<vec4, bitboard_t>> multiverse::gen_purely_sp_bishop_moves(vec4 p0) const
{
    std::vector<std::pair<vec4, bitboard_t>> result;
    std::shared_ptr<board> b0_ptr = get_board(p0.l(), p0.t(), C);
    bitboard_t lbishop = b0_ptr->lbishop() & b0_ptr->friendly<C>();
    for(auto d : diagonal_dtls)
    {
        bitboard_t remaining = lbishop;
        for(vec4 p1 = p0 + d; remaining && inbound(p1, C); p1 = p1 + d)
        {
            std::shared_ptr<board> b1_ptr = get_board(p1.l(), p1.t(), C);
            remaining &= ~b1_ptr->friendly<C>();
            if(remaining)
            {
                //result[p1.tl()] |= remaining;
                result.push_back(std::make_pair(p1.tl(), remaining));
                remaining &= ~b1_ptr->hostile<C>();
            }
        }
    }
    return result;
}


template<bool C>
std::vector<std::pair<vec4, bitboard_t>> multiverse::gen_purely_sp_knight_moves(vec4 p0) const
{
    std::vector<std::pair<vec4, bitboard_t>> result;
    std::shared_ptr<board> b0_ptr = get_board(p0.l(), p0.t(), C);
    bitboard_t lknight = b0_ptr->lknight() & b0_ptr->friendly<C>();
    const static std::vector<vec4> knight_pure_sp_delta = {vec4(0, 0, 2, 1), vec4(0, 0, 1, 2), vec4(0, 0, -2, 1), vec4(0, 0, 1, -2),
        vec4(0, 0, 2, -1), vec4(0, 0, -1, 2), vec4(0, 0, -2, -1), vec4(0, 0, -1, -2)};
    for(vec4 delta : knight_pure_sp_delta)
    {
        vec4 p1 = p0 + delta;
        if(inbound(p1, C))
        {
            std::shared_ptr<board> b1_ptr = get_board(p1.l(), p1.t(), C);
            bitboard_t remaining = lknight;
            remaining &= ~b1_ptr->friendly<C>();
            if(remaining)
            {
                result.push_back(std::make_pair(p1.tl(), remaining));
            }
        }
    }
    return result;
}


template<piece_t P, bool C>
bitboard_t multiverse::gen_physical_moves_impl(vec4 p) const
{
	std::shared_ptr<board> b_ptr = get_board(p.l(), p.t(), C);
    bitboard_t friendly = b_ptr->friendly<C>();
    bitboard_t hostile = b_ptr->hostile<C>();
    bitboard_t a;
    int pos = p.xy();
    bitboard_t z = pmask(pos);
    bitboard_t empty = ~(friendly | hostile);
    if constexpr (P == KING_W || P == KING_B || P == COMMON_KING_W || P == COMMON_KING_B)
    {
        a = king_attack(p.xy()) & ~friendly;
    }
    else if constexpr (P == KING_UW || P == KING_UB)
    {
        a = king_attack(p.xy()) & ~friendly;
        bitboard_t urook = b_ptr->umove() & b_ptr->rook() & friendly;
        if(!b_ptr->is_under_attack(p.xy(), C))
        {
            for(vec4 d : {vec4(1,0,0,0), vec4(-1,0,0,0)})
            {
                int i = 0;
                for(vec4 q = p + d; !q.outbound(); q = q + d)
                {
                    bitboard_t w = pmask(q.xy());
                    if(i < 2 && b_ptr->is_under_attack(q.xy(), C))
                    {
                        break;
                    }
                    else if(w & urook)
                    {
                        /*
                        Consider the castling move valid only when the unmoved rook
                          is at the edge of the board
                        (this is because the state.apply_move only moves the rooks on edges)
                        */
                        if((q+d).outbound())
                        {
                            a |= pmask((p + d + d).xy());
                        }
                        break;
                    }
                    else if(w & b_ptr->occupied())
                    {
                        break;
                    }
                    i++;
                }
            }
        }
    }
    else if constexpr (P == ROOK_W || P == ROOK_B || P == ROOK_UW || P == ROOK_UB)
    {
		a = rook_attack(p.xy(), b_ptr->occupied()) & ~friendly;
	}
    else if constexpr (P == BISHOP_W || P == BISHOP_B)
    {
		a = bishop_attack(p.xy(), b_ptr->occupied()) & ~friendly;
    }
    else if constexpr (P == QUEEN_W || P == QUEEN_B || P == PRINCESS_W || P == PRINCESS_B
                       || P == ROYAL_QUEEN_W || P == ROYAL_QUEEN_B)
    {
        a = queen_attack(p.xy(), b_ptr->occupied()) & ~friendly;
	}
    else if constexpr (P == PAWN_W || P == BRAWN_W || P == PAWN_UW || P == BRAWN_UW)
    {
        bitboard_t patt = white_pawn_attack(pos);
        // normal move and capture
		a = (patt & hostile) | (shift_north(z) & empty);
        // en passant
        bitboard_t r = (shift_west(z) | shift_east(z)) & hostile & b_ptr->pawn();
        bitboard_t s = shift_north(shift_north(r)) & empty;
        if(s)
        {
            vec4 q = p+vec4(0, 2, -1, 0);
            if(inbound(q, C))
            {
                std::shared_ptr<board> b1_ptr = get_board(q.l(), q.t(), C);
                bitboard_t j = s & b1_ptr->umove() & ~friendly & b1_ptr->pawn();
                a |= shift_south(j);
            }
        }
        // additional move for unmoved pawns
        if constexpr (P == PAWN_UW || P == BRAWN_UW)
        {
            a |= shift_north(shift_north(z) & empty) & empty;
        }
    }
    else if constexpr (P == PAWN_B || P == BRAWN_B || P == PAWN_UB || P == BRAWN_UB)
    {
        bitboard_t patt = black_pawn_attack(pos);
        // normal move and capture
        a = (patt & hostile) | (shift_south(z) & empty);
        // en passant
        bitboard_t r = (shift_west(z) | shift_east(z)) & hostile & b_ptr->pawn();
        bitboard_t s = shift_south(shift_south(r)) & empty;
        if(s)
        {
            vec4 q = p+vec4(0, 2, -1, 0);
            if(inbound(q, C))
            {
                std::shared_ptr<board> b1_ptr = get_board(q.l(), q.t(), C);
                bitboard_t j = s & b1_ptr->umove() & ~friendly & b1_ptr->pawn();
                a |= shift_north(j);
            }
        }
        // additional move for unmoved pawns
        if constexpr (P == PAWN_UB || P == BRAWN_UB)
        {
            a |= shift_south(shift_south(z) & empty) & empty;
        }
    }
	else if constexpr (P == KNIGHT_W || P == KNIGHT_B)
	{
		a = knight_attack(p.xy()) & ~friendly;
	}
    else if constexpr (P == UNICORN_W || P == UNICORN_B || P == DRAGON_W || P == DRAGON_B)
    {
        a = 0;
    }
	else
	{
		std::cerr << "gen_physical_moves_impl:" << P << "not implemented" << std::endl;
	}
	return a;
}

template<bool C, multiverse::axesmode TL, multiverse::axesmode XY>
void multiverse::gen_compound_moves(vec4 p, std::map<vec4, bitboard_t>& result) const
{
    int pos = p.xy();
    bitboard_t occ, fri;
    bitboard_t copy_mask;
    
    constexpr auto deltas = (TL==multiverse::axesmode::ORTHOGONAL) ? orthogonal_dtls : (TL==multiverse::axesmode::DIAGONAL) ? diagonal_dtls : both_dtls;
    
    constexpr auto copy_mask_fn = (XY==multiverse::axesmode::ORTHOGONAL) ? rook_copy_mask : (XY==multiverse::axesmode::DIAGONAL) ? bishop_copy_mask : queen_copy_mask;

    for(vec4 d : deltas)
    {
        vec4 q = p;
        occ = fri = 0;
        for (int n = 1; n < 8; n++)
        {
            copy_mask = copy_mask_fn(pos, n);
            q = q + d;
            // if the corresponding board exists, copy the cone slice
            if(inbound(q, C))
            {
                std::shared_ptr<board> b_ptr = get_board(q.l(), q.t(), C);
                occ |= copy_mask & b_ptr->occupied();
                fri |= copy_mask & b_ptr->friendly<C>();
            }
            // otherwise, set the cone slice to a blocker of friendly pieces, which prevents the attacking move towards this non-existant board
            else
            {
                occ |= copy_mask;
                fri |= copy_mask;
                break;
            }
        }
        bitboard_t loc = ~fri;
        if constexpr (XY == multiverse::axesmode::ORTHOGONAL)
        {
            loc &= rook_attack(pos, occ);
        }
        else if (XY == multiverse::axesmode::DIAGONAL)
        {
            loc &= bishop_attack(pos, occ);
        }
        else
        {
            loc &= queen_attack(pos, occ);
        }
        q = p;
        for (int n = 1; n < 8; n++)
        {
            copy_mask = copy_mask_fn(pos, n);
            q = q + d;
            bitboard_t c = loc & copy_mask;
            if(c)
            {
                result[q.tl()] |= c;
            }
            else
            {
                break;
            }
        }
    }
}

template<piece_t P, bool C, bool ONLY_SP>
movegen_t multiverse::gen_moves_impl(vec4 p) const
{
    if constexpr (!ONLY_SP)
    {
        bitboard_t bb = gen_physical_moves_impl<P, C>(p);
        if(bb)
        {
            // only generate this entry when there is at least one physical move
            co_yield std::make_pair(p.tl(), bb);
        }
    }
    if constexpr (P == KING_W || P == KING_B || P == COMMON_KING_W || P == COMMON_KING_B || P == KING_UW || P == KING_UB)
    {
        for(auto d : both_dtls)
        {
            vec4 q = p+d;
            if(inbound(q, C))
            {
                std::shared_ptr<board> b_ptr = get_board(q.l(), q.t(), C);
                bitboard_t bb = king_jump_attack(p.xy()) & ~b_ptr->friendly<C>();
                if(bb)
                {
                    co_yield std::make_pair(q.tl(), bb);
                }
            }
        }
    }
    else if constexpr (P == ROOK_W || P == ROOK_B || P == ROOK_UW || P == ROOK_UB)
    {
        bitboard_t z = pmask(p.xy());
        for(auto [index, bb] : gen_purely_sp_rook_moves<C>(p))
        {
            bitboard_t bb1 = bb & z;
            if(bb1)
            {
                co_yield std::make_pair(index.tl(), bb1);
            }
        }
    }
    else if constexpr (P == BISHOP_W || P == BISHOP_B)
    {
        bitboard_t z = pmask(p.xy());
        for(auto [index, bb] : gen_purely_sp_bishop_moves<C>(p))
        {
            bitboard_t bb1 = bb & z;
            if(bb1)
            {
                co_yield std::make_pair(index.tl(), bb1);
            }
        }
        std::map<vec4, bitboard_t> result;
        gen_compound_moves<C, multiverse::axesmode::ORTHOGONAL, multiverse::axesmode::ORTHOGONAL>(p, result);
        for(const auto& m : result)
        {
            co_yield m;
        }
    }
    else if constexpr (P == PRINCESS_W || P == PRINCESS_B)
    {
        bitboard_t z = pmask(p.xy());
        std::map<vec4, bitboard_t> result;
        for(auto [index, bb] : gen_purely_sp_rook_moves<C>(p))
        {
            bitboard_t bb1 = bb & z;
            if(bb1)
            {
                result[index.tl()] |= bb1;
            }
        }
        for(auto [index, bb] : gen_purely_sp_bishop_moves<C>(p))
        {
            bitboard_t bb1 = bb & z;
            if(bb1)
            {
                result[index.tl()] |= bb1;
            }
        }
        gen_compound_moves<C, multiverse::axesmode::ORTHOGONAL, multiverse::axesmode::ORTHOGONAL>(p, result);
        for(const auto x : result)
        {
            co_yield x;
        }
    }
    else if constexpr (P == QUEEN_W || P == QUEEN_B || P == ROYAL_QUEEN_W || P == ROYAL_QUEEN_B)
    {
        bitboard_t z = pmask(p.xy());
        std::map<vec4, bitboard_t> result;
        for(auto [index, bb] : gen_purely_sp_rook_moves<C>(p))
        {
            bitboard_t bb1 = bb & z;
            if(bb1)
            {
                result[index.tl()] |= bb1;
            }
        }
        for(auto [index, bb] : gen_purely_sp_bishop_moves<C>(p))
        {
            bitboard_t bb1 = bb & z;
            if(bb1)
            {
                result[index.tl()] |= bb1;
            }
        }
        gen_compound_moves<C, multiverse::axesmode::BOTH, multiverse::axesmode::BOTH>(p, result);
        for(const auto x : result)
        {
            co_yield x;
        }
    }
    else if constexpr (P == PAWN_W || P == BRAWN_W || P == PAWN_UW || P == BRAWN_UW)
    {
        bitboard_t z = pmask(p.xy());
        // pawn capture
        static std::vector<vec4> pawn_w_cap_tl_delta = {vec4(0, 0, 1, -1), vec4(0, 0, -1, -1)};
        for(vec4 d : pawn_w_cap_tl_delta)
        {
            vec4 q = p + d;
            if(inbound(q, C))
            {
                std::shared_ptr<board> b_ptr = get_board(q.l(), q.t(), C);
                bitboard_t bb = z & b_ptr->hostile<C>();
                if(bb)
                {
                    co_yield std::make_pair(q.tl(), bb);
                }
            }
        }
        // normal pawn movement -- bitboard saved in the very end of the if block
        vec4 q = p + vec4(0,0,0,-1);
        if(inbound(q, C))
        {
            std::shared_ptr<board> b_ptr = get_board(q.l(), q.t(), C);
            bitboard_t bb = z & ~b_ptr->occupied();
            if(bb)
            {
                // unmoved pawn movement
                if constexpr(P == PAWN_UW || P == BRAWN_UW)
                {
                    vec4 r = q + vec4(0,0,0,-1);
                    if(inbound(r,C))
                    {
                        std::shared_ptr<board> b1_ptr = get_board(r.l(), r.t(), C);
                        bitboard_t bc = z & ~b1_ptr->occupied();
                        if(bc)
                        {
                            //result[r.tl()] |= bc;
                            co_yield std::make_pair(r.tl(), bc);
                        }
                    }
                }
            }
            // brawn capture
            if constexpr(P == BRAWN_W || P == BRAWN_UW)
            {
                bitboard_t mask = shift_north(z) | shift_west(z) | shift_east(z);
                bb |= mask & b_ptr->hostile<C>();
            }
            if(bb)
            {
                co_yield std::make_pair(q.tl(), bb);
            }
        }
        if constexpr(P == BRAWN_W || P == BRAWN_UW)
        {
            static std::vector<vec4> brawn_w_cap_tl_delta = {vec4(1, 0, 0, -1), vec4(-1, 0, 0, -1), vec4(0, 1, 0, -1), vec4(0, 1, -1, 0)};
            for(vec4 d : brawn_w_cap_tl_delta)
            {
                vec4 s = p + d;
                if(inbound(s, C))
                {
                    std::shared_ptr<board> b2_ptr = get_board(s.l(), s.t(), C);
                    bitboard_t bd = shift_north(z) & ~b2_ptr->occupied();
                    if(bd)
                    {
                        co_yield std::make_pair(s.tl(), bd);
                    }
                }
            }
        }
    }
    else if constexpr (P == PAWN_B || P == BRAWN_B || P == PAWN_UB || P == BRAWN_UB)
    {
        bitboard_t z = pmask(p.xy());
        // pawn capture
        static std::vector<vec4> pawn_b_cap_tl_delta = {vec4(0, 0, 1, 1), vec4(0, 0, -1, 1)};
        for(vec4 d : pawn_b_cap_tl_delta)
        {
            vec4 q = p + d;
            if(inbound(q, C))
            {
                std::shared_ptr<board> b_ptr = get_board(q.l(), q.t(), C);
                bitboard_t bb = z & b_ptr->hostile<C>();
                if(bb)
                {
                    co_yield std::make_pair(q.tl(), bb);
                }
            }
        }
        // normal pawn movement -- bitboard saved in the very end of the if block
        vec4 q = p + vec4(0,0,0,1);
//        std::cout << p << " " << q << inbound(q,C) << "\n";
        if(inbound(q, C))
        {
            std::shared_ptr<board> b_ptr = get_board(q.l(), q.t(), C);
            bitboard_t bb = z & ~b_ptr->occupied();
            if(bb)
            {
                // unmoved pawn movement
                if constexpr(P == PAWN_UW || P == BRAWN_UW)
                {
                    vec4 r = q + vec4(0,0,0,1);
                    if(inbound(r,C))
                    {
                        std::shared_ptr<board> b1_ptr = get_board(r.l(), r.t(), C);
                        bitboard_t bc = z & ~b1_ptr->occupied();
                        if(bc)
                        {
                            co_yield std::make_pair(r.tl(), bc);
                        }
                    }
                }
            }
            // brawn capture
            if constexpr(P == BRAWN_W || P == BRAWN_UW)
            {
                bitboard_t mask = shift_south(z) | shift_west(z) | shift_east(z);
                bb |= mask & b_ptr->hostile<C>();
            }
            if(bb)
            {
                co_yield std::make_pair(q.tl(), bb);
            }
        }
        if constexpr(P == BRAWN_W || P == BRAWN_UW)
        {
            static std::vector<vec4> brawn_w_cap_tl_delta = {vec4(1, 0, 0, 1), vec4(-1, 0, 0, 1), vec4(0, 1, 0, 1), vec4(0, -1, -1, 0)};
            for(vec4 d : brawn_w_cap_tl_delta)
            {
                vec4 s = p + d;
                if(inbound(s, C))
                {
                    std::shared_ptr<board> b2_ptr = get_board(s.l(), s.t(), C);
                    bitboard_t bd = shift_north(z) & ~b2_ptr->occupied();
                    if(bd)
                    {
                        co_yield std::make_pair(s.tl(), bd);
                    }
                }
            }
        }
    }
    else if constexpr (P == KNIGHT_W || P == KNIGHT_B)
    {
        for(auto [index, bb] : gen_purely_sp_knight_moves<C>(p))
        {
            bitboard_t bb1 = bb & pmask(p.xy());
            if(bb1)
            {
                co_yield std::make_pair(index.tl(), bb1);
            }
        }
        for(auto d : orthogonal_dtls)
        {
            vec4 q = p+d;
            if(inbound(q, C))
            {
                std::shared_ptr<board> b_ptr = get_board(q.l(), q.t(), C);
                bitboard_t bb = knight_jump1_attack(p.xy()) & ~b_ptr->friendly<C>();
                if(bb)
                {
                    co_yield std::make_pair(q.tl(), bb);
                }
            }
        }
        for(auto d : double_dtls)
        {
            vec4 q = p+d;
            if(inbound(q, C))
            {
                std::shared_ptr<board> b_ptr = get_board(q.l(), q.t(), C);
                bitboard_t bb = knight_jump2_attack(p.xy()) & ~b_ptr->friendly<C>();
                if(bb)
                {
                    co_yield std::make_pair(q.tl(), bb);
                }
            }
        }
    }
    else if constexpr (P == UNICORN_W || P == UNICORN_B)
    {
        std::map<vec4, bitboard_t> r1, r2;
        gen_compound_moves<C, multiverse::axesmode::ORTHOGONAL, multiverse::axesmode::DIAGONAL>(p, r1);
        for(const auto& m : r1)
        {
            co_yield m;
        }
        gen_compound_moves<C, multiverse::axesmode::DIAGONAL, multiverse::axesmode::ORTHOGONAL>(p, r2);
        for(const auto& m : r2)
        {
            co_yield m;
        }
    }
    else if constexpr (P == DRAGON_W || P == DRAGON_B)
    {
        std::map<vec4, bitboard_t> result;
        gen_compound_moves<C, multiverse::axesmode::DIAGONAL, multiverse::axesmode::DIAGONAL>(p, result);
        for(const auto& m : result)
        {
            co_yield m;
        }
    }
    else
    {
        std::cerr << "gen_superphysical_moves_impl:" << P << "not implemented" << std::endl;
    }
}

template <bool C>
generator<vec4> multiverse::gen_board_move_impl(vec4 p0) const
{
    std::shared_ptr<board> b_ptr = get_board(p0.l(), p0.t(), C);
    bitboard_t bb = b_ptr->friendly<C>() & ~b_ptr->wall();
    for(int pos : marked_pos(bb))
    {
        vec4 p = vec4(pos, p0.tl());
        // TODO: optimize code below
        // note: gen_purely_sp_*_moves for *=rxook,bishop,knight are fixed for the whole board
        // but we are calling them for each piece here
        movegen_t gen = C ? gen_moves<true>(p) : gen_moves<false>(p);
        for (const auto& [r, bb] : gen)
        {
            for(int pos : marked_pos(bb))
            {
                vec4 q = vec4(pos, r);
				if (!outofrange(p, q, C))
                co_yield q;
            }
        }
    }
}

// Explicit instantiation of the template for specific types
#define INIT_TEMPLATE(PIECE) \
template bitboard_t multiverse::gen_physical_moves_impl<PIECE, true>(vec4 p) const; \
template bitboard_t multiverse::gen_physical_moves_impl<PIECE, false>(vec4 p) const; \
template movegen_t multiverse::gen_moves_impl<PIECE, true, true>(vec4 p) const; \
template movegen_t multiverse::gen_moves_impl<PIECE, false, true>(vec4 p) const; \
template movegen_t multiverse::gen_moves_impl<PIECE, true, false>(vec4 p) const; \
template movegen_t multiverse::gen_moves_impl<PIECE, false, false>(vec4 p) const;

INIT_TEMPLATE(KING_W)
INIT_TEMPLATE(KING_B)
INIT_TEMPLATE(KING_UW)
INIT_TEMPLATE(KING_UB)
INIT_TEMPLATE(COMMON_KING_W)
INIT_TEMPLATE(COMMON_KING_B)
INIT_TEMPLATE(ROOK_W)
INIT_TEMPLATE(ROOK_B)
INIT_TEMPLATE(ROOK_UW)
INIT_TEMPLATE(ROOK_UB)
INIT_TEMPLATE(BISHOP_W)
INIT_TEMPLATE(BISHOP_B)
INIT_TEMPLATE(QUEEN_W)
INIT_TEMPLATE(QUEEN_B)
INIT_TEMPLATE(ROYAL_QUEEN_W)
INIT_TEMPLATE(ROYAL_QUEEN_B)
INIT_TEMPLATE(PRINCESS_W)
INIT_TEMPLATE(PRINCESS_B)
INIT_TEMPLATE(PAWN_W)
INIT_TEMPLATE(BRAWN_W)
INIT_TEMPLATE(PAWN_B)
INIT_TEMPLATE(BRAWN_B)
INIT_TEMPLATE(PAWN_UW)
INIT_TEMPLATE(BRAWN_UW)
INIT_TEMPLATE(PAWN_UB)
INIT_TEMPLATE(BRAWN_UB)
INIT_TEMPLATE(KNIGHT_W)
INIT_TEMPLATE(KNIGHT_B)
INIT_TEMPLATE(UNICORN_W)
INIT_TEMPLATE(UNICORN_B)
INIT_TEMPLATE(DRAGON_W)
INIT_TEMPLATE(DRAGON_B)
#undef INIT_TEMPLATE

template std::vector<std::pair<vec4, bitboard_t>> multiverse::gen_purely_sp_rook_moves<false>(vec4 p) const;
template std::vector<std::pair<vec4, bitboard_t>> multiverse::gen_purely_sp_rook_moves<true>(vec4 p) const;
template std::vector<std::pair<vec4, bitboard_t>> multiverse::gen_purely_sp_bishop_moves<false>(vec4 p) const;
template std::vector<std::pair<vec4, bitboard_t>> multiverse::gen_purely_sp_bishop_moves<true>(vec4 p) const;
template std::vector<std::pair<vec4, bitboard_t>> multiverse::gen_purely_sp_knight_moves<false>(vec4 p) const;
template std::vector<std::pair<vec4, bitboard_t>> multiverse::gen_purely_sp_knight_moves<true>(vec4 p) const;


template void multiverse::gen_compound_moves<false, multiverse::axesmode::ORTHOGONAL, multiverse::axesmode::ORTHOGONAL>(vec4 p, std::map<vec4, bitboard_t>& result) const;
template void multiverse::gen_compound_moves<true, multiverse::axesmode::ORTHOGONAL, multiverse::axesmode::ORTHOGONAL>(vec4 p, std::map<vec4, bitboard_t>& result) const;
template void multiverse::gen_compound_moves<false, multiverse::axesmode::DIAGONAL, multiverse::axesmode::DIAGONAL>(vec4 p, std::map<vec4, bitboard_t>& result) const;
template void multiverse::gen_compound_moves<true, multiverse::axesmode::DIAGONAL, multiverse::axesmode::DIAGONAL>(vec4 p, std::map<vec4, bitboard_t>& result) const;
template void multiverse::gen_compound_moves<false, multiverse::axesmode::BOTH, multiverse::axesmode::BOTH>(vec4 p, std::map<vec4, bitboard_t>& result) const;
template void multiverse::gen_compound_moves<true, multiverse::axesmode::BOTH, multiverse::axesmode::BOTH>(vec4 p, std::map<vec4, bitboard_t>& result) const;

template bitboard_t multiverse::gen_physical_moves<true>(vec4 p) const;
template bitboard_t multiverse::gen_physical_moves<false>(vec4 p) const;

template movegen_t multiverse::gen_superphysical_moves<true>(vec4 p) const;
template movegen_t multiverse::gen_superphysical_moves<false>(vec4 p) const;

template movegen_t multiverse::gen_moves<true>(vec4 p) const;
template movegen_t multiverse::gen_moves<false>(vec4 p) const;

template std::vector<std::tuple<int,int,bool,std::string>> multiverse::get_boards<true>() const;
template std::vector<std::tuple<int,int,bool,std::string>> multiverse::get_boards<false>() const;

template std::vector<std::pair<int,std::vector<uint64_t>>> multiverse::get_operable_boards_moves<true>(bool) const;
template std::vector<std::pair<int,std::vector<uint64_t>>> multiverse::get_operable_boards_moves<false>(bool) const;
