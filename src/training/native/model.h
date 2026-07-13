#ifndef BOMBER_NATIVE_ALphAZERO_MODEL_H
#define BOMBER_NATIVE_ALphAZERO_MODEL_H

#include <torch/torch.h>

#include <cstdint>
#include <tuple>

namespace bomber::az {

struct ResidualBlockImpl final : torch::nn::Module {
    explicit ResidualBlockImpl(int64_t channels);
    torch::Tensor forward(torch::Tensor input);

    torch::nn::Conv2d conv1{nullptr};
    torch::nn::BatchNorm2d norm1{nullptr};
    torch::nn::Conv2d conv2{nullptr};
    torch::nn::BatchNorm2d norm2{nullptr};
};
TORCH_MODULE(ResidualBlock);

struct PolicyValueNetImpl final : torch::nn::Module {
    PolicyValueNetImpl(int64_t input_channels, int64_t channels, int64_t blocks,
                       int64_t actions, int64_t board_size);

    std::tuple<torch::Tensor, torch::Tensor> forward(torch::Tensor input);
    int64_t parameter_count() const;

    int64_t actions_;
    int64_t board_size_;
    torch::nn::Conv2d stem{nullptr};
    torch::nn::BatchNorm2d stem_norm{nullptr};
    torch::nn::ModuleList residuals;
    torch::nn::Conv2d policy_conv{nullptr};
    torch::nn::BatchNorm2d policy_norm{nullptr};
    torch::nn::Linear policy_linear{nullptr};
    torch::nn::Conv2d value_conv{nullptr};
    torch::nn::BatchNorm2d value_norm{nullptr};
    torch::nn::Linear value_hidden{nullptr};
    torch::nn::Linear value_output{nullptr};
};
TORCH_MODULE(PolicyValueNet);

}  // namespace bomber::az

#endif
