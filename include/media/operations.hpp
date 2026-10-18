#pragma once

#include "media/image.hpp"

void threshold(Image& image, std::uint8_t cutoff);
Image to_grayscale(const Image& image);