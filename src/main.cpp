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
    media_demo <input-image> <output.png> [operations...]
    media_demo --help
    media_demo -h

Load a PNG/JPEG image, apply operations from left to right,
and save the result as PNG. Without operations, copy the image.

Operations:
    --grayscale
        Convert RGB input to grayscale.
        Rejects input that is already grayscale.

    --threshold <cutoff>
        Set pixels below cutoff to 0; all others to 255.
        Cutoff must be an integer from 0 to 255.
        Requires grayscale input.

    --box-blur
        Apply a fixed 3x3 box blur.
        Requires grayscale input.

    --gaussian-blur <radius> <sigma>
        Apply Gaussian blur using horizontal and vertical passes.
        Radius must be a nonnegative integer.
        Sigma must be positive and finite.
        Radius 0 leaves pixels unchanged.
        A radius around 3 times sigma is a useful starting point.
        Requires grayscale input.

    --sobel
        Detect edges using Sobel gradient magnitude.
        Flat areas appear black; stronger edges appear brighter.
        Magnitudes are rounded and clamped to 0-255.
        Requires grayscale input.

Options:
    --help, -h
        Show this help message.

Examples:
    media_demo input.jpg copy.png
    media_demo input.jpg gray.png --grayscale
    media_demo input.jpg binary.png --grayscale --threshold 128
    media_demo input.jpg blurred.png --grayscale --box-blur
    media_demo input.jpg gaussian.png --grayscale --gaussian-blur 3 1.0
    media_demo input.jpg edges.png --grayscale --sobel
    media_demo input.jpg smooth-edges.png --grayscale --gaussian-blur 3 1.0 --sobel

Notes:
    - For RGB input, use --grayscale before grayscale-only operations.
    - Grayscale input can be processed directly.
    - Blur and Sobel operations replicate the nearest edge pixel
    - when sampling outside the image.
    - Operations execute in the order supplied and may be repeated
    - when their input-format requirements are satisfied.
    - Output is always PNG. Use a .png extension.
    - The output directory must already exist.
)";

    enum class Operation {
        Threshold, 
        Grayscale,
        BoxBlur,
        GaussianBlur,
        Sobel
    };

    // Convert cli argument to their corresponding operations.
    // Throws an error if the argument is an invalid operation.

    /// Convert a CLI argument to their corresponding operations.
    /// @throws std::invalid_argument if the argument is an invalid operation.
    Operation parse_operation(std::string_view operation) {
        // Enumerate conversion to corresponding enum values
        if(operation == "--threshold") return Operation::Threshold;
        if(operation == "--grayscale") return Operation::Grayscale;
        if(operation == "--box-blur") return Operation::BoxBlur;
        if(operation == "--gaussian-blur") return Operation::GaussianBlur;
        if(operation == "--sobel") return Operation::Sobel;

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
        if(current_pos + 1 >= end) {
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

    /// Call the to_grayscale operation and reassign the local image.
    void call_grayscale(Image& image) {
        image = to_grayscale(image); // converts the original image to its grayscale format
    }
 
    /// Call the box_blur operation and reassign the local image. 
    void call_box_blur(Image& image) {
        image = box_blur(image); // converts original image to blurred representation
    }

    /// Parse sigma and radius positional arguments for Operation::GaussianBlur,
    /// and call gaussian_blur from operations.cpp, modifying the image directly.
    /// @throws std::invalid_argument if the cutoff is not an integer, or is outside [0, 255].
    void call_gaussian_blur(
        Image& image,
        char**& current_pos,
        char** end
    ) {
        if(current_pos + 2 >= end) {
            throw std::invalid_argument("\"--gaussian-blur\" requires a supplied radius and sigma argument");
        }

        std::string radius_text{*++current_pos};
        std::string sigma_text{*++current_pos};
        std::size_t parsed_radius{0};
        std::size_t parsed_sigma{0};
        int radius{std::stoi(radius_text, &parsed_radius)};
        double sigma{std::stod(sigma_text, &parsed_sigma)};

        if(parsed_radius != radius_text.size()) {
            throw std::invalid_argument("Radius must be a nonnegative integer");
        }

        if(parsed_sigma != sigma_text.size()) {
            throw std::invalid_argument("Sigma must be supplied as a decimal value");
        }

        image = gaussian_blur(image, radius, sigma);
    }


    /// Call the sobel operation and reassign the local image.
    void call_sobel(Image& image) {
        image = sobel(image); // convert to sobel image
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

                        // Parses 0 positional arguments
                        case Operation::Grayscale: {
                            call_grayscale(image);
                            break;
                        }

                        // Parses 0 positional arguments
                        case Operation::BoxBlur: {
                            call_box_blur(image);
                            break;
                        }

                        // Parses 2 positional arguments
                        case Operation::GaussianBlur: {
                            call_gaussian_blur(image, current_pos, end);
                            break;
                        }

                        // Parses 0 positional argument
                        case Operation::Sobel: {
                            call_sobel(image);
                            break;
                        }
                    }

                    // Move ptr to next char* in argv[]
                    ++current_pos;
                }
                    
                save_png(image, output_path);
                std::cout << ("Saved to " + output_path + ".\n");
                break;
            }
        }
    } catch (const std::exception& error) {
        std::cout << usage << "\n\n";
        std::cerr << ("ERROR: " + static_cast<std::string>(error.what())) << "\n\n";
        return 1;
    }
}