#ifndef OPEN_SPIEL_GAMES_FIVE_D_CHESS_FIVE_D_CHESS_H_
#define OPEN_SPIEL_GAMES_FIVE_D_CHESS_FIVE_D_CHESS_H_

#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <vector>
#include <functional>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>

#include "state.h"
#include "hypercuboid.h"
#include "pgnparser.h"
#include "open_spiel/spiel.h"
#include "zobrist_cache.h"

namespace open_spiel {
namespace five_d_chess {

using BoardId = int; 
using MoveId = uint64_t;
using TensorIndex = int;

// ==================== Global Constant Definitions ====================
// Bit width definitions for action encoding
constexpr int kTotalBoardNumBits = 9;
constexpr int kOperableBoardNumBits = 7;
constexpr int kMoveNumPerBoardBits = 8;

constexpr int kNumDistinctActions = 1 << (kOperableBoardNumBits + kMoveNumPerBoardBits); // Total action count (32768, aligned with model)
constexpr int kMaxMovesPerBoard = 1 << kMoveNumPerBoardBits; // Maximum moves per single board (256)

constexpr int kMaxOperableBoards = 1 << kOperableBoardNumBits;       // Maximum operable boards (128)
constexpr int kMaxRuntimeBoards = 1 << kTotalBoardNumBits;    // Maximum active boards during runtime
constexpr int kMaxRuntimeEdges = kMaxRuntimeBoards * 2; // Maximum graph edges during runtime

constexpr int kNumPieceChannels = 12;         // Total bitboard channels for chess pieces
constexpr int kBoardSize = 8;                 // Standard chess board size (8x8)

// Fixed header size of observation tensor
constexpr int kFixedHeaderSize =
    4                                           // Metadata: total_boards, num_operable, num_edges, current_player
    + 2 * kMaxOperableBoards                    // Operable board local index + selection prior
    + kNumDistinctActions;                      // Legal move prior mask array

// Total size of full observation tensor
constexpr int kObservationTensorSize =
    kFixedHeaderSize
    + kMaxRuntimeBoards * (2 + kNumPieceChannels * kBoardSize * kBoardSize)  // u coordinate + v coordinate + bitboard data
    + 2 * kMaxRuntimeEdges;                      // Graph edge index pairs (src, dst)

constexpr int kMaxGameLength = kMaxRuntimeBoards;          // Maximum allowed game steps
constexpr Action kInvalidAction = -1;         // Mark for invalid action
constexpr MoveId kInvalidMoveId = -1;         // Mark for invalid move id

// ========================= Utility Functions ==============================
// Convert move string list to formatted output string
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

/*constexpr*/ static std::vector<std::pair<::state, ::ext_move>> load_all_books_from_directory(const std::string book_dir)
{
    std::vector<std::pair<::state, ::ext_move>> all_books;

    if (!std::filesystem::exists(book_dir) || !std::filesystem::is_directory(book_dir)) {
        return all_books;
    }

    for (const auto& entry : std::filesystem::directory_iterator(book_dir)) {
        const auto& path = entry.path();
		std::cout << "Loading: " << path.string() << std::endl;
        if (path.extension() == ".5dpgn") {
			
			std::ifstream ifs(path.string(), std::ios::in | std::ios::binary);
			if (!ifs) {
				continue;
			}

			std::string pgn_content = std::string((std::istreambuf_iterator<char>(ifs)),
                      std::istreambuf_iterator<char>());

			auto parsed_game = pgnparser(pgn_content).parse_game();

			::state s2(*parsed_game, [&](const ::state& s, const ::ext_move& m) {
				all_books.emplace_back(s, m);
			});
        }
    }

    std::cout << "All loaded book steps: " << all_books.size() << std::endl;
    return all_books;
}

// Game state implementation for 5D Chess
class FiveDChessState : public State {
 public:
  explicit FiveDChessState(std::shared_ptr<const Game> game);
  FiveDChessState(const FiveDChessState& other);
  ~FiveDChessState() override;

  // Mandatory OpenSpiel interface
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
  // Internal move execution interface of OpenSpiel
  void DoApplyAction(Action action) override;
  void UndoAction(Player player, Action action) override;

  // Action / MoveId encoding & decoding logic
  Action EncodeAction(MoveId moveid) const;
  MoveId DecodeAction(Action action) const;

  // Game runtime status
  bool is_first_real_selfplay_game_;
  Player current_player_;
  //int num_moves_;
  int current_big_round_;

  // Core game engine data (aligned with native engine type)
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
   //std::vector<std::string> history_moves_list_; // Move history for debug
   
  struct UndoEntry {
    //int prev_num_moves;
    int prev_big_round;
    Player prev_player;
    match_status_t prev_ms;
    std::vector<std::pair<BoardId, std::vector<uint64_t>>> prev_all_boards;
    std::vector<std::pair<BoardId, BoardId>> prev_edges;
    std::vector<std::pair<BoardId, std::vector<MoveId>>> prev_operable;

    std::vector<int> apply_new_lines;
    bool did_submit;
    std::tuple<int, bool, bool> submit_params;
    Player actor_player;  // only for debug
  };
  std::vector<UndoEntry> undo_stack_;
};

// Game class definition for 5D Chess
class FiveDChessGame : public Game {
 public:
  explicit FiveDChessGame(const GameParameters& params);

  // Mandatory OpenSpiel interface
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