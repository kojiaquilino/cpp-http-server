#include "http/error.hpp"
#include "http/header.hpp"
#include "http/http_utils.hpp"

#ifdef _WIN32
#include <winsock2.h> 
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>  // IP address conversion functions
#include <netinet/in.h> // Internet address structures and constants
#endif

#include <algorithm>
#include <bitset>
#include <charconv>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
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
    void validate_hostname(std::string_view hostname) {
        // Allow one optional trailing dot
        if(!hostname.empty() && hostname.back() == '.') {
            hostname.remove_suffix(1);
        }

        // Validate hostname length constraints (1-253 characters)
        if(hostname.empty() || hostname.size() > 253) {
            throw http_errors::bad_request("Invalid hostname length");
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
                throw http_errors::bad_request("Invalid hostname label length");
            }

            if(!is_alphanumeric(label.front())
            || !is_alphanumeric(label.back())) {
                throw http_errors::bad_request(
                    "Hostname must start and end with a letter or digit"
                );
            }

            if(!std::all_of(
                label.begin(), label.end(),
                [&](unsigned char c) {
                    return is_alphanumeric(c) || c == '-'; 
                })) {
                throw http_errors::bad_request("Invalid hostname chracter");
            }

            if(dot == std::string::npos) {
                break;
            }

            // Look at next label in hostname
            start = dot + 1;

            // Reject empty final label after removing 
            // one trailing dot (if another dot exists)
            if(start == hostname.size()) {
                throw http_errors::bad_request("Empty hostname label");
            }
        }
    }


    // Validate IPv4 address
    bool is_valid_ipv4(std::string_view addr_text) {
        // InetPtonA requires null-terminated string--convert to std::string
        const std::string address{addr_text};

#ifdef _WIN32
        IN_ADDR parsed{};

        return InetPtonA(
            AF_INET,
            address.c_str(),
            &parsed
        ) == 1;
#else
        in_addr parsed{};

        return inet_pton(
            AF_INET,
            address.c_str(),
            &parsed
        ) == 1;
#endif  
    }


    // Validate IPv6 address
    bool is_valid_ipv6(std::string_view addr_text) {
        
        // InetPtonA requires null-terminated string--convert to std::string
        const std::string address{addr_text};
        
#ifdef _WIN32
        IN6_ADDR parsed{};

        return InetPtonA(
            AF_INET6,
            address.c_str(),
            &parsed
        ) == 1;
#else
        in6_addr parsed{};

        return inet_pton(
            AF_INET6,
            address.c_str(),
            &parsed
        ) == 1;
#endif  
    }

    // Validate address port
    std::uint16_t parse_address_port(std::string_view value) {
        if(std::any_of(
            value.begin(),
            value.end(),
            [](unsigned char c) {
                return (c < '0' || c > '9');
            }
        )) {
            throw http_errors::bad_request("Address port must be a valid port number");
        }

        int port;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), port);

        // Ensure all characters were consumed in conversion (e.g. ptr at end of string)
        if(ec != std::errc() || ptr != value.data() + value.size()) {
            throw http_errors::bad_request("Port number must be a valid number");
        }

        // Valid ports are 0 - 65535
        if(port < 0 || port > 65535) {
            throw http_errors::bad_request("Port number must be in the range 0 - 65535");
        }

        // std::uint16_t guarantees 16 bits to represent port (0-65535)
        return static_cast<std::uint16_t>(port);
    }


    // Validate value of Host header
    void validate_host(std::string_view host_authority) {
        if(host_authority.empty()) {
            throw http_errors::bad_request("Host value cannot be empty.");
        }


        const bool is_ipv6{host_authority.front() == '['};
        std::string_view port;
    
        if(is_ipv6) {
            // find ipv6 terminating bracket
            const auto closing_bracket = host_authority.find(']');

            if(closing_bracket == std::string::npos) {
                throw http_errors::bad_request("Missing closing IPv6 bracket");
            }

            // Exclude enclosing brackets
            std::string_view ipv6_address{host_authority.substr(1, closing_bracket - 1)};
            if(!is_valid_ipv6(ipv6_address)) {
                throw http_errors::bad_request("IPv6 address as hostname is malformed");
            }

            // Either pointing to colon or no character
            std::size_t hostname_end{closing_bracket + 1};

            if(hostname_end == host_authority.size())       return; // No port attached to hostname, report valid host header (containing only hostname)
            else if(host_authority.at(hostname_end) == ':') port = host_authority.substr(hostname_end + 1); // Get port string as host substring 
            else                                            throw http_errors::bad_request("Missing colon delimiter between hostname and port");
        } else {

            // Might not exist, then just take whole string as IPv4 address or hostname
            const auto delimiter = host_authority.find(':');

            // Represented either as a hostname of IPv4 address
            std::string_view host{host_authority.substr(0, delimiter)};

            // Check whether the host is in IPv4 format or as a hostname
            const bool is_ipv4{
                host.find('.') != std::string::npos
                && std::all_of(
                    host.begin(),
                    host.end(),
                    [](unsigned char c) {
                        return (c >= '0' && c <= '9' || c == '.');
                    })
            };

            // Validate IPv4 foramt
            if(is_ipv4) {
                if(!is_valid_ipv4(host)) {
                    throw http_errors::bad_request("IPv4 address as hostname is malformed");
                }

            // Validate hostname format
            } else {
                validate_hostname(host);
            }

            if(delimiter != std::string::npos) port = host_authority.substr(delimiter + 1); // Get port as host substring
            else return;                                                                    // host_authority only contains valid hostname
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
            throw http_errors::bad_request("Content-Length must be a valid number");
        }

        // value is guaranteed to only have integers (validated above)
        std::size_t content_length;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), content_length);

        // Ensure all characters were consumed in conversion (e.g. ptr at end of string)
        if(ec != std::errc() || ptr != value.data() + value.size()) {
            throw http_errors::bad_request("Content-Length must be a valid number");
        }

        if(content_length > max_body_bytes) {
            throw http_errors::content_too_large("Content-Length exceeds maximum 12MB limit");
        }

        return content_length;
    }
}

Header parse_header_line(std::string_view line) {
    std::size_t split{line.find(':')};
    
    if(split == std::string::npos) {
        throw http_errors::bad_request("Malformed HTTP header: header is malformed without a separator.");
    }

    std::string name{line.substr(0, split)};
    std::string value{line.substr(split + 1)}; // exclude colon separator


    //// Validating Header name ////

    if(name.empty()) {
        throw http_errors::bad_request("Malformed HTTP header: header cannot be empty.");
    }

    // Check if contains illegal char
    if(has_invalid_header_name_char(name)) {
        throw http_errors::bad_request("Malformed HTTP header: header name contains an illegal symbol.");
    }
    
    // Check if leading/trailing whitespace on header
    if(std::isspace(name.front()) || std::isspace(name.back())) {
        throw http_errors::bad_request("Malformed HTTP header: cannot have whitespace before or after header.");
    }

    //// Validating Header value ////

    HTTPUtils::trim(value); // Trims in-place

    // Check if contains illegal char
    if(has_invalid_header_value_char(value)) {
        throw http_errors::bad_request("Malformed HTTP header: header value contains an illegal symbol.");
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
    using enum HeaderType;

    // Track and reject duplicate headers 
    std::bitset<header_count> visited_headers;
    std::optional<std::size_t> content_length;

    auto check_duplicate = [&](HeaderType header) {
        auto idx = HTTPUtils::to_int(header);

        if(visited_headers.test(idx)) {
            // can add conditionals here for headers that allow duplicates
            throw http_errors::bad_request("Cannot contain duplicate " + HTTPUtils::header_to_string(header) + " header.");
        }
        visited_headers.set(idx);
    };

    // Process and validate each header passed into the function via the vector
    for(const auto& [name, value] : headers) {

        // Preserve header name, but retrieve enum type
        HeaderType type{HTTPUtils::classify_header(name)};

        switch(type) {
            case Host: {
                check_duplicate(type);
                validate_host(value);
                continue;
            }
            case ContentLength: {
                check_duplicate(type);
                content_length = parse_content_length(value);
                continue;
            }
            case TransferEncoding: {
                check_duplicate(type);
                continue; // Rejected later after parsing all headers
            }
            case Allow: {
                check_duplicate(type);
                continue;   
            }
            case Expect: {
                throw http_errors::expectation_failed("Expect is unsupported.");
            }
            case HeaderType::Other: {
                // Do not check for duplicates for all other headers
                // since enters this case more than once
            }
        }
    }

    // If both Content-Length and Transfer-Encoding present in header, return bad request
    if(visited_headers.test(HTTPUtils::to_int(HeaderType::ContentLength))
    && visited_headers.test(HTTPUtils::to_int(HeaderType::TransferEncoding))) {
        throw http_errors::bad_request("Transfer-Encoding and Content-Length must not appear together.");
    }


    // Throw Transfer-Encoding unimplemented error here
    if(visited_headers.test(HTTPUtils::to_int(HeaderType::TransferEncoding))) {
        throw http_errors::not_implemented("Transfer-Encoding is unsupported.");
    }

    if(!visited_headers.test(HTTPUtils::to_int(HeaderType::Host))) {
        throw http_errors::bad_request("HTTP Request header requires at least one \"Host\" header");
    }

    return RequestFraming{
        content_length
    };
}