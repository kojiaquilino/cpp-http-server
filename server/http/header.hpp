#pragma once

#include <string>
#include <string_view>

struct Header {
    std::string name;
    std::string value;
};

Header parse_header_line(std::string_view line);