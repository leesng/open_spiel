// Copyright 2021 DeepMind Technologies Limited
// Licensed under the Apache License, Version 2.0

#ifndef FIVE_D_CHESS_5DC_ALPHA_ZERO_TORCH_H
#define FIVE_D_CHESS_5DC_ALPHA_ZERO_TORCH_H

#include <string>
#include <cstdint>

// AI 推理配置：全部用基础通用类型，不引入任何 OpenSpiel/Abseil 定义
struct AIZeroConfig {
  // 模型与路径
  std::string game_name = "five_d_chess";
  std::string az_model_path = "az_5d_train_log";
  std::string az_graph_file = "vpnet.pb";
  int checkpoint = -1;

  // MCTS 核心参数
  double uct_c = 2.0;
  int max_simulations = 100;
  int max_memory_mb = 1000;
  bool use_solver = true;

  // 推理性能参数
  int batch_size = 1;
  int infer_threads = 1;
  int cache_size = 16384;
  int cache_shards = 1;

  // 玩家控制：true = 该玩家由后台AI控制
  bool player0_is_ai = false;
  bool player1_is_ai = true;

  // 其他
  uint32_t random_seed = 0;
  bool verbose = false;
  bool quiet = false;
};

// 启动后台 AI 推理线程
// 纯人人对战时内部会自动跳过线程创建，无额外开销
void StartAI(const AIZeroConfig& config);

// 停止后台 AI 线程，优雅退出，阻塞等待线程回收
void StopAI();

// UI 调用：人类玩家走棋后，推送动作给后台 AI 同步状态
// player: 0/1 对应玩家编号；action: 走法编号（和引擎 action 一一对应）
bool TryPushHumanAction(int player, int action);

// UI 调用：每帧非阻塞读取 AI 走法
// 返回 true 表示读到有效走法，action 输出结果；false 表示 AI 尚未计算完成
bool TryPopAIAction(int player, int* out_action);

#endif  // FIVE_D_CHESS_5DC_ALPHA_ZERO_TORCH_H