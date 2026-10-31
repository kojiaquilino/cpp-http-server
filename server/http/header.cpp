#include "http/header.hpp"
#include "http/http_utils.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace {
    // Validates header name field of request line
    bool has_invalid_header_name_char(std::string_view header_name) {  

        // Check for whitespace before delimiting colon character
        if(header_name.back() == ' ') {
            return true;
        }

        // Compare characters against allowlist for valid header name characters
        return std::any_of(
            header_name.begin(),
            header_name.end(),
            [](unsigned char c) {
                return !(c >= 'A' && c <= 'Z')
                    && !(c >= 'a' && c <= 'z')
                    && !(c >= '0' && c <= '9')
                    && std::string_view{"!#$%&'*+-.^_`|~"}.find(c) == std::string_view::npos; // Use allowlist
            }
        );
    }


    // Validates header value field of request line
    bool has_invalid_header_value_char(std::string_view header_value) {
        return std::any_of(
            header_value.begin(),
            header_value.end(),
            [](unsigned char c) {
                return (c < 32 && c != '\t') || c == 127;
            }
        );
    }
}


Header parse_header_line(std::string_view line) {
    std::size_t split{line.find(':')};
    
    if(split == std::string::npos) {
        throw std::invalid_argument("Invalid HTTP header: header is malformed without a separator");
    }

    std::string name{line.substr(0, split)};
    std::string value{line.substr(split + 1)}; // exclude colon separator


    //// Validating Header name ////

    // Check if empty
    if(name.size() == 0) {
        throw std::invalid_argument("Invalid HTTP header: header cannot be empty");
    }

    // Check if contains illegal char
    if(has_invalid_header_name_char(name)) {
        throw std::invalid_argument("Invalid HTTP header: header name contains an illegal symbol");
    }
    
    // Check if leading/trailing whitespace on header
    if(std::isspace(name.front()) || std::isspace(name.back())) {
        throw std::invalid_argument("Invalid HTTP header: cannot have whitespace before or after header");
    }

    // Convert header name to lowercase
    for(auto& ch : name) {
        ch = std::tolower(ch);
    }

    //// Validating Header value ////

    trim(value); // Trims in-place

    // Check if contains illegal char
    if(has_invalid_header_value_char(value)) {
        throw std::invalid_argument("Invalid HTTP header: header value contains an illegal symbol");
    }

    // Return new header after values are validated and sanitized
    return Header{
        name,
        value
    };
}