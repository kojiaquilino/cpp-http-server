#include <iostream>
#include "media/image.hpp"
#include "media/image_io.hpp"

int main()
{
    // testing image
    try
    {
        Image image = load_image("test/IMG_5618.jpeg");
        std::cout << "Width: " << image.width << '\n' 
                  << "Height: " << image.height << '\n' 
                  << "Number of pixels: " << image.pixels.size();

        save_png(image, "outputs/test.png");
    }
    catch (const std::exception &error)
    {
        std::cout << error.what();
        return 1;
    }
}