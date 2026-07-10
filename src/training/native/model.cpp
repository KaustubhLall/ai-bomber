#include "training/native/model.h"

namespace bomber::az {

namespace {

torch::nn::Conv2dOptions conv_options(int64_t input, int64_t output,
                                      int64_t kernel, int64_t padding) {
    return torch::nn::Conv2dOptions(input, output, kernel)
        .padding(padding)
        .bias(false);
}

}  // namespace

ResidualBlockImpl::ResidualBlockImpl(int64_t channels)
    : conv1(register_module("conv1", torch::nn::Conv2d(
          conv_options(channels, channels, 3, 1)))),
      norm1(register_module("norm1", torch::nn::BatchNorm2d(channels))),
      conv2(register_module("conv2", torch::nn::Conv2d(
          conv_options(channels, channels, 3, 1)))),
      norm2(register_module("norm2", torch::nn::BatchNorm2d(channels))) {}

torch::Tensor ResidualBlockImpl::forward(torch::Tensor input) {
    auto residual = input;
    input = torch::relu(norm1(conv1(input)));
    input = norm2(conv2(input));
    return torch::relu(input + residual);
}

PolicyValueNetImpl::PolicyValueNetImpl(int64_t input_channels, int64_t channels,
                                       int64_t blocks, int64_t actions,
                                       int64_t board_size)
    : actions_(actions),
      board_size_(board_size),
      stem(register_module("stem", torch::nn::Conv2d(
          conv_options(input_channels, channels, 3, 1)))),
      stem_norm(register_module("stem_norm", torch::nn::BatchNorm2d(channels))),
      residuals(register_module("residuals", torch::nn::ModuleList())),
      policy_conv(register_module("policy_conv", torch::nn::Conv2d(
          conv_options(channels, 32, 1, 0)))),
      policy_norm(register_module("policy_norm", torch::nn::BatchNorm2d(32))),
      policy_linear(register_module("policy_linear", torch::nn::Linear(
          32 * board_size * board_size, actions))),
      value_conv(register_module("value_conv", torch::nn::Conv2d(
          conv_options(channels, 8, 1, 0)))),
      value_norm(register_module("value_norm", torch::nn::BatchNorm2d(8))),
      value_hidden(register_module("value_hidden", torch::nn::Linear(
          8 * board_size * board_size, 256))),
      value_output(register_module("value_output", torch::nn::Linear(256, 1))) {
    for (int64_t index = 0; index < blocks; ++index) {
        residuals->push_back(ResidualBlock(channels));
    }
}

std::tuple<torch::Tensor, torch::Tensor> PolicyValueNetImpl::forward(torch::Tensor input) {
    input = torch::relu(stem_norm(stem(input)));
    for (auto& module : *residuals) {
        input = module->as<ResidualBlockImpl>()->forward(input);
    }
    auto policy = torch::relu(policy_norm(policy_conv(input)));
    policy = policy.flatten(1);
    policy = policy_linear(policy);

    auto value = torch::relu(value_norm(value_conv(input)));
    value = value.flatten(1);
    value = torch::relu(value_hidden(value));
    value = torch::tanh(value_output(value)).squeeze(1);
    return {policy, value};
}

int64_t PolicyValueNetImpl::parameter_count() const {
    int64_t result = 0;
    for (const auto& parameter : parameters()) result += parameter.numel();
    return result;
}

}  // namespace bomber::az
