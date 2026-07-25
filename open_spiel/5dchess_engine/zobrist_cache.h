#pragma once

#include <cstdint>
#include <unordered_map>
#include <optional>
#include <random>
#include <shared_mutex>
#include <mutex>
#include <vector>
#include <utility>
#include "absl/numeric/int128.h"

namespace open_spiel {
namespace five_d_chess {

using Hash128 = absl::uint128;

struct Hash128Hash {
    size_t operator()(Hash128 h) const noexcept {
        uint64_t hi = absl::Uint128High64(h);
        uint64_t lo = absl::Uint128Low64(h);
        return std::hash<uint64_t>{}(hi) ^ (std::hash<uint64_t>{}(lo) << 1);
    }
};

// Cross-platform bit scan utility
inline int CountTrailingZeros(uint64_t x) noexcept {
#ifdef _MSC_VER
    unsigned long pos;
    _BitScanForward64(&pos, x);
    return static_cast<int>(pos);
#else
    return __builtin_ctzll(x);
#endif
}

// Zobrist hashing for state representation
class Zobrist {
public:
    static Zobrist& Get() noexcept {
        static Zobrist inst;
        return inst;
    }

    Zobrist(const Zobrist&) = delete;
    Zobrist& operator=(const Zobrist&) = delete;
    Zobrist(Zobrist&&) = delete;
    Zobrist& operator=(Zobrist&&) = delete;

    static constexpr int kPieceTypes = 12;
    static constexpr int kBoardSize = 64;
    static constexpr int kMaxBoardId = 1 << 16;

    Hash128 piece[kPieceTypes][kBoardSize];
    Hash128 board_tag[kMaxBoardId];
    Hash128 side[2];

private:
    Zobrist() {
        std::mt19937_64 rng(0x12345678);
        auto rnd128 = [&]() noexcept -> Hash128 {
            return absl::MakeUint128(rng(), rng());
        };

        for (int piece_type = 0; piece_type < kPieceTypes; ++piece_type) {
            for (int pos = 0; pos < kBoardSize; ++pos) {
                piece[piece_type][pos] = rnd128();
            }
        }

        for (int id = 0; id < kMaxBoardId; ++id) {
            board_tag[id] = rnd128();
        }

        side[0] = rnd128(); // whitle
        side[1] = rnd128(); // black
    }
};

// Thread-safe shared cache for game state results
template<typename T>
class SharedCache {
public:

    static SharedCache& Get() noexcept {
        static SharedCache<T> cache;
        return cache;
    }

    SharedCache(const SharedCache&) = delete;
    SharedCache& operator=(const SharedCache&) = delete;
    SharedCache(SharedCache&&) = delete;
    SharedCache& operator=(SharedCache&&) = delete;

    std::optional<T> Query(Hash128 h) const {
        std::shared_lock<std::shared_mutex> lock(mtx);
        auto it = mp.find(h);
        return (it != mp.end()) ? std::optional<T>(it->second) : std::nullopt;
    }

    void Upsert(Hash128 h, const T& value) {
        std::unique_lock<std::shared_mutex> lock(mtx);
        mp[h] = value;
    }

    void Upsert(Hash128 h, T&& value) {
        std::unique_lock<std::shared_mutex> lock(mtx);
        mp[h] = std::move(value);
    }

    void Clear() {
        std::unique_lock<std::shared_mutex> lock(mtx);
        mp.clear();
    }

    size_t Size() const {
        std::shared_lock<std::shared_mutex> lock(mtx);
        return mp.size();
    }

private:
    SharedCache() = default;

    mutable std::unordered_map<Hash128, T, Hash128Hash> mp;
    mutable std::shared_mutex mtx;
};

// Calculate 128-bit hash for game state
inline Hash128 CalcStateHash(const std::vector<std::pair<int, std::vector<uint64_t>>>& all_boards, bool is_black_turn) {
    const auto& zob = Zobrist::Get();
    Hash128 hash = absl::MakeUint128(0, 0);

    hash ^= zob.side[is_black_turn];

    for (const auto& bd : all_boards) {
        uint16_t board_id = (uint16_t)bd.first;
        const std::vector<uint64_t>& bitboards = bd.second;

        Hash128 board_piece_hash = absl::MakeUint128(0, 0);
        for (int g = 0; g < Zobrist::kPieceTypes; ++g) {
            uint64_t bb = bitboards[g];
            while (bb) {
                int pos = CountTrailingZeros(bb);
                board_piece_hash ^= zob.piece[g][pos];
                bb &= bb - 1;
            }
        }

        board_piece_hash ^= zob.board_tag[board_id];
        hash ^= board_piece_hash;
    }

    return hash;
}

/*
   int GetMatchStatus() {
	Hash128 h = CalcStateHash(all_boards_, current_player_);
	auto cache = SharedCache::Get().Query(h);
	
   //static int aaa = 0;
   //static int bbb = 0;
	//aaa++;
	if (cache) {
		//bbb++;
		//if (aaa % 1000 == 999) std::cout << "~v~" << bbb << "/" << aaa << "=" << bbb * 100 / aaa << "%" <<std::endl;
		return *cache;
	}

	int status = (int)s->get_match_status(move_list_to_string(history_moves_list_));
	SharedCache::Get().Upsert(h, status);
	return status;
  };
*/

}  // namespace five_d_chess
}  // namespace open_spiel