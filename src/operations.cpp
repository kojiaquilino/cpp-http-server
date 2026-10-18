#include "media/operations.hpp"
#include "media/image.hpp"

#include <cstdint>
#include <stdexcept>

/// Converts a Gray8 image to black and white in place.
/// @throws std::invalid_argument for invalid images or non-Gray8 formats.
/// @throws std::overflow_error if image size calculations overflow.
void threshold(Image &image, std::uint8_t cutoff) {
    image.validate();

    if(image.format != PixelFormat::Gray8) {
        throw std::invalid_argument("Grayscale thresholding only supported grayscale images.");
    }

    for(auto &pixel : image.pixels) {
        pixel = (pixel < cutoff) ? 0 : 255;
    }
}