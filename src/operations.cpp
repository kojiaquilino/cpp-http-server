#include "media/operations.hpp"
#include "media/image.hpp"

#include <cstdint>
#include <stdexcept>


namespace {

    /// Calulates uniform average of pixel brightness in a 3x3 neighborhood sample.
    /// Replicates nearest neighbor for out of bounds indices. 
    /// @returns a uniform average of the 3x3 neighborhood sample.
    std::uint8_t calculate_average(const Image& image, const std::size_t index) {
        std::size_t w{image.width};
        std::size_t h{image.height};
        std::size_t x{index % w};
        std::size_t y{index / w};

        std::size_t top_y{y > 0 ? y - 1 : y};
        std::size_t bottom_y{y + 1 < h ? y + 1 : y};
        int sum{};

        // evaluate per column
        for(int offset{-1}; offset <= 1; ++offset) {
            const std::size_t candidate_x{x + offset};
            const std::size_t sample_x{ 
                candidate_x < 0
                    ? 0
                    : candidate_x >= w
                        ? w - 1 
                        : candidate_x
            };

            std::size_t top_index{top_y * w + sample_x};
            std::size_t mid_index{y * w + sample_x};
            std::size_t bot_index{bottom_y * w + sample_x};

            sum += image.pixels.at(top_index);
            sum += image.pixels.at(mid_index);
            sum += image.pixels.at(bot_index);
        }

        return (sum + 4) / 9;
    }
    
}


/// Converts a Gray8 image to black and white in place.
/// @throws std::invalid_argument for invalid images or non-Gray8 formats.
/// @throws std::overflow_error if image size calculations overflow.
void threshold(Image& image, std::uint8_t cutoff) {
    image.validate();

    if(image.format != PixelFormat::Gray8) {
        throw std::invalid_argument("Grayscale thresholding only supports grayscale images.");
    }

    for(auto& pixel : image.pixels) {
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

    for(std::size_t i{0}; i < image.pixels.size(); i += 3) {
        const std::uint8_t r{image.pixels.at(i)};
        const std::uint8_t g{image.pixels.at(i + 1)};
        const std::uint8_t b{image.pixels.at(i + 2)};

        // approximate brightness value calcualted by rgb values
        const std::uint32_t luma{static_cast<std::uint32_t>((299 * r + 587 * g + 114 * b + 500) / 1000)};
        grayscale_image.pixels.at(pixel_index) = luma;
        ++pixel_index;
    }

    return grayscale_image;
}


Image box_blur(const Image& image) {
    image.validate();
    
    if(image.format != PixelFormat::Gray8) {
        throw std::invalid_argument("Box blur only supports grayscale images.");
    }

    Image blurred_image = image;

    for(std::size_t i{0}; i < image.pixels.size(); i++) {
        blurred_image.pixels.at(i) = calculate_average(image, i);
    }

    return blurred_image;
}