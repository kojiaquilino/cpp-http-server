#include <iostream>
#include <cstddef> // provides size_t
#include <cstdint> // provides uint8_t
#include <vector> // provides vector
#include <stdexcept>
#include <limits>

// Forward declarations
struct Image;
void validate_image(const Image& image);



// Image definitions //

// Define pixel representation
enum class PixelFormat {
    Gray8, // 8 bits per channel (0-255)
    RGB8
};

//  Define Image representation
struct Image {
    std::size_t width;
    std::size_t height;
    PixelFormat format;
    std::vector<std::uint8_t> pixels;

    // Constructor
    Image(std::size_t w,
          std::size_t h,
          PixelFormat f,
          const std::vector<std::uint8_t>& data)
        : width(w), height(h), format(f), pixels(data)
    {
        validate_image(*this);    
    } 
};


// Helper functions
void validate_image(const Image& image) {
    
    std::size_t bytes_per_pixel;
    const auto maximum = std::numeric_limits<std::size_t>::max();

    // Validate dimensions
    
    if (image.width == 0 || image.height == 0) {
        throw std::invalid_argument("Dimensions must be greater than zero");
    }
    // Validate format
    switch (image.format) {
        case PixelFormat::Gray8:
            bytes_per_pixel = 1;
            break;
            
        case PixelFormat::RGB8:
            bytes_per_pixel = 3;
            break;

        default:
            throw std::invalid_argument("Unsupported pixel format");
    }

    // Checking for value overflow

    if (image.width > maximum / image.height) {
        throw std::overflow_error("Pixel count overflow");
    } 
    
    const std::size_t pixel_count = image.width * image.height;

    if (pixel_count > maximum / bytes_per_pixel) {
        throw std::overflow_error("Byte count overflow");
    }

    const std::size_t required_size = pixel_count * bytes_per_pixel; 

    if (image.pixels.size() != required_size) {
        throw std::invalid_argument("Pixel storage size does not match dimensions");
    }   
}



int main() {
    // testing image
    try {
        Image image {
            3, 
            2, 
            PixelFormat::Gray8, 
            {0, 127, 128, 129, 200, 255}
        };

        for(std::size_t y = 0; y < image.height; y++) {
            for(std::size_t x = 0; x < image.width; x++) {
                std::size_t index = y * image.width + x;
                std::cout << static_cast<int>(image.pixels[index]) << ' ';
            }

            std::cout << '\n';
        }
    } catch(const std::exception& error) {
        std::cout << error.what();
        return 1;
    }
}