#include <iostream>
#include "media/image.hpp"
#include "media/image_io.hpp"

int main()
{
    // testing image
    try
    {
        Image image = load_image("test/IMG_5618.jpeg");
        std::cout << image.width << '\n' << image.height << '\n' << image.pixels.size();
    }
    catch (const std::exception &error)
    {
        std::cout << error.what();
        return 1;
    }
}