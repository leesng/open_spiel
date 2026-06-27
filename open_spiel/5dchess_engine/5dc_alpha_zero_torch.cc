// Copyright 2021 DeepMind Technologies Limited
// Licensed under the Apache License, Version 2.0

#include "5dc_alpha_zero_torch.h"

#include <memory>
#include <random>
#include <vector>
#include <atomic>
#include <thread>

#include "open_spiel/abseil-cpp/absl/strings/str_join.h"
#include "open_spiel/abseil-cpp/absl/time/clock.h"
#include "open_spiel/abseil-cpp/absl/time/time.h"
#include "open_spiel/algorithms/alpha_zero_torch/device_manager.h"
#include "open_spiel/algorithms/alpha_zero_torch/vpevaluator.h"
#include "open_spiel/algorithms/alpha_zero_torch/vpnet.h"
#include "open_spiel/algorithms/mcts.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"

namespace {

// ========== 内部全局状态（完全不对外暴露） ==========
std::atomic<bool> g_running{false};
std::thread g_worker_thread;
// 玩家0/1动作槽：-1为空，单写单读无竞态
std::atomic<int> g_action_slot[2] = {{-1}, {-1}};
AIZeroConfig g_config;

uint32_t ResolveSeed() {
  return g_config.random_seed != 0
      ? g_config.random_seed
      : static_cast<uint32_t>(absl::ToUnixMicros(absl::Now()));
}

// ========== 后台线程核心逻辑 ==========
void AIWorker() {
  std::mt19937 rng(ResolveSeed());
  std::shared_ptr<const open_spiel::Game> game =
      open_spiel::LoadGame(g_config.game_name);

  // 基础校验
  open_spiel::GameType type = game->GetType();
  SPIEL_CHECK_EQ(game->NumPlayers(), 2);
  SPIEL_CHECK_EQ(type.reward_model, open_spiel::GameType::RewardModel::kTerminal);
  SPIEL_CHECK_EQ(type.dynamics, open_spiel::GameType::Dynamics::kSequential);

  // 初始化模型与评估器
  open_spiel::algorithms::torch_az::DeviceManager dm;
  dm.AddDevice(open_spiel::algorithms::torch_az::VPNetModel(
      *game, g_config.az_model_path, g_config.az_graph_file, "/cpu:0"));
  dm.Get(0, 0)->LoadCheckpoint(g_config.checkpoint);

  auto az_eval = std::make_shared<open_spiel::algorithms::torch_az::VPNetEvaluator>(
      &dm, g_config.batch_size, g_config.infer_threads,
      g_config.cache_size, g_config.cache_shards);

  // 创建 MCTS Bot
  auto mcts = std::make_unique<open_spiel::algorithms::MCTSBot>(
      *game, az_eval, g_config.uct_c,
      g_config.max_simulations, g_config.max_memory_mb,
      g_config.use_solver, ResolveSeed(), g_config.verbose,
      open_spiel::algorithms::ChildSelectionPolicy::PUCT, 0, 0, true);

  std::unique_ptr<open_spiel::State> state = game->NewInitialState();
  const bool is_ai[2] = {g_config.player0_is_ai, g_config.player1_is_ai};

  if (!g_config.quiet) {
    std::cerr << "[AI] Thread started. Waiting for moves." << std::endl;
  }

  // 统一主循环：不区分对手类型，只看当前玩家是否由AI控制
  while (g_running.load() && !state->IsTerminal()) {
    const int p = state->CurrentPlayer();

    if (is_ai[p]) {
      // AI 回合：计算走法 → 写入槽位 → 等待UI读取确认
      open_spiel::Action act = mcts->Step(*state);

      // 等待槽位空闲，避免覆盖未读结果
      while (g_running && g_action_slot[p].load() != -1)
        std::this_thread::yield();
      if (!g_running) break;

      g_action_slot[p].store(static_cast<int>(act));

      // 同步自身状态
      state->ApplyAction(act);
      mcts->InformAction(*state, p, act);

      // 等待 UI 读取确认（槽位清空）
      while (g_running && g_action_slot[p].load() != -1)
        std::this_thread::yield();
    } else {
      // 人类回合：等待 UI 推送动作 → 消费 → 清空槽位
      int act = -1;
      while (g_running && (act = g_action_slot[p].load()) == -1)
        std::this_thread::yield();
      if (!g_running) break;

      state->ApplyAction(static_cast<open_spiel::Action>(act));
      mcts->InformAction(*state, p, static_cast<open_spiel::Action>(act));
      g_action_slot[p].store(-1);
    }
  }

  if (!g_config.quiet) {
    std::cerr << "[AI] Thread exited." << std::endl;
  }
}

}  // namespace

// ========== 对外接口实现 ==========

void StartAI(const AIZeroConfig& config) {
  if (g_running.load()) return;
  g_config = config;

  // 纯人人对战：不启动线程，直接返回
  if (!g_config.player0_is_ai && !g_config.player1_is_ai) return;

  // 清空槽位
  g_action_slot[0].store(-1);
  g_action_slot[1].store(-1);

  g_running.store(true);
  g_worker_thread = std::thread(AIWorker);
}

void StopAI() {
  g_running.store(false);
  if (g_worker_thread.joinable()) {
    g_worker_thread.join();
  }
}

bool TryPushHumanAction(int player, int action) {
  int act = g_action_slot[player].load();
  if (act != -1) return false;
  
  g_action_slot[player].store(action);
  return true;
}

bool TryPopAIAction(int player, int* out_action) {
  int act = g_action_slot[player].load();
  if (act == -1) return false;

  *out_action = act;
  g_action_slot[player].store(-1);
  return true;
}