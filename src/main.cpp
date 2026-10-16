#include <iostream>
#include <stdexcept>
#include <limits>
#include "media/image.hpp"

int main()
{
    // testing image
    try
    {
        Image image{
            3,
            2,
            PixelFormat::Gray8,
            {0, 127, 128, 129, 200, 255}};

        for (std::size_t y = 0; y < image.height; y++)
        {
            for (std::size_t x = 0; x < image.width; x++)
            {
                std::size_t index = y * image.width + x;
                std::cout << static_cast<int>(image.pixels[index]) << ' ';
            }

            std::cout << '\n';
        }
    }
    catch (const std::exception &error)
    {
        std::cout << error.what();
        return 1;
    }
}