#include "open_spiel/games/five_d_chess/five_d_chess.h"

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
  return 1;
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
      num_moves_(0),
      current_big_round_(1) {
  is_first_real_selfplay_game_ = true;

  // update observation
  auto [t, c] = s->get_present();
  std::tie(all_boards_, boards_edges_) = s->get_boards_and_edges();
  operable_boards_ = s->get_operable_boards_moves_and_match_status(ms);

  current_player_ = c;
  history_moves_list_.clear();
}

FiveDChessState::FiveDChessState(const FiveDChessState& other)
    : State(other),
      s(other.s),
      ms(other.ms),
      current_player_(other.current_player_),
      num_moves_(other.num_moves_),
      current_big_round_(other.current_big_round_),
      all_boards_(other.all_boards_),
      operable_boards_(other.operable_boards_),
      boards_edges_(other.boards_edges_),
      history_moves_list_(other.history_moves_list_) {
  is_first_real_selfplay_game_ = false;
}

FiveDChessState::~FiveDChessState() = default;

Player FiveDChessState::CurrentPlayer() const {
  return current_player_;
}

// Generate legal actions by direct iteration
std::vector<Action> FiveDChessState::LegalActions() const {
  if (IsTerminal()) return {};

  std::vector<Action> result;
  int board_idx = 0;
  for (const auto& [board_id, move_list] : operable_boards_) {
    int move_idx = 0;
    for (MoveId moveid : move_list) {
      auto [u0, v0, y0, x0, u1, v1, y1, x1, promotion, flags] = DecodeMoveId(moveid);
      if ((flags & 1)) {
        result.push_back(static_cast<Action>(board_idx * kMaxMovesPerBoard + move_idx));
      }
      move_idx++;
    }
    board_idx++;
  }
  return result;
}

// Encode MoveId to Action by real-time matching
Action FiveDChessState::EncodeAction(MoveId moveid) const {
  int board_idx = 0;
  for (const auto& [board_id, move_list] : operable_boards_) {
    int move_idx = 0;
    for (MoveId mid : move_list) {
      auto [u0, v0, y0, x0, u1, v1, y1, x1, promotion, flags] = DecodeMoveId(mid);
      if (mid == moveid) {
        return static_cast<Action>(board_idx * kMaxMovesPerBoard + move_idx);
      }
      move_idx++;
    }
    board_idx++;
  }
  return kInvalidAction;
}

// Decode Action to MoveId by real-time lookup (no cache)
MoveId FiveDChessState::DecodeAction(Action action) const {
  int board_idx = action / kMaxMovesPerBoard;
  int move_idx = action % kMaxMovesPerBoard;

  if (board_idx < operable_boards_.size()) {
    if (move_idx < operable_boards_[board_idx].second.size()) {
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

  std::string SrcStr("(" + std::to_string(u0) + "," + std::to_string(v0) + "," + std::to_string(y0) + "," + std::to_string(x0) + ")");
  std::string DesStr("(" + std::to_string(u1) + "," + std::to_string(v1) + "," + std::to_string(y1) + "," + std::to_string(x1) + ")");
  std::string ArrowStr(u0 == u1 && v0 == v1 || 0 == u1 && 0 == v1 ? " -> " : " >> ");
  std::string PassStr(0 == u1 && 0 == v1 ? " [PASS]" : "");
  std::string BeingCheckedStr(flags & 16 ? " [BEING-CHECKED]" : "");
  std::string ChecksStr(flags & 8 ? " [CHECKING]" : "");
  std::string OptionalStr(flags & 4 ? " [OPTIONAL]" : "");
  std::string BranchStr(flags & 2 ? " [BRANCH]" : "");
  std::string PromotionStr(!(flags & 32) ? "" : prto == 0 ? " [QUEEN]" : prto == 1 ? " [KNIGHT]" :
                             prto == 2 ? " [ROOK]" : prto == 3 ? " [BISHOP]" : "");

  return absl::StrCat(
      SrcStr + ArrowStr + DesStr + PromotionStr +
      PassStr + BeingCheckedStr + ChecksStr + OptionalStr + BranchStr
  );
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
      " | Moves: " + std::to_string(num_moves_) + 
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

  int total_boards = all_boards_.size();
  int num_operable = operable_boards_.size();
  int num_edges = boards_edges_.size();

  std::unordered_map<BoardId, int> board_local_id;
  int local_idx = 0;
  for (const auto& [bid, _] : all_boards_) {
    board_local_id[bid] = local_idx++;
  }

  // ==================== Metadata Section ====================
  values[ptr++] = static_cast<float>(total_boards);
  values[ptr++] = static_cast<float>(num_operable);
  values[ptr++] = static_cast<float>(num_edges);
  values[ptr++] = static_cast<float>(current_player_); // Current player (0=White, 1=Black)

  // ==================== Operable Boards Section ====================
  int op_fill = 0;
  std::unordered_map<BoardId, int> board_prob_id;
  for (const auto& [board_id, move_list] : operable_boards_) {
    if (op_fill >= kMaxOperableBoards) break;
    
    // Convert global board ID to local tensor index
    values[ptr++] = static_cast<float>(board_local_id.at(board_id));
    
    // Board selection prior probability 
	bool is_optional_line = std::any_of(move_list.begin(), move_list.end(), [](uint64_t x) {
        return !!(x & 4);
    });
    values[ptr] = is_optional_line ? 0.01f : 1.0f;
	board_prob_id[board_id] = ptr++;
    op_fill++;
  }
  // Fill remaining operable board slots with invalid markers
  for (; op_fill < kMaxOperableBoards; ++op_fill) {
    values[ptr++] = -1.0f;  // Invalid board index
    values[ptr++] = 0.0f;   // Zero prior for invalid boards
  }

  // ==================== Legal Move Mask Section ====================
  auto legal_actions = LegalActions();
  for (Action a : legal_actions) {
    if (a >= 0 && a < kNumDistinctActions) {
      MoveId moveid = DecodeAction(a);
      auto [u0, v0, y0, x0, u1, v1, y1, x1, promotion, flags] = DecodeMoveId(moveid);
      if ((u1 == 0 && v1 == 0) || (flags & 8) || (flags & 16)) { //Pass or checking or being-checked
        values[ptr + a] = 1.0f;
		values[board_prob_id.at(EncodeBoardId(u0, v0))] = 1.0f;  // fix board prob
      } else {
        values[ptr + a] = 0.5f;
        if (flags & 4) values[ptr + a] *= 0.07f; // optional
        if (flags & 2) values[ptr + a] *= 0.00f; // branch
      }
    }
  }
  ptr += kNumDistinctActions;

  // ==================== Board Data Section ====================
  int board_base = ptr;
  local_idx = 0;
  for (const auto& [bid, bitboards] : all_boards_) {
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
    local_idx++;
  }
  ptr += total_boards * (2 + kNumPieceChannels * kBoardSize * kBoardSize);

  // ==================== Graph Edges Section ====================
  int edge_fill = 0;
  for (const auto& [src_bid, dst_bid] : boards_edges_) {
    if (edge_fill >= kMaxRuntimeEdges) break;
    values[ptr++] = static_cast<float>(board_local_id.at(src_bid));
    values[ptr++] = static_cast<float>(board_local_id.at(dst_bid));
    edge_fill++;
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
  history_moves_list_.push_back(std::to_string(current_big_round_) + (current_player_ ? "b" : "w") + "." + fm.to_string());
  if (is_first_real_selfplay_game_)
    std::cout << "{" << std::this_thread::get_id() << "." << num_moves_ << ":" << flags << pto << "}"
              << history_moves_list_.back() << std::endl;
  bool success = s->apply_move<true>(fm, pto);
  SPIEL_CHECK_TRUE(success);
  //std::tie(all_boards_, boards_edges_) = s->apply_move_and_return_new_boards(fm, pto);

  if (s->big_round_over()) {
    bool submit_success = s->submit();
    SPIEL_CHECK_TRUE(submit_success);
  }

  auto [t, c] = s->get_present();
  std::tie(all_boards_, boards_edges_) = s->get_boards_and_edges();
  operable_boards_ = s->get_operable_boards_moves_and_match_status(ms);
  if (ms != match_status_t::PLAYING) {
	if (is_first_real_selfplay_game_) std::cout << "check ms=" << ms << std::endl;
	return;
  }

  // check out of range
  auto it = std::max_element(operable_boards_.cbegin(), operable_boards_.cend(),
      [](const auto& a, const auto& b) { return a.second.size() < b.second.size(); });
  if (it->second.size() > kMaxMovesPerBoard ||
      all_boards_.size() > kMaxRuntimeBoards ||
      operable_boards_.size() > kMaxOperableBoards ||
      boards_edges_.size() > kMaxRuntimeEdges) {
    ms = match_status_t::STALEMATE;
	if (is_first_real_selfplay_game_) std::cout << "max_board_mvs_cnt=" << it->second.size()
			<< ",all_boards=" << all_boards_.size()
			<< ",operable_boards=" << operable_boards_.size()
			<< ",boards_edges=" << boards_edges_.size()
			<< ",force ms=" << ms << std::endl;
    return;
  }

  if (current_player_ != c) {
    current_player_ = c;
    if (!current_player_) {
      current_big_round_++;
    }
  }
  num_moves_++;
  if (num_moves_ >= kMaxGameLength) {
	ms = match_status_t::STALEMATE;
	if (is_first_real_selfplay_game_) std::cout << "num_moves_=" << num_moves_
		<< ",force2 ms=" << ms << std::endl;
  }
}

}  // namespace five_d_chess
}  // namespace open_spiel