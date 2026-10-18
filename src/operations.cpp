#include "media/operations.hpp"
#include "media/image.hpp"

#include <cstdint>
#include <stdexcept>

/// Converts a Gray8 image to black and white in place.
/// @throws std::invalid_argument for invalid images or non-Gray8 formats.
/// @throws std::overflow_error if image size calculations overflow.
void threshold(Image& image, std::uint8_t cutoff) {
    image.validate();

    if(image.format != PixelFormat::Gray8) {
        throw std::invalid_argument("Grayscale thresholding only supported grayscale images.");
    }

    for(auto &pixel : image.pixels) {
        pixel = (pixel < cutoff) ? 0 : 255;
    }
}

/// Converts an image to its grayscale representation.
/// @throws std::invalid_argument exception if the input image is already in a grayscale format.
/// @returns a new image since the format and storage representation may change.
Image to_grayscale(const Image& image) {
    image.validate();

    if(image.format == PixelFormat::Gray8) {
        throw std::invalid_argument("Image is already in a grayscale format");
    }

    std::size_t pixel_index{0}; 
    std::vector<std::uint8_t> grayscale_pixels(image.width * image.height);
    Image grayscale_image{
        image.width,
        image.height,
        PixelFormat::Gray8,
        grayscale_pixels
    };

    for(int i = 0; i < image.pixels.size(); i += 3) {
        const std::uint8_t r{image.pixels.at(i)};
        const std::uint8_t g{image.pixels.at(i + 1)};
        const std::uint8_t b{image.pixels.at(i + 2)};

        // approximate brightness value calcualted by rgb values
        const std::uint32_t luma{static_cast<std::uint32_t>((299 * r + 587 * g + 114 * b + 500) / 1000)};
        grayscale_image.pixels.at(pixel_index) = luma;
        pixel_index++;
    }

    return grayscale_image;
}