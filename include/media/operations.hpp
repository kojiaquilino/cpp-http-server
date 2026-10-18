#pragma once

#include "media/image.hpp"

#include <vector>

void threshold(Image& image, std::uint8_t cutoff);
Image to_grayscale(const Image& image);
Image box_blur(const Image& image);
std::vector<double> gaussian_kernel(int radius, double sigma);