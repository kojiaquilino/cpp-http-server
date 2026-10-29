#include "media/pipeline.hpp"

#include <type_traits>
#include <variant>

#include "media/image.hpp"
#include "media/operations.hpp"

Image apply_pipeline(const Image& image,
                     const std::vector<PipelineOperation>& operations) {
  image.validate();
  Image output{image};

  for (const auto& op : operations) {
    std::visit(
        [&](const auto& value) {
          using T = std::decay_t<decltype(value)>;

          if constexpr (std::is_same_v<T, GrayscaleOperation>) {
            output = to_grayscale(output);
          } else if constexpr (std::is_same_v<T, ThresholdOperation>) {
            threshold(output, value.cutoff);
          } else if constexpr (std::is_same_v<T, BoxBlurOperation>) {
            output = box_blur(output);
          } else if constexpr (std::is_same_v<T, GaussianBlurOperation>) {
            output = gaussian_blur(output, value.radius, value.sigma);
          } else if constexpr (std::is_same_v<T, SobelOperation>) {
            output = sobel(output);
          }
        },
        op);
  }

  return output;
}