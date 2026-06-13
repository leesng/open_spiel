// Copyright 2021 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "open_spiel/algorithms/alpha_zero_torch/model.h"

#include <torch/torch.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "open_spiel/abseil-cpp/absl/strings/match.h"

namespace open_spiel {
namespace algorithms {
namespace torch_az {

std::istream& operator>>(std::istream& stream, ModelConfig& config) {
  int channels;
  int height;
  int width;

  stream >> channels >> height >> width >> config.number_of_actions >>
      config.nn_depth >> config.nn_width >> config.learning_rate >>
      config.weight_decay >> config.nn_model;

  config.observation_tensor_shape = {channels, height, width};

  return stream;
}

std::ostream& operator<<(std::ostream& stream, const ModelConfig& config) {
  int shape_dim = config.observation_tensor_shape.size();
  int height = shape_dim > 1 ? config.observation_tensor_shape[1] : 1;
  int width = shape_dim > 2 ? config.observation_tensor_shape[2] : 1;

  stream << config.observation_tensor_shape[0] << " " << height << " " << width
         << " " << config.number_of_actions << " " << config.nn_depth << " "
         << config.nn_width << " " << config.learning_rate << " "
         << config.weight_decay << " " << config.nn_model;
  return stream;
}

ResInputBlockImpl::ResInputBlockImpl(const ResInputBlockConfig& config)
    : conv_(torch::nn::Conv2dOptions(
                /*input_channels=*/config.input_channels,
                /*output_channels=*/config.filters,
                /*kernel_size=*/config.kernel_size)
                .stride(1)
                .padding(config.padding)
                .dilation(1)
                .groups(1)
                .bias(true)
                .padding_mode(torch::kZeros)),
      batch_norm_(torch::nn::BatchNorm2dOptions(
                      /*num_features=*/config.filters)
                      .eps(0.001)      // Make it the same as TF.
                      .momentum(0.01)  // Torch momentum = 1 - TF momentum.
                      .affine(true)
                      .track_running_stats(true)) {
  channels_ = config.input_channels;
  height_ = config.input_height;
  width_ = config.input_width;

  register_module("input_conv", conv_);
  register_module("input_batch_norm", batch_norm_);
}

torch::Tensor ResInputBlockImpl::forward(torch::Tensor x) {
  torch::Tensor output = x.view({-1, channels_, height_, width_});
  output = torch::relu(batch_norm_(conv_(output)));

  return output;
}

ResTorsoBlockImpl::ResTorsoBlockImpl(const ResTorsoBlockConfig& config,
                                     int layer)
    : conv1_(torch::nn::Conv2dOptions(
                 /*input_channels=*/config.input_channels,
                 /*output_channels=*/config.filters,
                 /*kernel_size=*/config.kernel_size)
                 .stride(1)
                 .padding(config.padding)
                 .dilation(1)
                 .groups(1)
                 .bias(true)
                 .padding_mode(torch::kZeros)),
      conv2_(torch::nn::Conv2dOptions(
                 /*input_channels=*/config.filters,
                 /*output_channels=*/config.filters,
                 /*kernel_size=*/config.kernel_size)
                 .stride(1)
                 .padding(config.padding)
                 .dilation(1)
                 .groups(1)
                 .bias(true)
                 .padding_mode(torch::kZeros)),
      batch_norm1_(torch::nn::BatchNorm2dOptions(
                       /*num_features=*/config.filters)
                       .eps(0.001)      // Make it the same as TF.
                       .momentum(0.01)  // Torch momentum = 1 - TF momentum.
                       .affine(true)
                       .track_running_stats(true)),
      batch_norm2_(torch::nn::BatchNorm2dOptions(
                       /*num_features=*/config.filters)
                       .eps(0.001)      // Make it the same as TF.
                       .momentum(0.01)  // Torch momentum = 1 - TF momentum.
                       .affine(true)
                       .track_running_stats(true)) {
  register_module("res_" + std::to_string(layer) + "_conv_1", conv1_);
  register_module("res_" + std::to_string(layer) + "_conv_2", conv2_);
  register_module("res_" + std::to_string(layer) + "_batch_norm_1",
                  batch_norm1_);
  register_module("res_" + std::to_string(layer) + "_batch_norm_2",
                  batch_norm2_);
}

torch::Tensor ResTorsoBlockImpl::forward(torch::Tensor x) {
  torch::Tensor residual = x;

  torch::Tensor output = torch::relu(batch_norm1_(conv1_(x)));
  output = batch_norm2_(conv2_(output));
  output += residual;
  output = torch::relu(output);

  return output;
}

ResOutputBlockImpl::ResOutputBlockImpl(const ResOutputBlockConfig& config)
    : value_conv_(torch::nn::Conv2dOptions(
                      /*input_channels=*/config.input_channels,
                      /*output_channels=*/config.value_filters,
                      /*kernel_size=*/config.kernel_size)
                      .stride(1)
                      .padding(config.padding)
                      .dilation(1)
                      .groups(1)
                      .bias(true)
                      .padding_mode(torch::kZeros)),
      value_batch_norm_(
          torch::nn::BatchNorm2dOptions(
              /*num_features=*/config.value_filters)
              .eps(0.001)      // Make it the same as TF.
              .momentum(0.01)  // Torch momentum = 1 - TF momentum.
              .affine(true)
              .track_running_stats(true)),
      value_linear1_(torch::nn::LinearOptions(
                         /*in_features=*/config.value_linear_in_features,
                         /*out_features=*/config.value_linear_out_features)
                         .bias(true)),
      value_linear2_(torch::nn::LinearOptions(
                         /*in_features=*/config.value_linear_out_features,
                         /*out_features=*/1)
                         .bias(true)),
      value_observation_size_(config.value_observation_size),
      policy_conv_(torch::nn::Conv2dOptions(
                       /*input_channels=*/config.input_channels,
                       /*output_channels=*/config.policy_filters,
                       /*kernel_size=*/config.kernel_size)
                       .stride(1)
                       .padding(config.padding)
                       .dilation(1)
                       .groups(1)
                       .bias(true)
                       .padding_mode(torch::kZeros)),
      policy_batch_norm_(
          torch::nn::BatchNorm2dOptions(
              /*num_features=*/config.policy_filters)
              .eps(0.001)      // Make it the same as TF.
              .momentum(0.01)  // Torch momentum = 1 - TF momentum.
              .affine(true)
              .track_running_stats(true)),
      policy_linear_(torch::nn::LinearOptions(
                         /*in_features=*/config.policy_linear_in_features,
                         /*out_features=*/config.policy_linear_out_features)
                         .bias(true)),
      policy_observation_size_(config.policy_observation_size) {
  register_module("value_conv", value_conv_);
  register_module("value_batch_norm", value_batch_norm_);
  register_module("value_linear_1", value_linear1_);
  register_module("value_linear_2", value_linear2_);
  register_module("policy_conv", policy_conv_);
  register_module("policy_batch_norm", policy_batch_norm_);
  register_module("policy_linear", policy_linear_);
}

std::vector<torch::Tensor> ResOutputBlockImpl::forward(torch::Tensor x,
                                                       torch::Tensor mask) {
  torch::Tensor value_output = torch::relu(value_batch_norm_(value_conv_(x)));
  value_output = value_output.view({-1, value_observation_size_});
  value_output = torch::relu(value_linear1_(value_output));
  value_output = torch::tanh(value_linear2_(value_output));

  torch::Tensor policy_logits =
      torch::relu(policy_batch_norm_(policy_conv_(x)));
  policy_logits = policy_logits.view({-1, policy_observation_size_});
  policy_logits = policy_linear_(policy_logits);
  policy_logits = torch::where(mask, policy_logits,
                               -(1 << 16) * torch::ones_like(policy_logits));

  return {value_output, policy_logits};
}

MLPBlockImpl::MLPBlockImpl(const int in_features, const int out_features)
    : linear_(torch::nn::LinearOptions(
                         /*in_features=*/in_features,
                         /*out_features=*/out_features)
                         .bias(true)) {
  register_module("linear", linear_);
}

torch::Tensor MLPBlockImpl::forward(torch::Tensor x) {
  return torch::relu(linear_(x));
}

MLPOutputBlockImpl::MLPOutputBlockImpl(const int nn_width,
                                       const int policy_linear_out_features)
    : value_linear1_(torch::nn::LinearOptions(
                         /*in_features=*/nn_width,
                         /*out_features=*/nn_width)
                         .bias(true)),
      value_linear2_(torch::nn::LinearOptions(
                         /*in_features=*/nn_width,
                         /*out_features=*/1)
                         .bias(true)),
      policy_linear1_(torch::nn::LinearOptions(
                          /*input_channels=*/nn_width,
                          /*output_channels=*/nn_width)
                          .bias(true)),
      policy_linear2_(torch::nn::LinearOptions(
                          /*in_features=*/nn_width,
                          /*out_features=*/policy_linear_out_features)
                          .bias(true)) {
  register_module("value_linear_1", value_linear1_);
  register_module("value_linear_2", value_linear2_);
  register_module("policy_linear_1", policy_linear1_);
  register_module("policy_linear_2", policy_linear2_);
}

std::vector<torch::Tensor> MLPOutputBlockImpl::forward(torch::Tensor x,
                                                       torch::Tensor mask) {
  torch::Tensor value_output = torch::relu(value_linear1_(x));
  value_output = torch::tanh(value_linear2_(value_output));

  torch::Tensor policy_logits = torch::relu(policy_linear1_(x));
  policy_logits = policy_linear2_(policy_logits);
  policy_logits = torch::where(mask, policy_logits,
                               -(1 << 16) * torch::ones_like(policy_logits));

  return {value_output, policy_logits};
}

// ==================== AlphaGateau Hierarchical Implementation ====================
AGHBoardEncoderImpl::AGHBoardEncoderImpl(int embedding_dim)
  : embedding_dim_(embedding_dim) {
  // Adapt network for 12 input channels of board bitboard
  conv1_ = register_module("agh_conv1",
      torch::nn::Conv2d(torch::nn::Conv2dOptions(kBoardInputChannels, embedding_dim, 3).padding(1)));
  conv2_ = register_module("agh_conv2",
      torch::nn::Conv2d(torch::nn::Conv2dOptions(embedding_dim, embedding_dim, 3).padding(1)));
  relu_ = register_module("agh_relu", torch::nn::ReLU());
  // Coordinate projection layer: maps (u,v) to embedding dimension
  coord_proj_ = register_module("agh_coord_proj", torch::nn::Linear(2, embedding_dim));
}

torch::Tensor AGHBoardEncoderImpl::forward(torch::Tensor x, torch::Tensor coords) {
  x = x.view({-1, kBoardInputChannels, kBoardHeight, kBoardWidth});
  // Encode board bitboard features
  torch::Tensor board_feat = relu_->forward(conv2_->forward(relu_->forward(conv1_->forward(x))));
  board_feat = board_feat.mean({2, 3}); // Global average pooling
  
  // Encode (u,v) coordinates and fuse with board features
  torch::Tensor coord_feat = coord_proj_->forward(coords);
  return board_feat + coord_feat;
}

AGHGATEAUImpl::AGHGATEAUImpl(int embedding_dim)
  : embedding_dim_(embedding_dim) {
  edge_lin_ = register_module("agh_edge_lin", torch::nn::Linear(embedding_dim, embedding_dim));
  attn_lin_ = register_module("agh_attn_lin", torch::nn::Linear(embedding_dim, 1));
  leaky_relu_ = register_module("agh_leaky", 
      torch::nn::LeakyReLU(torch::nn::LeakyReLUOptions().negative_slope(0.2)));
}

torch::Tensor AGHGATEAUImpl::forward(torch::Tensor nodes, torch::Tensor edge_index) {
  torch::Tensor src = edge_index[0];
  torch::Tensor dst = edge_index[1];
  torch::Tensor sent_nodes = nodes.index({src});
  torch::Tensor dst_nodes = nodes.index({dst});

  // Edge feature computation
  torch::Tensor edge_feat = sent_nodes + dst_nodes;
  // Attention score computation
  torch::Tensor attn_logits = leaky_relu_->forward(attn_lin_->forward(edge_feat));
  
  // Numerically stable segmented softmax (optimized for large node counts)
  // 1. Node feature buffer [N, D]
  torch::Tensor max_buffer = torch::zeros_like(nodes);
  // Get feature dimension D
  int64_t feat_dim = max_buffer.size(1);

  // 2. Flatten edge attention logits to 1D array [E]
  torch::Tensor attn_e = attn_logits.flatten();

  // 3. Explicit dimension expansion for C++ compatibility (avoid implicit broadcast)
  attn_e = attn_e.unsqueeze(1).expand({-1, feat_dim});

  // 4. Cast index to long type to prevent assertion failure
  torch::Tensor dst_long = dst.to(torch::kLong);

  // 5. Safe index addition
  max_buffer.index_add_(0, dst_long, attn_e);

  // 6. Standard softmax calculation
  torch::Tensor max_logits = max_buffer.index({dst_long});

  attn_logits = attn_logits - max_logits;
  torch::Tensor exp_attn = torch::exp(attn_logits);
  torch::Tensor sum_exp = torch::zeros_like(nodes).index_add_(0, dst_long, exp_attn).index({dst_long});
  torch::Tensor attn_weights = exp_attn / (sum_exp + 1e-8);

  // Message aggregation across graph edges
  return torch::zeros_like(nodes).index_add_(0, dst_long, sent_nodes * attn_weights);
}

AGHHierarchicalHeadImpl::AGHHierarchicalHeadImpl(int embedding_dim) {
  value_head_ = register_module("agh_value", torch::nn::Linear(embedding_dim, 1));
  board_selector_head_ = register_module("agh_board_sel", torch::nn::Linear(embedding_dim, 1));
  move_selector_head_ = register_module("agh_move_sel", torch::nn::Linear(embedding_dim, kMaxMovesPerBoard));

  // 4-head cross-board multi-head attention
  cross_board_attn_ = register_module("agh_cross_attn", 
      torch::nn::MultiheadAttention(torch::nn::MultiheadAttentionOptions(embedding_dim, 4)));
}

std::vector<torch::Tensor> AGHHierarchicalHeadImpl::forward(
    torch::Tensor all_node_features,
    torch::Tensor global_feature,
    torch::Tensor operable_board_indices,
    torch::Tensor operable_board_priors,
    torch::Tensor legal_move_mask,
    int num_operable_boards) {
  // 1. Position value prediction
  torch::Tensor value = torch::tanh(value_head_->forward(global_feature));

  // Initialize fixed-size logit tensors for 32768-dimensional policy output
  torch::Tensor full_board_logits = torch::full({kMaxOperableBoards}, -1e9, all_node_features.options());
  torch::Tensor full_move_logits = torch::full({kMaxOperableBoards, kMaxMovesPerBoard}, -1e9, all_node_features.options());

  // Reshape and slice legal move mask
  auto move_priorities_2d = legal_move_mask.view({kMaxOperableBoards, kMaxMovesPerBoard});
  auto move_priorities = move_priorities_2d.narrow(0, 0, num_operable_boards);

  // =============== Parse board index information from game side ===============
  torch::Tensor valid_board_mask = operable_board_indices != -1;
  torch::Tensor real_board_indices = operable_board_indices.to(torch::kLong);
  real_board_indices = torch::where(valid_board_mask, real_board_indices, torch::zeros_like(real_board_indices));

  torch::Tensor operable_node_features = all_node_features.index({real_board_indices});
  // =======================================================================

  // Apply cross-board multi-head attention
  torch::Tensor attn_input = operable_node_features.unsqueeze(1);
  auto attn_result = cross_board_attn_->forward(attn_input, attn_input, attn_input);
  torch::Tensor attn_output = std::get<0>(attn_result);
  operable_node_features = attn_output.squeeze(1) + operable_node_features;

  // Calculate board logits and add prior weight
  torch::Tensor operable_board_logits = board_selector_head_->forward(operable_node_features).squeeze(-1);
  constexpr float PRIOR_BOARD_STRENGTH = 2.0f;
  operable_board_logits = operable_board_logits + PRIOR_BOARD_STRENGTH * torch::log(operable_board_priors.clamp_min(1e-8));

  // Mask out invalid board entries
  operable_board_logits = torch::where(
      valid_board_mask,
      operable_board_logits,
      -1e20 * torch::ones_like(operable_board_logits)
  );
//#if 0
  // ========== 1. Board level soft pruning & generate board pruning mask ==========
  float max_board_logit = operable_board_logits.max().item<float>();
  torch::Tensor board_cut_mask = operable_board_logits > (max_board_logit - 5.0f);
  operable_board_logits = torch::where(
      board_cut_mask,
      operable_board_logits,
      -1e9 * torch::ones_like(operable_board_logits)
  );
//#endif
  // ========== 2. Cascade pruning: prune all moves belonging to pruned boards ==========
  torch::Tensor operable_move_logits = move_selector_head_->forward(operable_node_features);
  constexpr float PRIOR_MOVE_STRENGTH = 10.0f;
  operable_move_logits = operable_move_logits + PRIOR_MOVE_STRENGTH * torch::log(move_priorities.clamp_min(1e-8));//(1e-40));
//#if 0
  // Force mask all moves from pruned boards
  operable_move_logits = torch::where(
      board_cut_mask.unsqueeze(1),
      operable_move_logits,
      -1e9 * torch::ones_like(operable_move_logits)
  );

  // ========== 3. Preserve original move level soft pruning ==========
  auto [max_per_board, _] = torch::max(operable_move_logits, /*dim=*/1, /*keepdim=*/true);
  operable_move_logits = torch::where(
    operable_move_logits > max_per_board - 5.0f,
    operable_move_logits,
    -1e9 * torch::ones_like(operable_move_logits)
  );
//#endif
  // Fill computed logits into fixed-size tensors
  full_board_logits.index_put_({torch::indexing::Slice(0, num_operable_boards)}, operable_board_logits);
  full_move_logits.index_put_({torch::indexing::Slice(0, num_operable_boards)}, operable_move_logits);

  // Compute final probability distribution
  torch::Tensor board_probs = torch::softmax(full_board_logits, -1);
  torch::Tensor move_probs = torch::softmax(full_move_logits, -1);
  torch::Tensor final_policy = board_probs.unsqueeze(1) * move_probs;
  final_policy = final_policy.flatten().unsqueeze(0);

// ==================== Enhanced Debug Print: 24 decimal precision + raw prior output ====================
//#define DEBUG_PROBABILITY 1
#ifdef DEBUG_PROBABILITY
{
    std::cout << "\n======================================" << std::endl;
    std::cout << "=== PROBABILITY DISTRIBUTION DEBUG ===" << std::endl;
    std::cout << "Total operable boards: " << num_operable_boards << std::endl;
    std::cout << "Global value prediction: " << value.item<float>() << std::endl;
    std::cout << "--------------------------------------" << std::endl;
    
    float total_board_prob = 0.0f;
    for (int i = 0; i < num_operable_boards; ++i) {
        float board_prob = board_probs[i].item<float>();
        float board_raw_prior = operable_board_priors[i].item<float>(); // Raw prior of the board
        int board_idx = (int)real_board_indices[i].item<int64_t>();
        total_board_prob += board_prob;

        // Print board info: model predicted probability and raw game prior
        std::cout << "Board " << i 
                  << " (ID:" << board_idx 
                  << ") | Model Prob: " << std::fixed << std::setprecision(4) << board_prob * 100 << "%"
                  << " | Raw Prior: " << std::fixed << std::setprecision(4) << board_raw_prior * 100 << "%"
                  << std::endl;
        
        auto move_probs_this_board = move_probs[i];
        auto move_logits_this_board = operable_move_logits[i];
        auto move_raw_prior_this_board = move_priorities[i]; // Raw prior of the move

        std::cout << "  Logits range: min=" << move_logits_this_board.min().item<float>()
                  << ", max=" << move_logits_this_board.max().item<float>() << std::endl;
		  
        std::vector<std::tuple<float, float, int>> move_list; // (model probability, raw prior, move id)
        for (int j = 0; j < kMaxMovesPerBoard; ++j) {
            float move_prob = move_probs_this_board[j].item<float>();
            float move_raw_prior = move_raw_prior_this_board[j].item<float>();
            if (move_prob > 1e-40) {
                move_list.emplace_back(move_prob, move_raw_prior, j);
            }
        }
        
        // Sort moves in descending order by model probability
        std::sort(move_list.rbegin(), move_list.rend());
        
        // Extend print limit to 100 moves
        int print_count = std::min(100, (int)move_list.size());
        for (int k = 0; k < print_count; ++k) {
            float m_prob = std::get<0>(move_list[k]);
            float m_prior = std::get<1>(move_list[k]);
            int m_id = std::get<2>(move_list[k]);
            std::cout << "  Move " << m_id 
                      << " | Model: " << std::fixed << std::setprecision(24) << m_prob * 100 << "%"
                      << " | Raw Prior: " << std::fixed << std::setprecision(24) << m_prior * 100 << "%"
                      << std::endl;
        }
    }
    
    std::cout << "--------------------------------------" << std::endl;
    std::cout << "Total board probability: " << std::fixed << std::setprecision(6) << total_board_prob << std::endl;
    std::cout << "Final policy sum: " << std::fixed << std::setprecision(6) << final_policy.sum().item<float>() << std::endl;
    std::cout << "======================================\n" << std::endl;
}
#endif
// ============================================================================

  return {value, final_policy, board_probs, move_probs};
}

ModelImpl::ModelImpl(const ModelConfig& config, const std::string& device)
    : device_(device),
      num_torso_blocks_(config.nn_depth),
      weight_decay_(config.weight_decay),
  // Save config.nn_model to class
      nn_model_(config.nn_model) {

  int input_size = 1;
  for (const auto& num : config.observation_tensor_shape) {
    if (num > 0) {
      input_size *= num;
    }
  }
  // Decide if resnet or MLP
  if (config.nn_model == "resnet") {
    int obs_dims = config.observation_tensor_shape.size();
    int channels = config.observation_tensor_shape[0];
    int height = obs_dims > 1 ? config.observation_tensor_shape[1] : 1;
    int width = obs_dims > 2 ? config.observation_tensor_shape[2] : 1;

    ResInputBlockConfig input_config = {/*input_channels=*/channels,
                                        /*input_height=*/height,
                                        /*input_width=*/width,
                                        /*filters=*/config.nn_width,
                                        /*kernel_size=*/3,
                                        /*padding=*/1};

    ResTorsoBlockConfig residual_config = {/*input_channels=*/config.nn_width,
                                           /*filters=*/config.nn_width,
                                           /*kernel_size=*/3,
                                           /*padding=*/1};

    ResOutputBlockConfig output_config = {
        /*input_channels=*/config.nn_width,
        /*value_filters=*/1,
        /*policy_filters=*/2,
        /*kernel_size=*/1,
        /*padding=*/0,
        /*value_linear_in_features=*/1 * width * height,
        /*value_linear_out_features=*/config.nn_width,
        /*policy_linear_in_features=*/2 * width * height,
        /*policy_linear_out_features=*/config.number_of_actions,
        /*value_observation_size=*/1 * width * height,
        /*policy_observation_size=*/2 * width * height};

    layers_->push_back(ResInputBlock(input_config));
    for (int i = 0; i < num_torso_blocks_; i++) {
      layers_->push_back(ResTorsoBlock(residual_config, i));
    }
    layers_->push_back(ResOutputBlock(output_config));

    register_module("layers", layers_);

  } else if (config.nn_model == "mlp") {
    layers_->push_back(MLPBlock(input_size, config.nn_width));
    for (int i = 0; i < num_torso_blocks_; i++) {
      layers_->push_back(MLPBlock(config.nn_width, config.nn_width));
    }
    layers_->push_back(
        MLPOutputBlock(config.nn_width, config.number_of_actions));

    register_module("layers", layers_);

  } else if (config.nn_model == "gateau") {
    // Initialize hierarchical GNN model
    ag_hier_encoder_ = register_module("agh_encoder", AGHBoardEncoder(kEmbeddingDim));
    ag_hier_gateau_ = register_module("agh_gateau", AGHGATEAU(kEmbeddingDim));
    ag_hier_output_ = register_module("agh_output", AGHHierarchicalHead(kEmbeddingDim));

  } else {
    throw std::runtime_error("Unknown nn_model: " + config.nn_model);
  }
}

std::vector<torch::Tensor> ModelImpl::forward(torch::Tensor x, torch::Tensor mask) {
  std::vector<torch::Tensor> output = this->forward_(x, mask);
  
  // Validate output shape to match vpnet.cc requirements
  torch::Tensor value = output[0];
  torch::Tensor policy_logits = output[1];
  
  // vpnet.cc requirement: Value tensor must be 2D [batch_size, 1]
  TORCH_CHECK(value.dim() == 2, "Value must be 2D [batch_size, 1], got ", value.dim(), "D");
  TORCH_CHECK(value.size(1) == 1, "Value must have 1 channel, got ", value.size(1));
  // vpnet.cc requirement: Policy tensor must be 2D [batch_size, num_actions]
  TORCH_CHECK(policy_logits.dim() == 2, "Policy must be 2D [batch_size, num_actions], got ", policy_logits.dim(), "D");

  // Handle softmax for different model types
  torch::Tensor policy_probs;
  if (this->nn_model_ == "gateau") {
    // Gateau model already applies softmax in head layer, use probabilities directly
    policy_probs = policy_logits;
  } else {
    // ResNet/MLP return logits, apply softmax externally
    policy_probs = torch::softmax(policy_logits, -1);
  }
  
  // Force normalization to ensure sum of probabilities equals 1
  torch::Tensor sum_probs = policy_probs.sum(-1, true);
  policy_probs = policy_probs / sum_probs.clamp_min(1e-8);
  
  return {value, policy_probs};
}

std::vector<torch::Tensor> ModelImpl::losses(torch::Tensor inputs,
                                             torch::Tensor masks,
                                             torch::Tensor policy_targets,
                                             torch::Tensor value_targets) {
  std::vector<torch::Tensor> output = this->forward_(inputs, masks);

  torch::Tensor value_predictions = output[0];
  torch::Tensor policy_predictions = output[1];

  // Adjust shape for loss calculation (2D -> 1D)
  if (value_predictions.dim() == 2) {
    value_predictions = value_predictions.squeeze(-1);
  }
  if (value_targets.dim() == 2) {
    value_targets = value_targets.squeeze(-1);
  }
  
  // Policy loss (cross-entropy).
  torch::Tensor policy_loss = torch::sum(
      -policy_targets * torch::log_softmax(policy_predictions, 1), -1);
  policy_loss = torch::mean(policy_loss);

  // Value loss (mean-squared error).
  torch::nn::MSELoss mse_loss;
  torch::Tensor value_loss = mse_loss(value_predictions, value_targets);

  // L2 regularization loss (weights only).
  torch::Tensor l2_regularization_loss = torch::full(
      {1, 1}, 0, torch::TensorOptions().dtype(torch::kFloat32).device(device_));
  for (auto& named_parameter : this->named_parameters()) {
    // named_parameter is essentially a key-value pair:
    //   {key, value} == {std::string name, torch::Tensor parameter}
    std::string parameter_name = named_parameter.key();

    // Do not include bias' in the loss.
    if (absl::StrContains(parameter_name, "bias")) {
      continue;
    }

    // Copy TensorFlow's l2_loss function.
    // https://www.tensorflow.org/api_docs/python/tf/nn/l2_loss
    l2_regularization_loss +=
        weight_decay_ * torch::sum(torch::square(named_parameter.value())) / 2;
  }

  return {policy_loss, value_loss, l2_regularization_loss};
}

std::vector<torch::Tensor> ModelImpl::forward_(torch::Tensor x, torch::Tensor mask) {
  std::vector<torch::Tensor> output;
  if (this->nn_model_ == "resnet") {
    for (int i = 0; i < num_torso_blocks_ + 2; i++) {
      if (i == 0) {
        x = layers_[i]->as<ResInputBlock>()->forward(x);
      } else if (i >= num_torso_blocks_ + 1) {
        output = layers_[i]->as<ResOutputBlock>()->forward(x, mask);
      } else {
        x = layers_[i]->as<ResTorsoBlock>()->forward(x);
      }
    }
  } else if (this->nn_model_ == "mlp") {
    for (int i = 0; i < num_torso_blocks_ + 1; i++) {
      x = layers_[i]->as<MLPBlock>()->forward(x);
    }
    output = layers_[num_torso_blocks_ + 1]->as<MLPOutputBlockImpl>()
        ->forward(x, mask);
  } else if (this->nn_model_ == "gateau") {
    // Restore original GNN forward logic and keep output shape compliance
    int64_t batch_size = x.size(0);
    TORCH_CHECK(batch_size == 1, "gateau only supports batch size 1, got ", batch_size);

    torch::Tensor obs_flat = x.flatten();

	// 1. Parse metadata section
	int total_boards   = (int)obs_flat[0].item<float>();
	int num_operable   = (int)obs_flat[1].item<float>();
	int num_edges      = (int)obs_flat[2].item<float>();
	int current_player = (int)obs_flat[3].item<float>();

	// Add runtime boundary checks (recommended)
	TORCH_CHECK(total_boards >= 0 && total_boards <= kMaxRuntimeBoards, 
				"Total boards out of range: ", total_boards);
	TORCH_CHECK(num_operable >= 0 && num_operable <= kMaxOperableBoards, 
				"Num operable boards out of range: ", num_operable);
	TORCH_CHECK(num_edges >= 0 && num_edges <= kMaxRuntimeEdges, 
				"Num edges out of range: ", num_edges);
	TORCH_CHECK(current_player == 0 || current_player == 1, 
				"Invalid current player: ", current_player);

    // 2. Calculate memory offsets for observation sections
    int64_t oper_offset    = 4;
    int64_t mask_offset    = oper_offset + 2 * kMaxOperableBoards;  // 2 floats per operable board (index + prior)
    int64_t boards_offset  = mask_offset + kFixedPolicyDim;
    int64_t edges_offset    = boards_offset + total_boards * kTotalBoardDataSize;

    // 3. Parse operable board indices and selection priors
    torch::Tensor operable_board_indices = obs_flat.index({
      torch::indexing::Slice(oper_offset, oper_offset + 2 * num_operable, 2)  // Step 2: extract indices
    }).to(torch::kLong);
    
    torch::Tensor operable_board_priors = obs_flat.index({
      torch::indexing::Slice(oper_offset + 1, oper_offset + 1 + 2 * num_operable, 2)  // Step 2: extract priors
    }).to(torch::kFloat32);

    // 4. Parse legal move prior mask
    torch::Tensor legal_move_mask = obs_flat.index({
      torch::indexing::Slice(mask_offset, mask_offset + kFixedPolicyDim)
    });
    legal_move_mask = legal_move_mask.view({kMaxOperableBoards, kMaxMovesPerBoard}).to(torch::kFloat32);

    // 5. Parse board data (u, v coordinates + bitboard)
    std::vector<torch::Tensor> all_boards;
    std::vector<torch::Tensor> all_coords;  // Store (u, v) coordinates for each board
    
    for (int i = 0; i < total_boards; ++i) {
      int64_t board_start = boards_offset + i * kTotalBoardDataSize;
      
      // Read u and v coordinates
      float u = obs_flat[board_start + 0].item<float>();
      float v = obs_flat[board_start + 1].item<float>();
      all_coords.push_back(torch::tensor({u, v}, torch::kFloat32).to(device_));
      
      // Read bitboard data (skip first 2 floats for coordinates)
      torch::Tensor board = obs_flat.index({
        torch::indexing::Slice(board_start + 2, board_start + kTotalBoardDataSize)
      }).view({kBoardInputChannels, kBoardHeight, kBoardWidth});
      all_boards.push_back(board);
    }
    
    torch::Tensor batch_board = torch::stack(all_boards, 0).to(device_);
    torch::Tensor batch_coords = torch::stack(all_coords, 0).to(device_);

    // 6. Parse edge indices
    torch::Tensor edge_index = obs_flat.index({torch::indexing::Slice(edges_offset, edges_offset + 2 * num_edges)});
    edge_index = edge_index.view({2, -1}).to(torch::kLong).to(device_);

    // 7. Board encoding: fuse u,v coordinates with bitboard features
    torch::Tensor node_features = ag_hier_encoder_->forward(batch_board, batch_coords);

    // 8. GNN graph convolution with residual connection
    node_features = node_features + ag_hier_gateau_->forward(node_features, edge_index);

	// 9. Global feature: fuse current player information (improved version)
	torch::Tensor global_feature = node_features.mean(0);
	// Create a 128-dimensional player embedding
	torch::Tensor player_embedding = torch::full({kEmbeddingDim}, 
												 static_cast<float>(current_player) * 2.0f - 1.0f,
												 torch::kFloat32).to(global_feature.device());
	// Add to global feature (maps player to [-1, 1] range for better gradient flow)
	global_feature = global_feature + player_embedding;
	global_feature = global_feature.view({1, kEmbeddingDim});

    // 10. Hierarchical output head forward pass
    auto head_output = ag_hier_output_->forward(
        node_features, global_feature, operable_board_indices, 
        operable_board_priors, legal_move_mask, num_operable);
    torch::Tensor value = head_output[0];
    torch::Tensor final_policy = head_output[1];

    // Output shape validation
    TORCH_CHECK(value.dim() == 2 && value.size(1) == 1, 
      "Value final shape must be [batch_size, 1], got ", value.sizes());
    TORCH_CHECK(final_policy.dim() == 2 && final_policy.size(1) == kFixedPolicyDim, 
      "Policy final shape must be [batch_size, ", kFixedPolicyDim, "], got ", final_policy.sizes());

    output = {value, final_policy};
  } else {
    TORCH_CHECK(false, "Unknown nn_model: ", this->nn_model_);
  }
  return output;
}

}  // namespace torch_az
}  // namespace algorithms
}  // namespace open_spiel