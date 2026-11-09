#include "http/header.hpp"
#include "http/http_utils.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <bitset>
#include <charconv>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
    // Byte limit for Content-Length
    constexpr std::size_t max_body_bytes{12 * 1024 * 1024};

    // Validates header name field of request line
    bool has_invalid_header_name_char(std::string_view header_name) {  

        // Check for whitespace before delimiting colon character
        if(header_name.back() == ' ') {
            return true;
        }

        // Compare characters against allowlist for valid header name characters
        return std::any_of(
            header_name.begin(), header_name.end(),
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
            header_value.begin(), header_value.end(),
            [](unsigned char c) {
                return (c < 32 && c != '\t') || c == 127;
            }
        );
    }

    // Validate hostname
    bool is_valid_hostname(std::string_view hostname) {
        // Allow one optional trailing dot
        if(!hostname.empty() && hostname.back() == '.') {
            hostname.remove_suffix(1);
        }

        // Validate hostname length constraints (1-253 characters)
        if(hostname.empty() || hostname.size() > 253) {
            throw std::invalid_argument("Invalid hostname length");
        }

        // Define local helper function
        const auto is_alphanumeric = [](unsigned char c) {
            return (c >= 'A' && c <= 'Z')
                || (c >= 'a' && c <= 'z')
                || (c >= '0' && c <= '9');
        };

        std::size_t start{0};

        while(start < hostname.size()) {
            const auto dot = hostname.find('.', start);
            const auto end = dot ==  std::string::npos
                ? hostname.size()
                : dot;

            // Process each label separated by dots individually
            const auto label = hostname.substr(start, end - start);

            if(label.empty() || label.size() > 63) {
                throw std::invalid_argument("Invalid hostname label length");
            }

            if(!is_alphanumeric(label.front())
            || !is_alphanumeric(label.back())) {
                throw std::invalid_argument(
                    "Hostname must start and end with a letter or digit"
                );
            }

            if(!std::all_of(
                label.begin(), label.end(),
                [&](unsigned char c) {
                    return is_alphanumeric(c) || c == '-'; 
                })) {
                throw std::invalid_argument("Invalid hostname chracter");
            }

            if(dot == std::string::npos) {
                break;
            }

            // Look at next label in hostname
            start = dot + 1;

            // Reject empty final label after removing 
            // one trailing dot (if another dot exists)
            if(start == hostname.size()) {
                throw std::invalid_argument("Empty hostname label");
            }
        }
    }


    // Validate IPv4 address
    bool is_valid_ipv4(std::string_view addr_text) {
        
        // InetPtonA requires null-terminated string--convert to std::string
        const std::string address{addr_text};
        IN_ADDR parsed{};

        return InetPtonA(
            AF_INET,
            address.c_str(),
            &parsed
        ) == 1;
    }


    // Validate IPv6 address
    bool is_valid_ipv6(std::string_view addr_text) {
        
        // InetPtonA requires null-terminated string--convert to std::string
        const std::string address{addr_text};
        IN6_ADDR parsed{};

        return InetPtonA(
            AF_INET6,
            address.c_str(),
            &parsed
        ) == 1;
    }


    std::uint16_t parse_address_port(std::string_view value) {
        if(std::any_of(
            value.begin(),
            value.end(),
            [](unsigned char c) {
                return (c < '0' || c > '9');
            }
        )) {
            throw std::invalid_argument("Address port must be a valid port number");
        }

        int port;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), port);

        // Ensure all characters were consumed in conversion (e.g. ptr at end of string)
        if(ec != std::errc() || ptr != value.data() + value.size()) {
            throw std::invalid_argument("Port number must be a valid number");
        }

        // Valid ports are 0 - 65535
        if(port < 0 || port > 65535) {
            throw std::invalid_argument("Port number must be in the range 0 - 65535");
        }

        // std::uint16_t guarantees 16 bits to represent port (0-65535)
        return static_cast<std::uint16_t>(port);
    }


    // Validate value of Host header
    void validate_host(std::string_view host) {
        if(host.empty()) {
            throw std::invalid_argument("Host value cannot be empty");
        }


        const bool is_ipv6{host.front() == '['};
        std::string_view port;
    
        if(is_ipv6) {
            // find ipv6 terminating bracket
            const auto closing_bracket = host.find(']');

            if(closing_bracket == std::string::npos) {
                throw std::invalid_argument("Missing closing IPv6 bracket");
            }

            // Exclude enclosing brackets
            std::string_view ipv6_address{host.substr(1, closing_bracket - 1)};
            if(!is_valid_ipv6(ipv6_address)) {
                throw std::invalid_argument("IPv6 address as hostname is malformed");
            }

            // Either pointing to colon or no character
            std::size_t hostname_end{closing_bracket + 1};

            if(hostname_end == host.size())         return; // No port attached to hostname, report valid host header (containing only hostname)
            else if(host.at(hostname_end) == ':')   port = host.substr(hostname_end + 1); // Get port string as host substring 
            else                                    throw std::invalid_argument("Missing colon delimiter between hostname and port");
        } else {

            // Might not exist, then just take whole string as IPv4 address or hostname
            const auto delimiter = host.find(':');

            const bool is_ipv4{
                host.find('.') != std::string::npos
                && std::all_of(
                    host.begin(),
                    host.end(),
                    [](unsigned char c) {
                        return (c >= '0' && c <= '9' || c == '.');
                    })
            };

            if(is_ipv4) {
                std::string_view ipv4_address{host.substr(0, delimiter)};
                if(!is_valid_ipv4(ipv4_address)) {
                    throw std::invalid_argument("IPv4 address as hostname is malformed");
                }
            } else {
                std::string_view hostname{host.substr(0, delimiter)};
                if(!is_valid_hostname(hostname)) {
                    throw std::invalid_argument("Hostname is malformed");
                }


            }

            if(delimiter == std::string::npos) return; // Host header only contains valid hostname, report valid host header (containing only hostname)
            else                               port = host.substr(delimiter + 1); // Get port as host substring
        }

        // Attempts to parse an address port,
        // throw an internal error on failure
        parse_address_port(port);
    }

    // Validate and convert Content-Length header value
    std::size_t parse_content_length(std::string_view value) {
        if(std::any_of(
            value.begin(),
            value.end(),
            [](unsigned char c) {
                return (c < 48 || c > 57);
            }
        )) {
            throw std::invalid_argument("Content-Length must be a valid number");
        }

        // value is guaranteed to only have integers (validated above)
        std::size_t content_length;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), content_length);

        // Ensure all characters were consumed in conversion (e.g. ptr at end of string)
        if(ec != std::errc() || ptr != value.data() + value.size()) {
            throw std::invalid_argument("Content-Length must be a valid number");
        }

        if(content_length > max_body_bytes) {
            throw std::invalid_argument("Content-Length exceeds maximum 12MB limit");
        }

        return content_length;
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

    if(name.empty()) {
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

RequestFraming validate_request_headers(
    const std::vector<Header>& headers
) {

    // Track and reject duplicate headers 
    std::bitset<header_count> visited_headers;
    std::optional<std::size_t> content_length;

    auto check_duplicate = [&](HeaderType header) {
        auto idx = to_int(header);
        if(visited_headers.test(idx)) {
            throw std::runtime_error("Cannot contain duplicate " + header_to_string(header) + " header");
        }
        visited_headers.set(idx);
    };

    // Process and validate each header passed into the function via the vector 
    for(const auto& [name, value] : headers) {

        // Preserve header name, but retrieve enum type
        HeaderType type{classify_header(name)};

        switch(type) {
            case HeaderType::Host: {
                check_duplicate(type);
                validate_host(value);
                continue;
            }
            case HeaderType::ContentLength: {
                content_length = parse_content_length(value);
                check_duplicate(type);
                continue;
            }
            case HeaderType::TransferEncoding: {
                throw std::invalid_argument("Transfer-Encoding is unsupported");
            }
            case HeaderType::Other: {
                // Do not check for duplicates for all other headers
                // since enters this case more than once
            }
        }
    }

    return RequestFraming{
        content_length
    };
}