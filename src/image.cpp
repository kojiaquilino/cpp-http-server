#include "media/image.hpp"

#include <limits>
#include <stdexcept>

Image::Image(std::size_t w, std::size_t h, PixelFormat f,
             const std::vector<std::uint8_t>& data)
    : width(w), height(h), format(f), pixels(data) {
  validate();
}

void Image::validate() const {
  std::size_t bytes_per_pixel{};
  const auto maximum = std::numeric_limits<std::size_t>::max();

  //// Validate dimensions and format ////

  if (width == 0 || height == 0) {
    throw std::invalid_argument("Dimensions must be greater than zero");
  }

  // Validate format
  switch (format) {
    case PixelFormat::Gray8:
      bytes_per_pixel = 1;
      break;

    case PixelFormat::RGB8:
      bytes_per_pixel = 3;
      break;

    default:
      throw std::invalid_argument("Unsupported pixel format");
  }

  //// Check for value overflow ////

  if (width > maximum / height) {
    throw std::overflow_error("Pixel count overflow");
  }

  const std::size_t pixel_count{width * height};

  if (pixel_count > maximum / bytes_per_pixel) {
    throw std::overflow_error("Byte count overflow");
  }

  const std::size_t required_size{pixel_count * bytes_per_pixel};

  if (pixels.size() != required_size) {
    throw std::invalid_argument("Pixel storage size does not match dimensions");
  }
}