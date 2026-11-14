#pragma once

#include <string>
#include <string_view>

// Represents the method, request target, and HTTP version from a request line.
struct RequestLine {
    std::string method;
    std::string target;
    std::string version;
};

// Parses an HTTP request line into its method, target, and version components.
RequestLine parse_request_line(std::string_view line);