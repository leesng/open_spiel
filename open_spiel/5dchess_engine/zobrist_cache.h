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

struct Hash128Hash {
    size_t operator()(absl::uint128 h) const noexcept {
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

    absl::uint128 piece[kPieceTypes][kBoardSize];
    absl::uint128 board_tag[kMaxBoardId];
    absl::uint128 side[2];

private:
    Zobrist() {
        std::mt19937_64 rng(0x12345678);
        auto rnd128 = [&]() noexcept -> absl::uint128 {
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

    void SetMaxSize(size_t max_size) {
        std::unique_lock<std::shared_mutex> lock(mtx);
        max_size_ = max_size;
    }

    std::optional<T> Query(absl::uint128 h) const {
        std::shared_lock<std::shared_mutex> lock(mtx);
        auto it = mp.find(h);
        return (it != mp.end()) ? std::optional<T>(it->second) : std::nullopt;
    }

    void Upsert(absl::uint128 h, const T& value) {
        std::unique_lock<std::shared_mutex> lock(mtx);
        mp[h] = value;
        CheckAndTrimLocked();
    }

    void Upsert(absl::uint128 h, T&& value) {
        std::unique_lock<std::shared_mutex> lock(mtx);
        mp[h] = std::move(value);
        CheckAndTrimLocked();
    }

    void Clear() {
        std::unique_lock<std::shared_mutex> lock(mtx);
        ClearAndShrinkLocked();
    }

    size_t Size() const {
        std::shared_lock<std::shared_mutex> lock(mtx);
        return mp.size();
    }

private:
    SharedCache() = default;

    void CheckAndTrimLocked() {
        if (mp.size() > max_size_) {
            ClearAndShrinkLocked();
        }
    }

    void ClearAndShrinkLocked() {
        mp.clear();
        std::unordered_map<absl::uint128, T, Hash128Hash>().swap(mp);
    }

    mutable std::unordered_map<absl::uint128, T, Hash128Hash> mp;
    mutable std::shared_mutex mtx;
    size_t max_size_ = 100000;
};

// Calculate 128-bit hash for game state
inline absl::uint128 CalcSingleBoardHash(int board_id, const std::vector<uint64_t>& bitboards) {
    const auto& zob = Zobrist::Get();
    absl::uint128 board_hash = absl::MakeUint128(0, 0);
    uint16_t bid = static_cast<uint16_t>(board_id);

    for (int g = 0; g < Zobrist::kPieceTypes; ++g) {
        uint64_t bb = bitboards[g];
        while (bb) {
            int pos = CountTrailingZeros(bb);
            board_hash ^= zob.piece[g][pos];
            bb &= bb - 1;
        }
    }
    board_hash ^= zob.board_tag[bid];
    return board_hash;
}

inline absl::uint128 AddBoardsToHash(absl::uint128 current_hash, const std::vector<std::pair<int, std::vector<uint64_t>>>& boards, int start_index) {
    for (int i = start_index; i < (int)boards.size(); i++) {
        current_hash ^= CalcSingleBoardHash(boards[i].first, boards[i].second);
    }
    return current_hash;
}

inline absl::uint128 RemoveBoardsFromHash(absl::uint128 current_hash, const std::vector<std::pair<int, std::vector<uint64_t>>>& boards, int start_index) {
    return AddBoardsToHash(current_hash, boards, start_index);
}

inline absl::uint128 FlipPlayerInHash(absl::uint128 current_hash) {
    const auto& zob = Zobrist::Get();
    return current_hash ^ zob.side[0] ^ zob.side[1];
}

inline absl::uint128 CalcStateHash(const std::vector<std::pair<int, std::vector<uint64_t>>>& all_boards, bool is_black_turn) {
    const auto& zob = Zobrist::Get();
    absl::uint128 hash = absl::MakeUint128(0, 0);
    hash ^= zob.side[is_black_turn];
    hash = AddBoardsToHash(hash, all_boards, 0);
    return hash;
}

inline std::atomic<int> aaa{0};
inline std::atomic<int> bbb{0};

}  // namespace five_d_chess
}  // namespace open_spiel