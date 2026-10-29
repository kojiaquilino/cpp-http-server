#include "media/image_io.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "stb_image_write.h"

Image load_image(const std::string& path) {
  int width{}, height{}, channels{};

  stbi_uc* raw_pixels{stbi_load(path.c_str(), &width, &height, &channels, 0)};

  // Wrap pixel buffer in unique ptr
  std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decoded_pixels(
      raw_pixels, &stbi_image_free);

  // No data loaded from image -> throw error
  if (!decoded_pixels) {
    throw std::runtime_error("Failed to load image: " + path);
  }

  // Malformed channels -> throw error
  if (channels == 2 || channels == 4) {
    throw std::runtime_error("Images with alpha channels are not supported: " +
                             path);
  }

  // Malformed dimensions -> throw error
  if (width <= 0 || height <= 0) {
    throw std::runtime_error("Image dimensions must be positive values: " +
                             path);
  }

  const std::size_t image_width{static_cast<std::size_t>(width)};
  const std::size_t image_height{static_cast<std::size_t>(height)};
  const auto maximum = std::numeric_limits<std::size_t>::max();

  if (image_width > maximum / height) {
    throw std::overflow_error("Pixel count overflow");
  }

  const std::size_t pixel_count{image_width * image_height};

  if (pixel_count > maximum / channels) {
    throw std::overflow_error("Byte count overflow");
  }

  const std::size_t byte_count{pixel_count * channels};

  // Starts copying bytes from first pointer until last pointer (exclusive)
  std::vector<std::uint8_t> pixels(
      decoded_pixels.get(),  // Type stbi_uc* (unsigned char, 0-255)
      decoded_pixels.get() + byte_count);

  // Construct the resulting image to be returned
  return Image(image_width, image_height,
               (channels == 1) ? PixelFormat::Gray8 : PixelFormat::RGB8,
               pixels);
}

void save_png(const Image& image, const std::string& path) {
  // Validate image before saving -> throws an error is invalid members
  image.validate();

  // Checking stride size capacity (row of an array) within an int
  // only ints are accepted by stbi_write_png()
  const auto maximum_int =
      static_cast<std::size_t>(std::numeric_limits<int>::max());
  const int bytes_per_pixel{image.format == PixelFormat::RGB8 ? 3 : 1};

  if (image.width > maximum_int / bytes_per_pixel) {
    throw std::overflow_error("Row stride exceeds int capacity");
  }

  if (image.height > maximum_int) {
    throw std::overflow_error("Height exceeds int capacity");
  }

  const std::size_t stride{image.width * bytes_per_pixel};

  // Save image as png to filepath
  int err{stbi_write_png(path.c_str(), static_cast<int>(image.width),
                         static_cast<int>(image.height), bytes_per_pixel,
                         image.pixels.data(), stride)};

  // stbi_write_png returns 0 on error
  if (err == 0) {
    throw std::runtime_error("Failed to save image as a png to " + path);
  }
}