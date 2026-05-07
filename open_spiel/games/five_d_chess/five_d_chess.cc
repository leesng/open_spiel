#include "open_spiel/games/five_d_chess/five_d_chess.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/numbers.h"
#include "open_spiel/observer.h"
#include "open_spiel/spiel_utils.h"

#include "state.h"
#include "hypercuboid.h"
#include "pgnparser.h"

#include <thread>

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

// ==================== Game 类实现 ====================
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

// ==================== FiveDChessState 类实现 ====================
FiveDChessState::FiveDChessState(std::shared_ptr<const Game> game)
    : State(game),
      s(*pgnparser(init_str).parse_game()),
      num_moves_(0),
      current_big_round_(1) {
  operated_boards_.clear();
  is_first_real_selfplay_game_ = true;
  // get match status
  ms = s->get_match_status();
  // update observation
  auto [t, c] = s->get_present();
  std::tie(all_boards_, operable_boards_, boards_edges_) = 
    c ? s->get_observation_information<true>() : s->get_observation_information<false>();
  current_player_ = c;
}

FiveDChessState::FiveDChessState(const FiveDChessState& other)
    : State(other),
      s(other.s),
      ms(other.ms),
      current_player_(other.current_player_),
      num_moves_(other.num_moves_),
      current_big_round_(other.current_big_round_),
      operated_boards_(other.operated_boards_),
      all_boards_(other.all_boards_),
      operable_boards_(other.operable_boards_),
      boards_edges_(other.boards_edges_) {
  is_first_real_selfplay_game_ = false;
}

FiveDChessState::~FiveDChessState() = default;

Player FiveDChessState::CurrentPlayer() const {
  return current_player_;
}

// ==================== 核心：合法动作（直接遍历生成下标） ====================
std::vector<Action> FiveDChessState::LegalActions() const {
  if (IsTerminal()) return {};

  std::vector<Action> result;
  BoardId earliest_non_branch_boardid = std::numeric_limits<BoardId>::max();

  int board_idx = 0;
  for (const auto& [board_id, move_list] : operable_boards_) {
    if (operated_boards_.count(board_id)) continue;

    int move_idx = 0;
    for (MoveId moveid : move_list) {
      auto [u0, v0, y0, x0, u1, v1, y1, x1, promotion, flags] = DecodeMoveId(moveid);
      if (!(flags & 1)) continue;

      bool will_use_it = false;
      if ((flags & 2) || (u1 == 0 && v1 == 0)) {
        will_use_it = true; // pass and branch
      } else {
        if (earliest_non_branch_boardid == std::numeric_limits<BoardId>::max()) {
          earliest_non_branch_boardid = board_id;
        }
        if (earliest_non_branch_boardid == board_id) {
          will_use_it = true;
        }
      }

      if (will_use_it) {
        // 直接返回全局索引
        result.push_back(static_cast<Action>(board_idx * kMaxMovesPerBoard + move_idx));
      }

      move_idx++;
    }

    board_idx++;
  }

  return result;
}

// ==================== Action 编码：实时遍历匹配 ====================
Action FiveDChessState::EncodeAction(MoveId moveid) const {
  int board_idx = 0;
  for (const auto& [board_id, move_list] : operable_boards_) {
    if (operated_boards_.count(board_id)) continue;

    int move_idx = 0;
    for (MoveId mid : move_list) {
      auto [u0, v0, y0, x0, u1, v1, y1, x1, promotion, flags] = DecodeMoveId(mid);
      if (!(flags & 1)) continue;

      if (mid == moveid) {
        // 直接按固定偏移计算全局 Action 编号
        return static_cast<Action>(board_idx * kMaxMovesPerBoard + move_idx);
      }

      move_idx++;
    }

    board_idx++;
  }
  return kInvalidAction;
}
// ==================== Action 解码：实时遍历查找（无缓存！） ====================
MoveId FiveDChessState::DecodeAction(Action action) const {
  // 直接从全局 Action 编号反推出棋盘索引和走法索引
  int board_idx = action / kMaxMovesPerBoard;
  int move_idx = action % kMaxMovesPerBoard;

  // 找到对应的棋盘
  int current_board_idx = 0;
  for (const auto& [board_id, move_list] : operable_boards_) {
    if (operated_boards_.count(board_id)) continue;

    if (current_board_idx == board_idx) {
      // 找到对应的走法
      int current_move_idx = 0;
      for (MoveId mid : move_list) {
        auto [u0, v0, y0, x0, u1, v1, y1, x1, promotion, flags] = DecodeMoveId(mid);
        if (!(flags & 1)) continue;

        if (current_move_idx == move_idx) {
          return mid;
        }

        current_move_idx++;
      }
      break;
    }

    current_board_idx++;
  }

  std::cerr << "DecodeAction Invalid Action: " << action << std::endl;
  return kInvalidMoveId;
}
std::string FiveDChessState::ActionToString(Player player, Action action) const {
  MoveId moveid = DecodeAction(action);
  auto [u0, v0, y0, x0, u1, v1, y1, x1, prto, flags] = DecodeMoveId(moveid);
  
  std::string SrcStr("("+std::to_string(u0)+","+std::to_string(v0)+","+std::to_string(y0)+","+std::to_string(x0)+")");
  std::string DesStr("("+std::to_string(u1)+","+std::to_string(v1)+","+std::to_string(y1)+","+std::to_string(x1)+")");
  std::string ArrowStr(u0 == u1 && v0 == v1 || 0 == u1 && 0 == v1 ? " -> " : " >> ");
  std::string PassStr(0 == u1 && 0 == v1 ? " [PASS]" : "");
  std::string ChecksStr(flags & 8 ? " [CHECKING]" : "");
  std::string OptionalStr(flags & 4 ? " [OPTIONAL]" : "");
  std::string BranchStr(flags & 2 ? " [BRANCH]" : "");
  std::string PromotionStr(prto == 0 ? " = Q" : prto == 1 ? " = N" :
                           prto == 2 ? " = R" : prto == 3 ? " = B" : "");

  return absl::StrCat(
      SrcStr+ArrowStr+DesStr+PromotionStr+ 
	  PassStr+ChecksStr+OptionalStr+BranchStr
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
    return {1.0, -1.0};  // 白胜
  } else if (ms == match_status_t::BLACK_WINS) {
    return {-1.0, 1.0};  // 黑胜
  } else {
    return {0.0, 0.0};   // 和棋
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

// ==================== 观察张量 ====================
void FiveDChessState::ObservationTensor(Player player, absl::Span<float> values) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, NumPlayers());
  
  std::fill(values.begin(), values.end(), 0.0f);
  int ptr = 0;

  int total_boards = all_boards_.size();
  int num_operable = operable_boards_.size();
  int num_edges = boards_edges_.size();

  // 构建 BoardId → 连续局部ID (0,1,2...)
  std::unordered_map<BoardId, int> board_local_id;
  int local_idx = 0;
  for (const auto& [bid, _] : all_boards_) {
    board_local_id[bid] = local_idx++;
  }

  // 1. 元数据
  values[ptr++] = static_cast<float>(total_boards);
  values[ptr++] = static_cast<float>(num_operable);
  values[ptr++] = static_cast<float>(num_edges);
  values[ptr++] = 0.0f;

  // 2. 边索引（局部ID）
  int edge_fill = 0;
  for (const auto& [src_bid, dst_bid] : boards_edges_) {
    if (edge_fill >= kMaxRuntimeEdges) break;
    values[ptr++] = static_cast<float>(board_local_id.at(src_bid));
    values[ptr++] = static_cast<float>(board_local_id.at(dst_bid));
    edge_fill++;
  }
  for (; edge_fill < kMaxRuntimeEdges; ++edge_fill) {
    values[ptr++] = -1.0f;
    values[ptr++] = -1.0f;
  }

  // 3. 可操作棋盘索引（局部ID）
  int op_fill = 0;
  for (const auto& [bid, _] : operable_boards_) {
    if (op_fill >= kMaxOperableBoards) break;
    values[ptr++] = static_cast<float>(board_local_id.at(bid));
    op_fill++;
  }
  for (; op_fill < kMaxOperableBoards; ++op_fill) {
    values[ptr++] = -1.0f;
  }

  // 4. 【核心：按固定位置填合法动作掩码】
  std::fill(values.begin() + ptr, values.begin() + ptr + kNumDistinctActions, 0.0f);
  
  // 先找到最早的非分支棋盘
  BoardId earliest_non_branch_boardid = std::numeric_limits<BoardId>::max();
  // 按棋盘索引，在固定位置填值
  int board_idx = 0;  int b0 = 0, b1 = 0;
  for (const auto& [board_id, move_list] : operable_boards_) {
    if (operated_boards_.count(board_id)) continue;

    // 该棋盘在向量中的固定起始偏移
    int board_base = ptr + board_idx * kMaxMovesPerBoard;
    int move_idx = 0;

    for (MoveId moveid : move_list) {
      auto [u0, v0, y0, x0, u1, v1, y1, x1, promotion, flags] = DecodeMoveId(moveid);
      if (!(flags & 1)) continue;

      // 只在该棋盘的固定位置范围内填值
	  SPIEL_CHECK_LT(move_idx, kMaxMovesPerBoard);
	  bool will_use_it = false;
      if ((flags & 2) || (u1 == 0 && v1 == 0)) {
        will_use_it = true; // pass and branch
		b1++;
      } else {
        // 非分支走法：只有最早棋盘填高优先级，其他棋盘填0
		if (earliest_non_branch_boardid == std::numeric_limits<BoardId>::max()) {
			earliest_non_branch_boardid = board_id;
		}
		 // only get the 1'ts board non-branch moves
		if (earliest_non_branch_boardid == board_id) {
			will_use_it = true;
			b0++;
		}
	  }
	  if (will_use_it) {
		
		if ((u1 == 0 && v1 == 0) || (moveid & 8)) { //Pass and checking
			values[board_base + move_idx] = 1.0f;
		} else {
			values[board_base + move_idx] = 0.95f;
			if (moveid & 4) values[board_base + move_idx] *= 0.01f; // optional
			if (moveid & 2) values[board_base + move_idx] *= 0.01f; // branch
		}
	  }

      move_idx++;
    }

    board_idx++;
  }
  if (is_first_real_selfplay_game_) std::cout << "{" << b0 << "," << b1 << "}";
  ptr += kNumDistinctActions;

  // 对齐到固定头结束
  while (ptr < kFixedHeaderSize)
    values[ptr++] = 0.0f;

  // 5. 棋盘数据（按 all_boards_ 顺序连续写入）
  int board_base = ptr;
  local_idx = 0;
  for (const auto& [bid, bitboards] : all_boards_) {
    int base = board_base + local_idx * kNumPieceChannels * kBoardSize * kBoardSize;
    for (int c = 0; c < kNumPieceChannels && c < bitboards.size(); c++) {
      uint64_t bb = bitboards[c];
      int ch_base = base + c * kBoardSize * kBoardSize;
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
}

// ==================== 游戏逻辑辅助函数 ====================
int FiveDChessState::IsBigRoundOver() const {
  int total_operable = 0;
  int already_operated = 0;

  for (const auto& [board_id, move_list] : operable_boards_) {
    total_operable++;
    if (operated_boards_.count(board_id)) {
      already_operated++;
    }
  }

  // 返回剩余可操作棋盘数
  return total_operable - already_operated;
}

void FiveDChessState::StartNewBigRound() {
  operated_boards_.clear();

}

void FiveDChessState::MarkBoardAsOperated(BoardId board_id) {
  operated_boards_.insert(board_id);
}

// ==================== DoApplyAction ====================
void FiveDChessState::DoApplyAction(Action action) {
  SPIEL_CHECK_TRUE(!IsTerminal());
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, game_->NumDistinctActions());

  // 解码成核心moveid
  MoveId core_moveid = DecodeAction(action);
  SPIEL_CHECK_NE(core_moveid, kInvalidMoveId);

  // 解码走法参数
  auto [u0, v0, y0, x0, u1, v1, y1, x1, promotion, flags] = DecodeMoveId(core_moveid);

  // 标记棋盘为已操作
  MarkBoardAsOperated(EncodeBoardId(u0, v0));
  MarkBoardAsOperated(EncodeBoardId(u1, v1));

  // 执行走法
  full_move fm(vec4(x0, y0, v_to_tc(v0).first, u_to_l(u0)), vec4(x1, y1, v_to_tc(v1).first, u_to_l(u1)));
  piece_t pto((piece_t)("QNRB"[promotion]));
  if (is_first_real_selfplay_game_)
	std::cout << "{" << std::this_thread::get_id() << "." << num_moves_ <<"." << flags << "}"
              << current_big_round_ << (current_player_ ? "b" : "w") << "." << fm.to_string() << pto << std::endl;
  bool success = s->apply_move(fm, pto);
  SPIEL_CHECK_TRUE(success);

  // 检查大回合是否结束
  if (IsBigRoundOver() == 0) {
    // 更新游戏状态
	bool submit_success = s->submit();
    ///SPIEL_CHECK_TRUE(submit_success);
	if (!submit_success) {
		if (is_first_real_selfplay_game_) std::cout << "cannot submit for PASS." << std::endl;
	} else {
		ms = s->get_match_status();
		if (ms != match_status_t::PLAYING) {
			if (is_first_real_selfplay_game_) std::cout << "check ms=" << ms << std::endl;
			return;
		}
	}
    StartNewBigRound();
  }
  
  // 更新游戏观察信息
  auto [t, c] = s->get_present();
  std::tie(all_boards_, operable_boards_, boards_edges_) = 
    c ? s->get_observation_information<true>() : s->get_observation_information<false>();
  // check move information
  int mvs_cnt = 0;
  for (const auto& [board_id, move_list] : operable_boards_) {
	  if (move_list.empty()) {
		if (is_first_real_selfplay_game_) std::cout << "has empty board id:" << board_id << std::endl;
		ms = s->get_match_status();
		if (ms != match_status_t::PLAYING) {
			if (is_first_real_selfplay_game_) std::cout << "check2 ms=" << ms << std::endl;
			return;
		}
	  }
	  mvs_cnt += move_list.size();
  }
  // check out of range
  if (mvs_cnt == 0 || mvs_cnt > kNumDistinctActions ||
	  all_boards_.size() > kMaxRuntimeBoards ||
	  operable_boards_.size() > kMaxOperableBoards ||
	  boards_edges_.size() > kMaxRuntimeEdges) {
	ms = match_status_t::STALEMATE;
	if (is_first_real_selfplay_game_) std::cout << "mvs_cnt=" << mvs_cnt
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
}

}  // namespace five_d_chess
}  // namespace open_spiel