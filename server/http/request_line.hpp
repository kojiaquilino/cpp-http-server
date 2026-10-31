#pragma once

#include <string>
#include <string_view>

struct RequestLine {
    std::string method;
    std::string target;
    std::string version;
};

RequestLine parse_request_line(std::string_view line);