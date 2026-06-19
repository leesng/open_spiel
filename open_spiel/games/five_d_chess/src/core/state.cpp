#include "state.h"
#include <algorithm>
#include <cassert>
#include <functional>
#include "utils.h"
#include "variants.h"
#include "pgnparser.h"
#include "hypercuboid.h"

//#define DEBUGMSG
#include "debug.h"


state::state(multiverse &mtv) noexcept : m(mtv.clone())
{
    std::tie(present, player) = m->get_present();
	has_passed = false;
}

state::state(const pgnparser_ast::game &g, std::function<void(const state&, const ext_move&)> on_step)
{
    auto variant_setup = derive_variant_setup(g);
    m = create_multiverse_from_variant_setup(variant_setup);
    std::tie(present, player) = m->get_present();
	has_passed = false;

    // parse moves
    const pgnparser_ast::gametree *gt = &g.gt;
    while(true)
    {
        if(!std::holds_alternative<pgnparser_ast::gametree::variations_t>(gt->variations_or_outcome))
        {
            // pgnparser_ast::token_t outcome = std::get<pgnparser_ast::token_t>(gt->variations_or_outcome);
            // (void)outcome;
            break;
        }

        const auto &variations = std::get<pgnparser_ast::gametree::variations_t>(gt->variations_or_outcome);
        if(variations.empty())
            break;

        const auto &[act, last_gt] = *(variations.end() - 1);
        //std::cout << act;
        for(const auto& mv: act.moves)
        {
            auto [fm_opt, pt_opt, candidates] = parse_move(mv);
            if(!fm_opt.has_value())
            {
                if(candidates.empty())
                {
                    std::ostringstream oss;
                    dprint(to_string());
                    oss << "state(): Invalid move: " << mv;
                    throw std::runtime_error(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    dprint(to_string());
                    oss << "state(): Ambiguous move: " << mv << "; candidates: ";
                    oss << range_to_string(candidates, "", "");
                    throw std::runtime_error(oss.str());
                }
            }
            else
            {
                state current_state_before_move = *this;
                full_move fm = fm_opt.value();
                bool flag;
                if(pt_opt.has_value())
                {
                    piece_t pt = to_white(*pt_opt);
                    flag = apply_move<false>(fm, pt);
                }
                else
                {
                    flag = apply_move<false>(fm);
                }
                if(!flag)
                {
                    std::ostringstream oss;
                    oss << "state(): Illegal move: " << mv << " (parsed as: " << fm << ")";
                    throw std::runtime_error(oss.str());
                }
                if (on_step) {
                    ext_move target_move = pt_opt.has_value()
                        ? ext_move(fm, to_white(*pt_opt))
                        : ext_move(fm);
                    on_step(current_state_before_move, target_move);
                }
            }
        }
        if(std::holds_alternative<pgnparser_ast::gametree::variations_t>(last_gt->variations_or_outcome))
        {
            const auto &last_variations = std::get<pgnparser_ast::gametree::variations_t>(last_gt->variations_or_outcome);
            if(!last_variations.empty())
            {
                bool flag = submit();
                if(!flag)
                {
                    std::ostringstream oss;
                    oss << "state(): Cannot submit after parsing these moves: " << act;
                    throw std::runtime_error(oss.str());
                }
            }
            else
            {
                bool flag = submit();
                if(!flag)
                {
                    std::cerr << "[WARNING]state(): Cannot submit after parsing these moves: " << act;
                }
            }
        }
        else
        {
            pgnparser_ast::token_t outcome = std::get<pgnparser_ast::token_t>(last_gt->variations_or_outcome);
            (void)outcome;
            // TODO: Handle game outcome token when checking continuation state.
        }
        gt = last_gt.get();
    }
}

int state::new_line() const
{
    auto [l_min, l_max] = m->get_lines_range();
    if(player == 0)
        return l_max + 1;
    else
        return l_min - 1;
}

std::optional<state> state::can_submit() const
{
    state new_state = *this;
    bool flag = new_state.submit<false>();
    if(flag)
    {
        return std::optional<state>(new_state);
    }
    else
    {
        return std::nullopt;
    }
}

std::optional<state> state::can_apply(full_move fm, piece_t promote_to) const
{
    state new_state = *this;
    bool flag = new_state.apply_move<false>(fm, promote_to);
    if(flag)
    {
        return std::make_optional<state>(new_state);
    }
    else
    {
        return std::nullopt;
    }
}

std::optional<state> state::can_apply(const action &act) const
{
    state new_state = *this;
    for(const auto& em : act.get_moves())
    {
        bool flag = new_state.apply_move<false>(em.fm, em.promote_to);
        if(!flag)
        {
            return std::nullopt;
        }
    }
    bool flag = new_state.submit<false>();
    if(!flag)
    {
        return std::nullopt;
    }
    return std::make_optional<state>(new_state);
}

template<bool UNSAFE>
bool state::apply_move(full_move fm, piece_t promote_to)
{
    dprint("applying move", fm);

    if constexpr (!UNSAFE)
    {
		vec4 p = fm.from;
		vec4 q = fm.to;
		vec4 d = q - p;
	
		if (q != vec4(0, 0, 0, 0)) {
#ifndef NDEBUG
        auto te = m->get_timeline_end(p.l());
        assert(std::make_pair(p.t(), player) == te && "moves must be made on an active board");
#endif
        auto mvs = player ? m->gen_moves<true>(p) : m->gen_moves<false>(p);
        //auto it = mvbbs.find(q.tl());
        const auto &res = mvs.find([&q](const auto &pair){
            const auto &[tl, bb] = pair;
            return tl == q.tl();
        });
        // is it a pseudolegal move?
        if(res)
        {
            bitboard_t bb = res.value().second;
            if(!(pmask(q.xy()) & bb))
            {
                return false;
            }
        }
        else
        {
            return false;
        }
		}
    }
    
    /* WARNING: similiar logic used in hypercuboid.cpp for applying semimoves
     If some move logic needs to be changed here, make sure also perform change
     in HC_info::build_HC()
     */
	/*{
		auto [l_min, l_max] = m->get_lines_range();
		auto [active_min, active_max] = m->get_active_range();
		std::cout << "111l_min=" << l_min << ",l_max=" << l_max
					<< ",active_min=" << active_min << ",active_max=" << active_max << std::endl;
		for (int l = l_min; l <= l_max; l++) {
			auto ts = m->get_timeline_start(l);
			auto te = m->get_timeline_end(l);
			std::cout << "u=" << l_to_u(l) << ",ts_v=" << tc_to_v(ts.first, ts.second)
										   << ",te_v=" << tc_to_v(te.first, te.second) << std::endl;
		}
	}*/
	apply_move_and_return_new_boards(fm, promote_to);

    return true;
}

std::vector<int> state::apply_move_and_return_new_boards(full_move fm, piece_t promote_to)
{
	std::vector<int> new_lines;
	
    dprint("applying move", fm);
    vec4 p = fm.from;
    vec4 q = fm.to;
    vec4 d = q - p;

    // pass move
    if (q == vec4(0, 0, 0, 0)) {
		// mark it has beed passed
		has_passed = true;

        return new_lines;
    }

    /* WARNING: similiar logic used in hypercuboid.cpp for applying semimoves
     If some move logic needs to be changed here, make sure also perform change
     in HC_info::build_HC()
     */
    // physical move, no time travel
    if(d.l() == 0 && d.t() == 0)
    {
        const std::shared_ptr<board>& b_ptr = m->get_board(p.l(), p.t(), player);
        bitboard_t z = pmask(p.xy());
        const auto &[size_x, size_y] = m->get_board_size();
        std::shared_ptr<board> new_board;
        // en passant
        if((b_ptr->lpawn()&z) && d.x()!=0 && b_ptr->get_piece(q.xy()) == NO_PIECE)
        {
            dprint(" ... en passant");
            new_board = b_ptr->replace_piece(ppos(q.x(), p.y()), NO_PIECE)
                ->move_piece(p.xy(), q.xy());
            m->append_board(p.l(), new_board);
        }
        // promotion
        else if((b_ptr->lpawn()&z) && (q.y() == 0 || q.y() == size_y - 1))
        {
            dprint(" ... promotion");
            piece_t promoted = player ? to_black(promote_to) : promote_to;
            new_board = b_ptr->replace_piece(p.xy(), NO_PIECE)
                ->replace_piece(q.xy(), promoted);
            m->append_board(p.l(), new_board);
        }
        // castling
        else if((b_ptr->king()&z) && abs(d.x()) > 1)
        {
            dprint(" ... castling");
            int rook_x1 = d.x() < 0 ? 0 : (size_x - 1); //rook's original x coordinate
            int rook_x2 = q.x() + (d.x() < 0 ? 1 : -1); //rook's new x coordinate
            new_board = b_ptr->move_piece(ppos(rook_x1, p.y()), ppos(rook_x2, q.y()))
                ->move_piece(p.xy(), q.xy());
            m->append_board(p.l(), new_board);
        }
        // normal move
        else
        {
            dprint(" ... normal move/capture");
            new_board = b_ptr->move_piece(p.xy(), q.xy());
            m->append_board(p.l(), new_board);
        }

        new_board->contact() = (static_cast<uint64_t>(p.l()) & 0xFF) << 8;
		
		new_lines.push_back(p.l());

    }
    // non-branching superphysical move
    else if (std::make_pair(q.t(), player) == m->get_timeline_end(q.l()))
    {
        const std::shared_ptr<board>& b_ptr = m->get_board(p.l(), p.t(), player);
        const piece_t& pic = static_cast<piece_t>(piece_name(b_ptr->get_piece(p.xy())));
        std::shared_ptr<board> new_board_from = b_ptr->replace_piece(p.xy(), NO_PIECE);
        m->append_board(p.l(), new_board_from);
        
        bitboard_t z = pmask(p.xy());
        const auto &[size_x, size_y] = m->get_board_size();
        const std::shared_ptr<board>& c_ptr = m->get_board(q.l(), q.t(), player);
        std::shared_ptr<board> new_board_to;
        
        // promotion (only brawns can do)
        if ((b_ptr->lrawn()&z) && (q.y() == 0 || q.y() == size_y - 1))
        {
            dprint(" ... nonbranching brawn promotion");
            piece_t promoted = player ? to_black(promote_to) : promote_to;
            new_board_to = c_ptr->replace_piece(q.xy(), promoted);
            m->append_board(q.l(), new_board_to);
        }
        // normal non_branching move
        else
        {
            dprint(" ... nonbranching jump");
            new_board_to = c_ptr->replace_piece(q.xy(), pic);
            m->append_board(q.l(), new_board_to);
        }
        new_board_from->contact() = (static_cast<uint64_t>(p.l()) & 0xFF) << 8;
		new_board_from->contact() |= (static_cast<uint64_t>(q.l()) & 0xFF) << 24;
        new_board_from->contact() |= (static_cast<uint64_t>(q.t() + player) & 0xFF) << 16;
		
        new_board_to->contact() = (static_cast<uint64_t>(q.l()) & 0xFF) << 8;
        new_board_to->contact() |= (static_cast<uint64_t>(p.l()) & 0xFF) << 24;
        new_board_to->contact() |= (static_cast<uint64_t>(p.t() + player) & 0xFF) << 16;
		
		new_lines.push_back(p.l());
		new_lines.push_back(q.l());
		
    }
    //branching move
    else
    {
        const std::shared_ptr<board>& b_ptr = m->get_board(p.l(), p.t(), player);
        const piece_t& pic = static_cast<piece_t>(piece_name(b_ptr->get_piece(p.xy())));
        std::shared_ptr<board> new_board_from = b_ptr->replace_piece(p.xy(), NO_PIECE);
        m->append_board(p.l(), new_board_from);
        const std::shared_ptr<board>& x_ptr = m->get_board(q.l(), q.t(), player);
        auto [t, c] = next_turn({q.t(), player});
        
        bitboard_t z = pmask(p.xy());
        const auto &[size_x, size_y] = m->get_board_size();
        std::shared_ptr<board> new_board_to;
        int new_l_to;
        
        // promotion (only brawns can do)
        if ((b_ptr->lrawn()&z) && (q.y() == 0 || q.y() == size_y - 1))
        {
            dprint(" ... branching brawn promotion");
            piece_t promoted = player ? to_black(promote_to) : promote_to;
            new_board_to = x_ptr->replace_piece(q.xy(), promoted);
            m->insert_board(new_l_to = new_line(), t, c, new_board_to);
        }
        // normal non_branching move
        else
        {
            dprint(" ... branching jump");
            new_board_to = x_ptr->replace_piece(q.xy(), pic);
            m->insert_board(new_l_to = new_line(), t, c, new_board_to);
        }
        auto [new_present, _] = m->get_present();
        if(new_present < present)
        {
            // if a historical board is activated by this travel, go back
            present = new_present;
        }

        new_board_from->contact() = (static_cast<uint64_t>(p.l()) & 0xFF) << 8;
		new_board_from->contact() |= (static_cast<uint64_t>(new_l_to) & 0xFF) << 24;
        new_board_from->contact() |= (static_cast<uint64_t>(q.t() + player) & 0xFF) << 16;
		
        new_board_to->contact() = (static_cast<uint64_t>(q.l()) & 0xFF) << 8;
        new_board_to->contact() |= (static_cast<uint64_t>(p.l()) & 0xFF) << 24;
        new_board_to->contact() |= (static_cast<uint64_t>(p.t() + player) & 0xFF) << 16;
		
		new_lines.push_back(p.l());
		new_lines.push_back(new_l_to);
    }
    return new_lines;
}

void state::unapply_move_by_new_boards(std::vector<int> new_lines, bool maybe_passed)
{
    if (new_lines.empty())
    {
		if (maybe_passed) {
			// Undo pass move: reset the flag
			has_passed = false;
		}
        return;
    }

    // Undo in reverse order (LIFO):
    // remove newly created timelines first, then roll back the original timeline.
    // This avoids transient invalid boundary states during the undo process.
    for (auto it = new_lines.rbegin(); it != new_lines.rend(); ++it)
    {
        auto l = *it;
        m->drop_board(l);
    }

    // Restore present timestamp.
    // Branching moves may push present backward; recalculating via get_present()
    // works correctly for physical, non-branching and branching moves alike.
    present = m->get_present().first;
}

state::move_info state::get_move_info(full_move fm, piece_t pt) const
{
    dprint("get_move_info", fm);
    std::optional<state> new_state_opt = can_apply(fm, pt);
    vec4 new_pos(0,0,0,0);
    std::unique_ptr<state> new_state;
    bool checking_opponent = false;
    
    auto find_board_check = [](const state &s, int l) -> bool {
        auto [t,c] = s.get_timeline_end(l);
        assert(c==s.player);
        //find checks on the source board
        std::shared_ptr<board> b = s.get_board(l, t, c);
        bitboard_t pieces = c ? b->black()&~b->white() : b->white()&~b->black();
        // for each friendly piece on this board
        for (int src_pos : marked_pos(pieces))
        {
            vec4 p = vec4(src_pos, vec4(0,0,t,l));
            // generate the aviliable moves
            auto moves = c ? s.m->gen_moves<true>(p) : s.m->gen_moves<false>(p);
            // for each destination board and bit location
            for (const auto& [q0, bb] : moves)
            {
                std::shared_ptr<board> b1_ptr = s.m->get_board(q0.l(), q0.t(), c);
                if (bb)
                {
                    // if the destination square is royal, this is a check
                    bitboard_t c_pieces = bb & b1_ptr->royal();
                    if (c_pieces)
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    };
    
    if(new_state_opt)
    {
        new_state = std::make_unique<state>(*new_state_opt);
        
        state s = *new_state_opt;
        const auto [l_min, l_max] = s.get_lines_range();
        for(int l = l_min; l <= l_max; l++)
        {
            auto [t,c] = s.get_timeline_end(l);
            if(c == !s.player)
            {
                dprint("duplicated board on line", l, "turn", t, c?"b":"w");
                s.m->append_board(l, s.m->get_board(l, t, c));
            }
        }
        
        vec4 p = fm.from;
        vec4 q = fm.to;
        vec4 d = q - p;
        
        /* WARNING: similiar logic used in hypercuboid.cpp for applying semimoves
         If some move logic needs to be changed here, make sure also perform change
         in HC_info::build_HC()
         */
        // pass move
        if (q == vec4(0, 0, 0, 0)) {
            dprint(" ... pass move");
            new_pos = p;
            checking_opponent = find_board_check(s, p.l());
        } else 
        // physical move, no time travel
        if(d.l() == 0 && d.t() == 0)
        {
            dprint(" ... physical move");
            new_pos = q + vec4(0,0,1,0);
            checking_opponent = find_board_check(s, q.l());
        }
        // non-branching superphysical move
        else if (std::make_pair(q.t(), player) == m->get_timeline_end(q.l()))
        {
            dprint(" ... non-branching superphysical move");
            new_pos = q + vec4(0,0,1,0);
            checking_opponent = find_board_check(s, q.l()) || find_board_check(s, p.l());
        }
        //branching move
        else
        {
            dprint(" ... branching superphysical move");
            new_pos = vec4(q.x(), q.y(), q.t()+1, new_line());
            checking_opponent = find_board_check(s, new_line()) || find_board_check(s, p.l());
        }
    }
    dprint(checking_opponent ? "checking" : "not checking");
    return {std::move(new_state), new_pos, checking_opponent};
}

template <bool UNSAFE>
bool state::submit()
{
    auto [t, c] = m->get_present();
    if constexpr (!UNSAFE)
    {
        if(player == c)
        {
            return false;
        }
    }
    present = t;
    player  = c;
	has_passed = false;
	
    return true;
}

state state::phantom() const
{
    const auto [l_min, l_max] = get_lines_range();
    state s = *this;
    for(int l = l_min; l <= l_max; l++)
    {
        auto [t,c] = get_timeline_end(l);
        if(c == player)
        {
            s.m->append_board(l, m->get_board(l, t, c));
        }
    }
    return s;
}

std::vector<int> state::phantom_and_return_new_boards(bool color)
{
	std::vector<int> new_lines;
    const auto [l_min, l_max] = get_lines_range();
    for(int l = l_min; l <= l_max; l++)
    {
        auto [t, c] = get_timeline_end(l);
        if(c == color)
        {
            m->append_board(l, m->get_board(l, t, c));
			new_lines.push_back(l);
        }
    }
    return new_lines;
}

std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> state::get_timeline_status() const
{
    return get_timeline_status(present, player);
}

std::tuple<std::vector<int>, std::vector<int>, std::vector<int>> state::get_timeline_status(int present_t, bool present_c) const
{
    auto [l_min, l_max] = m->get_lines_range();
    auto [active_min, active_max] = m->get_active_range();
    turn_t present_tc = std::make_pair(present_t, present_c);
    std::vector<int> mandatory_timelines, optional_timelines, unplayable_timelines;
    for(int l = l_min; l <= l_max; l++)
    {
        turn_t tc = m->get_timeline_end(l);
        if (active_min <= l && active_max >= l && tc == present_tc)
        {
            mandatory_timelines.push_back(l);
        }
        else
        {
            auto [t, c] = tc;
            if(present_c==c)
            {
                optional_timelines.push_back(l);
            }
            else
            {
                unplayable_timelines.push_back(l);
            }
        }
    }
    return std::make_tuple(mandatory_timelines, optional_timelines, unplayable_timelines);
}


/**********
 *********
 ********
 *******
 ******
 *****
 ****
 ***
 **
 */

generator<full_move> state::find_checks(bool c) const
{
    auto [l_min, l_max] = m->get_lines_range();
    std::vector<int> lines;
    for(int i = l_min; i <= l_max; i++)
    {
        auto [t, color] = m->get_timeline_end(i);
        if(color == c)
        {
            lines.push_back(i);
        }
    }
    if (c)
    {
        return find_checks_impl<true>(lines);
    }
    else
    {
        return find_checks_impl<false>(lines);
    }
}

template<bool C>
generator<full_move> state::find_checks_impl(std::vector<int> lines) const
{
//    print_range(__PRETTY_FUNCTION__, lines);
    for (int l : lines)
    {
        // take the active board
        auto [t, c] = m->get_timeline_end(l);
        assert(c == C);
        std::shared_ptr<board> b_ptr = m->get_board(l, t, C);
        bitboard_t b_pieces = b_ptr->friendly<C>() & ~b_ptr->wall();
        // for each friendly piece on this board
        for (int src_pos : marked_pos(b_pieces))
        {
            vec4 p = vec4(src_pos, vec4(0,0,t,l));
            // generate the aviliable moves
            auto moves = m->gen_moves<C>(p);
            // for each destination board and bit location
            for (const auto& [q0, bb] : moves)
            {
                std::shared_ptr<board> b1_ptr = m->get_board(q0.l(), q0.t(), C);
                if (bb)
                {
                    // if the destination square is royal, this is a check
                    bitboard_t c_pieces = bb & b1_ptr->royal();
                    if (c_pieces)
                    {
                        for(int dst_pos : marked_pos(c_pieces))
                        {
                            vec4 q = vec4(dst_pos, q0);
                            dprint("found check", full_move(p,q), "source:", p);
							//if (!outofrange(p, q, C))
                            co_yield full_move(p, q);
                        }
                    }
                }
            }
        }
    }
}


std::vector<vec4> state::gen_movable_pieces() const
{
    auto [mandatory_timelines, optional_timelines, unplayable_timelines] = get_timeline_status(present, player);
    auto lines = concat_vectors(mandatory_timelines, optional_timelines);
    return get_movable_pieces(lines);
}

std::vector<vec4> state::get_movable_pieces(std::vector<int> lines) const
{
    if (player == 0)
    {
        return gen_movable_pieces_impl<false>(lines);
    }
    else
    {
        return gen_movable_pieces_impl<true>(lines);
    }
}

template <bool C>
std::vector<vec4> state::gen_movable_pieces_impl(std::vector<int> lines) const
{
    dprint("gen_movable_pieces_impl()");
    std::vector<vec4> result;
    for (int l : lines)
    {
        // take the active board
        auto [t, c] = get_timeline_end(l);
        const vec4 p0 = vec4(0,0,t,l);
//        assert(c == C);
        std::shared_ptr<board> b_ptr = m->get_board(l, t, C);
        bitboard_t b_pieces = b_ptr->friendly<C>() & ~b_ptr->wall();
        // for each friendly piece on this board
        for (int src_pos : marked_pos(b_pieces))
        {
            vec4 p = vec4(src_pos, p0);
            // generate the aviliable moves
            auto moves = m->gen_moves<C>(p);
            // for each destination board and bit location
			bool found = false;
			for (const auto& [r, bb] : moves)
			{
				for(int pos : marked_pos(bb))
				{
					vec4 q = vec4(pos, r);
					if (!m->outofrange(p, q, C)) 
					{
						found = true;
						break;
					}
				}
				if (found) {
					break;
				}
			}
            if(found)
            {
                result.push_back(p);
            }
        }
    }
    dprint(range_to_string(result));
    return result;
}

state::mate_type state::get_mate_type() const
{
    dprint("state::get_mate_type()");
    auto [w, ss] = HC_info::build_HC(*this);
    auto hc = ss.back();
    search_space ss1 {{hc}};
    ss.pop_back();
    // check if there is a non-branching move
    if(w.search(ss1).first())
    {
        dprint("has non-branching action");
        return mate_type::NONE;
    }
    search_space ss2 = ss;
    /* player can only create timeline_advantage+1 active lines */
    const auto [l0_min, l0_max] = get_initial_lines_range();
    const auto [l_min, l_max] = get_lines_range();
    int whites_lines = l_max - l0_max;
    int blacks_lines = l0_min - l_min;
    int timeline_advantage = player ? (whites_lines - blacks_lines) : (blacks_lines - whites_lines);
    /* Build the search space `ss2` from `ss` so that
    1. On new lines that are active, erase all moves traveling back in time
    2. On other lines, do nothing */
    for(HC &hc : ss2)
    {
        //NOTE: because most axes are the same, the following code can be optimized
        int max_axis = std::min(w.new_axis+timeline_advantage+1, w.dimension-1);
        for(int n = w.new_axis; n <= max_axis; n++)
        {
            hc[n].erase_if([&w, n, old_t=present](int i){
                if(std::holds_alternative<arriving_move>(w.axis_coords[n][i]))
                {
                    auto am = std::get<arriving_move>(w.axis_coords[n][i]);
                    int new_t = am.m.to.t();
                    return new_t < old_t;
                }
                return false;
            });
        }
    }
    if(w.search(ss2).first())
    {
        dprint("has branching non-jump back solution");
        return mate_type::NONE;
    }
    if(w.search(ss).first())
    {
        if(phantom().find_checks(!player).first().has_value())
        {
            dprint("softmate");
            return mate_type::SOFTMATE;
        }
        else
        {
            dprint("almost softmate except for not checking opponent");
            return mate_type::NONE;
        }
    }
    else
    {
        if(phantom().find_checks(!player).first().has_value())
        {
            dprint("checkmate");
            return mate_type::CHECKMATE;
        }
        else
        {
            dprint("stalemate");
            return mate_type::STALEMATE;
        }
    }
}

std::pair<int, int> state::get_board_size() const
{
    return m->get_board_size();
}


turn_t state::get_present() const
{
    return std::make_pair(present, player);
}

turn_t state::apparent_present() const
{
    return m->get_present();
}

std::pair<int, int> state::get_initial_lines_range() const
{
    return m->get_initial_lines_range();
}

std::pair<int, int> state::get_lines_range() const
{
    return m->get_lines_range();
}

std::pair<int, int> state::get_active_range() const
{
    return m->get_active_range();
}

turn_t state::get_timeline_start(int l) const
{
    return m->get_timeline_start(l);
}

turn_t state::get_timeline_end(int l) const
{
    return m->get_timeline_end(l);
}

piece_t state::get_piece(vec4 p, bool color) const
{
    return m->get_piece(p, color);
}

std::shared_ptr<board> state::get_board(int l, int t, bool c) const
{
    return m->get_board(l, t, c);
}

std::vector<std::tuple<int, int, bool, std::string>> state::get_boards() const
{
    return m->get_boards();
}

generator<vec4> state::gen_piece_move(vec4 p) const
{
    return m->gen_piece_move(p, player);
}

generator<vec4> state::gen_piece_move(vec4 p, bool c) const
{
    return m->gen_piece_move(p, c);
}

std::string state::to_string() const
{
    std::ostringstream ss;
    ss << "State(present=" << present << ", player=" << player << "):\n";
    return ss.str() + m->to_string();
}

std::string state::show_fen() const
{
    std::ostringstream oss;
    for(const auto &[l,t,c,s] : m->get_boards<true>())
    {
        oss << "[" << s << ":" << m->pretty_l(l);
        oss << ":" << t << ":" << (c?"b":"w") << "]\n";
    }
    return oss.str();
}

state::parse_pgn_res state::parse_move(const pgnparser_ast::move &move) const
{
    std::vector<full_move> matched;
    std::vector<full_move> pawn_move_matched;
    std::optional<full_move> fm;
    std::optional<piece_t> promotion;
    dprint("parse_move(",move,")");
    constexpr static uint16_t FLAGS = SHOW_PAWN | SHOW_CAPTURE | SHOW_PROMOTION;
    if(std::holds_alternative<pgnparser_ast::physical_move>(move.data))
    {
        auto mv = std::get<pgnparser_ast::physical_move>(move.data);
        // for all physical moves avilable in current state
        for(vec4 p : gen_movable_pieces())
        {
            char piece = to_white(piece_name(get_piece(p, player)));
            bitboard_t bb = player ? m->gen_physical_moves<true>(p) : m->gen_physical_moves<false>(p);
            for(int pos : marked_pos(bb))
            {
                vec4 q(pos, p.tl());
                full_move fm(p,q);
                dprint("matching", fm);
                // test if this physical move matches any of them
                std::string full_notation = pretty_move<FLAGS>(fm);
                auto full = pgnparser(full_notation).parse_physical_move();
                assert(full.has_value());
                bool match = pgnparser::match_physical_move(mv, *full);
                if(match)
                {
                    dprint("matched");
                    matched.push_back(fm);
                    if(piece == PAWN_W)
                    {
                        pawn_move_matched.push_back(fm);
                    }
                }
            }
        }
        if(matched.size()==1)
        {
            // if there is exactly one match, we are good
            fm = matched[0];
        }
        else if(pawn_move_matched.size() == 1)
        {
            /* if there are more than one match, test if it this can be
             parsed as the unique pawn move
             */
            fm = pawn_move_matched[0];
        }
        if(fm.has_value())
        {
            promotion = mv.promote_to.transform([](char pt){
                return static_cast<piece_t>(pt);
            });
        }
    }
    else if(std::holds_alternative<pgnparser_ast::superphysical_move>(move.data))
    {
        // do the same for superphysical moves
        auto spm = std::get<pgnparser_ast::superphysical_move>(move.data);
        bool is_relative = std::holds_alternative<pgnparser_ast::relative_board>(spm.to_board);
        for(vec4 p : gen_movable_pieces())
        {
            char piece = to_white(piece_name(get_piece(p, player)));
            auto gen = player ? m->gen_superphysical_moves<true>(p) : m->gen_superphysical_moves<false>(p);
            for(const auto& [p0, bb] : gen)
            {
                for(int pos : marked_pos(bb))
                {
                    vec4 q(pos, p0);
                    full_move fm(p,q);
                    dprint("matching", fm);
                    // test if this physical move matches any of them
                    std::string full_notation;
                    if(is_relative)
                    {
                        full_notation = pretty_move<FLAGS | SHOW_RELATIVE>(fm);
                    }
                    else
                    {
                        full_notation = pretty_move<FLAGS>(fm);
                    }
                    auto full = pgnparser(full_notation).parse_superphysical_move();
                    assert(full.has_value());
                    bool match = pgnparser::match_superphysical_move(spm, *full);
                    if(match)
                    {
                        dprint("matched");
                        matched.push_back(fm);
                        if(piece == PAWN_W)
                        {
                            pawn_move_matched.push_back(fm);
                        }
                    }
                }
            }
        }
        if(matched.size()==1)
        {
            fm = matched[0];
        }
        else if(pawn_move_matched.size() == 1)
        {
            fm = pawn_move_matched[0];
        }
        if(fm.has_value())
        {
            promotion = spm.promote_to.transform([](char pt){
                return static_cast<piece_t>(pt);
            });
        }
    }
    return std::make_tuple(fm, promotion, matched);
}

state::parse_pgn_res state::parse_move(const std::string &move) const
{
    auto parsed_move = pgnparser(move).parse_move();
    if(!parsed_move.has_value())
    {
        return std::make_tuple(std::nullopt, std::nullopt, std::vector<full_move>{});
    }
    return parse_move(*parsed_move);
}

template bool state::apply_move<false>(full_move, piece_t);
template bool state::apply_move<true>(full_move, piece_t);
template bool state::submit<false>();
template bool state::submit<true>();

template generator<full_move> state::find_checks_impl<false>(std::vector<int>) const;
template generator<full_move> state::find_checks_impl<true>(std::vector<int>) const;
template std::vector<vec4> state::gen_movable_pieces_impl<false>(std::vector<int>) const;
template std::vector<vec4> state::gen_movable_pieces_impl<true>(std::vector<int>) const;

bool state::big_round_over() const {
    auto [l_min, l_max] = m->get_lines_range();
    for (int l = l_min; l <= l_max; l++) {
        auto te_tc = m->get_timeline_end(l);
        if (te_tc.second != player) {
            continue;
        }
		auto present_tc = m->get_present();
		// cannot submit
		if (present_tc.second == player) {
			return false;
		}
		// can submit but should choose pass move
		return has_passed;
		
    }
    return true;
}

std::tuple<std::vector<std::pair<int,std::vector<uint64_t>>>, std::vector<std::pair<int,int>>> state::get_boards_and_edges() const{
	return m->get_boards_and_edges();
}

std::vector<std::pair<int,std::vector<uint64_t>>> state::get_operable_boards_moves_and_match_status_const(match_status_t &ms) const {	

    std::vector<std::pair<int,std::vector<uint64_t>>> operable_boards
                = player ? m->get_operable_boards_moves<true>(!has_passed) : m->get_operable_boards_moves<false>(!has_passed);
	bool being_checked = phantom().find_checks(!player).first().has_value();
    /*for (auto& outer_pair : operable_boards) {
		std::cout << "===[" <<outer_pair.first << "]:" << std::endl;
        for (uint64_t& moveid : outer_pair.second) {
			auto [u0, v0, y0, x0, u1, v1, y1, x1, pto, flags] = DecodeMoveId(moveid);
            full_move fm(vec4(x0, y0, v_to_tc(v0).first, u_to_l(u0)), vec4(x1, y1, v_to_tc(v1).first, u_to_l(u1)));
			std::cout << fm.to_string() << ",";
		}
		std::cout << std::endl;
	}*/
    for (auto& outer_pair : operable_boards) {
        for (uint64_t& moveid : outer_pair.second) {
            auto [u0, v0, y0, x0, u1, v1, y1, x1, pto, flags] = DecodeMoveId(moveid);
            if (!(flags & 1)) {
                continue;
            }
            
            full_move fm(vec4(x0, y0, v_to_tc(v0).first, u_to_l(u0)), vec4(x1, y1, v_to_tc(v1).first, u_to_l(u1)));
            piece_t pt((piece_t)("QNRB"[pto]));
            std::optional<state> new_state_opt = can_apply(fm, pt);
            //std::cout << std::endl <<  "<" << fm.to_string() << ">";
            assert(new_state_opt && "failed to apply move here2!");
            if (new_state_opt->find_checks(!player).first().has_value()) {
				moveid &= ~(1ULL << 0);
                continue;
            }
			
			// added being_checked information
			if(being_checked) {
				moveid |= 1ULL << 4;
			}
			// added checking information
			if(get_move_info(fm, pt).checking_opponent) {
				moveid |= 1ULL << 3;
			}
        }
		outer_pair.second.erase(
		  remove_if(outer_pair.second.begin(), outer_pair.second.end(), [](uint64_t x){return !(x & 1ULL);}),
		  outer_pair.second.end());
    }
    /*for (auto& outer_pair : operable_boards) {
		std::cout << "~~~[" <<outer_pair.first << "]:" << std::endl;
        for (uint64_t& moveid : outer_pair.second) {
			auto [u0, v0, y0, x0, u1, v1, y1, x1, pto, flags] = DecodeMoveId(moveid);
            full_move fm(vec4(x0, y0, v_to_tc(v0).first, u_to_l(u0)), vec4(x1, y1, v_to_tc(v1).first, u_to_l(u1)));
			std::cout << fm.to_string() << ",";
		}
		std::cout << std::endl;
	}*/
	operable_boards.erase(
		std::remove_if(operable_boards.begin(), operable_boards.end(), [](const std::pair<int, std::vector<uint64_t>> &p) {return (p.second.empty());}),
		operable_boards.end());
	// get match status
	if (!operable_boards.empty()) {
		ms = match_status_t::PLAYING;
	} else {
		if (being_checked /*phantom().find_checks(!player).first().has_value()*/) {
			ms = player ? match_status_t::WHITE_WINS : match_status_t::BLACK_WINS;
		} else {
			ms = match_status_t::STALEMATE;
		}
	}
	
    return operable_boards;
}

std::vector<std::pair<int,std::vector<uint64_t>>> state::get_operable_boards_moves_and_match_status(match_status_t &ms) {	

    std::vector<std::pair<int,std::vector<uint64_t>>> operable_boards
                = player ? m->get_operable_boards_moves<true>(!has_passed) : m->get_operable_boards_moves<false>(!has_passed);
	//bool being_checked = phantom().find_checks(!player).first().has_value();
	auto phantom_new_lines = phantom_and_return_new_boards(player);
	bool being_checked = this->find_checks(!player).first().has_value();
	unapply_move_by_new_boards(phantom_new_lines, false);

    /*for (auto& outer_pair : operable_boards) {
		std::cout << "===[" <<outer_pair.first << "]:" << std::endl;
        for (uint64_t& moveid : outer_pair.second) {
			auto [u0, v0, y0, x0, u1, v1, y1, x1, pto, flags] = DecodeMoveId(moveid);
            full_move fm(vec4(x0, y0, v_to_tc(v0).first, u_to_l(u0)), vec4(x1, y1, v_to_tc(v1).first, u_to_l(u1)));
			std::cout << fm.to_string() << ",";
		}
		std::cout << std::endl;
	}*/
    for (auto& outer_pair : operable_boards) {
        for (uint64_t& moveid : outer_pair.second) {
            auto [u0, v0, y0, x0, u1, v1, y1, x1, pto, flags] = DecodeMoveId(moveid);
            if (!(flags & 1)) {
                continue;
            }
            
            full_move fm(vec4(x0, y0, v_to_tc(v0).first, u_to_l(u0)), vec4(x1, y1, v_to_tc(v1).first, u_to_l(u1)));
            piece_t pt((piece_t)("QNRB"[pto]));
            //std::optional<state> new_state_opt = can_apply(fm, pt);
			auto apply_new_lines = apply_move_and_return_new_boards(fm, pt);
            //std::cout << std::endl <<  "<" << fm.to_string() << ">";
            //assert(new_state_opt && "failed to apply move here2!");
			bool will_be_checked = this->find_checks(!player).first().has_value();
			if (will_be_checked) {
				moveid &= ~(1ULL << 0);
				unapply_move_by_new_boards(apply_new_lines, true);
                continue;
            }
			
			auto phantom_new_lines2 = phantom_and_return_new_boards(!player);
			//bool checking_opponent = this->find_checks(player).first().has_value();
			bool checking_opponent = player ? find_checks_impl<true>(apply_new_lines).first().has_value()
			                                : find_checks_impl<false>(apply_new_lines).first().has_value();
			unapply_move_by_new_boards(phantom_new_lines2, false);
			unapply_move_by_new_boards(apply_new_lines, true);
			// added checking information
			if(checking_opponent) {
				moveid |= 1ULL << 3;
			}
			
			// added being_checked information
			if(being_checked) {
				moveid |= 1ULL << 4;
			}
        }
		outer_pair.second.erase(
		  remove_if(outer_pair.second.begin(), outer_pair.second.end(), [](uint64_t x){return !(x & 1ULL);}),
		  outer_pair.second.end());
    }
    /*for (auto& outer_pair : operable_boards) {
		std::cout << "~~~[" <<outer_pair.first << "]:" << std::endl;
        for (uint64_t& moveid : outer_pair.second) {
			auto [u0, v0, y0, x0, u1, v1, y1, x1, pto, flags] = DecodeMoveId(moveid);
            full_move fm(vec4(x0, y0, v_to_tc(v0).first, u_to_l(u0)), vec4(x1, y1, v_to_tc(v1).first, u_to_l(u1)));
			std::cout << fm.to_string() << ",";
		}
		std::cout << std::endl;
	}*/
	operable_boards.erase(
		std::remove_if(operable_boards.begin(), operable_boards.end(), [](const std::pair<int, std::vector<uint64_t>> &p) {return (p.second.empty());}),
		operable_boards.end());
	// get match status
	if (!operable_boards.empty()) {
		ms = match_status_t::PLAYING;
	} else {
		if (being_checked /*phantom().find_checks(!player).first().has_value()*/) {
			ms = player ? match_status_t::WHITE_WINS : match_status_t::BLACK_WINS;
		} else {
			ms = match_status_t::STALEMATE;
		}
	}
	
	//match_status_t ms_const;
    //auto res_const = get_operable_boards_moves_and_match_status_const(ms_const);
    //assert(ms == ms_const && "match status mismatch");
    //assert(operable_boards == res_const && "operable boards count mismatch");
	
    return operable_boards;
}

