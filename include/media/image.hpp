#pragma once

#include <cstddef>  // provides size_t
#include <cstdint>  // provides uint8_t
#include <vector>   // provides vector

// Pixel representation
enum class PixelFormat {
  Gray8,  // 8 bits per channel (0-255)
  RGB8
};

// Image representation
struct Image {
  std::size_t width;
  std::size_t height;
  PixelFormat format;
  std::vector<std::uint8_t> pixels;

  // Constructor declaration
  Image(std::size_t w, std::size_t h, PixelFormat f,
        const std::vector<std::uint8_t>& data);

  void validate() const;
};