#pragma once

#include <string>

#include "media/image.hpp"

// Load an image from a specified filepath.
Image load_image(const std::string& path);

// Save an image as a png to a specified filepath.
void save_png(const Image& image, const std::string& path);