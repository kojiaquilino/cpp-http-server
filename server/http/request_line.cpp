#include "http/request_line.hpp"

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <string_view>

namespace {
    bool has_special_characters(std::string_view str) {
        return std::any_of(str.begin(), str.end(), [](unsigned char c) {
            return c > 127; // Check per character if ascii value > 127 (range of valid chars)
        });
    }
}


RequestLine parse_request_line(std::string_view line) {
    auto fields = std::views::split(line, ' '); // Split delimited by ' '
    auto num_fields = std::ranges::distance(fields);

    // Reject if != 3 fields in request line
    if(num_fields != 3) {
        throw std::invalid_argument("Malformed request line in incoming HTTP request");
    }

    RequestLine r{};

    // Iterate over fields of request line
    for(size_t index{0}; const auto f : fields) {
        // Place request line field into std::string_view
        const std::string_view request_field{std::string_view(f.begin(), f.end())};

        if(request_field.size() == 0) {
            throw std::invalid_argument("Request fields in HTTP request must be nonempty");
        }

        if(has_special_characters(request_field)) {
            throw std::invalid_argument("Request fields in HTTP request must not contain special characters");
        }

        // Different checks based on request line field
        switch(index) {
            case 0: {
                r.method = request_field;    
            }
            case 1: {
                if(request_field.front() != '/') {
                    throw std::invalid_argument("Requested path must start with '/'");
                }
                r.target = request_field;   
            }
            case 2: {
                if(request_field != "HTTP/1.1") {
                    throw std::invalid_argument("HTTP requests require version HTTP/1.1");
                }
                r.version = request_field;  
            }
        }
    }

    return r;
}