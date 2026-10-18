#include "media/image.hpp"
#include "media/image_io.hpp"
#include "media/operations.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>



// Helper vars/functions for CLI parsing
namespace {
    constexpr std::string_view usage = R"(
Usage:
    media_demo <input-image> <output.png>
    media_demo <input-image> <output.png> --threshold <cutoff>
    media_demo --help
    
Copies an image to PNG, optionally applying grayscale thresholding.
    --threshold <cutoff>  Black/white cutoff from 0 to 255; requires grayscale input.
    --help, -h            Show this help message.

Supports opaque PNG/JPEG input. The output directory must already exist.)";

    enum class Operation {
        Threshold,
    };

    // Convert cli argument to their corresponding operations.
    // Throws an error if the argument is an invalid operation.

    /// Convert a CLI argument to their corresponding operations.
    /// @throws std::invalid_argument if the argument is an invalid operation.
    Operation parse_operation(std::string_view operation) {
        // Enumerate conversion to corresponding enum values
        if(operation == "--threshold") return Operation::Threshold;

        // Throw error if provided an invalid operation
        throw std::invalid_argument(
            "Unsupported argument: " + static_cast<std::string>(operation)
        );
    }

    //// Operation-specific wrapper functions ////

    /// Parse the cutoff positional argument for Operation::Threshold,
    /// and call threshold from operations.cpp, modifying the image directly.
    /// @throws std::invalid_argument if the cutoff is not an integer, or is outside [0, 255].
    void call_threshold(
        Image& image,
        char**& current_pos,
        char** end
    ) {
        if(current_pos + 1 == end) {
            throw std::invalid_argument("\"--threshold\" requires a grayscale cutoff parameter from 0-255");
        }

        std::string text{*++current_pos};
        std::size_t parsed{0};
        int value{std::stoi(text, &parsed)};

        // Ensure that the whole string was
        // consumed when parsing the int
        if(parsed != text.size() || value < 0 || value > 255) {
            throw std::invalid_argument("Cutoff must be an integer from 0-255");
        }

        auto cutoff = static_cast<std::uint8_t>(value);
        threshold(image, cutoff);
    }

}

/**
 * argv[0]: executable name
 * argv[1]: input filepath
 * argv[2]: output filepath
 */
int main(int argc, char* argv[]) {
    try {
        switch(argc) {

            // Malformed arguments -> return error code
            case 0:
            case 1: {
                std::cerr << usage << '\n';
                return 1;
            }

            // media_demo (--help || -h)
            case 2: {
                const std::string argument{argv[1]};

                if(argument == "-h" || argument == "--help") {
                    std::cout << usage << '\n';
                    return 0;
                } else {
                    std::cerr << usage << '\n';
                    return 1;
                }
                break;
            }

            // media_demo input_filepath output_filepath
            case 3: {
                const std::string input_path{argv[1]};
                const std::string output_path{argv[2]};
                Image image{load_image(input_path)};

                std::cout << ("Filepath: " + input_path + '\n')
                        << "Width: " << image.width << '\n'
                        << "Height: " << image.height << '\n'
                        << "Number of bytes: " << image.pixels.size() << '\n'
                        << "Format: " << (image.format == PixelFormat::RGB8 ? "RGB8" : "Gray8") << '\n';

                save_png(image, output_path);
                std::cout << ("Saved to " + output_path + ".\n");
                break;
            }

            // 1+ operations invoked, perform them sequentially,
            // then save to output_filepath
            default: {
                const std::string input_path{argv[1]};
                const std::string output_path{argv[2]};

                Image image {load_image(input_path)};

                // Start from fourth positional argument
                // Skip program name (0), and input/output (1/2) filepaths
                char** current_pos = argv + 3;
                char** end = argv + argc;

                while(current_pos < end) {
                    std::string_view argument{*current_pos};
                    Operation operation{parse_operation(argument)};

                    switch(operation) {
                    
                        // Parses 1 positional argument
                        case Operation::Threshold: {
                            call_threshold(image, current_pos, end);                            
                            break;
                        }

                    }

                    // Move ptr to next char* in argv
                    current_pos++;
                }
                    
                save_png(image, output_path);
                std::cout << ("Saved to " + output_path + ".\n");
                break;
            }
        }
    } catch (const std::exception& error) {
        std::cout << usage << "\n\n";
        std::cerr << ("Error: " + static_cast<std::string>(error.what())) << "\n\n";
        return 1;
    }
}