#include "five_d_chess.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/numbers.h"
#include "open_spiel/observer.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace five_d_chess {
namespace {

const GameType kGameType{
    /*short_name=*/"five_d_chess",
    /*long_name=*/"5D Chess with Multiverse Time Travel",
    GameType::Dynamics::kSequential,
    GameType::ChanceMode::kDeterministic,
    GameType::Information::kPerfectInformation,
    GameType::Utility::kZeroSum,
    GameType::RewardModel::kTerminal,
    /*max_num_players=*/2,
    /*min_num_players=*/2,
    /*provides_information_state_string=*/true,
    /*provides_information_state_tensor=*/false,
    /*provides_observation_string=*/true,
    /*provides_observation_tensor=*/true,
    /*parameter_specification=*/{}
};

std::shared_ptr<const Game> Factory(const GameParameters& params) {
  return std::shared_ptr<const Game>(new FiveDChessGame(params));
}

REGISTER_SPIEL_GAME(kGameType, Factory);
RegisterSingleTensorObserver single_tensor(kGameType.short_name);

}  // namespace

// ==================== Game Class Implementation ====================
FiveDChessGame::FiveDChessGame(const GameParameters& params)
    : Game(kGameType, params) {}

int FiveDChessGame::NumDistinctActions() const {
  return kNumDistinctActions;
}

std::unique_ptr<State> FiveDChessGame::NewInitialState() const {
  return std::unique_ptr<State>(new FiveDChessState(shared_from_this()));
}

int FiveDChessGame::MaxChanceOutcomes() const {
  return 0;
}

int FiveDChessGame::NumPlayers() const {
  return 2;
}

double FiveDChessGame::MinUtility() const {
  return -1.0;
}

double FiveDChessGame::MaxUtility() const {
  return 1.0;
}

int FiveDChessGame::MaxGameLength() const {
  return kMaxGameLength;
}

std::vector<int> FiveDChessGame::ObservationTensorShape() const {
  return {kObservationTensorSize};
}

// ==================== FiveDChessState Class Implementation ====================
FiveDChessState::FiveDChessState(std::shared_ptr<const Game> game)
    : State(game),
      s(*pgnparser(init_str).parse_game()),
      current_big_round_(1) {
  is_first_real_selfplay_game_ = true;

  // update observation
  auto [t, c] = s->get_present();
  std::tie(all_boards_, boards_edges_) = s->get_boards_and_edges();
  operable_boards_ = s->get_operable_boards_moves_and_match_status(ms);

  current_player_ = c;
  undo_stack_.clear();
}

FiveDChessState::FiveDChessState(const FiveDChessState& other)
    : State(other),
      s(other.s),
      ms(other.ms),
      current_player_(other.current_player_),
      current_big_round_(other.current_big_round_),
      all_boards_(other.all_boards_),
      operable_boards_(other.operable_boards_),
      boards_edges_(other.boards_edges_),
      undo_stack_(other.undo_stack_),
      is_first_real_selfplay_game_(false) {
}
	  
FiveDChessState::~FiveDChessState() = default;

Player FiveDChessState::CurrentPlayer() const {
  return current_player_;
}

// Generate legal actions by direct iteration
std::vector<Action> FiveDChessState::LegalActions() const {
  if (IsTerminal()) return {};

  std::vector<Action> result;
  for (int board_idx = 0; board_idx < std::min(kMaxOperableBoards, (int)operable_boards_.size()); board_idx++) {
	  const auto& [board_id, move_list] = operable_boards_[board_idx];
	  for (int move_idx = 0; move_idx < std::min(kMaxMovesPerBoard, (int)move_list.size()); move_idx++) {
		  result.push_back(static_cast<Action>(board_idx * kMaxMovesPerBoard + move_idx));
	  }
  }
  return result;
}

// Encode MoveId to Action by real-time matching
Action FiveDChessState::EncodeAction(MoveId moveid) const {
	
  for (int board_idx = 0; board_idx < std::min(kMaxOperableBoards, (int)operable_boards_.size()); board_idx++) {
	  const auto& [board_id, move_list] = operable_boards_[board_idx];
	  for (int move_idx = 0; move_idx < std::min(kMaxMovesPerBoard, (int)move_list.size()); move_idx++) {
		  if (move_list[move_idx] == moveid) {
			return static_cast<Action>(board_idx * kMaxMovesPerBoard + move_idx);
		  }
	  }
  }
  return kInvalidAction;
}

// Decode Action to MoveId by real-time lookup (no cache)
MoveId FiveDChessState::DecodeAction(Action action) const {
  int board_idx = action / kMaxMovesPerBoard;
  int move_idx = action % kMaxMovesPerBoard;

  if (board_idx < std::min(kMaxOperableBoards, (int)operable_boards_.size())) {
    if (move_idx < std::min(kMaxMovesPerBoard, (int)operable_boards_[board_idx].second.size())) {
      return operable_boards_[board_idx].second[move_idx];
    }
  }

  std::cerr <<"[" << std::this_thread::get_id() << "] DecodeAction Invalid Action: " << action << ", bid" << board_idx << ", mid" << move_idx << std::endl;
  std::cerr << "operable boards size: " << operable_boards_.size() << std::endl;
  std::cerr << "operable  moves size: " << (board_idx < operable_boards_.size() ? operable_boards_[board_idx].second.size() : 0) << std::endl;
  return kInvalidMoveId;
}

std::string FiveDChessState::ActionToString(Player player, Action action) const {
  MoveId moveid = DecodeAction(action);
  auto [u0, v0, y0, x0, u1, v1, y1, x1, prto, flags] = DecodeMoveId(moveid);
  full_move fm(vec4(x0, y0, v_to_tc(v0).first, u_to_l(u0)), vec4(x1, y1, v_to_tc(v1).first, u_to_l(u1)));
  std::string PlayerStr(player ? "b." : "w.");

  std::string BeingCheckedStr(flags & 16 ? "-" : "");
  std::string ChecksStr(flags & 8 ? "+" : "");
  std::string OptionalStr(flags & 4 ? "~" : "");
  std::string BranchStr(flags & 2 ? "^" : "");
  std::string PromotionStr(!(flags & 32) ? "" : prto == 0 ? "=Q" : prto == 1 ? "=N" :
                             prto == 2 ? "=R" : prto == 3 ? "=B" : "");
  
  std::string TotalStr(PlayerStr + fm.to_string() + PromotionStr + BeingCheckedStr + ChecksStr + OptionalStr + BranchStr);
  if (TotalStr.length() < 17) TotalStr.append(17 - TotalStr.length(), ' ');
  return absl::StrCat(TotalStr);
}

Action FiveDChessState::StringToAction(Player player, const std::string& str) const {
  Action action;
  if (absl::SimpleAtoi(str, &action)) {
    return action;
  }
  return kInvalidAction;
}

std::unique_ptr<State> FiveDChessState::Clone() const {
  auto st = std::unique_ptr<FiveDChessState>(new FiveDChessState(*this));
  st->is_first_real_selfplay_game_ = false;
  return st;
}

bool FiveDChessState::IsTerminal() const {
  return ms != match_status_t::PLAYING;
}

std::vector<double> FiveDChessState::Returns() const {
  if (ms == match_status_t::PLAYING) {
    return {0.0, 0.0};
  }
  if (ms == match_status_t::WHITE_WINS) {
    return {1.0, -1.0}; // White wins, Black loses
  } else if (ms == match_status_t::BLACK_WINS) {
    return {-1.0, 1.0}; // Black wins, White loses
  } else {
    return {0.0, 0.0}; // Draw/stalemate
  }
}

std::string FiveDChessState::ToString() const {
  return absl::StrCat(std::string(
	  "5D Chess | Round: " + std::to_string(current_big_round_) +
      " | Player: " + (current_player_ ? "Black" : "White") +
      " | Moves: " + std::to_string(move_number_) + 
      " | Terminal: " + (IsTerminal() ? "YES " : "NO ") + s->to_string()
  ));
}

std::string FiveDChessState::InformationStateString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, NumPlayers());
  return HistoryString();
}

std::string FiveDChessState::ObservationString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, NumPlayers());
  return ToString();
}

void FiveDChessState::ObservationTensor(Player player, absl::Span<float> values) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, NumPlayers());

  std::fill(values.begin(), values.end(), 0.0f);
  int ptr = 0;

  int total_boards = std::min(kMaxRuntimeBoards, (int)all_boards_.size());
  int num_operable = std::min(kMaxOperableBoards, (int)operable_boards_.size());
  int num_edges = std::min(kMaxRuntimeEdges, (int)boards_edges_.size());

  std::unordered_map<BoardId, int> board_local_id;
  for (int local_idx = 0; local_idx < all_boards_.size(); local_idx++) {
    const auto& [bid, _] = all_boards_[local_idx];
    board_local_id[bid] = local_idx;
  }

  // ==================== Metadata Section ====================
  values[ptr++] = static_cast<float>(total_boards);
  values[ptr++] = static_cast<float>(num_operable);
  values[ptr++] = static_cast<float>(num_edges);
  values[ptr++] = static_cast<float>(current_player_); // Current player (0=White, 1=Black)

  // ==================== Operable Boards Section ====================
  for (int op_fill = 0; op_fill < kMaxOperableBoards; op_fill++) {
    if (op_fill < operable_boards_.size()) {
      // Convert global board ID to local tensor index
	  const auto& [board_id, move_list] = operable_boards_[op_fill];
      values[ptr++] = static_cast<float>(board_local_id.at(board_id));
      values[ptr++] = 1.0f;
    }
  // Fill remaining operable board slots with invalid markers
    else {
      values[ptr++] = -1.0f;  // Invalid board index
      values[ptr++] = 0.0f;   // Zero prior for invalid boards
    }
  }

  // ==================== Legal Move Mask Section ====================
  for (int board_idx = 0; board_idx < num_operable; board_idx++) {
    const auto& [board_id, move_list] = operable_boards_[board_idx];
    bool is_optional_line = false, has_check_move = false;
	for (int move_idx = 0; move_idx < std::min(kMaxMovesPerBoard, (int)move_list.size()); move_idx++) {
        Action a = board_idx * kMaxMovesPerBoard + move_idx;
        MoveId moveid = move_list[move_idx];
        auto [u0, v0, y0, x0, u1, v1, y1, x1, promotion, flags] = DecodeMoveId(moveid);
		
        if ((u1 == 0 && v1 == 0) || (flags & 8) || (flags & 16)) {
            values[ptr + a] = 1.0f;
            // need fix board prob
			has_check_move = true;
        } else {
            values[ptr + a] = 0.5f;
            if (flags & 4) {
				values[ptr + a] *= 0.07f; // optional
				is_optional_line = true;
			}
            if (flags & 2) {
				values[ptr + a] *= 0.01f; // branch
			}
        }
    }
	// overwrite board prob
	if (has_check_move) {
		values[4 + board_idx * 2 + 1] = 1.0f;
	} else if (is_optional_line) {
		values[4 + board_idx * 2 + 1] = 0.01f;
	} else {
		values[4 + board_idx * 2 + 1] = 0.5f;
	}
  }
  ptr += kNumDistinctActions;

  // ==================== Board Data Section ====================
  int board_base = ptr;
  for (int local_idx = 0; local_idx < total_boards; local_idx++) {
    const auto& [bid, bitboards] = all_boards_[local_idx];
    int base = board_base + local_idx * (2 + kNumPieceChannels * kBoardSize * kBoardSize);
    
    // Extract u and v coordinates from board ID (high 8 bits = u, low 8 bits = v)
    auto [u, v] = DecodeBoardId(bid);
    values[base + 0] = static_cast<float>(u);
    values[base + 1] = static_cast<float>(v);

    // Write bitboard data (offset by 2 floats for coordinates)
    int bb_base = base + 2;
    for (int c = 0; c < kNumPieceChannels && c < bitboards.size(); c++) {
      uint64_t bb = bitboards[c];
      int ch_base = bb_base + c * kBoardSize * kBoardSize;
      for (int y = 0; y < kBoardSize; y++) {
        for (int x = 0; x < kBoardSize; x++) {
          int sq = y * kBoardSize + x;
          if (bb & (1ULL << sq)) {
            values[ch_base + sq] = 1.0f;
          }
        }
      }
    }

  }
  ptr += total_boards * (2 + kNumPieceChannels * kBoardSize * kBoardSize);

  // ==================== Graph Edges Section ====================
  for (int edge_fill = 0; edge_fill < num_edges; edge_fill++) {
    const auto& [src_bid, dst_bid] = boards_edges_[edge_fill];
    values[ptr++] = static_cast<float>(board_local_id.at(src_bid));
    values[ptr++] = static_cast<float>(board_local_id.at(dst_bid));

  }
  return;
}

void FiveDChessState::DoApplyAction(Action action) {
  SPIEL_CHECK_TRUE(!IsTerminal());
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, game_->NumDistinctActions());

  MoveId core_moveid = DecodeAction(action);
  SPIEL_CHECK_NE(core_moveid, kInvalidMoveId);

  auto [u0, v0, y0, x0, u1, v1, y1, x1, promotion, flags] = DecodeMoveId(core_moveid);
  full_move fm(vec4(x0, y0, v_to_tc(v0).first, u_to_l(u0)), vec4(x1, y1, v_to_tc(v1).first, u_to_l(u1)));
  piece_t pto((piece_t)("QNRB"[promotion]));
  
  // ========== Backup 1 for undo state ==========
  UndoEntry entry;
  std::string moveStr(std::to_string(current_big_round_) + (current_player_ ? "b" : "w") + "." + fm.to_string());
  if (is_first_real_selfplay_game_)
    std::cout << "{" << std::this_thread::get_id() << "." << move_number_ << ":" << flags << pto << "}"
              << moveStr << std::endl;

  // ========== Backup 2 for unapply move ==========
  entry.apply_new_lines = s->apply_move_and_return_new_boards(fm, pto);

  // ========== Backup 3 for unsunmit ==========
  entry.did_submit = s->big_round_over();
  if (entry.did_submit) {
    entry.submit_params = s->submit_and_return_params();
    SPIEL_CHECK_TRUE(std::get<0>(entry.submit_params) >= 0);
  }

  auto [t, c] = s->get_present();
  std::tie(all_boards_, boards_edges_) = s->get_boards_and_edges();
  operable_boards_ = s->get_operable_boards_moves_and_match_status(ms);

  if (ms != match_status_t::PLAYING) {
    if (is_first_real_selfplay_game_) std::cout << "check ms=" << ms << std::endl;
    // for undo
    undo_stack_.push_back(std::move(entry));
    return;
  }

  // check out of range
  auto it = std::max_element(operable_boards_.cbegin(), operable_boards_.cend(),
      [](const auto& a, const auto& b) { return a.second.size() < b.second.size(); });
  if (it->second.size() > kMaxMovesPerBoard ||
      all_boards_.size() > kMaxRuntimeBoards ||
      operable_boards_.size() > kMaxOperableBoards ||
      boards_edges_.size() > kMaxRuntimeEdges ||
      move_number_ + 1 >= kMaxGameLength) {
    int wc = 0, bc = 0;
    auto [l_min, l_max] = s->get_lines_range();
    auto [active_min, active_max] = s->get_active_range();
    if (l_min < active_min) {
      ms = match_status_t::WHITE_WINS;
    } else if (active_max < l_max) {
      ms = match_status_t::BLACK_WINS;
    } else {
      for(int l = l_min; l <= l_max; l++) {
        auto [tl, tc] = s->get_timeline_end(l);
        if (tc) bc++;
        else wc++;
      }
      if (wc > bc) ms = match_status_t::WHITE_WINS;
      else if (wc < bc) ms = match_status_t::BLACK_WINS;
      else ms = match_status_t::STALEMATE;
    }
    
    if (is_first_real_selfplay_game_) 
      std::cout << "max_board_mvs_cnt=" << it->second.size()
        << ",all_boards=" << all_boards_.size()
        << ",operable_boards=" << operable_boards_.size()
        << ",boards_edges=" << boards_edges_.size()
        << ",move_number_=" << move_number_ 
        << ",l_min=" << l_min
        << ",l_max=" << l_max 
        << ",active_min=" << active_min
        << ",active_max=" << active_max 
        << ",wc=" << wc
        << ",bc=" << bc 
        << ",force ms=" << ms
        << std::endl;
	// for undo
    undo_stack_.push_back(std::move(entry));
    return;
  }

  if (current_player_ != c) {
    current_player_ = c;
    if (!current_player_) {
      current_big_round_++;
    }
  }

  // for undo
  undo_stack_.push_back(std::move(entry));
}

void FiveDChessState::UndoAction(Player player, Action action) {
  SPIEL_CHECK_TRUE(!undo_stack_.empty());
  UndoEntry entry = std::move(undo_stack_.back());
  undo_stack_.pop_back();

  // 1. reroll history and num (openspiel need it)
  history_.pop_back();
  --move_number_;

  // 2. revovery uplayer snapshot
  // 3. undo summit and apply
  if (entry.did_submit) {
    s->unsubmit_by_params(entry.submit_params);
  }
  s->unapply_move_by_new_boards(entry.apply_new_lines, /*maybe_passed=*/true);
  
  // rewrite
  auto [t, c] = s->get_present();
  if (current_player_ != c) {
	  if (!current_player_) {
		  current_big_round_--;
	  }
	  current_player_ = c;
  }
#ifndef NDEBUG
  SPIEL_CHECK_EQ(player, current_player_);
#endif

  std::tie(all_boards_, boards_edges_) = s->get_boards_and_edges();
  operable_boards_ = s->get_operable_boards_moves_and_match_status(ms);

#ifndef NDEBUG
  //match_status_t check_ms;
  //auto [check_t, check_c] = s->get_present();
  //auto [check_boards, check_edges] = s->get_boards_and_edges();
  //auto check_operable = s->get_operable_boards_moves_and_match_status(check_ms);
  //SPIEL_CHECK_EQ(static_cast<bool>(check_c), static_cast<bool>(current_player_));
  //SPIEL_CHECK_EQ(check_ms, ms);
  //SPIEL_CHECK_EQ(check_boards.size(), all_boards_.size());
  //SPIEL_CHECK_EQ(check_operable.size(), operable_boards_.size());
#endif
}

}  // namespace five_d_chess
}  // namespace open_spiel