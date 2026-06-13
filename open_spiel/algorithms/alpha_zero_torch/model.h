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

#ifndef OPEN_SPIEL_ALGORITHMS_ALPHA_ZERO_TORCH_MODEL_H_
#define OPEN_SPIEL_ALGORITHMS_ALPHA_ZERO_TORCH_MODEL_H_

#include <torch/torch.h>

#include <iostream>
#include <string>
#include <vector>

namespace open_spiel {
namespace algorithms {
namespace torch_az {

// ====================  Fixed Hyperparameters ====================
constexpr int kBoardInputChannels = 12;    // Bitboard input channels
constexpr int kBoardHeight = 8;
constexpr int kBoardWidth = 8;
constexpr int kMaxMovesPerBoard = 256;     // Maximum legal moves for a single board
constexpr int kMaxOperableBoards = 128;    // Maximum number of operable game boards
constexpr int kFixedPolicyDim = kMaxOperableBoards * kMaxMovesPerBoard; // Fixed policy output dimension (32768)
constexpr int kEmbeddingDim = 128;         // Node feature embedding dimension

constexpr int kMaxRuntimeBoards = 1 << 9; // Maximum active boards during runtime
constexpr int kMaxRuntimeEdges = kMaxRuntimeBoards * 2;
constexpr int kTotalBoardDataSize = 2 + kBoardInputChannels * kBoardHeight * kBoardWidth;
	
// ========================================================================

struct ResInputBlockConfig {
  int input_channels;
  int input_height;
  int input_width;
  int filters;
  int kernel_size;
  int padding;
};

struct ResTorsoBlockConfig {
  int input_channels;
  int filters;
  int kernel_size;
  int padding;
  int layer;
};

struct ResOutputBlockConfig {
  int input_channels;
  int value_filters;
  int policy_filters;
  int kernel_size;
  int padding;
  int value_linear_in_features;
  int value_linear_out_features;
  int policy_linear_in_features;
  int policy_linear_out_features;
  int value_observation_size;
  int policy_observation_size;
};

// Information for the model. This should be enough for any type of model
// (residual, convultional, or MLP). It needs to be saved/loaded to/from
// a file so the input and output stream operators are overload.
struct ModelConfig {
  std::vector<int> observation_tensor_shape;
  int number_of_actions;
  int nn_depth;
  int nn_width;
  double learning_rate;
  double weight_decay;
  std::string nn_model = "resnet";
};
std::istream& operator>>(std::istream& stream, ModelConfig& config);
std::ostream& operator<<(std::ostream& stream, const ModelConfig& config);

// A block of the residual model's network that handles the input. It consists
// of one convolutional layer (CONV) and one batch normalization (BN) layer, and
// the output is passed through a rectified linear unit function (RELU).
//
// Illustration:
//   [Input Tensor] --> CONV --> BN --> RELU
//
// There is only one input block per model.
class ResInputBlockImpl : public torch::nn::Module {
 public:
  ResInputBlockImpl(const ResInputBlockConfig& config);
  torch::Tensor forward(torch::Tensor x);

 private:
  int channels_;
  int height_;
  int width_;
  torch::nn::Conv2d conv_;
  torch::nn::BatchNorm2d batch_norm_;
};
TORCH_MODULE(ResInputBlock);

// A block of the residual model's network that makes up the 'torso'. It
// consists of two convolutional layers (CONV) and two batchnormalization layers
// (BN). The activation function is rectified linear unit (RELU). The input to
// the layer is added to the output before the final activation function.
//
// Illustration:
//   [Input Tensor] --> CONV --> BN --> RELU --> CONV --> BN --> + --> RELU
//          \___________________________________________________/
//
// Unlike the input and output blocks, one can specify how many of these torso
// blocks they want in their model.
class ResTorsoBlockImpl : public torch::nn::Module {
 public:
  ResTorsoBlockImpl(const ResTorsoBlockConfig& config, int layer);
  torch::Tensor forward(torch::Tensor x);

 private:
  torch::nn::Conv2d conv1_;
  torch::nn::Conv2d conv2_;
  torch::nn::BatchNorm2d batch_norm1_;
  torch::nn::BatchNorm2d batch_norm2_;
};
TORCH_MODULE(ResTorsoBlock);

// A block of the residual model's network that creates the output. It consists
// of a value and policy head. The value head takes the input through one
// convoluational layer (CONV), one batch normalization layers (BN), and two
// linear layers (LIN). The output activation function is tanh (TANH), the
// rectified linear activation function (RELU) is within. The policy head
// consists of one convolutional layer, batch normalization layer, and linear
// layer. There is no softmax activation function in this layer. The softmax
// on the output is applied in the forward function of the residual model.
// This design was chosen because the loss function of the residual model
// requires the policy logits, not the policy distribution. By providing the
// policy logits as output, the residual model can either apply the softmax
// activation function, or calculate the loss using Torch's log softmax
// function.
//
// Illustration:
//                    --> CONV --> BN --> RELU --> LIN --> RELU --> LIN --> TANH
//   [Input Tensor] --
//                    --> CONV --> BN --> RELU --> LIN (no SOFTMAX here)
//
// There is only one output block per model.
class ResOutputBlockImpl : public torch::nn::Module {
 public:
  ResOutputBlockImpl(const ResOutputBlockConfig& config);
  std::vector<torch::Tensor> forward(torch::Tensor x, torch::Tensor mask);

 private:
  torch::nn::Conv2d value_conv_;
  torch::nn::BatchNorm2d value_batch_norm_;
  torch::nn::Linear value_linear1_;
  torch::nn::Linear value_linear2_;
  int value_observation_size_;
  torch::nn::Conv2d policy_conv_;
  torch::nn::BatchNorm2d policy_batch_norm_;
  torch::nn::Linear policy_linear_;
  int policy_observation_size_;
};
TORCH_MODULE(ResOutputBlock);

// A dense block with ReLU activation.
class MLPBlockImpl : public torch::nn::Module {
 public:
  MLPBlockImpl(const int in_features, const int out_features);
  torch::Tensor forward(torch::Tensor x);

 private:
  torch::nn::Linear linear_;
};
TORCH_MODULE(MLPBlock);

class MLPOutputBlockImpl : public torch::nn::Module {
 public:
  MLPOutputBlockImpl(const int nn_width, const int policy_linear_out_features);
  std::vector<torch::Tensor> forward(torch::Tensor x, torch::Tensor mask);

 private:
  torch::nn::Linear value_linear1_;
  torch::nn::Linear value_linear2_;
  torch::nn::Linear policy_linear1_;
  torch::nn::Linear policy_linear2_;
};
TORCH_MODULE(MLPOutputBlock);

// ====================  AlphaGateau Hierarchical Modules ====================
// Board Node Encoder: Encode raw board input to 128-dimensional node embedding
class AGHBoardEncoderImpl : public torch::nn::Module {
 public:
  explicit AGHBoardEncoderImpl(int embedding_dim);
  torch::Tensor forward(torch::Tensor x, torch::Tensor coords);
 private:
  torch::nn::Conv2d conv1_ = nullptr;
  torch::nn::Conv2d conv2_ = nullptr;
  torch::nn::ReLU relu_ = nullptr;
  torch::nn::Linear coord_proj_ = nullptr; // Coordinate projection layer
  int embedding_dim_;
};
TORCH_MODULE(AGHBoardEncoder);

// GATEAU Graph Attention Layer: Optimized for spatiotemporal graph with up to 65536 nodes
class AGHGATEAUImpl : public torch::nn::Module {
 public:
  explicit AGHGATEAUImpl(int embedding_dim);
  torch::Tensor forward(torch::Tensor nodes, torch::Tensor edge_index);
 private:
  torch::nn::Linear edge_lin_ = nullptr;
  torch::nn::Linear attn_lin_ = nullptr;
  torch::nn::LeakyReLU leaky_relu_ = nullptr;
  int embedding_dim_;
};
TORCH_MODULE(AGHGATEAU);

// Hierarchical Output Head: Fixed policy dimension (32768), decoupled from actual board count
class AGHHierarchicalHeadImpl : public torch::nn::Module {
 public:
  AGHHierarchicalHeadImpl(int embedding_dim);
  std::vector<torch::Tensor> forward(
    torch::Tensor all_node_features,
    torch::Tensor global_feature,
    torch::Tensor operable_board_indices,  // Indices of operable boards in full node list [kMaxOperableBoards]
    torch::Tensor operable_board_priors,   // Board selection prior probabilities [num_operable]
    torch::Tensor legal_move_mask,        // Legal move mask [kMaxOperableBoards, kMaxMovesPerBoard]
    int num_operable_boards               // Current actual number of operable boards
  );
 private:
  torch::nn::MultiheadAttention cross_board_attn_ = nullptr; // Cross-board multi-head attention
  torch::nn::Linear value_head_ = nullptr;
  torch::nn::Linear board_selector_head_ = nullptr;
  torch::nn::Linear move_selector_head_ = nullptr;
};
TORCH_MODULE(AGHHierarchicalHead);
// ================================================================================

// The model class that interacts with the VPNet. The ResInputBlock,
// ResTorsoBlock, and ResOutputBlock are not to be used by the VPNet directly.
class ModelImpl : public torch::nn::Module {
 public:
  ModelImpl(const ModelConfig& config, const std::string& device);
  std::vector<torch::Tensor> forward(torch::Tensor x, torch::Tensor mask);
  std::vector<torch::Tensor> losses(torch::Tensor inputs, torch::Tensor masks,
                                    torch::Tensor policy_targets,
                                    torch::Tensor value_targets);

 private:
  std::vector<torch::Tensor> forward_(torch::Tensor x, torch::Tensor mask);
  torch::nn::ModuleList layers_;

  // AlphaGateau hierarchical sub-modules
  AGHBoardEncoder ag_hier_encoder_ = nullptr;
  AGHGATEAU ag_hier_gateau_ = nullptr;
  AGHHierarchicalHead ag_hier_output_ = nullptr;

  torch::Device device_;
  int num_torso_blocks_;
  double weight_decay_;
  std::string nn_model_;
};
TORCH_MODULE(Model);

}  // namespace torch_az
}  // namespace algorithms
}  // namespace open_spiel

#endif  // OPEN_SPIEL_ALGORITHMS_ALPHA_ZERO_TORCH_MODEL_H_
