#ifndef OPEN_SPIEL_GAMES_FIVE_D_CHESS_FIVE_D_CHESS_H_
#define OPEN_SPIEL_GAMES_FIVE_D_CHESS_FIVE_D_CHESS_H_

#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>

#include "open_spiel/spiel.h"
#include "state.h"

namespace open_spiel {
namespace five_d_chess {

using BoardId = int; 
using MoveId = uint64_t;
using TensorIndex = int;

// ==================== 全局常量定义（最终版） ====================
constexpr int kTotalBoardNumBits = 10;
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
    + 2 * kMaxRuntimeEdges                      // 边索引
    + kMaxOperableBoards                        // 可操作棋盘索引
    + kNumDistinctActions;                      // 合法走法掩码（32768位）

constexpr int kObservationTensorSize =
    kFixedHeaderSize
    + kMaxRuntimeBoards * kNumPieceChannels * kBoardSize * kBoardSize;

constexpr int kMaxGameLength = 8192;          // 最大游戏长度
constexpr Action kInvalidAction = -1;         // 无效动作标识
constexpr MoveId kInvalidMoveId = -1;         // 无效走法标识

// =========================基础工具函数==============================

constexpr static int l_to_u(int l) {
    if(l >= 0)
        return l << 1;
    else
        return ~(l << 1);
}
constexpr static int tc_to_v(int t, bool c) {
    return t << 1 | static_cast<int>(c);
}
constexpr static int u_to_l(int u) {
    if(u & 1)
        return ~(u >> 1);
    else
        return u >> 1;
}
constexpr static std::pair<int, bool> v_to_tc(int v) {
    return {v >> 1, static_cast<bool>(v & 1)};
}

constexpr static MoveId EncodeMoveId(int u0, int v0, int y0, int x0,
                                     int u1, int v1, int y1, int x1,
                                     int promotion, int flags) {
  MoveId moveid = 0;
  
  // 附加信息
  moveid |= (static_cast<uint64_t>(flags) & 0xFULL) << 0;
  moveid |= (static_cast<uint64_t>(promotion) & 0xFULL) << 4;
  
  // 目的位置
  moveid |= (static_cast<uint64_t>(x1) & 0x7ULL) << 8;
  moveid |= (static_cast<uint64_t>(y1) & 0x7ULL) << 11;
  moveid |= (static_cast<uint64_t>(v1) & 0xFFULL) << 14;
  moveid |= (static_cast<uint64_t>(u1) & 0xFFULL) << 22;
  
  // 源位置
  moveid |= (static_cast<uint64_t>(x0) & 0x7ULL) << 30;
  moveid |= (static_cast<uint64_t>(y0) & 0x7ULL) << 33;
  moveid |= (static_cast<uint64_t>(v0) & 0xFFULL) << 36;
  moveid |= (static_cast<uint64_t>(u0) & 0xFFULL) << 44;
  
  return moveid;
}

constexpr static std::tuple<int, int, int, int, int, int, int, int, int, int> 
DecodeMoveId(MoveId moveid) {
  int u0 = static_cast<int>((moveid >> 44) & 0xFF);
  int v0 = static_cast<int>((moveid >> 36) & 0xFF);
  int y0 = static_cast<int>((moveid >> 33) & 0x7);
  int x0 = static_cast<int>((moveid >> 30) & 0x7);
  
  int u1 = static_cast<int>((moveid >> 22) & 0xFF);
  int v1 = static_cast<int>((moveid >> 14) & 0xFF);
  int y1 = static_cast<int>((moveid >> 11) & 0x7);
  int x1 = static_cast<int>((moveid >> 8) & 0x7);
  
  int promotion = static_cast<int>((moveid >> 4) & 0xF);
  int flags = static_cast<int>((moveid >> 0) & 0xF);
  
  return {u0, v0, y0, x0, u1, v1, y1, x1, promotion, flags};
}

constexpr static BoardId EncodeBoardId(int u, int v) {
  // 统一编码：u(8位高位) + v(8位低位)，全局唯一
  return static_cast<BoardId>(((u & 0xFF) << 8) | (v & 0xFF));
}

constexpr static std::pair<int, int> DecodeBoardId(BoardId board_id) {
  int u = (board_id >> 8) & 0xFF;
  int v = board_id & 0xFF;
  return {u, v};
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

  // 游戏逻辑辅助函数
  int IsBigRoundOver() const;
  void StartNewBigRound();
  void MarkBoardAsOperated(BoardId board_id);

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
//    const std::string init_str = R"(
//[Timeline "odd"]
//[Size "8x8"]
//[Board "custom"]
//[r*nbqk*bnr*/p*p*p*p*p*p*p*p*/8/8/8/8/P*P*P*P*P*P*P*P*/R*NBQK*BNR*:0:0:b]
//[r*nbqk*bnr*/p*p*p*p*p*p*p*p*/8/8/8/8/P*P*P*P*P*P*P*P*/R*NBQK*BNR*:0:1:w]
//
//1. (0T1)d2d3 / (0T1)b7b5 
//2. (0T2)Nb1d2 / (0T2)Bc8a6 
//3. (0T3)a2a3 / (0T3)Ng8h6 
//4. (0T4)e2e4 / (0T4)b5b4 
//5. (0T5)Ra1a2 / (0T5)e7e6 
//6. (0T6)a3a4 / (0T6)Ba6>>(0T2)e6 
//7. (-1T3)a2a3 (0T7)Ke1>>(0T6)e2 / (1T6)Qd8h4 (-1T3)Be6>>(0T2)e6 (0T7)Nb8>>(0T5)c8 
//8. (-2T3)e2e4 (-1T4)e2e4 (1T7)Ra2a1 (0T8)Bc1>>(0T6)a1 / (-1T4)e7e6 (-2T3)Bc8b7 (0T8)Qd8>>(0T4)h4 (2T6)Ba6>>(1T6)a5 
//)";

   std::optional<::state> s;
   ::match_status_t ms;
   std::vector<std::pair<BoardId, std::vector<uint64_t>>> all_boards_;
   std::vector<std::pair<BoardId, std::vector<MoveId>>> operable_boards_;
   std::vector<std::pair<BoardId, BoardId>> boards_edges_;
   // 新增：Action -> MoveId 缓存映射
   //mutable std::unordered_map<Action, MoveId> action_to_moveid_cache_;
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