#pragma once  // 头文件保护：防止被重复包含（必须加）

#include <cstdint>
#include <unordered_map>
#include <optional>
#include <random>
#include <shared_mutex>
#include <vector>
#include <utility>
#include "absl/numeric/int128.h"

namespace open_spiel {
namespace five_d_chess {

// 全平台统一用Abseil uint128，零平台差异
using Hash128 = absl::uint128;

// 哈希表支持函数
struct Hash128Hash {
    size_t operator()(Hash128 h) const noexcept {
        uint64_t hi = absl::Uint128High64(h);
        uint64_t lo = absl::Uint128Low64(h);
        // 优化：更好的哈希合并方式，减少碰撞
        return std::hash<uint64_t>{}(hi) ^ (std::hash<uint64_t>{}(lo) << 1);
    }
};

// ====================== 跨平台位扫描函数（必须加！）======================
// __builtin_ctzll 在MSVC下不存在，统一封装
inline int CountTrailingZeros(uint64_t x) noexcept {
#ifdef _MSC_VER
    unsigned long pos;
    _BitScanForward64(&pos, x);
    return static_cast<int>(pos);
#else
    return __builtin_ctzll(x);
#endif
}

// ====================== Zobrist表（全平台通用）======================
class Zobrist {
public:
    // 单例：全局唯一实例
    static Zobrist& Get() noexcept {
        static Zobrist inst;
        return inst;
    }

    // 禁止拷贝和移动
    Zobrist(const Zobrist&) = delete;
    Zobrist& operator=(const Zobrist&) = delete;
    Zobrist(Zobrist&&) = delete;
    Zobrist& operator=(Zobrist&&) = delete;

    // 常量定义：集中管理，方便修改
    static constexpr int kPieceTypes = 12;
    static constexpr int kBoardSize = 64;
    static constexpr int kMaxBoardId = 1 << 16;

    // 1. 基础棋子哈希：12种棋子 × 64个位置
    Hash128 piece[kPieceTypes][kBoardSize];
    // 2. 棋盘ID标签哈希：完整16位ID，静态数组（1MB，可忽略）
    Hash128 board_tag[kMaxBoardId];
    // 3. 全局走子方哈希
    Hash128 side[2];

private:
    // 构造函数私有：单例模式
    Zobrist() {
        // 固定种子：保证所有平台、所有运行生成的随机数完全一致
        std::mt19937_64 rng(0x12345678);
        auto rnd128 = [&]() noexcept -> Hash128 {
            return absl::MakeUint128(rng(), rng());
        };

        // 初始化所有棋子位置
        for (int piece_type = 0; piece_type < kPieceTypes; ++piece_type) {
            for (int pos = 0; pos < kBoardSize; ++pos) {
                piece[piece_type][pos] = rnd128();
            }
        }

        // 初始化所有16位ID的标签
        for (int id = 0; id < kMaxBoardId; ++id) {
            board_tag[id] = rnd128();
        }

        // 初始化走子方
        side[0] = rnd128(); // 白方
        side[1] = rnd128(); // 黑方
    }
};

// ====================== 全局共享缓存（已修改为int多状态）======================
class SharedCache {
public:
    // 单例：全局唯一缓存
    static SharedCache& Get() noexcept {
        static SharedCache cache;
        return cache;
    }

    // 禁止拷贝和移动
    SharedCache(const SharedCache&) = delete;
    SharedCache& operator=(const SharedCache&) = delete;
    SharedCache(SharedCache&&) = delete;
    SharedCache& operator=(SharedCache&&) = delete;

    // 读缓存：多线程并行，无阻塞
    // 修改1：返回值从 std::optional<bool> → std::optional<int>
    std::optional<int> Query(Hash128 h) const {
        std::shared_lock<std::shared_mutex> lock(mtx);
        auto it = mp.find(h);
        return it != mp.end() ? std::optional(it->second) : std::nullopt;
    }

    // 写缓存：仅写入瞬间加锁
    // 修改2：参数从 bool res → int res
    void Save(Hash128 h, int res) {
        std::unique_lock<std::shared_mutex> lock(mtx);
        mp[h] = res;
    }

    // 清空缓存：训练结束后释放内存
    void Clear() {
        std::unique_lock<std::shared_mutex> lock(mtx);
        mp.clear();
    }

private:
    // 构造函数私有：单例模式
    SharedCache() = default;

    // 修改3：map值类型从 bool → int
    mutable std::unordered_map<Hash128, int, Hash128Hash> mp;
    mutable std::shared_mutex mtx;
};

// ====================== 核心哈希计算（完全不变）======================
// 必须加 inline！否则多个cpp文件包含会导致多重定义错误
inline Hash128 CalcStateHash(const std::vector<std::pair<int, std::vector<uint64_t>>>& all_boards, bool is_black_turn) {
    const auto& zob = Zobrist::Get();
    Hash128 hash = absl::MakeUint128(0, 0);

    // 1. 全局走子方
    hash ^= zob.side[is_black_turn];

    // 2. 遍历每个有效棋盘
    for (const auto& bd : all_boards) {
        uint16_t board_id = (uint16_t)bd.first;
        const std::vector<uint64_t>& bitboards = bd.second;

        // 计算这个棋盘内所有棋子的哈希
        Hash128 board_piece_hash = absl::MakeUint128(0, 0);
        for (int g = 0; g < Zobrist::kPieceTypes; ++g) {
            uint64_t bb = bitboards[g];
            while (bb) {
                int pos = CountTrailingZeros(bb);
                board_piece_hash ^= zob.piece[g][pos];
                bb &= bb - 1;
            }
        }

        // 直接用16位ID当下标，零开销
        board_piece_hash ^= zob.board_tag[board_id];

        // 加入全局哈希
        hash ^= board_piece_hash;
    }

    return hash;
}

}  // namespace five_d_chess
}  // namespace open_spiel