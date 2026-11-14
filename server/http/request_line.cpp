#include "http/error.hpp"
#include "http/request_line.hpp"

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
    // Max size of a request line (-2 to reflect 2 bytes for CRLF control chars)
    constexpr int max_request_line_bytes{4096 - 2};
    const std::vector<std::string> allowed_methods{"GET", "POST"};

    // Validates method field of request line
    bool has_invalid_method_character(std::string_view method) {
        return std::any_of(
            method.begin(),
            method.end(),
            [](unsigned char c) {
                return !(c >= 'A' && c <= 'Z')
                    && !(c >= 'a' && c <= 'z')
                    && !(c >= '0' && c <= '9')
                    && std::string_view{"!#$%&'*+-.^_`|~"}.find(c) == std::string_view::npos; // Use allowlist
            }
        );
    }

    // Validates target field of request line
    bool has_invalid_target_character(std::string_view target) {
        return std::any_of(
            target.begin(),
            target.end(),
            [](unsigned char c) {
                return c <= 32 || c >= 127;
            }
        );
    }
}

RequestLine parse_request_line(std::string_view line) {

    // Reject request lines exceeding 4KiB limit
    if(line.size() > max_request_line_bytes) {
        throw http_errors::uri_too_long("HTTP request line exceeds limit.");
    }


    auto fields = std::views::split(line, ' '); // Split delimited by ' '
    auto num_fields = std::ranges::distance(fields);

    // Reject if != 3 fields in request line
    if(num_fields != 3) {
        throw http_errors::bad_request("Malformed request line in incoming HTTP request.");
    }

    RequestLine r{};

    // Iterate over fields of request line
    for(size_t index{0}; const auto f : fields) {
        // Place request line field into std::string_view
        const std::string_view field{std::string_view(f.begin(), f.end())};

        if(field.size() == 0) {
            throw http_errors::bad_request("Request fields in HTTP request must be nonempty.");
        }

        // Different checks based on request line field
        switch(index) {
            case 0: { // HTTP Method
                // Validate all characters in method
                if(has_invalid_method_character(field)) {
                    throw http_errors::bad_request("Request line contains an invalid HTTP method.");
                } else if(std::find(allowed_methods.begin(), allowed_methods.end(), field) == allowed_methods.end()) {
                    throw http_errors::not_implemented("This server supports only GET and POST.");
                }

                r.method = field;
                break;
            }
            case 1: { // HTTP Target Path
                // Must start with '/'
                if(field.front() != '/') {
                    throw http_errors::bad_request("Requested path must start with '/'.");
                }

                // Validate all characters in target/path
                if(has_invalid_target_character(field)) {
                    throw http_errors::bad_request("Request line contains an invalid HTTP target.");
                }

                r.target = field;
                break;   
            }
            case 2: { // HTTP Version
                if(field != "HTTP/1.1") {
                    throw http_errors::bad_request("HTTP requests require version HTTP/1.1.");
                }
                r.version = field;  
                break;
            }
        }

        ++index;
    }

    return r;
}