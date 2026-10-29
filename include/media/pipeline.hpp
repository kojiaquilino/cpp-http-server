#pragma once

#include "media/image.hpp"

#include <cstdint>
#include <variant>
#include <vector>

struct GrayscaleOperation {};
struct ThresholdOperation { std::uint8_t cutoff; };
struct BoxBlurOperation {};
struct GaussianBlurOperation { int radius; double sigma; };
struct SobelOperation {};

using PipelineOperation = std::variant<
    GrayscaleOperation,
    ThresholdOperation,
    BoxBlurOperation,
    GaussianBlurOperation,
    SobelOperation
>;

Image apply_pipeline(const Image& image, const std::vector<PipelineOperation>& operations);
