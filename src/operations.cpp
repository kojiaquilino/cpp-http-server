#include "media/operations.hpp"
#include "media/image.hpp"

#include <cstdint>
#include <stdexcept>

/// @brief Converts a Gray8 image to black and white in place.
/// @throws std::invalid_argument for invalid images or non-Gray8 formats.
/// @throws std::overflow_error if image size calculations overflow.
/// @param image provides a reference to the image.
/// @param cutoff specifies the grayscale cutoff value.
void threshold(Image &image, std::uint8_t cutoff) {
    image.validate();

    if(image.format != PixelFormat::Gray8) {
        throw std::invalid_argument("Grayscale threshold only supported grayscale images.");
    }

    for(auto &pixel : image.pixels) {
        pixel = (pixel < cutoff) ? 0 : 255;
    }
}