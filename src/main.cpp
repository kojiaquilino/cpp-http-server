#include "media/image.hpp"
#include "media/image_io.hpp"

#include <exception>
#include <iostream>
#include <string>


/**
 * argv[0]: executable name
 * argv[1]: input filepath
 * argv[2]: output filepath
 */
int main(int argc, char* argv[]) {
    try {
        /**
         * media_demo (--help || -h)
         */
        if(argc == 2) {
            const std::string argument = argv[1];

            if(argument == "-h" || argument == "--help") {
                std::cout << R"(Usage: media_demo <input-image> <output.png>
    Converts supported PNG/JPEG input to PNG.
    The output directory must already exist.)" << '\n';
                return 0;
            }
        }

        /**
         * media_demo input_filepath output_filepath
         */
        else if(argc == 3) {
            const std::string input_path = argv[1];
            const std::string output_path = argv[2];

            Image image = load_image(input_path);
            std::cout << ("Filepath: " + input_path + '\n')
                      << "Width: " << image.width << '\n'
                      << "Height: " << image.height << '\n'
                      << "Number of bytes: " << image.pixels.size() << '\n';

            save_png(image, output_path);
            std::cout << ("Saved to " + output_path + ".\n");
        }

        



    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}