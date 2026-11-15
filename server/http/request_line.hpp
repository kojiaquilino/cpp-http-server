#pragma once

#include <string>
#include <string_view>
#include <vector>

// 4096 minus 2 control characters CR + LF
constexpr std::size_t max_request_line_bytes{4094};
const std::vector<std::string> supported_methods{"GET", "POST"};

// Represents the method, request target, and HTTP version from a request line.
struct RequestLine {
    std::string method;
    std::string target;
    std::string version;
};

namespace request_line {
    // Validates method field of request line
    bool has_invalid_method_character(std::string_view method);
    // Validates target field of request line
    bool has_invalid_target_character(std::string_view target);
}

// Parses an HTTP request line into its method, target, and version components.
RequestLine parse_request_line(std::string_view line);