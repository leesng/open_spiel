#ifndef OPEN_SPIEL_GAMES_FIVE_D_CHESS_FIVE_D_CHESS_H_
#define OPEN_SPIEL_GAMES_FIVE_D_CHESS_FIVE_D_CHESS_H_

#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

#include "open_spiel/spiel.h"
#include "zobrist_cache.h"
#include "state.h"

namespace open_spiel {
namespace five_d_chess {

using BoardId = int; 
using MoveId = uint64_t;
using TensorIndex = int;

// ==================== 全局常量定义（最终版） ====================
constexpr int kTotalBoardNumBits = 11;
constexpr int kOperableBoardNumBits = 7;
constexpr int kMoveNumPerBoardBits = 8;

constexpr int kNumDistinctActions = 1 << (kOperableBoardNumBits + kMoveNumPerBoardBits); // 总走法数量32768（和模型完全对齐）
constexpr int kMaxMovesPerBoard = 1 << kMoveNumPerBoardBits; // 256 每个棋盘最大走法数量

constexpr int kMaxOperableBoards = 1 << kOperableBoardNumBits;       // 最大可操作棋盘数 128
constexpr int kMaxRuntimeBoards = 1 << kTotalBoardNumBits;    // 运行时最大活跃棋盘数，1024 
constexpr int kMaxRuntimeEdges = kMaxRuntimeBoards * 2; // 运行时最大边数

constexpr int kNumPieceChannels = 12;         // 棋子通道数（12个bit平面）
constexpr int kBoardSize = 8;                 // 棋盘大小 8x8

constexpr int kFixedHeaderSize =
    4                                           // 元数据：total_boards, num_operable, num_edges, 保留
    + kMaxOperableBoards                        // 可操作棋盘索引
    + kNumDistinctActions;                      // 合法走法掩码（32768位）

constexpr int kObservationTensorSize =
    kFixedHeaderSize
    + kMaxRuntimeBoards * kNumPieceChannels * kBoardSize * kBoardSize
	+ 2 * kMaxRuntimeEdges;                      // 边索引

constexpr int kMaxGameLength = kMaxRuntimeBoards;          // 最大游戏长度
constexpr Action kInvalidAction = -1;         // 无效动作标识
constexpr MoveId kInvalidMoveId = -1;         // 无效走法标识

// =========================基础工具函数==============================

constexpr static std::string move_list_to_string(std::vector<std::string> str_list) {
	std::string prefix_prev;
	std::string all_move_str;
	std::string delimiter;
	std::string prefix;
	std::string suffix;
	size_t dotPos;
	for (const auto & input : str_list) {
		dotPos = input.find('.');
		if (dotPos != std::string::npos) {
			prefix = input.substr(0, dotPos + 1);
			suffix = input.substr(dotPos + 1);
			if (suffix.find("PASS") != std::string::npos) {
				suffix.clear();
			}
			if (prefix_prev == prefix) {
				prefix.clear();
				delimiter = " ";
				if (!all_move_str.empty()) {
					if (all_move_str.back() == ' ' || all_move_str.back() == '.')
						delimiter.clear();
				}
			} else {
				prefix_prev = prefix;
				delimiter = "\n";
			}
			all_move_str += delimiter + prefix + suffix;
		}
	}
	return all_move_str;
}

//================================ 框架类型定义=================================
// 前向声明你的引擎类
class state;

class FiveDChessState : public State {
 public:
  explicit FiveDChessState(std::shared_ptr<const Game> game);
  FiveDChessState(const FiveDChessState& other);
  ~FiveDChessState() override;

  // OpenSpiel 强制接口
  Player CurrentPlayer() const override;
  std::vector<Action> LegalActions() const override;
  std::string ActionToString(Player player, Action action) const override;
  Action StringToAction(Player player, const std::string& str) const override;
  std::unique_ptr<State> Clone() const override;
  bool IsTerminal() const override;
  std::vector<double> Returns() const override;
  std::string ToString() const override;
  std::string InformationStateString(Player player) const override;
  std::string ObservationString(Player player) const override;
  void ObservationTensor(Player player, absl::Span<float> values) const override;

 private:
  // OpenSpiel 内部调用的走子接口
  void DoApplyAction(Action action) override;

  // 动作编解码（最终版）
  Action EncodeAction(MoveId moveid) const;
  MoveId DecodeAction(Action action) const;

  // 棋盘张量索引映射（全局唯一，GNN用，完全不动）
  TensorIndex GetTensorIndexForBoard(BoardId board_id) const;

  // 游戏状态
  bool is_first_real_selfplay_game_;
  Player current_player_;
  int num_moves_;
  int current_big_round_;
  std::unordered_set<BoardId> operated_boards_;  // 存储全局BoardId

  // 引擎数据（直接使用，类型与引擎一致）
    const std::string init_str = R"(
[Board "Standard - Turn Zero"]
[r*nbqk*bnr*/p*p*p*p*p*p*p*p*/8/8/8/8/P*P*P*P*P*P*P*P*/R*NBQK*BNR*:0:0:b]
[r*nbqk*bnr*/p*p*p*p*p*p*p*p*/8/8/8/8/P*P*P*P*P*P*P*P*/R*NBQK*BNR*:0:1:w]

)";

   std::optional<::state> s;
   ::match_status_t ms;
   std::vector<std::pair<BoardId, std::vector<uint64_t>>> all_boards_;
   std::vector<std::pair<BoardId, std::vector<MoveId>>> operable_boards_;
   std::vector<std::pair<BoardId, BoardId>> boards_edges_;
   // 新增：Action -> MoveId 缓存映射
   //mutable std::unordered_map<Action, MoveId> action_to_moveid_cache_;
   BoardId earliest_non_branch_boardid_;
   std::vector<std::string> history_moves_list_; //for debug
   
};

class FiveDChessGame : public Game {
 public:
  explicit FiveDChessGame(const GameParameters& params);

  // OpenSpiel 强制接口
  int NumDistinctActions() const override;
  std::unique_ptr<State> NewInitialState() const override;
  int MaxChanceOutcomes() const override;
  int NumPlayers() const override;
  double MinUtility() const override;
  double MaxUtility() const override;
  std::vector<int> ObservationTensorShape() const override;
  int MaxGameLength() const override;
};

}  // namespace five_d_chess
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_FIVE_D_CHESS_FIVE_D_CHESS_H_