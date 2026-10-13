#include <iostream>
#include <cstddef> // provides size_t
#include <cstdint> // provides uint8_t
#include <vector> // provides vector


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
};


int main() {
    // testing image
    Image image {
        3, 
        2, 
        PixelFormat::Gray8, 
        {0, 127, 128, 129, 200, 255}
    };

    for(int y = 0; y < image.height; y++) {
        for(int x = 0; x < image.width; x++) {
            int index = y * image.height + x;
            std::cout << static_cast<int>(image.pixels[index]) << ' ';
        }

        std::cout << '\n';
    }
}